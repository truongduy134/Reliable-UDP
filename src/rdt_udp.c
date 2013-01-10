#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "dropper.h"
#include "helperlib.h"
#include "rdt_udp.h"
#include "socket_util.h"

#define NUM_EXTRA_PKT_BYTE_RDT 1
#define TIMEOUT_SECS 1		/* 1 second */
#define min(a, b) (a < b) ? a : b
#define ACK_TOKEN "ACK"
#define MAX_TRY 10
#define DEFAULT_MAX_PACKET_SIZE 5000
extern int debug;
extern int isClient;

size_t max_packet_size = DEFAULT_MAX_PACKET_SIZE;
static int send_seq_num = 0;
static int rcv_seq_num = 0;
/* convert_int_digit_to_char(...)
 *	Convert a digit (0 ... 9) of type int into the corresponding character.
 * 
 * Assumption: the input is valid (from 0 to 9)
 */
char convert_int_digit_to_char(int digit);

/* convert_char_digit_to_int(...)
 *	Convert a character digit ('0' ... '9') of type char into the corresponding integer.
 * 
 * Assumption: the input is valid (from '0' to '9')
 */
int convert_char_digit_to_int(char digit);

void make_packet(const void *data, size_t lenData, int seq_num, void *packet, size_t *lenPacket);
void extract_data_from_packet(const void *packet, size_t packet_len, void *buffer_data, size_t *data_len);

int extract_seq_num_from_packet(const void *packet, size_t packet_len);
int is_ack(const void *packet, size_t packet_len, int expect_ack_num);

/**********************************************************************************************/

void set_max_packet_size(size_t newSize)
{
	max_packet_size = newSize;
}

void initialize_rdt_recv()
{
	rcv_seq_num = 0;
}

void initialize_rdt_send()
{
	send_seq_num = 0;
}

ssize_t rdt_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
	const size_t MAX_PKT_SIZE = max_packet_size;
	char packet[MAX_PKT_SIZE + 1], ack_packet[MAX_PKT_SIZE + 1];
	size_t lenPacket;
	ssize_t numByteSent, numByteRcv;
	size_t numLeftSend = len, numWillSend, numSent = 0;
	int numTried;
	struct sockaddr_storage src_addr;
	socklen_t src_addrLen;
 	int flgContinue;
	char *printable_dest_ip, *printable_src_ip = NULL;

	get_printable_ip_addr(dest_addr, &printable_dest_ip);
	while(numLeftSend)
	{
		numWillSend = min(numLeftSend, MAX_PKT_SIZE - NUM_EXTRA_PKT_BYTE_RDT);
		numLeftSend -= numWillSend;
			
		/* Send chunks of packet, each of size at most MAX_PACKET_SIZE */
		make_packet(buf + numSent, numWillSend, send_seq_num, packet, &lenPacket);
/*		if(debug)
		{
			packet[lenPacket] = '\0';
			printf("Content of the package = %s", packet);
		} */

		numByteSent = sendto_dropper(sockfd, packet, lenPacket, flags, dest_addr, addrlen);
		if(numByteSent < 0)
			terminate_with_system_msg("sendto() fails");
		else
		{
			if(numByteSent != lenPacket)
				terminate_with_user_msg("sendto() error", "sent unexpected number of bytes");
		}

		/* Set alarm */
		alarm(TIMEOUT_SECS);

		/* Wait for ACK. Check ACK if received */
		numTried = 0;
		src_addrLen = sizeof(struct sockaddr_storage);
		flgContinue = 1;

		/* Here is the state of waiting for ACK */
		while(flgContinue)
		{
			if(debug)
				printf("Now wait for ACK\n");
			numByteRcv = recvfrom(sockfd, ack_packet, MAX_PKT_SIZE, 0, (struct sockaddr *) &src_addr, &src_addrLen);

			if(numByteRcv >= 0)
			{
				if(debug)
				{
					printf("Waiting ACK from source = %s\n", printable_dest_ip);
					get_printable_ip_addr((struct sockaddr *) &src_addr, &printable_src_ip);
					printf("Received a packet from source = %s\n", printable_src_ip);
				}

				if(!is_same_ip_and_port((struct sockaddr *) &src_addr, dest_addr))
				{
					if(debug)
						printf("Not expect packets from this source. Ignore the packet\n");
					flgContinue = 1;
				}
				else
				{
					if(debug)
						printf("Expect packets from this source. Check the packet\n");
					if(is_ack(ack_packet, numByteRcv, send_seq_num))
					{
						/* Confirm packet has been received */
						flgContinue = 0;
					}
					else
					{
						if(is_ack(ack_packet, numByteRcv, (send_seq_num + 1) % 2))
						{
							if(debug)
								printf("False ACK. Continue waiting\n");
							flgContinue = 1;
						}
						else
						{
							if(debug)
								printf("Not ACK or False ACK.\n");
							if(isClient)
							{
								if(debug)
									printf("Assume this is ACK because the receiver starts to send something else\n");
								flgContinue = 0;
							}
							else
							{
								if(debug)
									printf("Continue waiting\n");
								flgContinue = 1;
							}
						}
						
					}
				}				
			}
			else
			{
				if(errno == EINTR)
				{
					numTried++;
					if(numTried > MAX_TRY)
					{
						/* Give up sending */
						if(debug)
						{
							printf("Number of resending exceeds MAX_TRY = %d. Give up sending\n", MAX_TRY);
							printf("Stop communicating with this host\n");
						}
						/* Return with the number of byte sent = 0 */
						return 0;
						
					}
					/* Packet lost or corrupted */
					if(debug)
						printf("Packet has lost (or dropped because of corruption). Resend: %d\n", numTried);
					numByteSent = sendto_dropper(sockfd, packet, lenPacket, flags, dest_addr, addrlen);
					if(numByteSent < 0)
						terminate_with_system_msg("sendto() fails");
					else
					{
						if(numByteSent != lenPacket)
							terminate_with_user_msg("sendto() error", "sent unexpected number of bytes");
					}
					alarm(TIMEOUT_SECS);
					flgContinue = 1;
				}
				else
					terminate_with_system_msg("recvfrom() fails");
			}
		}

		/* Send successfully */
		numSent += numWillSend;

		/* Stop alarm */
		alarm(0);
		if(debug)
			printf("Confirm a packet has been received. Move on!\n");
		/* Change state */
		send_seq_num = (send_seq_num + 1) % 2;
	} 
	if(debug)
		printf("Send all content\n");
	return numSent;
}

ssize_t rdt_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, const struct sockaddr *expect_src_addr)
{
	const size_t MAX_PKT_SIZE = max_packet_size;
	ssize_t numByteRcv;
	ssize_t numByteSent;
	int packet_seq_num;
	char ack_packet[MAX_PKT_SIZE];
	size_t len_ack_packet;
	int flgContinue;
	char *printable_ip_expect, *printable_ip_recv;

	flgContinue = 1;
	get_printable_ip_addr(expect_src_addr, &printable_ip_expect);
	printable_ip_recv = NULL;

	while(flgContinue)
	{
		numByteRcv = recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
		
		if(numByteRcv > 0)
		{
			/* Checking packet */
			if(debug)
			{
				printf("Receive a packet of size %ld\n", numByteRcv);
				if(expect_src_addr != NULL)
					printf("Expected source IP address = %s\n", printable_ip_expect);
				get_printable_ip_addr(src_addr, &printable_ip_recv);
				printf("Received source IP address = %s\n", printable_ip_recv); 
			}

			if(expect_src_addr != NULL)
			{			
				if(!is_same_ip_and_port(src_addr, expect_src_addr))
				{
					if(debug)
						printf("Different addresses. Ignore the packet\n");
					continue;
				}
				else
				{
					if(debug)
						printf("Received from expected source. Handle the packet\n");
				}
			}
			
			packet_seq_num = extract_seq_num_from_packet(buf, (size_t) numByteRcv);
			if(debug)
			{
				printf("Expected sequence number = %d\n", rcv_seq_num);
				printf("Packet sequence number = %d\n", packet_seq_num);
			}
			if(packet_seq_num != rcv_seq_num)
			{
				if(debug)
					printf("Discard packet and send FALSE ACK\n");
				make_packet(ACK_TOKEN, strlen(ACK_TOKEN), (rcv_seq_num + 1) % 2, ack_packet, &len_ack_packet);	
				/* Duplicate packets. So continue to wait for a new packet */
			}
			else
			{
				if(debug)
					printf("Accept packet with sequence number = %d and send ACK\n", rcv_seq_num);
				make_packet(ACK_TOKEN, strlen(ACK_TOKEN), rcv_seq_num, ack_packet, &len_ack_packet);
				/* Change state */
				rcv_seq_num = (rcv_seq_num + 1) % 2;
				/* No need to continue waiting for packet */
				flgContinue = 0;
			}	 	

			/* Send ACK */
			numByteSent = sendto_dropper(sockfd, ack_packet, len_ack_packet, 0, src_addr, *addrlen);
			if(numByteSent < 0)
				terminate_with_system_msg("sendto() fails");
			else
				if(numByteSent != len_ack_packet)
					terminate_with_user_msg("sendto() error", "sent an unexpected number of bytes");		
		}
		else
			/* Finish receiving or occur error */
			flgContinue = 0;
	}

	free(printable_ip_expect);
	free(printable_ip_recv);

	return numByteRcv;
}

ssize_t rdt_recv_data(int sockfd, void *data, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, const struct sockaddr *expect_src_addr)
{
	const size_t MAX_PKT_SIZE = max_packet_size;
	char packet[MAX_PKT_SIZE + 1];
	ssize_t numByteRecv, numByteData;
	size_t numWillRecv;

	if(len + NUM_EXTRA_PKT_BYTE_RDT > MAX_PKT_SIZE)
		numWillRecv = MAX_PKT_SIZE;
	else
		numWillRecv = len + NUM_EXTRA_PKT_BYTE_RDT;

	numByteRecv = rdt_recvfrom(sockfd, packet, numWillRecv, flags, src_addr, addrlen, expect_src_addr);

	if(numByteRecv > 0)
	{
		extract_data_from_packet(packet, (size_t) numByteRecv, data, (size_t *) &numByteData);	
	}

	return numByteData;
}

/* make_packet(...)
 *
 *	Add more information regarding the reliable-data-transfer to the
 *		data before sending it.
 *
 * Note:
 *	+ packet is MAX_PACKET_SIZE byte long.
 *	+ The first byte is for the sequence number.
 *	+ Hence, data must be at most (MAX_PACKET_SIZE - 1) byte long.
 */
void make_packet(const void *data, size_t lenData, int seq_num, void *packet, size_t *lenPacket)
{
	char *char_data = (char *) data;

	char *char_packet = (char *) packet;
	*lenPacket = lenData + NUM_EXTRA_PKT_BYTE_RDT;

	char_packet[0] = convert_int_digit_to_char(seq_num);

	memcpy(char_packet + NUM_EXTRA_PKT_BYTE_RDT, char_data, lenData);
}

/* convert_int_digit_to_char(...)
 *	Convert a digit (0 ... 9) of type int into the corresponding character.
 * 
 * Assumption: the input is valid (from 0 to 9)
 */
char convert_int_digit_to_char(int digit)
{
	return digit + '0';
}

/* convert_char_digit_to_int(...)
 *	Convert a character digit ('0' ... '9') of type char into the corresponding integer.
 * 
 * Assumption: the input is valid (from '0' to '9')
 */
int convert_char_digit_to_int(char digit)
{
	return (int) (digit - '0');
}

/* extract_data_from_packet(...)
 *
 *	Extract the data from the packet.
 *
 * Note:
 *	+ packet is at most MAX_PACKET_SIZE byte long.
 *	+ The first byte is for the sequence number.
 *	+ Hence, data must be at most (MAX_PACKET_SIZE - 1) byte long.
 */
void extract_data_from_packet(const void *packet, size_t packet_len, void *buffer_data, size_t *data_len)
{	
	char *char_packet = (char *) packet;
	char *char_buffer_data = (char *) buffer_data;
	*data_len = packet_len - NUM_EXTRA_PKT_BYTE_RDT;

	memcpy(char_buffer_data, char_packet + NUM_EXTRA_PKT_BYTE_RDT, packet_len);
}

int extract_seq_num_from_packet(const void *packet, size_t packet_len)
{
	char *char_packet = (char *) packet;
	if(packet_len < 1)
		return -1;	/* Error */

	return convert_char_digit_to_int(char_packet[0]);
}


int is_ack(const void *packet, size_t packet_len, int expect_seq_num)
{
	const int lenAckToken = strlen(ACK_TOKEN);
	char packet_token[lenAckToken + 1];
	int index;
	char *char_packet = (char *) packet;

	if(extract_seq_num_from_packet(packet, packet_len) != expect_seq_num)
		return 0;

	if(packet_len < NUM_EXTRA_PKT_BYTE_RDT + strlen(ACK_TOKEN))
		return 0;

	for(index = 0; index < lenAckToken; index++)
		packet_token[index] = char_packet[index + NUM_EXTRA_PKT_BYTE_RDT];
	packet_token[lenAckToken] = '\0';

	if(strcmp(ACK_TOKEN, packet_token))
		return 0;
	return 1;	
}
