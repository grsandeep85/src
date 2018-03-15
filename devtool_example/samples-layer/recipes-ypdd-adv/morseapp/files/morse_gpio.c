/*
 * Copyright 2014 Wind River Systems, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "morse_app.h"
#include "morse_gpio.h"

#define MAX_BUF   1024
static char buf[MAX_BUF];

static int  device_key  = 0;
static int  device_led  = 0;
static int  user_key    = 0;
static int  fd_key;
static int  fd_led;
static int  fd_sim;

/**************************************************************/

static int open_a_gpio_port (int gpio, int file_mode) {
	int  fd;
	char buf[MAX_BUF];

	/* export the key port */
	fd = open("/sys/class/gpio/export", O_WRONLY);
	sprintf(buf, "%d\n", gpio);
	write(fd, buf, strlen(buf));
	close(fd);

	/* set the port directions */
	sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio);
	fd = open(buf, O_WRONLY);
	if (O_WRONLY == file_mode) {
		// Set out direction
		write(fd, "out\n", 3+1);
	} else {
		// Set in direction
		write(fd, "in\n", 2+1);
	}
	close(fd);

	/* open the port fd */
	sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
	return(open(buf, file_mode));
}

static void close_a_gpio_port (int gpio) {
	int  fd;
	char buf[MAX_BUF];

	/* unexport the ports */
	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	sprintf(buf, "%d", gpio);
	write(fd, buf, strlen(buf));
	close(fd);
}

void open_gpio_ports(void) {
	char module_dir[40];
	char module_port_dir[40];

	debug(1,"\nOpening GPIO Ports...");
	fd_key = -1;
	fd_sim = -1;
	fd_led = -1;

	if (GPIO_DEVICE_SIMULATOR == gpio_device) {
		if (morsemod_instance > 0) {
			sprintf(module_dir,"/sys/kernel/morsemod%d",morsemod_instance);
		} else {
			strcpy(module_dir,"/sys/kernel/morsemod");
		}
		/* open the key and led streams */
		sprintf(module_port_dir,"%s/key",module_dir);

		fd_key = open(module_port_dir, O_RDONLY);
		sprintf(module_port_dir,"%s/led",module_dir);
		fd_led = open(module_port_dir, O_WRONLY);
		sprintf(module_port_dir,"%s/simkey",module_dir);
		fd_sim = open(module_port_dir, O_RDWR);
	}
	if (GPIO_DEVICE_BEAGLEBLACK == gpio_device) {
		fd_key = open_a_gpio_port(60,O_RDONLY);
		fd_led = open_a_gpio_port(7,O_WRONLY);
	}
	if (GPIO_DEVICE_MINNOWMAX == gpio_device) {
		fd_key = open_a_gpio_port(84,O_RDONLY);
		fd_led = open_a_gpio_port(82,O_WRONLY);
	}
	if (GPIO_DEVICE_WANDBOARD == gpio_device) {
		fd_key = open_a_gpio_port(24,O_RDONLY);
		fd_led = open_a_gpio_port(91,O_WRONLY);
	}

	sprintf(buf," : fd_key=%d, fd_led=%d\n",fd_key,fd_led);
	if ((-1 == fd_key) || (-1 == fd_led)) {
		printf("ERROR %s",buf);
	} else {
		/* preset the LED to off */
		set_device_led(0);

		debug(1,buf);
	}
}

void close_gpio_ports(void) {
	int  fd;
	char buf[MAX_BUF];
	int  gpio;

	if (0 == fd_key) return;

	debug(1,"Closing GPIO Ports...");

	/* close the key and led ports */
	if (GPIO_DEVICE_SIMULATOR == gpio_device) {
		close(fd_key);
		close(fd_led);
	}
	if (GPIO_DEVICE_BEAGLEBLACK == gpio_device) {
		close_a_gpio_port(60);
		close_a_gpio_port(7);
	}
	if (GPIO_DEVICE_MINNOWMAX == gpio_device) {
		close_a_gpio_port(84);
		close_a_gpio_port(82);
	}
	if (GPIO_DEVICE_WANDBOARD == gpio_device) {
		close_a_gpio_port(24);
		close_a_gpio_port(91);
	}

	debug(1," Closed!");

	fd_key=0;
	fd_led=0;
}

/**************************************************************/

int get_device_key(void) {
	char value;
	int  ret=0;

	/* debug(2,"get_device_key:1\n"); */

	lseek(fd_key, 0, SEEK_SET);
	read(fd_key, &value, 1);

	/* debug(2,"get_device_key:2\n"); */

	if ('1' == value)
		ret = 1;
	else
		ret = 0;

	/* reverse sense on these boards */
	if ((GPIO_DEVICE_BEAGLEBLACK == gpio_device) ||
		(GPIO_DEVICE_MINNOWMAX   == gpio_device) ||
		(GPIO_DEVICE_WANDBOARD   == gpio_device)  ) {
		if (1 == ret)
			ret = 0;
		else
			ret = 1;
	}

	return (ret);
}

int set_device_led(int led) {

	if (1 == led) {
		// Set GPIO high status
		write(fd_led, "1\n", 2);
		debug(1,"LED=1");
	} else {
		// Set GPIO low status
		write(fd_led, "0\n", 2);
		debug(1,"LED=0");
	}

}

int get_user_key(void) {
	return user_key;
}

void set_user_key(int newkey) {
	user_key = newkey;
	set_device_led(user_key);
}

void toggle_user_key(void) {
	if (1 == user_key)
		user_key = 0;
	else
		user_key = 1;
	set_device_led(user_key);
}

int set_device_simkey(int key) {

	if (1 == key) {
		// Set GPIO high status
		write(fd_sim, "1", 2);
	} else {
		// Set GPIO low status
		write(fd_sim, "0", 2);
	}
}

void set_sim_broadcast(int broadcast) {
	if (GPIO_DEVICE_SIMULATOR == gpio_device) {
		if (1 == broadcast) {
			// Set GPIO high status
			write(fd_sim, "<", 2);
		} else {
			// Set GPIO low status
			write(fd_sim, ">", 2);
		}
	}
}

void set_sim_loopback(int loopback) {
	if (GPIO_DEVICE_SIMULATOR == gpio_device) {
		if (1 == loopback) {
			// Set GPIO high status
			write(fd_sim, "[", 2);
		} else {
			// Set GPIO low status
			write(fd_sim, "]", 2);
		}
	}
}

void get_device_simkey(int *key,int *led,int *broadcast,int *loopback) {
	char inmsg[100];

	if (GPIO_DEVICE_SIMULATOR == gpio_device) {
		/* 	return sprintf(buf, "K=%d L=%d B=%d LP=%d\n", key, led, broadcast, loopback); */
		strcpy(inmsg,"");
		key=0; led=0; broadcast=0; loopback=0;
		lseek(fd_sim, 0, SEEK_SET);
		read( fd_sim, &inmsg, 40);
		sscanf(inmsg, "K=%d L=%d B=%d LP=%d\n", key, led, broadcast, loopback);
	}
}

void toggle_device_simkey(void) {
	char inmsg[100];

	if (GPIO_DEVICE_SIMULATOR == gpio_device) {
		/* 	return sprintf(buf, "K=%d L=%d B=%d LP=%d\n", key, led, broadcast, loopback); */
		strcpy(inmsg,"");
		lseek(fd_sim, 0, SEEK_SET);
		read( fd_sim, &inmsg, 40);

		if (NULL != strstr(inmsg,"K=1"))
			set_device_simkey(0);
		else
			set_device_simkey(1);
	}
}

char *get_sim_device_state_pretty() {
	char         inmsg[100];
	static char outmsg[100];

	if (GPIO_DEVICE_SIMULATOR == gpio_device) {
		/* 	return sprintf(buf, "K=%d L=%d B=%d LP=%d\n", key, led, broadcast, loopback); */
		strcpy(inmsg,"");
		lseek(fd_sim, 0, SEEK_SET);
		read( fd_sim, &inmsg, 100);

		if (NULL != strstr(inmsg,"K=1"))
			strcpy(outmsg,"KEY ");
		else
			strcpy(outmsg,"    ");
		if (NULL != strstr(inmsg,"L=1"))
			strcat(outmsg,"LED ");
		else
			strcat(outmsg,"    ");

		/* strcpy(outmsg,inmsg); */
	} else {
		strcpy(outmsg,"");
	}

	return outmsg;
}
