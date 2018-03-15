/*
** morse_server.c -- a stream socket server for morse_app
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include "morse_app.h"
#include "morse_client.h"
#include "morse_server.h"

static void sigchld_handler(int s)
{
	while(wait(NULL) > 0);
}

/**************************************************************/

int morse_server_test(void) {
	int    sockfd;               /* listen on sock_fd      */
	struct sockaddr_in my_addr;  /* my address information */
	struct sigaction sa;
    char   buf[MAXDATASIZE];
	int    yes=1;
	char   disp_outTextStr[QUERY_REPLAY_MAX];
	int    quit=0;
	char   key;
	char   cmnd;
	int    read_size;

	printf("\n\n[ Server Mode Test : Return Test Strings to Client ]\n");
	printf("\nClick '#' to exit\n\n");

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("morse_server socket");
		return(1);
	}

	if (fcntl(sockfd, F_SETFL,O_NONBLOCK)!=0) {
		perror("fcntl nonblock");
		return(1);
	}

	if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
		perror("morse_server setsockopt");
		return(1);
	}
	my_addr.sin_family = AF_INET;         /* host byte order               */
	my_addr.sin_port = htons(MORSE_PORT); /* short, network byte order     */
	my_addr.sin_addr.s_addr = INADDR_ANY; /* automatically fill with my IP */
	memset(&(my_addr.sin_zero), '\0', 8); /* zero the rest of the struct   */

	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
		perror("morse_server bind");
		return(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("morse_server listen");
		return(1);
	}

	sa.sa_handler = sigchld_handler; /* reap all dead processes */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("morse_server sigaction");
		return(1);
	}

	/* main accept() loop */
	while(0 == quit) {
		int new_fd;                    /* new connection on new_fd       */
		struct sockaddr_in their_addr; /* connector's address information */
		int sin_size;

		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
			/* errno=11: morse_server accept: Resource temporarily unavailable */
			if ((errno !=EINPROGRESS) && (errno!=11)) {
				printf("errno=%d\n",errno);
				perror("morse_server accept");
				continue;
			}
		} else {
			printf("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));

			/* If connection is established then start communicating */
			bzero(buf,MAXDATASIZE);
			read_size = recv(new_fd , buf , MAXDATASIZE , 0);
			if (read_size < 0)
			{
				perror("ERROR reading from socket");
				return(1);
			}

			/* execute server query, send reply */
			printf("  Here is the client message: %s\n",buf);
			cmnd='f';
			strcpy(disp_outTextStr,server_query(cmnd));
			if (send(new_fd, disp_outTextStr, strlen(disp_outTextStr), 0) == -1) {
				perror("morse_server send");
			}
		}
		close(new_fd);

		/* read any user input */
		key = get_key(0);
		if (0 != key) {
			if (('#' == key) || (ESC == key)) {
				quit=1;
			}
		}
	}

	/* clean up */
	close(sockfd);
	return 0;
}

/**************************************************************/

static int    sockfd;               /* listen on sock_fd      */
static struct sockaddr_in my_addr;  /* my address information */
static struct sigaction sa;

int morse_server_init(void) {
	int    yes=1;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("morse_server socket");
		return(1);
	}

	if (fcntl(sockfd, F_SETFL,O_NONBLOCK)!=0) {
		perror("fcntl nonblock");
		return(1);
	}

	if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
		perror("morse_server setsockopt");
		return(1);
	}
	my_addr.sin_family = AF_INET;         /* host byte order               */
	my_addr.sin_port = htons(MORSE_PORT); /* short, network byte order     */
	my_addr.sin_addr.s_addr = INADDR_ANY; /* automatically fill with my IP */
	memset(&(my_addr.sin_zero), '\0', 8); /* zero the rest of the struct   */

	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
		perror("morse_server bind");
		return(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("morse_server listen");
		return(1);
	}

	sa.sa_handler = sigchld_handler; /* reap all dead processes */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("morse_server sigaction");
		return(1);
	}

	return(0);
}

char *morse_server_accept(int server_mode) {
    static char buf[MAXDATASIZE];
	char   disp_outTextStr[QUERY_REPLAY_MAX];
	int    read_size;

	int new_fd;                    /* new connection on new_fd       */
	struct sockaddr_in their_addr; /* connector's address information */
	int sin_size;

	sin_size = sizeof(struct sockaddr_in);
	if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
		/* errno=11: morse_server accept: Resource temporarily unavailable */
		if ((errno !=EINPROGRESS) && (errno!=11)) {
			perror("morse_server accept");
			fprintf(stderr,"  errno=%d\n",errno);
			return("");
		} else {
			/* nothing ready - give some time back */
			usleep(50000);
		}
	} else {
		printf("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));

		/* If connection is established then start communicating */
		bzero(buf,MAXDATASIZE);
		read_size = recv(new_fd , buf , MAXDATASIZE , 0);
		if (read_size < 0)
		{
			perror("ERROR reading from socket");
			return(NULL);
		}

		/* execute server query, send reply */
		if (SERVER_MODE_NORMAL == server_mode) {
			strcpy(disp_outTextStr,server_query(buf[0]));
			printf("  Client message '%c' => %s\n", buf[0], disp_outTextStr);
			}
	    if (SERVER_MODE_PEER == server_mode) {
			strcpy(disp_outTextStr,"_ack_");
		}
		if (send(new_fd, disp_outTextStr, strlen(disp_outTextStr), 0) == -1) {
			perror("morse_server send");
		}
	}
	close(new_fd);

	return(buf);
}

int morse_server_close(void) {
	/* close the socket to allow next connection */
	close(sockfd);

	return(0);
}

int morse_server(void) {
	int    ret;
	int    quit=0;
	char   key;
	char   cmnd;

	printf("\n\n[ Server Mode Morse : Return Morse Server Strings to Client ]\n");
	printf("\nClick '#' to exit\n\n");

	/* init the server */
	ret = morse_server_init();
	if (0 != ret)
		return(ret);

	/* main accept() loop */
	while(0 == quit) {

		/* See if there is any traffic waiting */
		if (NULL == morse_server_accept(SERVER_MODE_NORMAL))
			break;

		/* read any user input */
		key = get_key(0);
		if (0 != key) {
			if (('#' == key) || (ESC == key)) {
				quit=1;
			}
		}
	}

	/* clean up */
	morse_server_close();

	return(ret);
}

/**************************************************************/

static char *get_date_time(char dt) {
    static char ret_str[20];
    time_t current_time;
    char  *c_time_string;
    int    i=0;

	current_time = time(NULL);
    if (current_time == ((time_t)-1)) {
        return "qq";
    }
	c_time_string = ctime(&current_time);
	if (c_time_string == NULL) {
        return "qq";
    }

    /* Current time is Wed Aug 20 17:53:49 2014 */
    /*                 012345678901234567890123 */
    /*                 0         1         2    */
	if ('d' == dt) {
		if (' ' == c_time_string[8]) c_time_string[8] = '0';
		ret_str[i++] = c_time_string[ 0];
		ret_str[i++] = c_time_string[ 1];
		ret_str[i++] = c_time_string[ 2];
		ret_str[i++] = '\0';
	} else {
		if (' ' == c_time_string[11]) c_time_string[11] = '0';
		if (' ' == c_time_string[14]) c_time_string[14] = '0';
		if (' ' == c_time_string[17]) c_time_string[17] = '0';
		ret_str[i++] = c_time_string[11];
		ret_str[i++] = c_time_string[12];
		ret_str[i++] = c_time_string[14];
		ret_str[i++] = c_time_string[15];
		ret_str[i++] = c_time_string[17];
		ret_str[i++] = c_time_string[18];
		ret_str[i++] = '\0';
	}

	return ret_str;
}

/* list of 'fortune' strings */
static int   fortune_ptr=0;
static char *fortune_list[]= {
	"Altbeers",
	"Rhine",
	"Burgplatz",
	"Altstadt",
	"Marktplatz",
	"Dusseldorf",
	NULL
};

/* Server commands are:                                      */
/*   'e' ('*')   : echo an 'e' ('*')                         */
/*   'd' ('-**') : echo the day of the week ('mon' .. 'sun') */
/*   't' ('-')   : echo the time ('hhmmss')                  */

char *server_query(char query) {
	static char disp_outTextStr[QUERY_REPLAY_MAX];

	strcpy(disp_outTextStr,"");

	/* parse any incoming request */
	switch (query) {
		case '\0' : /* no command yet */
			break;
		case 'e' : /* echo */
			strcpy(disp_outTextStr,"e");
			break;
		case 'd' : /* day of the week */
			strcpy(disp_outTextStr,get_date_time('d'));
			break;
		case 'f' : /* fortune */
			strcpy(disp_outTextStr,fortune_list[fortune_ptr++]);
			if (NULL == fortune_list[fortune_ptr])
				fortune_ptr = 0;
			break;
		case 't' : /* time */
			strcpy(disp_outTextStr,get_date_time('t'));
			break;
		default: /* unknown command */
			/* strcpy(disp_outTextStr,"q"); */
			break;
	} /* End of switch. */

	return disp_outTextStr;
}