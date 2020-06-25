#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#include <asm/mach/map.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#define GPIO_BASE_ADDR                  0x3F200000
#define BCM2835_SPI0_BASE               0x3F204000
#define GPFSEL0  0x00
#define GPFSEL1 0x04
#define GPFSEL2 0x08
#define GPSET0 0x1C
#define GPCLR0 0x28
#define GPLEV0 0x34

#define SPI_MAJOR_NUMBER 511
#define SPI_DEV_NAME "spi_dev"

#define IOCTL_MAGIC_NUMBER          'l'
#define IOCTL_SPI_READ              _IOWR(IOCTL_MAGIC_NUMBER,1, int)

#define BCM2835_SPI0_CS                      0x0000 	/* SPI Master Control and Status */
#define BCM2835_SPI0_FIFO                    0x0004 	/* SPI Master TX and RX FIFO */
#define BCM2835_SPI0_CLK                     0x0008 	/* SPI Master Clock Divider */
#define BCM2835_SPI0_CS_TXD                  1 << 18   /* TXD TX FIFO can accept Data */
#define BCM2835_SPI0_CS_RXD                  1 << 17   /* RXD RX FIFO contains Data */


struct Data {
    char* tbuf;
    char* rbuf;
    uint32_t len;
};
static void __iomem *gpio_base;
static void __iomem *spi0_base;
volatile unsigned int *gpsel0;
volatile unsigned int *gpsel1;
volatile unsigned int *gpsel2;
volatile unsigned int *gpset0;
volatile unsigned int *gpset1;
volatile unsigned int *gpclr1;
volatile unsigned int *gplev0;

volatile unsigned int *spi0_cs;
volatile unsigned int *spi0_fifo;
volatile unsigned int *spi0_clk;

int spi_open(struct inode *inode, struct file *filp){
    printk(KERN_ALERT "SPI driver open!\n");

    gpio_base = ioremap(GPIO_BASE_ADDR,0x60);
    spi0_base = ioremap(BCM2835_SPI0_BASE,0x60);

    gpsel0 = (volatile unsigned int *)(gpio_base + GPFSEL0);
    gpsel1 = (volatile unsigned int *)(gpio_base + GPFSEL1);
    gpsel2 = (volatile unsigned int *)(gpio_base + GPFSEL2);
    gpset1 = (volatile unsigned int *)(gpio_base + GPSET0);
    gpclr1 = (volatile unsigned int *)(gpio_base + GPCLR0);
    gplev0 = (volatile unsigned int *)(gpio_base + GPLEV0);

    spi0_cs   = (volatile unsigned int *)(spi0_base + BCM2835_SPI0_CS);
    spi0_fifo = (volatile unsigned int *)(spi0_base + BCM2835_SPI0_FIFO);
    spi0_clk  = (volatile unsigned int *)(spi0_base + BCM2835_SPI0_CLK);
    *gpsel0 |= (7<<(3*9)); //GPIO9
    *gpsel0 &= ~(3<<(3*9)); //MISO

    *gpsel1 |= (7); //GPIO10
    *gpsel1 &= ~(3); //MOSI

    *gpsel1 |= (7<<3); //GPIO11
    *gpsel1 &= ~(3<<3); //CLK

    *gpsel0 |= (7<<24); //GPIO8
    *gpsel0 &= ~(3<<24); //CE0 select 0 enable


    *spi0_cs = 0;
    *spi0_cs |= (3<<4); // chip clear
    *spi0_clk = 64; // clock setting
    //set_bits(spi0_cs,0,BCM2835_SPI0_CS_CS); // chip select 0
    //p_write_nb(spi0_cs, BCM2835_SPI0_CS_CLEAR);
    return 0;
}

int spi_release(struct inode *inode, struct file *filp){
    printk(KERN_ALERT "SPI driver closed\n");

    *gpsel0 |= (7<<(3*9)); //GPIO9
    *gpsel0 &= ~(7<<(3*9)); //MISO

    *gpsel1 |= (7); //GPIO10
    *gpsel1 &= ~(7); //MOSI

    *gpsel1 |= (7<<3); //GPIO11
    *gpsel1 &= ~(7<<3); //CLK

    *gpsel0 |= (7<<24); //GPIO8
    *gpsel0 &= ~(7<<24); //CE0 select 0 enable
    iounmap((void *)gpio_base);
    iounmap((void *)spi0_base);
    return 0;
}

long spi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    struct Data *buf;
    int RXCnt=0,TXCnt=0;
	printk(KERN_ALERT "ioctl start\n");
    copy_from_user( (void *)&buf, (void *)arg, sizeof(buf));
	printk(KERN_ALERT "----2\n");
            
    *spi0_cs |= (3<<4); // clear fifo clear : 11 setting
    *spi0_cs |= (1<<7); // transfer_active : 1 setting
	printk(KERN_ALERT "----3\n");
    int len = (buf->len);
    switch(cmd){
      case IOCTL_SPI_READ:
		while(TXCnt < 3 || RXCnt < 3){
			printk(KERN_ALERT "----4\n");
			while(((*spi0_cs) & BCM2835_SPI0_CS_TXD)&&(TXCnt < len)){
				*spi0_fifo = (buf->tbuf)[TXCnt]; // until fifo can accept data
				TXCnt++;
			
			printk(KERN_ALERT "----tx\n");
			}
			while(((*spi0_cs) & BCM2835_SPI0_CS_RXD)&&(RXCnt < len)){
				(buf->rbuf)[RXCnt] = *spi0_fifo; // until fifo contain data 
				RXCnt++;
			printk(KERN_ALERT "----rx\n");
			}
		}
            
		printk(KERN_ALERT "----5\n");
		while (!(*spi0_cs) & (1<<16)){ // wating transffer
			printk(KERN_ALERT "in the while state\n"); 
		}
		copy_to_user((void *)arg,&buf,sizeof(buf));
		*spi0_cs |= (3<<4); // cs clear
		*spi0_cs &= ~(1<<7); // transfer active done
	break;
	}
	printk(KERN_ALERT "ioctl is done\n");   
    return 0;
}

static struct file_operations spi_fops = {
    .owner = THIS_MODULE,
    .open = spi_open,
    .release = spi_release,
    .unlocked_ioctl = spi_ioctl
};

int __init spi_init(void){
    if(register_chrdev(SPI_MAJOR_NUMBER, SPI_DEV_NAME, &spi_fops)<0)
        printk(KERN_ALERT "SPI driver initialization failed\n");
    else
        printk(KERN_ALERT "SPI driver initialization successful\n");
    
    return 0;
}

void __exit spi_exit(void){
    unregister_chrdev(SPI_MAJOR_NUMBER, SPI_DEV_NAME);
    printk(KERN_ALERT "SPI driver exit done\n");
}

module_init(spi_init);
module_exit(spi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Team A+");
MODULE_DESCRIPTION("SPI module");
