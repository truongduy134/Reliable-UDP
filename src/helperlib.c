#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include "helperlib.h"

void terminate_with_system_msg(const char *msg)
{
	perror(msg);
	exit(1);
}

void terminate_with_user_msg(const char *msg, const char *detail)
{
	fprintf(stderr, "%s: %s\n", msg, detail);
	exit(1);
}

void set_signal_handler(int sig_num, void (*handler_func)(int))
{
	struct sigaction handler;

	memset(&handler, 0, sizeof(struct sigaction));
	handler.sa_handler = handler_func;

	if(sigaction(sig_num, &handler, NULL) < 0)
		terminate_with_system_msg("sigaction() fails");
}

ssize_t get_file_size(FILE *filePtr)
{
	ssize_t fileSize;

	fseek(filePtr, 0, SEEK_END);
	fileSize = ftell(filePtr);
	rewind(filePtr);

	return fileSize;
}

char * copy_str_dynamic(const char * originalStr)
{
	if(originalStr == NULL)
		return NULL;

	/* NOTE: Make 1 more SLOT of '\0' !!! */
	char * newStrPointer = (char *) malloc((strlen(originalStr) + 1) * sizeof(char));

	if(newStrPointer == NULL)
		return NULL;

	strcpy(newStrPointer, originalStr);

	return newStrPointer;
}
