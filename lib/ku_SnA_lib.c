#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>

#define DEV_NAME "ku_sna_dev"
#define IOCTL_START_NUM 0x80
#define IOCTL_NUM1 IOCTL_START_NUM+1
#define IOCTL_NUM2 IOCTL_START_NUM+2
#define IOCTL_NUM3 IOCTL_START_NUM+3
#define SIMPLE_IOCTL_NUM 'z'
#define KU_SNA_FORWARD _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM1, unsigned long *)
#define KU_SNA_BACKWARD _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM2, unsigned long *)
#define KU_SNA_STOP _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM3, unsigned long *)
#define true 1
#define false 0

struct argu {
    int speed_index;
    int time_limit;
};

int go_forward(int speed_index) {
    speed_index=speed_index%3;
    struct argu arg;
    arg.speed_index = speed_index;
    arg.time_limit = false;

    int dev = open("/dev/ku_sna_dev", O_RDWR);
	if (dev==-1)    {
        printf("unable to open device\n");
        return -1;
    }
    ioctl(dev, KU_SNA_FORWARD, &arg);
    close(dev);
    return 0;
}

int go_backward(int speed_index) {
    speed_index=speed_index%3;
    struct argu arg;
    arg.speed_index = speed_index;
    arg.time_limit = false;

    int dev = open("/dev/ku_sna_dev", O_RDWR);
	if (dev==-1)    {
        printf("unable to open device\n");
        return -1;
    }
    ioctl(dev, KU_SNA_BACKWARD, &arg);
    close(dev);
    return 0;
}

int keep_go_forward(int speed_index) {
    speed_index=speed_index%3;
    struct argu arg;
    arg.speed_index = speed_index;
    arg.time_limit = true;

    int dev = open("/dev/ku_sna_dev", O_RDWR);
	if (dev==-1)    {
        printf("unable to open device\n");
        return -1;
    }
    ioctl(dev, KU_SNA_FORWARD, &arg);
    close(dev);
    return 0;
}


int keep_go_backward(int speed_index) {
    speed_index=speed_index%3;
    struct argu arg;
    arg.speed_index = speed_index;
    arg.time_limit = true;

    int dev = open("/dev/ku_sna_dev", O_RDWR);
	if (dev==-1)    {
        printf("unable to open device\n");
        return -1;
    }
    ioctl(dev, KU_SNA_BACKWARD, &arg);
    close(dev);
    return 0;
}
