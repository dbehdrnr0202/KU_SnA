#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include "ku_SnA_lib.h"

#define IOCTL_START_NUM 0x80
#define IOCTL_NUM1 IOCTL_START_NUM+1
#define IOCTL_NUM2 IOCTL_START_NUM+2
#define IOCTL_NUM3 IOCTL_START_NUM+3
#define SIMPLE_IOCTL_NUM 'z'
#define KU_SNA_FORWARD _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM1, unsigned long *)
#define KU_SNA_BACKWARD _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM2, unsigned long *)
#define KU_SNA_STOP _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM3, unsigned long *)

int main(int argc, char *argv[])    {
    int dev;
	int i, speedindex, op;

	if(argc != 3) {
		printf("Insert two arguments \n");
		printf("First argument = 1 : forward(for 30sec), 2 : backward(for 30sec), 3: forward, 4: backward\n");
		printf("Second argument = speed index : 0 ~ 2(bigger is slower)\n");
		return -1;
	}

	op = atoi(argv[1]);
	speedindex = atoi(argv[2]);
	speedindex=speedindex%3;

	dev = open("/dev/ku_sna_dev", O_RDWR);
	if (op == 1){
        go_forward(speedindex);
    } 
	else if(op == 2) {
        go_backward(speedindex);
    } 
	else if (op==3)	{
		keep_go_forward(speedindex);
	}
	else if (op==4)	{
		keep_go_backward(speedindex);
	}
	else {
		printf("Invalid Operation\n");
		close(dev);
		return -1;
	}
	close(dev);
	
	return 0;
}