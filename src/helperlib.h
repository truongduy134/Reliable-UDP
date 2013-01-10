#ifndef _HELPERLIB_H_
#define _HELPERLIB_H_

void terminate_with_system_msg(const char *msg);
void terminate_with_user_msg(const char *msg, const char *detail);

void set_signal_handler(int sig_num, void (*handler_func)(int));

ssize_t get_file_size(FILE *filePtr);

/* copy_str_dynamic(...)
 *
 * 	Copies the content of the input string to new dynamically allocated memory slots.
 * 
 *	Returns a pointer to the beginning of the new memory slots.
 * 	Returns NULL if there is an exception in allocation or copying. 
 */
char * copy_str_dynamic(const char * originalStr);

#endif /* ifndef _HELPERLIB_H_ */
