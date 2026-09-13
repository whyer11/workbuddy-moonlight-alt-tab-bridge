#include "touchpad_bridge.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int find_touchpad(char *path, size_t path_size) {
    for (int index = 0; index < 64; ++index) {
        snprintf(path, path_size, "/dev/input/event%d", index);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        char name[256] = {0};
        int name_length = ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        if (name_length >= 0 && strcmp(name, "Xiaomi Touch") == 0) {
            return fd;
        }
        close(fd);
    }
    path[0] = '\0';
    errno = ENODEV;
    return -1;
}

static int configure_virtual_mouse(int fd) {
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) != 0
            || ioctl(fd, UI_SET_KEYBIT, BTN_LEFT) != 0
            || ioctl(fd, UI_SET_EVBIT, EV_REL) != 0
            || ioctl(fd, UI_SET_RELBIT, REL_X) != 0
            || ioctl(fd, UI_SET_RELBIT, REL_Y) != 0) {
        return -1;
    }

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x2717;
    setup.id.product = 0x5743;
    setup.id.version = 1;
    snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "WorkBuddy Virtual Mouse");
    if (ioctl(fd, UI_DEV_SETUP, &setup) != 0 || ioctl(fd, UI_DEV_CREATE) != 0) {
        return -1;
    }
    return 0;
}

static int write_event(int fd, unsigned short type, unsigned short code, int value) {
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    ssize_t bytes = write(fd, &event, sizeof(event));
    return bytes == (ssize_t)sizeof(event) ? 0 : -1;
}

static int emit_output(int fd, struct wb_touchpad_drag_output output) {
    if (output.actions == WB_DRAG_NONE) {
        return 0;
    }
    if ((output.actions & WB_DRAG_LEFT_DOWN) != 0
            && write_event(fd, EV_KEY, BTN_LEFT, 1) != 0) {
        return -1;
    }
    if ((output.actions & WB_DRAG_MOVE) != 0) {
        if (output.dx != 0 && write_event(fd, EV_REL, REL_X, output.dx) != 0) {
            return -1;
        }
        if (output.dy != 0 && write_event(fd, EV_REL, REL_Y, output.dy) != 0) {
            return -1;
        }
    }
    if ((output.actions & WB_DRAG_LEFT_UP) != 0
            && write_event(fd, EV_KEY, BTN_LEFT, 0) != 0) {
        return -1;
    }
    return write_event(fd, EV_SYN, SYN_REPORT, 0);
}

void wb_touchpad_bridge_close(struct wb_touchpad_bridge *bridge) {
    if (bridge == NULL) {
        return;
    }
    if (bridge->virtual_fd >= 0) {
        struct wb_touchpad_drag_output output = wb_touchpad_drag_cancel(
                &bridge->drag_state);
        (void)emit_output(bridge->virtual_fd, output);
        if (bridge->virtual_created) {
            (void)ioctl(bridge->virtual_fd, UI_DEV_DESTROY);
        }
        close(bridge->virtual_fd);
    }
    if (bridge->physical_fd >= 0) {
        close(bridge->physical_fd);
    }
    bridge->physical_fd = -1;
    bridge->virtual_fd = -1;
    bridge->virtual_created = false;
    bridge->physical_path[0] = '\0';
}

int wb_touchpad_bridge_open(struct wb_touchpad_bridge *bridge,
                            struct wb_touchpad_drag_config config) {
    if (bridge == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(bridge, 0, sizeof(*bridge));
    bridge->physical_fd = -1;
    bridge->virtual_fd = -1;
    wb_touchpad_drag_init(&bridge->drag_state, config);

    bridge->physical_fd = find_touchpad(
            bridge->physical_path, sizeof(bridge->physical_path));
    if (bridge->physical_fd < 0) {
        return -1;
    }

    bridge->virtual_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (bridge->virtual_fd < 0 || configure_virtual_mouse(bridge->virtual_fd) != 0) {
        int saved_errno = errno;
        wb_touchpad_bridge_close(bridge);
        errno = saved_errno;
        return -1;
    }
    bridge->virtual_created = true;
    return 0;
}

int wb_touchpad_bridge_fd(const struct wb_touchpad_bridge *bridge) {
    return bridge == NULL ? -1 : bridge->physical_fd;
}

const char *wb_touchpad_bridge_path(const struct wb_touchpad_bridge *bridge) {
    return bridge == NULL ? "" : bridge->physical_path;
}

static int consume_event(struct wb_touchpad_bridge *bridge,
                         const struct input_event *event) {
    if (event->type == EV_ABS) {
        if (event->code == ABS_MT_SLOT) {
            wb_touchpad_drag_select_slot(&bridge->drag_state, event->value);
        } else if (event->code == ABS_MT_TRACKING_ID) {
            wb_touchpad_drag_set_tracking_id(&bridge->drag_state, event->value);
        } else if (event->code == ABS_MT_POSITION_X) {
            wb_touchpad_drag_set_x(&bridge->drag_state, event->value);
        } else if (event->code == ABS_MT_POSITION_Y) {
            wb_touchpad_drag_set_y(&bridge->drag_state, event->value);
        }
        return 0;
    }
    if (event->type != EV_SYN) {
        return 0;
    }
    if (event->code == SYN_DROPPED) {
        errno = EOVERFLOW;
        return -1;
    }
    if (event->code == SYN_REPORT) {
        return emit_output(bridge->virtual_fd,
                wb_touchpad_drag_finish_frame(&bridge->drag_state));
    }
    return 0;
}

int wb_touchpad_bridge_drain(struct wb_touchpad_bridge *bridge) {
    if (bridge == NULL || bridge->physical_fd < 0 || bridge->virtual_fd < 0) {
        errno = EINVAL;
        return -1;
    }
    for (;;) {
        struct input_event event;
        ssize_t bytes = read(bridge->physical_fd, &event, sizeof(event));
        if (bytes < 0 && errno == EINTR) {
            continue;
        }
        if (bytes < 0 && errno == EAGAIN) {
            return 0;
        }
        if (bytes != (ssize_t)sizeof(event)) {
            if (bytes == 0) {
                errno = ENODEV;
            } else if (bytes > 0) {
                errno = EIO;
            }
            return -1;
        }
        if (consume_event(bridge, &event) != 0) {
            return -1;
        }
    }
}
