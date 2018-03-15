/*
 * Sample morse code hardware GPIO simulation kernel module
 *
 * Copyright (C) 2014 David Reyna <david.reyna@windriver.com>
 *
 * Released under the BSP part 2
 *
 */

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/timer.h>

/*
 * This module simulates the GPIO ports for a key and an LED. It is intended
 * to support QEMU development in lieu of physical ports.
 *
 * The access points are under "/sys/kernel/morsemod":
 *     "key" = the input of the user key presses
 *     "led" = the output for the LED display
 *  "simkey".= the backdoor simulator access to set "key" and modes
 *
 *  Usage:
 *      echo 0 > key/led/simkey to set off (default)
 *      echo 1 > key/led/simkey to set on
 *      echo < > simkey         to start the broadcast mode
 *      echo > > simkey         to toggle the broadcast mode
 *      echo [ > simkey         to start  the loopback  mode
 *      echo ] > simkey         to stop   the loopback  mode
 *      cat key or led to see the current value
 *      cat simkey to see the simkey, broadcast, and loopback values
 *
 */

unsigned int key;
unsigned int led;
unsigned int simkey;
unsigned int loopback;
unsigned int broadcast;

/* allow the instance to be set by a parameter */
unsigned int morsemod_instance=0;
module_param_named(instance, morsemod_instance, int, 0);

/* Dit time: 500 = 1/2 second for default "dit" */
unsigned int morsemod_timeout = 500;

#define future_in_ms(n) (jiffies+(morsemod_timeout * HZ ) / 1000)

static struct timer_list morsemod_timer_struct;
static char message_str[1024];
static int  message_index = 0;


static void morsemod_timer(unsigned long dev_addr)
{
	/* printk(KERN_INFO "TICK: B=%d, I=%d\n",broadcast,message_index); */

	if (1 == broadcast) {

		/* printk(KERN_INFO "  TOCK: B=%d, I=%d, Key=%c\n",broadcast,message_index,message_str[message_index]); */

		if ('1' == message_str[message_index])
			key = 1;
		else
			key = 0;

		message_index++;
		if (message_index >= strlen(message_str)) {
			message_index=0;
		}
	}


	/* let's do this again */
	morsemod_timer_struct.expires = future_in_ms(morsemod_timeout);
	add_timer(&morsemod_timer_struct);
}


/*
 * create_timers(void) - This creates the programmable timer
 */

void create_timers(void) {

	/* printk(KERN_INFO "Setting up timers\n"); */

	/* Initialize the timer */
	init_timer(&morsemod_timer_struct);

	/* Set up the timer callback */
	morsemod_timer_struct.function = &morsemod_timer;

	/* Set up the timeout */
	morsemod_timer_struct.expires = future_in_ms(morsemod_timeout);

	/* Add the timer */
	add_timer(&morsemod_timer_struct);
}

/*
 * update_timer(void) - this updates the timer with a new timeout value
 *         stored in morsemod_timeout.
 */

void update_timer(void) {
	morsemod_timer_struct.expires = future_in_ms(morsemod_timeout);
}

/*
 * destroy_timers() - frees up and cancels existing timers.
 */

void destroy_timers(void) {

	/* printk(KERN_INFO "Destroying timers\n"); */

	del_timer(&morsemod_timer_struct);
}

void init_message(void)
{
	message_str[0]='\0';
	message_index = 0;
	key = 0;

    /*         */
	strcat(message_str," ");
	/* Y -*--  */
	strcat(message_str,"111 1 111 111   ");
	/* P *-*-  */
	strcat(message_str,"1 111 1 111   ");
    /*         */
	strcat(message_str," ");
	/* D -**   */
	strcat(message_str,"111 1 1   ");
	/* E *     */
	strcat(message_str,"1   ");
	/* V ***-  */
	strcat(message_str,"1 1 1 111   ");
    /*         */
	strcat(message_str," ");
	/* D -**   */
	strcat(message_str,"111 1 1   ");
	/* A *-    */
	strcat(message_str,"1 111   ");
	/* Y -*--  */
	strcat(message_str,"111 1 111 111   ");
    /*         */
	strcat(message_str,"  ");
	/* E *     */
	strcat(message_str,"1   ");
	/* L * *-**    */
	strcat(message_str,"1 111 1 1   ");
	/* C -*-*  */
	strcat(message_str,"111 1 111 1   ");
    /*         */
	strcat(message_str,"  ");
	/* 2 **--- */
	strcat(message_str,"1 1 111 111 111   ");
	/* 0 ----- */
	strcat(message_str,"111 111 111 111 111   ");
	/* 1 *---- */
	strcat(message_str,"1 111 111 111 111   ");
	/* 5 ***** */
	strcat(message_str,"1 1 1 1 1   ");
    /*         */
	strcat(message_str,"  ");

	/* start timer fresh */
	update_timer();
}

/*
 * The "simkey" file is a backdoor to set the public "key" value.
 */
static ssize_t simkey_show(struct kobject *kobj, struct kobj_attribute *attr,
                        char *buf)
{
	return sprintf(buf, "K=%d L=%d B=%d LP=%d \n", key, led, broadcast, loopback);
}

static ssize_t simkey_store(struct kobject *kobj, struct kobj_attribute *attr,
                         const char *buf, size_t count)
{
	int var;

	if        ('<' == buf[0]) broadcast=1;
	  else if ('>' == buf[0]) broadcast=0;
	  else if ('[' == buf[0]) loopback=1;
	  else if (']' == buf[0]) loopback=0;
	  else if ('0' == buf[0]) {simkey=0; key=simkey;}
	  else if ('1' == buf[0]) {simkey=1; key=simkey;}

	return count;
}

static struct kobj_attribute simkey_attribute =
	__ATTR(simkey, 0660, simkey_show, simkey_store);

/*
 * These are the core LED and Key attibutes.
 */
static ssize_t b_show(struct kobject *kobj, struct kobj_attribute *attr,
                      char *buf)
{
	int ret;

	if (strcmp(attr->attr.name, "led") == 0) {
		ret = sprintf(buf, "%d\n", led);
	} else {
		ret = sprintf(buf, "%d\n", key);
    }
	return ret;
}

static ssize_t b_store(struct kobject *kobj, struct kobj_attribute *attr,
                       const char *buf, size_t count)
{
	int var;

	sscanf(buf, "%du", &var);
	if (strcmp(attr->attr.name, "led") == 0) {
		led = var;
	} else {
		key = var;
		if (loopback) {
			led = var;
		}
	}
	return count;
}

static struct kobj_attribute led_attribute =
	__ATTR(led, 0660, b_show, b_store);
static struct kobj_attribute key_attribute =
	__ATTR(key, 0660, b_show, b_store);


/*
 * Create a group of attributes so that we can create and destory them all
 * at once.
 */
static struct attribute *attrs[] = {
	&key_attribute.attr,
	&led_attribute.attr,
	&simkey_attribute.attr,
	NULL,   /* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory.  If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *morsemod_kobj;

static int __init morsemod_init(void)
{
	int retval;
	char module_dir[40];

	/*
	 * Create the kobject for "morsemod",
	 * located under /sys/kernel/
	 */
	if (morsemod_instance > 0) {
		sprintf(module_dir,"morsemod%d",morsemod_instance);
	} else {
		strcpy(module_dir,"morsemod");
	}
	morsemod_kobj = kobject_create_and_add(module_dir, kernel_kobj);
	if (!morsemod_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(morsemod_kobj, &attr_group);
	if (retval)
		kobject_put(morsemod_kobj);

	/* init the broadcast message and start the background timer */
	init_message();
	create_timers();

	return retval;
}

static void __exit morsemod_exit(void)
{
	/* printk(KERN_INFO "DBG: EXIT_MORSE\n"); */

	kobject_del(morsemod_kobj);
	destroy_timers();
}

module_init(morsemod_init);
module_exit(morsemod_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Reyna <david.reyna@windriver.com>");
