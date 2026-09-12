#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *device = argc > 1 ? argv[1] : "/dev/input/event9";
    int duration_ms = argc > 2 ? atoi(argv[2]) : 3000;
    int fd = open(device, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "OPEN_FAILED device=%s errno=%d message=%s\n",
                device, errno, strerror(errno));
        return 2;
    }

    if (ioctl(fd, EVIOCGRAB, 1) != 0) {
        fprintf(stderr, "GRAB_FAILED device=%s errno=%d message=%s\n",
                device, errno, strerror(errno));
        close(fd);
        return 3;
    }

    printf("GRAB_OK device=%s duration_ms=%d\n", device, duration_ms);
    fflush(stdout);

    struct pollfd poll_fd = {.fd = fd, .events = POLLIN};
    int remaining_ms = duration_ms;
    while (remaining_ms > 0) {
        int slice_ms = remaining_ms > 250 ? 250 : remaining_ms;
        int ready = poll(&poll_fd, 1, slice_ms);
        remaining_ms -= slice_ms;
        if (ready < 0 && errno != EINTR) {
            fprintf(stderr, "POLL_FAILED errno=%d message=%s\n",
                    errno, strerror(errno));
            break;
        }
        if (ready > 0 && (poll_fd.revents & POLLIN)) {
            struct input_event event;
            ssize_t bytes = read(fd, &event, sizeof(event));
            if (bytes == (ssize_t)sizeof(event)) {
                printf("EVENT type=%u code=%u value=%d\n",
                        event.type, event.code, event.value);
                fflush(stdout);
            }
        }
    }

    int release_result = ioctl(fd, EVIOCGRAB, 0);
    printf("RELEASE_%s\n", release_result == 0 ? "OK" : "FAILED");
    close(fd);
    return release_result == 0 ? 0 : 4;
}
