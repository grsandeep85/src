/*
** client.c -- a stream socket client for quick_app
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

#define MAXDATASIZE 100 // max number of bytes we can get at once
#define PORT       3490 /* the port users will be connecting to         */

int verbose = 0;

int myconnect(int sockfd, struct sockaddr *ta,char *msg)
{
	if (-1 == connect(sockfd,ta,sizeof(struct sockaddr))) {
		perror("quick_client connect");
		return(1);
	}

	if (send(sockfd, msg, strlen(msg), 0) == -1) {
		perror("quick_client send");
	}
}

int recvprint(int sockfd)
{
	int numbytes;
    char buf[MAXDATASIZE];

	if ((numbytes=recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
		perror("quick_client recv");
		return(1);
	}
	buf[numbytes] = '\0';

	if (verbose) printf("  Received: %s\n",buf);
	return(0);
}

int quick_client(char *hostname,char *msg) {
	int    sockfd;
	struct hostent *he;
	struct sockaddr_in their_addr; /* connector's address information */
	int    numbytes;
    char   buf[MAXDATASIZE];
	int    quit=0;
	char   key;

	printf("\n\n[ Client Mode Test : Send a command Strings to Server ]\n");

	if ((he=gethostbyname(hostname)) == NULL) {  /* get the host info */
		perror("quick_client gethostbyname");
		return(1);
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("quick_client socket");
		return(1);
	}

	their_addr.sin_family = AF_INET;          /* host byte order             */
	their_addr.sin_port = htons(PORT);  /* short, network byte order   */
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	memset(&(their_addr.sin_zero), '\0', 8);  /* zero the rest of the struct */

	if (myconnect(sockfd, (struct sockaddr *)&their_addr, msg) == -1) {
		perror("quick_client connect");
		return(1);
	}
	recvprint(sockfd);

	close(sockfd);

	return 0;
}


/**************************************************************/

int main(int argc, char *argv[])
{
	int sockfd;
	struct hostent *he;
	struct sockaddr_in their_addr; // connector's address information

	if (argc != 3) {
		fprintf(stderr,"usage: quick_client hostname msg\n");
		exit(1);
	}

	quick_client(argv[1],argv[2]);

}