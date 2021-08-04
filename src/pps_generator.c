#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
//#include <linux/time.h>
#include <linux/hrtimer.h>

// Constants
/*****************************************/
#define DRV_NAME                "pps_generator"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matthias Mueller");
MODULE_DESCRIPTION("Raspberry Pi kernel module to generate a pulse per second signal for external time synchronisation");
MODULE_VERSION("0.01");

#define PPS_GPIO                5
#define LOCK_GPIO               6

#define N_SHIFT            1
#define LOCKED_THRESHOLD_NS    100000

// Typedefs
/*****************************************/

// function prototypes
/*****************************************/

// Global variables
/*****************************************/
//timer
long pps_interval_ns = 1000000000;
int pin_on_time_ms = 100;
static struct hrtimer pps_timer;
static struct hrtimer pin_on_timer;
// gpio
static unsigned int ppsGPIO = PPS_GPIO;
static unsigned int lockedGPIO = LOCK_GPIO;

// Main
/*****************************************/

/**
 * PPS timer callback function
*/
enum hrtimer_restart  _PpsTimerHandler(struct hrtimer *timer)
{
    struct timespec64 current_time;
    long timediff;

    // set the PPS pin to high
    gpio_set_value(ppsGPIO, 1);

    // save the current time
    ktime_get_real_ts64( &current_time );

    // calculate the difference to one whole second
    timediff = 1000000000 - current_time.tv_nsec;

    // check if the current time or the difference is bigger, and add / subtract the value accordingly
    if( current_time.tv_nsec < timediff)
    {
        pps_interval_ns = pps_interval_ns - (current_time.tv_nsec >> N_SHIFT);
    }
    else
    {
        pps_interval_ns = pps_interval_ns + (timediff >> N_SHIFT);
    }

    // restart the pps timer
    hrtimer_forward_now(&pps_timer, ns_to_ktime(pps_interval_ns));

    // start the pin on timer, until it will be set to zero again
    hrtimer_start(&pin_on_timer, ms_to_ktime(pin_on_time_ms), HRTIMER_MODE_REL);

    // check if the time difference is lower than the threshold; if so, enable the GPIO
    if( (current_time.tv_nsec < LOCKED_THRESHOLD_NS) || (timediff < LOCKED_THRESHOLD_NS) )
    {
        gpio_set_value(lockedGPIO, 1);
    }
    else
    {
        gpio_set_value(lockedGPIO, 0);
    }

   // printk(KERN_INFO "Timer Handler called\n");
   // printk(KERN_INFO "timediff: %li, pps_interval_ns: %li, actual ns: %li, jiffies: %li\n", timediff, pps_interval_ns, current_time.tv_nsec, nsecs_to_jiffies(pps_interval_ns));

    return HRTIMER_RESTART;
}

/**
 * Pin on timer callback function, just sets the pin back to zero
*/
static enum hrtimer_restart _PinOnTimerHandler(struct hrtimer *timer)
{
    // just set the PPS GPIO to zero again (is set to one in the PpsTimerHandler)
    gpio_set_value(ppsGPIO, 0);

    return HRTIMER_NORESTART;
}



/**
 * Init function of the LKM, is called at startup
*/
static int __init pps_init(void)
{
    int err;
    struct timespec64 current_time;
    long timediff;

    // initialize the pps timer
    hrtimer_init(&pps_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    pps_timer.function = &_PpsTimerHandler;

    // check the difference to the current time and start the timer
    ktime_get_real_ts64( &current_time );
    timediff = 1000000000 - current_time.tv_nsec;
    hrtimer_start( &pps_timer, ns_to_ktime(timediff), HRTIMER_MODE_REL);

    // initialize the pin on timer
    hrtimer_init(&pin_on_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    pin_on_timer.function = &_PinOnTimerHandler;

    /// setup pps gpio and set it to output
    err = gpio_request_one(ppsGPIO, GPIOF_OUT_INIT_LOW, DRV_NAME " pps");
    if (err < 0) {
        printk(KERN_ALERT DRV_NAME " : failed to request PPS pin %d.\n", ppsGPIO);
        return -1;
    }

    /// setup locked gpio and set it to output
    err = gpio_request_one(lockedGPIO, GPIOF_OUT_INIT_LOW, DRV_NAME " locked");
    if (err < 0) {
        printk(KERN_ALERT DRV_NAME " : failed to request locked pin %d.\n", lockedGPIO);
        gpio_free(ppsGPIO);
        return -1;
    }

    printk(KERN_INFO DRV_NAME " : PPS Generator initialized and started\n");
    return 0;
}

/**
 * Exit function of the LKM, is called when the LKM ends
*/
static void __exit pps_exit(void) 
{
    // remove timers
    hrtimer_cancel(&pps_timer);
    hrtimer_cancel(&pin_on_timer);

    // remove gpios
    gpio_set_value(ppsGPIO, 0);
    gpio_set_value(lockedGPIO, 0);

    gpio_free(ppsGPIO);
    gpio_free(lockedGPIO);

    printk(KERN_INFO DRV_NAME " : PPS Generator ended\n");
}

module_init(pps_init);
module_exit(pps_exit);


