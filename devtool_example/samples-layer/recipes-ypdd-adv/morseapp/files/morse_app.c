/*
 * Copyright 2014 Wind River Systems, Inc.
 *
 * Licenced under BSD part 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <termios.h>
#include <time.h>
#include <signal.h>

#include "morse_app.h"
#include "morse_gpio.h"
#include "morse_codec.h"
#include "morse_client.h"
#include "morse_server.h"

int  stdin_fd          = -1;
int  gpio_device       = GPIO_DEVICE_NONE;
int  morsemod_instance = 0;
char morse_server_ip[100];

/* debug level threshold */
int  debug_mode=0;

int parent (int [],pid_t); int child (int []); /* Prototypes. */

/**************************************************************/

void debug(int level, char* msg) {
	if (level <= debug_mode) {
		printf(msg);
	}
}

/* Non blocking char read in parent from child */
char get_a_key(void) {
	int nread; char buf[2];

	buf[0]='\0';
	/* Check the pipe for each key. Stop when the pipe is closed. */
	switch (nread = read(stdin_fd, buf, 2)) {
		case -1: /* Make sure that pipe is empty. */
			if (errno == EAGAIN) {
				if (2 <= debug_mode) {
					debug(2,"Parent: Pipe is empty\n");
					sleep(1);
				}
			} else { /* Reading from pipe failed. */
				fprintf(stderr, "Parent: Couldn’t read from pipe.\n");
				exit(1);
			}
			break;
		case 0: /* Pipe has been closed. */
			debug(2,"Parent: End of conversation.\n");
			exit(0);
			break;
		default: /* Received a message from the pipe. */
			debug(2,"Parent: Message -- \n");
			break;
	} /* End of switch. */

	return buf[0];
}

/* Get a char, in either blocking or non-blocking mode */
int get_key(int block) {
	char key;

	key = get_a_key();
	if (1 == block) {
		while ('\0' == key) {
			usleep(1000);
			key = get_a_key();
		}
	}

	return key;
}

char morse_char(int val) {
	if (1 == val)
		return '*';
	else
		return ' ';
}

/**************************************************************/

void loopback(void) {

	int quit = 0;
	char in_keyboard  = '\0';
	int key;

	if (GPIO_DEVICE_NONE == gpio_device) {
		printf("ERROR: No device selected.\n");
		return;
	}

	printf("\n\n[ Loopback : device keys are echoed back to the device LED ]\n\n");
	printf("Type '#' to quit\n\n");

	while (0 == quit) {

		/* read and echo the device state */
		key = get_device_key();
		set_device_led(key);
		printf("Device Key: [%c] \r",morse_char(key));

		/* read any user input */
		key = get_key(0);
		if (0 != key) {
			if (('#' == key) || (ESC == key))
				quit = 1;
		}

		/* sleep for 1/4 seconds = 250 mSec = 250000 uSec */
		usleep(250000);
	}
}

/**************************************************************/

void talk_morse(void) {

	int quit = 0;
	char in_keyboard  = '\0';
	int key;

	if (GPIO_DEVICE_NONE == gpio_device) {
		printf("ERROR: No device selected.\n");
		return;
	}

	printf("\n\n[ Talk Morse code ]\n\n");
	printf("Type the '.' period key to toggle your 'key'\n");
	printf("  * a 'dit' is about 1/2 seconds\n");
	printf("  * a 'dah' is about 1 1/2 seconds (three 'dits') \n");
	printf("  * a letter space is about 1/2 seconds\n");
	printf("  * a word   space is about 1 1/2 seconds\n");
	if (GPIO_DEVICE_SIMULATOR == gpio_device) {
		printf("Type these keys for the simulated device\n");
		printf("  '/' : toggle the device's key\n");
		printf("  '<' : broadcast mode on\n");
		printf("  '>' : broadcast mode off\n");
	}
	printf("Type '#' to quit\n\n");

	while (0 == quit) {

		/* read the device state */
		if (GPIO_DEVICE_SIMULATOR == gpio_device) {
			printf("UserKey: [%c] | DeviceKey: [%c] || SIM: %s\r",
				morse_char(get_user_key()),morse_char(get_device_key()),get_sim_device_state_pretty()
				);
		} else {
			printf("User: [%c] | Device: [%c] \r",
				morse_char(get_user_key()),morse_char(get_device_key())
				);
		}

		/* read any user input */
		key = get_key(0);
		if (0 != key) {
			if ('.' == key) toggle_user_key();
			if ('*' == key) toggle_user_key();
			if (GPIO_DEVICE_SIMULATOR == gpio_device) {
				if ('/' == key) toggle_device_simkey();
				if ('<' == key) set_sim_broadcast(1);
				if ('>' == key) set_sim_broadcast(0);
			}
			if (('#' == key) || (ESC == key))
				quit=1;
		}

		/* sleep for 1/4 seconds = 250 mSec = 250000 uSec */
		usleep(250000);
	}
}

/**************************************************************/

void talk_text(void) {

	int  i;
	int  quit = 0;
	char key;
	char disp_outTextStr[QUERY_REPLAY_MAX];
	char tagstr[20];

	int output_update_cnt=0;

	if (GPIO_DEVICE_NONE == gpio_device) {
		printf("ERROR: No device selected.\n");
		return;
	}

	/* init the parse structures. */
	parse_morse_init();

	printf("\n\n[ Talk Text (ASCII) ]\n\n");
	printf("Type your text message:'\n");
	printf("  * use the letters a-z, 0-9 (case is ignored)\n");
	printf("  * use a space for word separation\n");
	if (GPIO_DEVICE_SIMULATOR == gpio_device) {
		printf("Type these keys for the simulated device\n");
		printf("  '<' : broadcast mode on\n");
		printf("  '>' : broadcast mode off\n");
		printf("  '\\' : force clear the out buffer\n");
	}
	printf("Type '#' to quit\n\n");

	while (0 == quit) {

		/* truncate in the display the outgoing morse string */
		if (9 < strlen(outTextStr)) {
			strncpy(disp_outTextStr,outTextStr,9);
			disp_outTextStr[9] = '\0';
			sprintf(tagstr,"+%d",strlen(outTextStr)-9);
			strcat(disp_outTextStr,tagstr);
		} else {
			strcpy(disp_outTextStr,outTextStr);
		}

		/* display the device state */
		printf("In:[%s] (%-5s) | Out: [%s] (%s)  \r",inTextBuf,inMorseBuf,outTextBuf,disp_outTextStr);
		/* read the device key port @ 1/20 second */
		scan_morse_in();

		/* update output buffer @ 1/2 second */
		if (output_update_cnt++ > output_update_max) {
			output_update_cnt=0;
			scan_morse_out();
		}

		/* read any user input */
		key = tolower(get_key(0));
		if (0 != key) {
			if (('#' == key) || (ESC == key)) {
				quit=1;
			}
			if (GPIO_DEVICE_SIMULATOR == gpio_device) {
				if ('<' == key) set_sim_broadcast(1);
				if ('>' == key) set_sim_broadcast(0);
				if ('\\' == key) {
					clear_morse_out();
					printf("In:[%s] (%-5s) | Out: [%s] ()%14s  \r",inTextBuf,inMorseBuf,outTextBuf,"");
				}
			}
			if ( (('a' <= key) && ('z' >= key)) ||
			     (('0' <= key) && ('9' >= key)) ||
			     ( ' ' == key                 ) ) {
				parse_morse_out(key);
			}
		}

		/* sleep for 1/20 seconds = 50 mSec = 50000 uSec */
		usleep(50000);
	}
}

/**************************************************************/

#define BS    8
#define LF   10
#define CR   13
#define ESC  27
#define BS2 127

void input_digit(char *name,int *val) {
	int  v;
	int  quit = 0;
	char cmnd;
	int  len;

	v = *val;
	/* we turned off buffer control, so we have to do it ourselved */
	printf("\n\n[Click CR to accept, ESC to cancel]\n");
	while (!quit) {
		printf("\r%s: %d\b",name,v);
		cmnd = get_key(1);
		switch (cmnd) {
			case ESC: /* quit, no change */
				quit = 1;
				break;
			case CR: /* accept change */
			case LF:
				*val = v;
				quit = 1;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				v = cmnd - '0';
				break;
		}
	}

	printf("\n\n");
}

void input_string(char *name,char *str) {
	char s[100];
	int  quit = 0;
	char cmnd;
	int  len;

	strcpy(s,str);

	/* we turned off buffer control, so we have to do it ourselved */
	printf("\n\n[Click CR to accept, ESC to cancel]\n");
	while (!quit) {
		printf("\r%s: %s",name,s);
		cmnd = get_key(1);
		switch (cmnd) {
			case ESC: /* quit, no change */
				quit = 1;
				break;
			case BS:   /* back space CTRL-H */
			case BS2:  /* back space key    */
				len=strlen(s);
				if (len > 0) {
					s[len-1] = '\0';
					printf("\r%s: %s ",name,s);
				}
				break;
			case CR: /* accept change */
			case LF:
				strcpy(str,s);
				quit = 1;
				break;
			default :
				len=strlen(s);
				s[len  ] = cmnd;
				s[len+1] = '\0';
				break;
		}
	}

	printf("\n\n");
}


char configure_match(int device) {
	if ( device == gpio_device)
		return '>';
	else
		return ' ';
}

void configure_morse(void) {

	int quit = 0;
	int previous_device = gpio_device;
	int previous_instance = morsemod_instance;
	char cmnd;

	while (!quit) {
		printf("Hardware Device:\n");
		printf(" %c 1. No device\n"           ,configure_match(GPIO_DEVICE_NONE));
		printf(" %c 2. Beaglebone Black\n"    ,configure_match(GPIO_DEVICE_BEAGLEBLACK));
		printf(" %c 3. Minnowboard Max\n"     ,configure_match(GPIO_DEVICE_MINNOWMAX));
		printf(" %c 4. Wandboard Quad\n"      ,configure_match(GPIO_DEVICE_WANDBOARD));
		printf("Simulated Device:\n");
		printf(" %c 5. Simulated device  (morsemod)\n",configure_match(GPIO_DEVICE_SIMULATOR));
		printf("   6. Morsemod instance (%d)\n",morsemod_instance);
		printf("Remote Server:\n");
		printf("   7. IP Address (%s)\n",morse_server_ip);
		printf("   m. Main Menu\n");

		printf("Cmnd: ");
		cmnd = get_key(1); printf("%c\n",cmnd);
		if ( '1' == cmnd) gpio_device = GPIO_DEVICE_NONE;
		if ( '2' == cmnd) gpio_device = GPIO_DEVICE_BEAGLEBLACK;
		if ( '3' == cmnd) gpio_device = GPIO_DEVICE_MINNOWMAX;
		if ( '4' == cmnd) gpio_device = GPIO_DEVICE_WANDBOARD;
		if ( '5' == cmnd) gpio_device = GPIO_DEVICE_SIMULATOR;
		if ( '6' == cmnd) input_digit("Morsemod instance",&morsemod_instance);
		if ( '7' == cmnd) input_string("Remote Server IP Address",morse_server_ip);
		if (('m' == cmnd) || ('q' == cmnd) || ( ESC == cmnd)) {
			quit=1;
			if ((previous_device != gpio_device) || (previous_instance != morsemod_instance)) {
				close_gpio_ports();
				open_gpio_ports();
			}
		}
	}
}

/**************************************************************
 * Main: set up a child and a parent process
 *   Child : receive the blocking user keys and send to parent
 *   Parent: do non-blocking inputs tests as part of loops
 *
 */

static struct termios oldt, newt;

int main(void) {
	int pfd[2]; /* Pipe’s file descriptors. */
	pid_t child_pid;

	printf("Welcome to morseapp!\n\n");
	strcpy(morse_server_ip,"localhost");

	/* see "http://stackoverflow.com/questions/1798511/how-to-avoid-press-enter-with-any-getchar" */

	/*tcgetattr gets the parameters of the current terminal
	STDIN_FILENO will tell tcgetattr that it should write the settings
	of stdin to oldt*/
	tcgetattr( STDIN_FILENO, &oldt);
	/*now the settings will be copied*/
	newt = oldt;

	/*ICANON normally takes care that one line at a time will be processed
	that means it will return if it sees a "\n" or an EOF or an EOL*/
	newt.c_lflag &= ~(ICANON);
	newt.c_lflag &= ~(ICANON | ECHO);

	/*Those new settings will be set to STDIN
	TCSANOW tells tcsetattr to change attributes immediately. */
	tcsetattr( STDIN_FILENO, TCSANOW, &newt);

	/* Set up pipe. */
	if (pipe(pfd) == -1) {
		fprintf(stderr, "Call to pipe failed.\n");
		exit(1);
	}
	/* Set O_NONBLOCK flag for the read end (pfd[0]) of the pipe. */
	if (fcntl(pfd[0], F_SETFL, O_NONBLOCK) == -1) {
		fprintf(stderr, "Call to fcntl failed.\n");
		exit(1);
	}
	/* Fork a child process. */
	child_pid = fork();
	switch (child_pid) {
		case (pid_t) -1: /* Fork failed. */
			fprintf(stderr, "Call to fork failed.\n");
			exit(1);
			break;
		case 0: /* Child process. */
			child(pfd);
			break;
		default: /* Parent process. */
			parent(pfd,child_pid);
			break;
	} /* End of switch. */
} /* End of main. */


int child (int p[]) {
	char ch[10];

	/* Close the read-end of the pipe. */
	if (close(p[0]) == -1) { /* Failed to close read end of pipe. */
		fprintf(stderr, "Child: Couldn’t close read end of pipe.\n");
		exit(1);
	}

	/* continue to read STDIN, sending it to parent over the pipe */
	ch[1] = '\n';
	while (1) {
		ch[0] = getchar();
		/*fprintf(stderr, "Child: got a %c.\n",ch[0]);*/
		write(p[1], ch, 2);
	}

    /* restore the old terminal settings */
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);

} /* End of child. */


int parent (int p[],pid_t child_pid) {
	int quit = 0;
	char cmnd;

	/* Close the write-end of the pipe. */
	if (close(p[1]) == -1) { /* Failed to close write end of pipe. */
		fprintf(stderr, "Parent: Couldn’t close write end of pipe.\n");
		exit(1);
	}
	/* and here the pipe is where parent reads chars from */
	stdin_fd = p[0];

	/* Don't buffer the output strings */
	setbuf(stdout, NULL);

	/* main command loop */
	while (!quit) {
		printf("\n\nCommands  bb:\n");
		printf("  1. Configure\n");
		printf("  2. Loopback\n");
		printf("  3. Talk Morse\n");
		printf("  4. Talk Text\n");
		printf("  5. Talk to local Server\n");
		printf("---------------------------\n");
		printf("  6. Talk to Network Server\n");
		printf("  7. Be  the Network Server\n");
		printf("  8. Talk to Network Peer\n");
		printf("---------------------------\n");
		printf("  s. Test Server\n");
		printf("  c. Test client\n");
		printf("---------------------------\n");
		printf("  q. Quit\n");

		printf("Cmnd: ");
		cmnd = get_key(1); printf("%c\n",cmnd);
		if ( '1' == cmnd) configure_morse();
		if ( '2' == cmnd) loopback();
		if ( '3' == cmnd) talk_morse();
		if ( '4' == cmnd) talk_text();
		if ( '5' == cmnd) morse_client(CLIENT_MODE_LOCAL,morse_server_ip);
		if ( '6' == cmnd) morse_client(CLIENT_MODE_REMOTE_SERVER,morse_server_ip);
		if ( '7' == cmnd) morse_server();
		if ( '8' == cmnd) morse_client(CLIENT_MODE_REMOTE_PEER,morse_server_ip);
		if (('x' == cmnd) || ( 'q' == cmnd)) quit=1;

		if ( 'c' == cmnd) morse_client_test(morse_server_ip);
		if ( 's' == cmnd) morse_server_test();

		if ( 'd' == cmnd) debug_mode=1;
		if ( 'D' == cmnd) debug_mode=2;
		if ( 'e' == cmnd) debug_mode=0;
	}

	/* clean up the hardware */
	close_gpio_ports();

    /* restore the old terminal settings */
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);

	/* close the child, because it is blocked waiting for a char that will not come */
	kill(child_pid, SIGTERM);

	printf("\nBye!\n");
}

