#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "OPEN_FAILED errno=%d message=%s\n", errno, strerror(errno));
        return 2;
    }

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) != 0
            || ioctl(fd, UI_SET_KEYBIT, KEY_TAB) != 0
            || ioctl(fd, UI_SET_KEYBIT, KEY_LEFTALT) != 0) {
        fprintf(stderr, "CAPABILITY_FAILED errno=%d message=%s\n", errno, strerror(errno));
        close(fd);
        return 3;
    }

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x18d1;
    setup.id.product = 0x5742;
    snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "WorkBuddy Virtual Keyboard Probe");

    if (ioctl(fd, UI_DEV_SETUP, &setup) != 0 || ioctl(fd, UI_DEV_CREATE) != 0) {
        fprintf(stderr, "CREATE_FAILED errno=%d message=%s\n", errno, strerror(errno));
        close(fd);
        return 4;
    }

    printf("UINPUT_CREATE_OK\n");
    fflush(stdout);
    usleep(300000);
    int destroy_result = ioctl(fd, UI_DEV_DESTROY);
    printf("UINPUT_DESTROY_%s\n", destroy_result == 0 ? "OK" : "FAILED");
    close(fd);
    return destroy_result == 0 ? 0 : 5;
}
