#include <linux/version.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/time.h>

#include <asm/uaccess.h>
#include <rtdm/rtdm.h>
#include <rtdm/driver.h>

// Pin du GPIO du Raspberry Pi
#define GPIO_DISTANCE 5
#define GPIO_DHT11 12
#define GPIO_BUZZER 16
#define GPIO_BUTTON 19
#define GPIO_LED 18

#define MY_DATA_SIZE  4096
static char my_data[MY_DATA_SIZE];
static int  my_data_end = 0;
static rtdm_mutex_t my_data_mtx;
static rtdm_mutex_t my_alarm_mtx;  //définir le mutex pour le système d'alarme

rtdm_task_t task_distance;   //définir la tâche en temps réel pour le capteur de distance.
rtdm_task_t task_dht11;      //définir la tâche en temps réel pour le capteur de DHT11.

static int periode_us = 1000 * 100;     //la période pour le capteur de distance.
static int periode_us_dht = 1000 * 100;  //la période pour le capteur de DHT11.

static int my_open_function(struct rtdm_fd* fd, int flags)
{
    rtdm_printk(KERN_INFO "%s.%s()\n", THIS_MODULE->name, __FUNCTION__);
    return 0;
}

static void my_close_function(struct rtdm_fd* fd)
{
    rtdm_printk(KERN_INFO "%s.%s()\n", THIS_MODULE->name, __FUNCTION__);
}

static int my_read_nrt_function(struct rtdm_fd* fd, void __user* buffer, size_t lg)
{
    rtdm_mutex_lock(&my_data_mtx);

    if (lg > my_data_end)
        lg = my_data_end;

    if (lg > 0) {
        if (rtdm_safe_copy_to_user(fd, buffer, my_data, lg) != 0) {
            rtdm_mutex_unlock(&my_data_mtx);
            return -EFAULT;
        }
        my_data_end -= lg;
        if (my_data_end > 0)
            memmove(my_data, &my_data[lg], my_data_end);
    }

   // rtdm_printk("%s: sent %d bytes, \"%.*s\"\n", __FUNCTION__, lg, lg, buffer);

    rtdm_mutex_unlock(&my_data_mtx);
    return lg;
}

/*
la tâche pour calculer la distance, et surveiller l'invasion.
*/
void task_dis(void* arg) {

    int err;
    long dis;
    struct timeval tv1;
    struct timeval tv2;
    long time1, time2;

    while (!rtdm_task_should_stop()) {
        if ((err = gpio_direction_output(GPIO_DISTANCE, 1)) != 0) {
            gpio_free(GPIO_DISTANCE);
            rtdm_printk(KERN_INFO "err: %d \n", err);
            break;
        }
        gpio_set_value(GPIO_DISTANCE, 0);
        udelay(2);
        gpio_set_value(GPIO_DISTANCE, 1);
        udelay(10);
        gpio_set_value(GPIO_DISTANCE, 0);

        if ((err = gpio_direction_input(GPIO_DISTANCE)) != 0) {
            gpio_free(GPIO_DISTANCE);
            rtdm_printk(KERN_INFO "err: %d \n", err);
            break;
        }

        while (!(gpio_get_value(GPIO_DISTANCE) == 1));
        do_gettimeofday(&tv1);           //Obtenir le temps d'envoyer l'onde

        while (!(gpio_get_value(GPIO_DISTANCE) == 0));
        do_gettimeofday(&tv2);             //Obtenir le temps de la réception de l'onde
        time1 = tv1.tv_sec * 1000000 + tv1.tv_usec;
        time2 = tv2.tv_sec * 1000000 + tv2.tv_usec;
        dis = (time2 - time1) * 340 / 2000; //Calculer la distance en mm
        
        rtdm_printk(KERN_INFO "dis = %ld \n", dis);
        if (dis < 1000)                      //Si quelque chose apparaît à moins d'un mètre du capteur de distance
        {
            rtdm_mutex_lock(&my_alarm_mtx);
            if ((err = gpio_direction_output(GPIO_BUZZER, 0)) != 0) {
                gpio_free(GPIO_DISTANCE);
                rtdm_printk(KERN_INFO "err: %d \n", err);
                break;
            }

            if ((err = gpio_direction_output(GPIO_LED, 0)) != 0) {
                gpio_free(GPIO_LED);
                rtdm_printk(KERN_INFO "err: %d \n", err);
                break;
            }

            if ((err = gpio_direction_input(GPIO_BUTTON)) != 0) {
                gpio_free(GPIO_BUTTON);
                rtdm_printk(KERN_INFO "err: %d \n", err);
                break;
            }
            /*
            Système d'alarme           
            */
            while (!rtdm_task_should_stop()) {
                static int value = 0;
                gpio_set_value(GPIO_BUZZER, value); //Le buzzer sonne
                gpio_set_value(GPIO_LED, value);    //LED allumée
                value = 1 - value;
                rtdm_task_wait_period(NULL);

                /*  Si l'on appuie sur le bouton, le système d'alarme est désactivé.  */
                if (gpio_get_value(GPIO_BUTTON) == 0)  
                {
                    gpio_set_value(GPIO_LED, 0);
                    gpio_set_value(GPIO_BUZZER, 0);
                    break;
                }
            }
            rtdm_mutex_unlock(&my_alarm_mtx);
        }
        rtdm_task_wait_period(NULL);
    }
}

/*
La tâche pour stocker les données de dht11.
*/

char DHT11_Read(char* temperature, char* humidity)
{
    int err;
    unsigned char t, i, j;
    unsigned char buf[5] = { 0,0,0,0,0 };

    if ((err = gpio_direction_output(GPIO_DHT11, 1)) != 0) {
        gpio_free(GPIO_DHT11);
        return err;
    }
    //Activation du capteur
    gpio_set_value(GPIO_DHT11, 1);
    udelay(30);
    gpio_set_value(GPIO_DHT11, 0);
    mdelay(20);
    gpio_set_value(GPIO_DHT11, 1);
    udelay(30);

    if ((err = gpio_direction_input(GPIO_DHT11)) != 0) {
        gpio_free(GPIO_DHT11);
        return err;
    }

    //Préparation de la réception des données des capteurs
    while (gpio_get_value(GPIO_DHT11) && t < 100)
    {
        t++;
        udelay(1);
    };
    if (t >= 100)
        return err;
    t = 0;

    while (!gpio_get_value(GPIO_DHT11) && t < 100)
    {
        t++;
        udelay(1);
    };
    if (t >= 100)
    {
        return err;
    }

    //Stocker les données renvoyées dans un tableau de 8 bits
    for (i = 0; i < 5; i++)
    {
        buf[i] = 0;
        for (j = 0; j < 8; j++)
        {
            buf[i] <<= 1;
            t = 0;
            while (gpio_get_value(GPIO_DHT11) && t < 100)
            {
                t++;
                udelay(1);
            }
            if (t >= 100)
                return err;
            t = 0;
            while (!gpio_get_value(GPIO_DHT11) && t < 100)
            {
                t++;
                udelay(1);
            };
            if (t >= 100)
                return err;
            udelay(40);
            if (gpio_get_value(GPIO_DHT11))
                buf[i] += 1;
        }
    }
    if (buf[0] + buf[1] + buf[2] + buf[3] != buf[4])
        return 2;
    *humidity = buf[0];
    *temperature = buf[2];
    return 0;
}

/*
La tâche pour surveiller le température.
*/

void task_dht(void* arg)
{
    char temperature;
    char humidity;
    char value;
    int err;
    while (!rtdm_task_should_stop()) {
        int i = 0;
        rtdm_mutex_lock(&my_data_mtx);
        my_data_end = 0;
        value = DHT11_Read(&temperature, &humidity);
        my_data[0] = temperature;//Pour envoyer le valeur de température à l'espace utilisateur
        my_data_end++;
        my_data[1] = humidity;//Pour envoyer le valeur d'humidité à l'espace utilisateur
        my_data_end++;
        rtdm_mutex_unlock(&my_data_mtx);
        /*
            Système d'alarme
         */
        if (value == 0)
        {
            rtdm_printk(KERN_INFO "temperature = %d \n", temperature);
            rtdm_printk(KERN_INFO "humidity = %d \n", humidity);
            if (temperature > 40 )
            {
                 i++;
                rtdm_mutex_lock(&my_alarm_mtx);
                if ((err = gpio_direction_output(GPIO_BUZZER, 0)) != 0) {
                    gpio_free(GPIO_DISTANCE);
                    rtdm_printk(KERN_INFO "err: %d \n", err);
                    break;
                }

                if ((err = gpio_direction_output(GPIO_LED, 0)) != 0) {
                   gpio_free(GPIO_LED);
                   rtdm_printk(KERN_INFO "err: %d \n", err);
                   break;
                }

                if ((err = gpio_direction_input(GPIO_BUTTON)) != 0) {
                    gpio_free(GPIO_BUTTON);
                    rtdm_printk(KERN_INFO "err: %d \n", err);
                    break;
                }

                while (!rtdm_task_should_stop()) {
                    static int value = 0;
                    gpio_set_value(GPIO_BUZZER, value);
                    gpio_set_value(GPIO_LED, value);
                    value = 1 - value;
                    rtdm_task_wait_period(NULL);
                    if (gpio_get_value(GPIO_BUTTON) == 0)
                    {
                        gpio_set_value(GPIO_LED, 0);
                        gpio_set_value(GPIO_BUZZER, 0);
                        break;
                    }
                }
                rtdm_mutex_unlock(&my_alarm_mtx);
            }
            //Attendre que la température revienne en dessous de la température spécifiée
            while (i != 0 && temperature > 40){
                value = DHT11_Read(&temperature, &humidity);
            };

        }
       rtdm_task_wait_period(NULL);
    }
}

static struct rtdm_driver my_rt_driver = {

    .profile_info = RTDM_PROFILE_INFO(my_Project, RTDM_CLASS_TESTING, 1, 1),

    .device_flags = RTDM_NAMED_DEVICE,
    .device_count = 1,
    .context_size = 0,

    .ops = {
        .open = my_open_function,
        .close = my_close_function,
        .read_nrt = my_read_nrt_function,
    },
};

static struct rtdm_device my_rt_device = {

    .driver = &my_rt_driver,
    .label = "rtdm_driver_project_%d",
};

static int __init init_oscillateur(void)
{
	int ret, err;
	rtdm_printk(KERN_INFO "%s.%s() : Je suis la\n", THIS_MODULE->name, __FUNCTION__);
    rtdm_mutex_init(&my_data_mtx);
    rtdm_mutex_init(&my_alarm_mtx);
    rtdm_dev_register(&my_rt_device);
	if ((err = gpio_request(GPIO_DISTANCE, THIS_MODULE->name)) != 0) {
		return err;
	}

	if ((err = gpio_request(GPIO_DHT11, THIS_MODULE->name)) != 0) {
		return err;
	}

	if ((err = gpio_request(GPIO_BUZZER, THIS_MODULE->name)) != 0) {
		return err;
	}

    if ((err = gpio_request(GPIO_LED, THIS_MODULE->name)) != 0) {
        return err;
    }

    if ((err = gpio_request(GPIO_BUTTON, THIS_MODULE->name)) != 0) {
        return err;
    }

	ret = rtdm_task_init(&task_distance, "rtdm-task-distance", task_dis, NULL, 30, periode_us * 1000); //
	if (ret) {
		rtdm_printk(KERN_INFO "%s.%s() : error rtdm_task_distance_init\n", THIS_MODULE->name, __FUNCTION__);
	}
	else  rtdm_printk(KERN_INFO "%s.%s() : success rtdm_task_distance_init\n", THIS_MODULE->name, __FUNCTION__);

    ret = rtdm_task_init(&task_dht11, "rtdm-task-dht11", task_dht, NULL, 40, periode_us_dht * 1000);
    if (ret) {
        rtdm_printk(KERN_INFO "%s.%s() : error rtdm_task_dht11_init\n", THIS_MODULE->name, __FUNCTION__);
    }
    else  rtdm_printk(KERN_INFO "%s.%s() : success rtdm_task_dht11_init\n", THIS_MODULE->name, __FUNCTION__);
    
	return 0;

}

static void __exit exit_oscillateur(void)
{
    gpio_free(GPIO_DISTANCE);
    gpio_set_value(GPIO_BUZZER, 0);
    gpio_set_value(GPIO_LED, 0);
    gpio_free(GPIO_BUZZER);
    gpio_free(GPIO_LED);
    gpio_free(GPIO_DHT11);
    gpio_free(GPIO_BUTTON);
    rtdm_dev_unregister(&my_rt_device);
    rtdm_mutex_destroy(&my_data_mtx);
    rtdm_mutex_destroy(&my_alarm_mtx);
    rtdm_task_destroy(&task_distance);
    rtdm_task_destroy(&task_dht11);
}

module_init(init_oscillateur);
module_exit(exit_oscillateur);
MODULE_LICENSE("GPL");
