/* morse_client header */

#define CLIENT_MODE_LOCAL           0 /* local server */
#define CLIENT_MODE_REMOTE_PEER     1 /* local server */
#define CLIENT_MODE_REMOTE_SERVER   2 /* local server */

#define MAXDATASIZE 100 // max number of bytes we can get at once

extern int  morse_client_test(char *hostname);
extern int  morse_client(int client_mode,char *hostname);
