/* morse_app header */

#define output_update_max  10 /* update the output = 10/20 sec */

#define ESC  27               /* escape key */

extern int  gpio_device;
extern int  morsemod_instance;
extern char morse_server_ip[];

extern char get_a_key(void);
