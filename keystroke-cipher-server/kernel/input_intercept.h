#ifndef INPUT_INTERCEPT_H
#define INPUT_INTERCEPT_H

/*
 * input_intercept_init - register input handler with the Linux input subsystem
 * works for any keyboard: laptop built-in, USB, Bluetooth
 * called from keycipher_init() on module load
 * returns 0 on success, negative error code on failure
 */
int input_intercept_init(void);

/*
 * input_intercept_exit - unregister the input handler cleanly
 * called from keycipher_exit() on module unload
 */
void input_intercept_exit(void);

#endif /* INPUT_INTERCEPT_H */
