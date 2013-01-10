/*
 * dropper.c
 * CS 3251 packet dropper
 * Russ Clark
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

/* GLOBAL DEFINITIONS
 * All defaults are set to zero
 */

extern int debug;  /* you should define this as a global in your program */

static int DROPPER_loss_percentage = 0;

int set_dropper(int L)
{
  struct timeval current_time;

  if ((L <0) || (L>100)) {
      if (debug) printf("set_dropper: Invalid value of loss percentage\n");
      return -1;
  }

  DROPPER_loss_percentage = L;

  if (debug) {
     printf("set_dropper: loss percentage set to %d\n",
             DROPPER_loss_percentage);
  }

  gettimeofday(&current_time,NULL);
  srand(current_time.tv_usec);

  return 1;
} /* set_dropper */

/* dropper version of sendto..accepting the same parameters */
ssize_t sendto_dropper(
     int s,
     const void *msg,
     size_t len,
     int flags,
     const struct sockaddr *to,
     int tolen
   ) {
   int randomvalue;
   int nbytes;

   randomvalue = rand() % 100;

   if (randomvalue < DROPPER_loss_percentage) {
       /* packet is lost --- do nothing, but make it look like success */
       return(len);
   }

   /* default: nothing wrong, call sendto as is.. */
   nbytes = sendto(s,msg,len,flags,to,tolen);
   return(nbytes);

} /* sendto_dropper */

