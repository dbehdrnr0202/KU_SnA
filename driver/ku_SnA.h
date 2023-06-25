#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/timer.h>

MODULE_LICENSE("GPL");

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
#define FLAG_STOP   0
#define FLAG_FORWARD 1
#define FLAG_BACKWARD 2
#define FLAG_FORCEDSTOP 4
#define RUNNING_TIME 30000
#define DANGER_DIST 5
#define WARN_DIST 10

//gpio PIN 설정
//motor
#define PIN1 6
#define PIN2 13
#define PIN3 19
#define PIN4 26
//ultra sonic
#define ULTRA_TRIG 17
#define ULTRA_ECHO 18
//switch
#define SWITCH 12
//pir
#define PIRSENSOR 27
//LED
#define LED_YELLOW  4
#define LED_RED     5

//motor
#define STEPS 8
#define ONEROUND 512


#define KU_DELAY 500


//ultrasonic, switch, pir ku_data type
#define ULTRASONIC_TYPE 1
#define PIR_TYPE        2
#define SWITCH_TYPE     3

struct ku_data  {
    struct list_head list;
    char type;
    int value;
};

struct my_timer_info    {
    struct timer_list timer;
    long delay_jiffies;
};

struct argu {
    int speed_index;
    int time_limit;
};

static struct my_timer_info my_timer;

struct ku_data ultra_data_list;
struct ku_data pir_data_list;

spinlock_t my_lock;

int blue[8] =   {1, 1, 0, 0, 0, 0, 0, 1};
int pink[8] =   {0, 1, 1, 1, 0, 0, 0, 0};
int yellow[8] = {0, 0, 0, 1, 1, 1, 0, 0};
int orange[8] = {0, 0, 0, 0, 0, 1, 1, 1};

int delay_speed[3] = {1000, 1500, 3000};
static int speed_index;

//ultra sonic
static int ultra_irq_num;
static int echo_valid_flag = 3;
static ktime_t echo_start;
static ktime_t echo_stop;
//switch
static int switch_irq_num;
//pir sensor
static int pir_irq_num;
static int stop_flag;

//cdev
static dev_t dev_num;
static struct cdev *cd_cdev;
//fops
struct file_operations ku_sna_fops;


//function
//motor
void setstep(int p1, int p2, int p3, int p4);
//time
static void my_timer_func(struct timer_list *t);
//ioctl
void backward(int delay_index, int time_limit);
void forward(int delay_index, int time_limit);

//interrupt
static irqreturn_t ultra_isr(int irq, void* dev_id);
static irqreturn_t switch_isr(int irq, void* dev_id);
static irqreturn_t pir_isr(int irq, void* dev_id);

//module 관련
static long ku_sna_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int ku_sna_open(struct inode *inode, struct file *file);
static int ku_sna_release(struct inode *inode, struct file *file);
static int __init ku_sna_init(void);
static void __exit ku_sna_exit(void);