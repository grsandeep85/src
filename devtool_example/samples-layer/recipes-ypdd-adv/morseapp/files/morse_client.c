/*
** client.c -- a stream socket client for morse_app
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "morse_app.h"
#include "morse_codec.h"
#include "morse_gpio.h"
#include "morse_server.h"
#include "morse_client.h"

/* list of test message strings */
static int   testmsg_ptr=0;
static char *testmsg_list[]= {
	"alpha",
	"beta",
	"gamma",
	"delta",
	"epsilon",
	NULL
};

int myconnect(int sockfd, struct sockaddr *ta)
{
	if (-1 == connect(sockfd,ta,sizeof(struct sockaddr))) {
		perror("morse_client connect");
		return(1);
	}

	if (send(sockfd, testmsg_list[testmsg_ptr], strlen(testmsg_list[testmsg_ptr]), 0) == -1) {
		perror("morse_client send");
	}
	if (NULL == testmsg_list[++testmsg_ptr])
		testmsg_ptr = 0;
}

int recvprint(int sockfd)
{
	int numbytes;
    char buf[MAXDATASIZE];

	if ((numbytes=recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
		perror("morse_client recv");
		return(1);
	}
	buf[numbytes] = '\0';

	printf("  Received: %s\n",buf);
	return(0);
}

int morse_client_test(char *hostname) {
	int    sockfd;
	struct hostent *he;
	struct sockaddr_in their_addr; /* connector's address information */
	int    numbytes;
    char   buf[MAXDATASIZE];
	int    quit=0;
	char   key;

	printf("\n\n[ Client Mode Test : Send Test Strings to Server ]\n");
	printf("\nClick '#' to exit\n\n");

	if ((he=gethostbyname(hostname)) == NULL) {  /* get the host info */
		perror("morse_client gethostbyname");
		return(1);
	}
	while (0 == quit) {
		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			perror("morse_client socket");
			return(1);
		}

		their_addr.sin_family = AF_INET;          /* host byte order             */
		their_addr.sin_port = htons(MORSE_PORT);  /* short, network byte order   */
		their_addr.sin_addr = *((struct in_addr *)he->h_addr);
		memset(&(their_addr.sin_zero), '\0', 8);  /* zero the rest of the struct */

		if (myconnect(sockfd, (struct sockaddr *)&their_addr) == -1) {
			perror("morse_client connect");
			return(1);
		}
		recvprint(sockfd);

		close(sockfd);
		sleep(1);

		/* read any user input */
		key = get_key(0);
		if (0 != key) {
			if (('#' == key) || (ESC == key)) {
				quit=1;
			}
		}
	}
	return 0;
}


/**************************************************************/

void send_reply(char *src, char cmnd, char *msg) {
	int i;

	/* display the communication in server window */
	printf("\n %s:%c => '%s'\n", src, cmnd, msg);

	/* send the reply */
	for (i=0; i<strlen(msg) ; i++) {
		parse_morse_out(msg[i]);
	}

	/* follow with a space and the server prompt */
	parse_morse_out(' ');
	parse_morse_out('s');
}

int morse_client(int client_mode,char *hostname) {
	int    sockfd;
	struct hostent *he;
	struct sockaddr_in their_addr; /* connector's address information */
	int  i;
	int  quit = 0;
	char key;
	char in_req[10];
	int  output_update_cnt=0;
	int  numbytes;
	char disp_outTextStr[QUERY_REPLAY_MAX];
	char tagstr[20];
    char buf[MAXDATASIZE];
	int  ret;
	char *retstr;

	if (GPIO_DEVICE_NONE == gpio_device) {
		printf("ERROR: No device selected.\n");
		return;
	}

	/* init the parse structures. */
	parse_morse_init();
	strcpy(in_req,"");

	if (CLIENT_MODE_LOCAL == client_mode) {
		printf("\n\n[ Client to Local Server Mode : respond to device queries ]\n\n");
	}
	if (CLIENT_MODE_REMOTE_SERVER == client_mode) {
		printf("\n\n[ Client to Remote Server Mode : respond to device queries ]\n\n");
		if ((he=gethostbyname(hostname)) == NULL) {  /* get the host info */
			perror("morse_client gethostbyname");
			return(1);
		}
	}
	if (CLIENT_MODE_REMOTE_PEER == client_mode) {
		printf("\n\n[ Client to Peer Server Mode : respond to peer queries ]\n\n");
		ret = morse_server_init();
		if (0 != ret) return(ret);

		/* TO-DO : poll for peer's server to come up, or quit on '#' key */
	}

	printf("Server commands are:\n");
	printf("  'e' ('*')   : echo an 'e' ('*') \n");
	printf("  'd' ('-**') : echo the day of the week ('mon' .. 'sun') \n");
	printf("  't' ('-')   : echo the time ('hhmmss') \n");
	printf("  'f' ('**-*'): echo a fortune place\n");
	printf("  * use a space for word separation\n");
	if (GPIO_DEVICE_SIMULATOR == gpio_device) {
		printf("Type these keys for the simulated device:\n");
		printf("  '/' : toggle the device's key\n");
		printf("  '\\' : force clear the out buffer\n");
	}
	printf("Server prompt at device is 's' ('---') \n");
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
		key = scan_morse_in();

		/* else read any user input */
		if ('\0' == key) {
			key = get_key(0);
			if ('\0' != key) {
				if (('#' == key) || (ESC == key)) {
					quit=1;
				} else if ('/' == key) {
					toggle_device_simkey();
					key = '\0';
				} else if ('\\' == key) {
					clear_morse_out();
					printf("In:[%s] (%-5s) | Out: [%s] ()%14s  \r",inTextBuf,inMorseBuf,outTextBuf,"");
					key = '\0';
				}
			}
		}

		/* execute server query, send reply */
		if ('\0' != key) {
			if (CLIENT_MODE_LOCAL == client_mode) {
				strcpy(disp_outTextStr,server_query(key));
				if (0 != strlen(disp_outTextStr)) {
					send_reply("Local server",key,disp_outTextStr);
				}
			}
			if (CLIENT_MODE_REMOTE_SERVER == client_mode) {
				if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
					perror("morse_client socket");
					return(1);
				}

				their_addr.sin_family = AF_INET;          /* host byte order             */
				their_addr.sin_port = htons(MORSE_PORT);  /* short, network byte order   */
				their_addr.sin_addr = *((struct in_addr *)he->h_addr);
				memset(&(their_addr.sin_zero), '\0', 8);  /* zero the rest of the struct */

				if (-1 == connect(sockfd,(struct sockaddr *)&their_addr,sizeof(struct sockaddr))) {
					perror("morse_client connect");
				} else {
					sprintf(buf,"%c\n",key);
					if (-1 == send(sockfd, buf, strlen(buf), 0)) {
						perror("morse_client send");
					} else {
						if (-1 == (numbytes=recv(sockfd, disp_outTextStr, QUERY_REPLAY_MAX-1, 0))) {
							perror("morse_client recv");
						} else {
							disp_outTextStr[numbytes] = '\0';
							sprintf(buf,"Server %s",morse_server_ip);
							if (0 != strlen(disp_outTextStr)) {
								send_reply(buf,key,disp_outTextStr);
							}
						}
					}
				}
				close(sockfd);
			}
			if (CLIENT_MODE_REMOTE_PEER == client_mode) {
				retstr=morse_server_accept(SERVER_MODE_PEER);
				if (NULL != retstr) {
					strcpy(disp_outTextStr,retstr);
					if (0 != strlen(disp_outTextStr)) {
						send_reply("Remote Peer",'>',disp_outTextStr);
					}
				}
			}
		}

		/* update output buffer @ 1/2 second */
		if (output_update_cnt++ > output_update_max) {
			output_update_cnt=0;
			scan_morse_out();
		}

		/* sleep for 1/20 seconds = 50 mSec = 50000 uSec */
		usleep(50000);
	}

	if (CLIENT_MODE_REMOTE_PEER == client_mode) {
		morse_server_close();
	}

	return(0);
}

