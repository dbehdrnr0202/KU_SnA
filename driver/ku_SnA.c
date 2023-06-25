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
void setstep(int p1, int p2, int p3, int p4)    {
    gpio_set_value(PIN1, p1);
    gpio_set_value(PIN2, p2);
    gpio_set_value(PIN3, p3);
    gpio_set_value(PIN4, p4);
}

static void my_timer_func(struct timer_list *t) {
    struct my_timer_info *info = from_timer(info, t, timer);
    printk("time ended\n");
    stop_flag = FLAG_FORCEDSTOP;
    //del_timer(&my_timer.timer);
}

void backward(int delay_index, int time_limit) {
    stop_flag = FLAG_BACKWARD;
    int i, j;
    if (time_limit==true)  {
        my_timer.timer.expires = jiffies+my_timer.delay_jiffies;
        add_timer(&my_timer.timer);
    }
    gpio_set_value(LED_RED, 0);
    gpio_set_value(LED_YELLOW, 1);
    printk("start runnig backward\n");
    
    while(1)    {
        for (i = 0;i<ONEROUND;i++)    {
            for (j = STEPS; j>0; j--)    {
                if (echo_valid_flag==3) {
		        echo_start = ktime_set(0, 1);
		        echo_stop = ktime_set(0, 1);
		        echo_valid_flag = 0;
		        //printk("start ultrsonic\n");
		        gpio_set_value(ULTRA_TRIG, 1);
		        udelay(10);
		        gpio_set_value(ULTRA_TRIG, 0);

        		echo_valid_flag = 1;
                }
                if (!list_empty(&(ultra_data_list.list))) {
                    struct ku_data *pos = 0;
                    struct ku_data *tmp = 0;
                    list_for_each_entry_safe(tmp, pos, &ultra_data_list.list, list) {
                        list_del(&(tmp->list));
                        if (tmp->type==ULTRASONIC_TYPE)    {
                            if (tmp->value < DANGER_DIST)
                                stop_flag = FLAG_STOP;
                            else 
                                stop_flag = FLAG_BACKWARD;
                        }
                        if (tmp->type==SWITCH_TYPE) {
                            int before_speed_index = speed_index;
                            speed_index = (speed_index+1)%3;
                            printk("switch: changed speed delay from %d tp %d\n", delay_speed[before_speed_index], delay_speed[speed_index]);
                        }
                        kfree(tmp);
                    }
                }
                if (stop_flag==FLAG_STOP||stop_flag==FLAG_FORCEDSTOP)   {
                    printk("something is too close\n");
                    gpio_set_value(LED_RED, 1);
                    return;
                }
                setstep(blue[j], pink[j], yellow[j], orange[j]);
                udelay(delay_speed[speed_index]);
            }
        }
        setstep(0, 0, 0, 0);
    }
    gpio_set_value(LED_RED, 1);
}

void forward(int delay_index, int time_limit) {
    stop_flag = FLAG_FORWARD;
    int i, j;
    if (time_limit==true)   {
        my_timer.timer.expires = jiffies+my_timer.delay_jiffies;
        add_timer(&my_timer.timer);
    }
    gpio_set_value(LED_RED, 0);
    gpio_set_value(LED_YELLOW, 0);
    printk("start runnig forward\n");
    while(1)    {
        for (i = 0;i<ONEROUND;i++)    {
            for (j = 0; j<STEPS;j++)    {
                if (!list_empty(&pir_data_list.list)) {
                    struct ku_data *pos = 0;
                    struct ku_data *tmp = 0;
                    list_for_each_entry_safe(tmp, pos, &pir_data_list.list, list) {
                        list_del(&(tmp->list));
                        if (tmp->type==PIR_TYPE)    {
                            stop_flag = FLAG_STOP;
                        }
                        if (tmp->type==SWITCH_TYPE) {
                            int before_speed_index = speed_index;
                            speed_index = (speed_index+1)%3;
                            printk("switch: changed speed delay from %d tp %d\n", delay_speed[before_speed_index], delay_speed[speed_index]);
                        }
                        kfree(tmp);
                    }
                }
                if (stop_flag==FLAG_STOP)   {
                    printk("DETECT PEOPLE\n");                    
                    gpio_set_value(LED_RED, 1);
                    return;
                }
                if (stop_flag==FLAG_FORCEDSTOP)   {
                    printk("TIME ENDED\n");                    
                    gpio_set_value(LED_RED, 1);
                    return;
                
                }
                setstep(blue[j], pink[j], yellow[j], orange[j]);
                udelay(delay_speed[speed_index]);
            }
        }
        setstep(0, 0, 0, 0);
    }
}

static irqreturn_t ultra_isr(int irq, void* dev_id)  {
    ktime_t tmp_time;
    s64 time;
    int distance;
    unsigned long flags;
    if (stop_flag==FLAG_FORCEDSTOP) {
        printk("ultra_isr: FORCED STOP\n");
        return IRQ_HANDLED;
    }
    tmp_time = ktime_get();
    if (echo_valid_flag==1) {
        //printk("simple_ultra: ECHO UP\n");
        if (gpio_get_value(ULTRA_ECHO)==1)  {
            echo_start = tmp_time;
            echo_valid_flag = 2;
        }
    }
    else if (echo_valid_flag==2) {
        //printk("simple_ultra: ECHO DOWN\n");
        if (gpio_get_value(ULTRA_ECHO)==0)  {
            echo_stop = tmp_time;
            time = ktime_to_us(ktime_sub(echo_stop, echo_start));
            distance = (int)time/58;
            
            if (distance < WARN_DIST)  {
                struct ku_data *new_data = (struct ku_data*)kmalloc(sizeof(struct ku_data), GFP_KERNEL);
                new_data->type = ULTRASONIC_TYPE;
                new_data->value = distance;
                spin_lock_irqsave(&my_lock, flags);
                list_add(&new_data->list, &ultra_data_list.list);
                //stop_flag = 1;
                spin_unlock_irqrestore(&my_lock, flags);
                printk("simple_ultra: send list (Distance %d cm)\n", distance);
            }

            echo_valid_flag = 3;
        }
    }
    return IRQ_HANDLED;
}

static irqreturn_t switch_isr(int irq, void* dev_id)    {
    unsigned long flags;
    struct ku_data *new_data = (struct ku_data*)kmalloc(sizeof(struct ku_data), GFP_KERNEL);
    new_data->type = SWITCH_TYPE;
    new_data->value = 0;
    if (stop_flag == FLAG_BACKWARD) {
        spin_lock_irqsave(&my_lock, flags);
        list_add(&new_data->list, &ultra_data_list.list);
        //stop_flag = 1;
        spin_unlock_irqrestore(&my_lock, flags);
        printk("switch: pushed, change backward speed\n");
    }
    else if(stop_flag== FLAG_FORWARD)    {
        spin_lock_irqsave(&my_lock, flags);
        list_add_tail(&new_data->list, &pir_data_list.list);
        //stop_flag = 1;
        spin_unlock_irqrestore(&my_lock, flags);
        printk("switch: pushed, change forward speed\n");
    }
    return IRQ_HANDLED;
}

static irqreturn_t pir_isr(int irq, void* dev_id)   {
    unsigned long flags;
    if (stop_flag==FLAG_FORCEDSTOP) {
        printk("ultra_isr: FORCED STOP\n");
        return IRQ_HANDLED;
    }
    struct ku_data *new_data = (struct ku_data*)kmalloc(sizeof(struct ku_data), GFP_KERNEL);
    new_data->type = PIR_TYPE;
    new_data->value = 0;
    spin_lock_irqsave(&my_lock, flags);
    list_add(&new_data->list, &pir_data_list.list);
    spin_unlock_irqrestore(&my_lock, flags);
    
    return IRQ_HANDLED;
}

static long ku_sna_ioctl(struct file *file, unsigned int cmd, unsigned long arg)    {
    struct argu *ar;
    ar = (struct argu*)arg;
    int time_limit, speed_index;
    speed_index = ar->speed_index;
    time_limit = ar->time_limit;
    printk("######KU_SNA_IOCTL######\n");
    switch(cmd) {
        case KU_SNA_FORWARD:
            printk("######FORWARD READY######\n");
            forward(speed_index, time_limit);
            printk("######FORWARD END######\n");
            break;
        case KU_SNA_BACKWARD:
            printk("######BACKWARD READY######\n");
            backward(speed_index, time_limit);
            printk("######BACKWARD END######\n");
            break;
        //case KU_SNA_STOP:

        //    break;
        default:
            return -1;
    }
    return 0;
}

static int ku_sna_open(struct inode *inode, struct file *file)  {
    return 0;
}

static int ku_sna_release(struct inode *inode, struct file *file)  {
    gpio_set_value(LED_RED, 0);
    struct ku_data *pos = 0;
    struct ku_data *tmp = 0;
    //struct ku_data *node = 0;
    list_for_each_entry_safe(tmp, pos, &pir_data_list.list, list) {
        list_del(&(tmp->list));
        kfree(tmp);
    }
    
    list_for_each_entry_safe(tmp, pos, &ultra_data_list.list, list) {
        list_del(&(tmp->list));
        kfree(tmp);
    }
    return 0;
}

struct file_operations ku_sna_fops={
    .unlocked_ioctl = ku_sna_ioctl, 
    .open = ku_sna_open, 
    .release = ku_sna_release
};


static int __init ku_sna_init(void) {
    int ret;
    printk("ku_sna: init\n");

    alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
    cd_cdev = cdev_alloc();
    cdev_init(cd_cdev, &ku_sna_fops);
    cdev_add(cd_cdev, dev_num, 1);
    
    //struct ku_data* new_node = (struct ku_data*)kmalloc(sizeof(struct ku_data),GFP_KERNEL);
    INIT_LIST_HEAD(&ultra_data_list.list);
    INIT_LIST_HEAD(&pir_data_list.list);

    spin_lock_init(&my_lock);
    
    my_timer.delay_jiffies = msecs_to_jiffies(RUNNING_TIME);
    timer_setup(&my_timer.timer, my_timer_func, 0);

    //ultra sonic
    printk("ku_sna: init ultrasonic\n");
    gpio_request_one(ULTRA_TRIG, GPIOF_OUT_INIT_LOW, "ULTRA_TRIG");
    gpio_request_one(ULTRA_ECHO, GPIOF_IN, "ULTRA_ECHO");

    ultra_irq_num = gpio_to_irq(ULTRA_ECHO);
    ret = request_irq(ultra_irq_num, ultra_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "ULTRA_ECHO", NULL);

    if (ret)    {
        printk("ku_sna: unable to request ultrasonic irq: %d\n", ret);
        free_irq(ultra_irq_num, NULL);
    }
    
    //motor
    printk("ku_sna: init motor\n");
    gpio_request_one(PIN1, GPIOF_OUT_INIT_LOW, "p1");
    gpio_request_one(PIN2, GPIOF_OUT_INIT_LOW, "p2");
    gpio_request_one(PIN3, GPIOF_OUT_INIT_LOW, "p3");
    gpio_request_one(PIN4, GPIOF_OUT_INIT_LOW, "p4");
    
    //switch
    printk("ku_sna: init switch\n");
    gpio_request_one(SWITCH, GPIOF_IN, "SWITCH");
    switch_irq_num = gpio_to_irq(SWITCH);
    ret = request_irq(switch_irq_num, switch_isr, IRQF_TRIGGER_RISING, "SWITCH_IRQ", NULL);
    if (ret)    {
        printk("ku_sna: unable to request switch irq: %d\n", ret);
        free_irq(switch_irq_num, NULL);
    }

    //pir sensor
    printk("ku_sna: init pir\n");
    gpio_request_one(PIRSENSOR, GPIOF_IN, "PIR_SENSOR");
    pir_irq_num = gpio_to_irq(PIRSENSOR);
    ret = request_irq(pir_irq_num, pir_isr, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "PIR", NULL);
    if (ret)    {
        printk("ku_sna: unable to request pir irq: %d\n", ret);
        free_irq(pir_irq_num, NULL);
    }

    //LED
    printk("ku_sna: init LED\n");
    gpio_request_one(LED_YELLOW, GPIOF_OUT_INIT_LOW, "LED_YELLOW");
    gpio_set_value(LED_YELLOW, 0);
    gpio_request_one(LED_RED, GPIOF_OUT_INIT_LOW, "LED_RED");
    gpio_set_value(LED_RED, 0);


    printk("######INIT END######\n");
    return 0;
}

static void __exit ku_sna_exit(void) {
    printk("ku_sna: exit\n");

    cdev_del(cd_cdev);
    unregister_chrdev_region(dev_num, 1);
    
    del_timer(&my_timer.timer);

    gpio_free(PIN1);
    gpio_free(PIN2);
    gpio_free(PIN3);
    gpio_free(PIN4); 
    
    struct ku_data *pos = 0;
    struct ku_data *tmp = 0;
    //struct ku_data *node = 0;
    list_for_each_entry_safe(tmp, pos, &pir_data_list.list, list) {
        list_del(&(tmp->list));
        kfree(tmp);
    }
    
    list_for_each_entry_safe(tmp, pos, &ultra_data_list.list, list) {
        list_del(&(tmp->list));
        kfree(tmp);
    }

    spin_lock_init(&my_lock);
    
    //ultra sonic
    printk("ku_sna: free ultrasonic\n");
    gpio_free(ULTRA_ECHO);
    gpio_free(ULTRA_TRIG);
    free_irq(ultra_irq_num, NULL);

    printk("ku_sna: free switch\n");
    gpio_free(SWITCH);
    free_irq(switch_irq_num, NULL);

    printk("ku_sna: free pir\n");    
    gpio_free(PIRSENSOR);
    free_irq(pir_irq_num, NULL);


    printk("ku_sna: free LED\n");
    gpio_set_value(LED_YELLOW, 0);
    gpio_set_value(LED_RED, 0);
    gpio_free(LED_YELLOW);
    gpio_free(LED_RED);

    printk("######EXIT END######\n");
}   

module_init(ku_sna_init);
module_exit(ku_sna_exit);
