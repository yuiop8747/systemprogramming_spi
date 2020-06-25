#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include "socket.h"

#define SPI_MAJOR_NUMBER 511
#define SPI_MINOR_NUMBER 111

#define LED_MAJOR_NUMBER 502
#define LED_MINOR_NUMBER 102
#define LED_DEV_PATH_NAME "/dev/led_dev"

#define SPI_DEV_PATH_NAME "/dev/spi_dev"

#define IOCTL_MAGIC_NUMBER          'l'
#define IOCTL_MAGIC_NUMBER_2 	    'j'

#define IOCTL_SPI_READ              _IOWR(IOCTL_MAGIC_NUMBER,1, int)
#define IOCTL_CMD_ON 	_IOWR(IOCTL_MAGIC_NUMBER_2,0,int)
#define IOCTL_CMD_OFF	_IOWR(IOCTL_MAGIC_NUMBER_2,1,int)

dev_t spi_dev,led_dev;
static int fd1,fd2;
struct Data {
    char* tbuf;
    char* rbuf;
    int len;
};

struct Data *buf;

int analog_read(int channel){
    buf->tbuf[0] = 0x06;
    buf->tbuf[1] = (channel & 0x07)<<6;
    buf->tbuf[2] = 0x0;
    buf->len = 3;
    ioctl(fd1, IOCTL_SPI_READ, &buf);
    return (buf->rbuf[2] | ((buf->rbuf[1] & 0x0F) << 8));
}

int spi_init(){
    buf = malloc(sizeof(struct Data));
    buf->rbuf = malloc(sizeof(char)*32);
    buf->tbuf = malloc(sizeof(char)*32);
    spi_dev = makedev(SPI_MAJOR_NUMBER, SPI_MINOR_NUMBER);
    mknod(SPI_DEV_PATH_NAME, S_IFCHR|0666, spi_dev);
    fd1 = open(SPI_DEV_PATH_NAME, O_RDWR);
    if(fd1<0){
        printf("fail to open spi\n");
        return -1;
    }
    
    return 0;
}
int led_init(){
    led_dev = makedev(LED_MAJOR_NUMBER, LED_MINOR_NUMBER);
    mknod(LED_DEV_PATH_NAME, S_IFCHR|0666, led_dev);

    fd2 = open(LED_DEV_PATH_NAME, O_RDWR);
    if(fd2<0){
        printf("fail to open led\n");
        return -1;
    }
    return 0;
}

int main(int argc,char *argv[]){
	Client(argv[1],atoi(argv[2]));
	if(spi_init() != 0)
	    return 0;
	if(led_init() != 0)
	    return 0;
	    
	while(1){
		sleep(1);
		float val = (float)analog_read(0);
		if(val < 3000){
		    printf("Flaim ! : 11 , LED : on\n");
		    ioctl(fd2,IOCTL_CMD_ON);
		    my_write("11");
		}
		else{
		    printf("It's OK : 10 , LED : off\n");
		    ioctl(fd2,IOCTL_CMD_OFF);
		    my_write("10");
		}
	}
	close(fd1);
	close(fd2);
	return 0;
}
