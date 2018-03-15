/*
** server.c -- a quick stream socket server for remote execution
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


#define PORT       3490 /* the port users will be connecting to         */
#define BACKLOG            30 /* how many pending connections queue will hold */
#define MAXDATASIZE 100 // max number of bytes we can get at once

int verbose = 0;


static void sigchld_handler(int s)
{
	while(wait(NULL) > 0);
}

/**************************************************************/

int server(void) {
	int    sockfd;               /* listen on sock_fd      */
	struct sockaddr_in my_addr;  /* my address information */
	struct sigaction sa;
    char   buf[MAXDATASIZE];
    char   retbuf[MAXDATASIZE];
	int    yes=1;
	int    quit=0;
	int    read_size;
	int    ret;

	printf("\n\n[ Quick Mode Test : Execute Strings from Client ]\n");

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("quick_server socket");
		return(1);
	}

	/*
	if (fcntl(sockfd, F_SETFL,O_NONBLOCK)!=0) {
		perror("fcntl nonblock");
		return(1);
	}
	*/

	if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
		perror("quick_server setsockopt");
		return(1);
	}
	my_addr.sin_family = AF_INET;         /* host byte order               */
	my_addr.sin_port = htons(PORT); /* short, network byte order     */
	my_addr.sin_addr.s_addr = INADDR_ANY; /* automatically fill with my IP */
	memset(&(my_addr.sin_zero), '\0', 8); /* zero the rest of the struct   */

	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
		perror("quick_server bind");
		return(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("quick_server listen");
		return(1);
	}

	sa.sa_handler = sigchld_handler; /* reap all dead processes */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("quick_server sigaction");
		return(1);
	}

	/* main accept() loop */
	while(0 == quit) {
		int new_fd;                    /* new connection on new_fd       */
		struct sockaddr_in their_addr; /* connector's address information */
		int sin_size;

		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
			/* errno=11: server accept: Resource temporarily unavailable */
			if ((errno !=EINPROGRESS) && (errno!=11)) {
				printf("errno=%d\n",errno);
				perror("quick_server accept");
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
			if (verbose) printf("  Client message: %s\n",buf);
			ret=system(buf);

			/* ack the request */
			sprintf(retbuf,"ret=%d",ret);
			if (send(new_fd, retbuf, strlen(retbuf), 0) == -1) {
				perror("quick_server send");
			}
		}
		close(new_fd);

	}

	/* clean up */
	close(sockfd);
	return 0;
}


int main(void) {
	// make PORT an optional paramter

	server();

}
