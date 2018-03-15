/* morse_server header */

#define MORSE_PORT       3490 /* the port users will be connecting to         */
#define BACKLOG            30 /* how many pending connections queue will hold */
#define QUERY_REPLAY_MAX  100 /* max for query reply string                   */

#define SERVER_MODE_NORMAL  0 /* reply from client with query results         */
#define SERVER_MODE_PEER    1 /* ack incoming client, pass to local client    */

extern int morse_server_test(void);

extern int morse_server_init(void);
extern char *morse_server_accept(int server_mode);
extern int morse_server_close(void);
extern int morse_server(void);

extern char *server_query(char query);
