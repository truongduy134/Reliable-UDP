/*
 * dropper.h
 * CS 3251 packet dropper
 * Russ Clark
 */

#ifndef _DROPPER_H_
#define _DROPPER_H_
int set_dropper(int L);
/*
L is a value between 0 and 100, inclusively.
L: loss rate

Return value:
1: success
-1: Invalid value of loss fraction
*/

ssize_t sendto_dropper(int s, const void *msg, size_t len,int flags,const struct sockaddr *to,int tolen);
/*
The parameters are the same as those in standard sendto() function
*/

#endif
