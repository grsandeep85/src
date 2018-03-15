/* morse_gpio header */

#define GPIO_DEVICE_NONE         0
#define GPIO_DEVICE_BEAGLEBLACK  1
#define GPIO_DEVICE_MINNOWMAX    2
#define GPIO_DEVICE_WANDBOARD    3
#define GPIO_DEVICE_SIMULATOR    4

extern void open_gpio_ports(void);
extern void close_gpio_ports(void);

extern int get_device_key(void);
extern int set_device_led(int led);
extern int get_user_key(void);
extern void set_user_key(int newkey);
extern void toggle_user_key(void);

extern int set_device_simkey(int key);
extern void get_device_simkey(int *key,int *led,int *broadcast,int *loopback);
extern void set_sim_broadcast(int broadcast);
extern void set_sim_loopback(int loopback);
extern void toggle_device_simkey(void);
extern char *get_sim_device_state_pretty();
