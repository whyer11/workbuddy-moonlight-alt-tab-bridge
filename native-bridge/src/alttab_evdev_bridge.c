#include "key_remap.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;

#define WB_BITS_PER_LONG (sizeof(unsigned long) * 8U)
#define WB_LONG_COUNT(bit_count) (((bit_count) + WB_BITS_PER_LONG - 1U) / WB_BITS_PER_LONG)

static void request_stop(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

static int find_keyboard(char *path, size_t path_size) {
    for (int index = 0; index < 64; ++index) {
        snprintf(path, path_size, "/dev/input/event%d", index);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        char name[256] = {0};
        int name_length = ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        if (name_length >= 0 && strcmp(name, "Xiaomi Keyboard") == 0) {
            return fd;
        }
        close(fd);
    }
    errno = ENODEV;
    return -1;
}

static bool bit_is_set(const unsigned long *bits, unsigned int bit) {
    return (bits[bit / WB_BITS_PER_LONG] & (1UL << (bit % WB_BITS_PER_LONG))) != 0;
}

static int configure_virtual_keyboard(int fd, int physical_fd) {
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) != 0) {
        return -1;
    }

    unsigned long key_bits[WB_LONG_COUNT(KEY_MAX + 1U)];
    memset(key_bits, 0, sizeof(key_bits));
    if (ioctl(physical_fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
        return -1;
    }
    for (int code = 0; code <= KEY_MAX; ++code) {
        if (bit_is_set(key_bits, (unsigned int)code)
                && ioctl(fd, UI_SET_KEYBIT, code) != 0) {
            return -1;
        }
    }
    if (ioctl(fd, UI_SET_KEYBIT, KEY_F12) != 0) {
        return -1;
    }
    if (ioctl(fd, UI_SET_EVBIT, EV_MSC) != 0
            || ioctl(fd, UI_SET_MSCBIT, MSC_SCAN) != 0
            || ioctl(fd, UI_SET_EVBIT, EV_LED) != 0) {
        return -1;
    }
    for (int code = 0; code <= LED_MAX; ++code) {
        if (ioctl(fd, UI_SET_LEDBIT, code) != 0) {
            return -1;
        }
    }

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x2717;
    setup.id.product = 0x5742;
    setup.id.version = 1;
    snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "WorkBuddy Virtual Keyboard");
    if (ioctl(fd, UI_DEV_SETUP, &setup) != 0 || ioctl(fd, UI_DEV_CREATE) != 0) {
        return -1;
    }
    return 0;
}

static int forward_event(int fd, const struct input_event *event) {
    ssize_t bytes = write(fd, event, sizeof(*event));
    return bytes == (ssize_t)sizeof(*event) ? 0 : -1;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    signal(SIGINT, request_stop);
    signal(SIGTERM, request_stop);

    char detected_path[64] = {0};
    const char *physical_path = NULL;
    int physical_fd = -1;
    if (argc > 1) {
        physical_path = argv[1];
        physical_fd = open(physical_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    } else {
        physical_fd = find_keyboard(detected_path, sizeof(detected_path));
        physical_path = detected_path;
    }
    if (physical_fd < 0) {
        fprintf(stderr, "PHYSICAL_OPEN_FAILED path=%s errno=%d message=%s\n",
                physical_path == NULL || physical_path[0] == '\0' ? "auto" : physical_path,
                errno,
                strerror(errno));
        return 2;
    }

    int virtual_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (virtual_fd < 0) {
        fprintf(stderr, "UINPUT_OPEN_FAILED errno=%d message=%s\n", errno, strerror(errno));
        close(physical_fd);
        return 3;
    }
    if (configure_virtual_keyboard(virtual_fd, physical_fd) != 0) {
        fprintf(stderr, "UINPUT_CREATE_FAILED errno=%d message=%s\n", errno, strerror(errno));
        close(virtual_fd);
        close(physical_fd);
        return 4;
    }

    usleep(350000);
    if (ioctl(physical_fd, EVIOCGRAB, 1) != 0) {
        fprintf(stderr, "PHYSICAL_GRAB_FAILED errno=%d message=%s\n", errno, strerror(errno));
        ioctl(virtual_fd, UI_DEV_DESTROY);
        close(virtual_fd);
        close(physical_fd);
        return 5;
    }

    printf("BRIDGE_READY physical=%s remap=ALT+TAB->ALT+F12\n", physical_path);

    struct key_remap_state remap_state = {0};
    struct input_event pending_scan;
    bool has_pending_scan = false;
    struct pollfd poll_fd = {.fd = physical_fd, .events = POLLIN};
    int result = 0;

    while (!stop_requested) {
        int ready = poll(&poll_fd, 1, 500);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "POLL_FAILED errno=%d message=%s\n", errno, strerror(errno));
            result = 6;
            break;
        }
        if (ready == 0) {
            continue;
        }

        struct input_event event;
        ssize_t bytes = read(physical_fd, &event, sizeof(event));
        if (bytes < 0 && (errno == EAGAIN || errno == EINTR)) {
            continue;
        }
        if (bytes != (ssize_t)sizeof(event)) {
            fprintf(stderr, "READ_FAILED bytes=%zd errno=%d message=%s\n",
                    bytes, errno, strerror(errno));
            result = 7;
            break;
        }

        if (event.type == EV_MSC && event.code == MSC_SCAN) {
            pending_scan = event;
            has_pending_scan = true;
            continue;
        }

        if (event.type == EV_KEY) {
            uint16_t original_code = event.code;
            uint16_t mapped_code = key_remap_apply(&remap_state, event.code, event.value);
            if (has_pending_scan && mapped_code == original_code) {
                if (forward_event(virtual_fd, &pending_scan) != 0) {
                    result = 8;
                    break;
                }
            }
            has_pending_scan = false;
            event.code = mapped_code;
            if (original_code == WB_KEY_TAB && mapped_code == WB_KEY_F12 && event.value == 1) {
                printf("REMAP_EVENT ALT+TAB->ALT+F12\n");
            }
        } else if (has_pending_scan) {
            if (forward_event(virtual_fd, &pending_scan) != 0) {
                result = 8;
                break;
            }
            has_pending_scan = false;
        }

        if (forward_event(virtual_fd, &event) != 0) {
            fprintf(stderr, "WRITE_FAILED errno=%d message=%s\n", errno, strerror(errno));
            result = 8;
            break;
        }
    }

    ioctl(physical_fd, EVIOCGRAB, 0);
    ioctl(virtual_fd, UI_DEV_DESTROY);
    close(virtual_fd);
    close(physical_fd);
    printf("BRIDGE_STOPPED result=%d\n", result);
    return result;
}
