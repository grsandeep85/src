/* morse_codec header */

extern char *char2morse(char c);
extern char morse2char(char* m);

extern char inTextBuf[];
extern char inMorseBuf[];
extern char outTextBuf[];
extern char outTextStr[];

extern void parse_morse_init(void);
extern char scan_morse_in(void);
extern void scan_morse_out(void);
extern void parse_morse_out(char c);
extern void clear_morse_out(void);


