#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "helperlib.h"
#include "socket_util.h"
#include "dropper.h"
#include "rdt_udp.h"

#define DEBUG_FLAG "-d"
#define DEBUG_FLAG_CAPITAL "-D"
#define MAX_LEN_REQUEST 8000
#define MAX_LEN_FILENAME 5000
#define MAX_LEN_RESPONSE 10000

#define BAD_REQUEST 400
#define OK 200
#define NOT_FOUND 404

int debug;
int isClient = 0;

void catch_alarm(int signum);
int parse_request(char *request, char *file_name, size_t *maxPacketSize);
void send_file_to_client(int sockfd, struct sockaddr *dest_addr, socklen_t dest_addrlen, int errorCode, FILE *fin);
char * read_request(int serverSocket, struct sockaddr *clientAddr, socklen_t *clientAddrLen);
int is_end_of_request(char bufferRequest[], int len);

int main(int argc, char *argv[])
{
	int serverSocket, errorCode;
	struct sockaddr_storage clientAddr;
	char filename[MAX_LEN_FILENAME + 1];
	char *request;
	size_t maxPacketSize;
	FILE *filePtr;
	socklen_t clientAddrLen;

	if(argc < 3)
		terminate_with_user_msg("Command-line arguments", "<server_port_number> <loss_percent> [-d]");

	/* Check if there is a debug flag */
	if(argc == 3)
		debug = 0;
	else
	{
		if(strcmp(argv[3], DEBUG_FLAG) == 0 || strcmp(argv[3], DEBUG_FLAG_CAPITAL) == 0)
			debug = 1;
		else
			debug = 0;
	}

	serverSocket = set_up_udp_server_socket(argv[1]);
	if(serverSocket < 0)
		terminate_with_user_msg("set_up_udp_server_socket() fails", argv[1]);

	/* Set dropper */
	if(set_dropper(atoi(argv[2])) < 0)
		terminate_with_user_msg("set_dropper() fails", argv[2]);

	/* Set handler for SIGALRM */
	set_signal_handler(SIGALRM, catch_alarm);

	/* Handle clients */
	while(1)
	{
		initialize_rdt_recv();
		initialize_rdt_send();
		request = read_request(serverSocket, (struct sockaddr *) &clientAddr, &clientAddrLen);
		
		if(request == NULL || parse_request(request, filename, &maxPacketSize) < 0)
		{
			errorCode = BAD_REQUEST;
			filePtr = NULL;
			printf("Bad request\n");
		}
		else
		{
			filePtr = fopen(filename, "r");
			set_max_packet_size(maxPacketSize);
			if(filePtr == NULL)
				errorCode = NOT_FOUND;
			else
				errorCode = OK;	
			printf("File name = %s\n", filename);
			printf("Max Packet Size = %d\n", (int) maxPacketSize);	
		}

		
		send_file_to_client(serverSocket, (struct sockaddr *) &clientAddr, clientAddrLen, errorCode, filePtr);
		if(filePtr != NULL)
			fclose(filePtr);
		free(request);	
	}

	return 0;	
}

void catch_alarm(int signum)
{
	if(signum == SIGALRM && debug)
	{
		printf("Catch SIGALRM signal\n");
	}
}

char * read_request(int serverSocket, struct sockaddr *clientAddr, socklen_t *clientAddrLen)
{
	 
	char requestBuffer[MAX_LEN_REQUEST + 1];
	ssize_t totalRcvByte = 0, numByteRcv;
	struct sockaddr_storage dummy;
	socklen_t dummyAddrLen = sizeof(struct sockaddr_storage);

	*clientAddrLen = sizeof(struct sockaddr_storage);
	numByteRcv = rdt_recv_data(serverSocket, requestBuffer, MAX_LEN_REQUEST, 0, clientAddr, clientAddrLen, NULL);
	if(numByteRcv < 0)
		terminate_with_system_msg("rdt_recv_data() fails");
	if(numByteRcv == 0)
		return NULL;

	totalRcvByte += numByteRcv;
	while(is_end_of_request(requestBuffer, (int) totalRcvByte) == 0)
	{
		numByteRcv = rdt_recv_data(serverSocket, requestBuffer + totalRcvByte, MAX_LEN_REQUEST - totalRcvByte, 0, (struct sockaddr *) &dummy, &dummyAddrLen, clientAddr);
		
		if(numByteRcv < 0)
			terminate_with_system_msg("rdt_recv_data() fails");
		if(numByteRcv == 0)
			return NULL;
		totalRcvByte += numByteRcv;
	}

	requestBuffer[totalRcvByte] = '\0';
	return copy_str_dynamic(requestBuffer);
}

/* \r\n\r\n indicates the end of the request */
int is_end_of_request(char bufferRequest[], int len)
{
	const char END_REQUEST_TOKEN[] = "\r\n\r\n";
	const int LEN_END_REQUEST_TOKEN = strlen(END_REQUEST_TOKEN);
	char endRequest[LEN_END_REQUEST_TOKEN + 1];
	int index, indexBuffer;

	if(len < LEN_END_REQUEST_TOKEN)
		return 0;

	for(index = LEN_END_REQUEST_TOKEN - 1, indexBuffer = 0; index >= 0; index--, indexBuffer++)
		endRequest[index] = bufferRequest[len - 1 - indexBuffer];
	endRequest[LEN_END_REQUEST_TOKEN] = '\0';
	
	if(strcmp(endRequest, END_REQUEST_TOKEN) == 0)
		return 1;
	return 0;
}

int parse_request(char *request, char *file_name, size_t *maxPacketSize)
{
	const char delim[] = "\r\n";
	char *lineRequest;

	lineRequest = strtok(request, delim);
	if(lineRequest == NULL)
		return -1;
	/* Extract remote file name */
	strcpy(file_name, lineRequest);
	lineRequest = strtok(NULL, delim);
	if(lineRequest == NULL)
		return -1;
	*maxPacketSize = atol(lineRequest);
	if(*maxPacketSize <= 0)
		return -1;
	
	return 0;
}


void send_file_to_client(int sockfd, struct sockaddr *dest_addr, socklen_t dest_addrlen, int errorCode, FILE *fin)
{
	char response[MAX_LEN_RESPONSE + 1];
	ssize_t	fileSize, numByteSent;
	size_t strLength, numWillRead, numRead;

	if(fin != NULL)
	{
		fileSize = get_file_size(fin);
		sprintf(response, "%d\r\n%ld\r\n", errorCode, fileSize);

		strLength = strlen(response);
		if(MAX_LEN_RESPONSE - strLength < fileSize)
			numWillRead = MAX_LEN_RESPONSE - strLength;
		else
			numWillRead = fileSize;

		/* Send the first packet that has a response header */
		numRead = fread(response + strLength, 1, numWillRead, fin);
		if(numRead != numWillRead && ferror(fin))
			terminate_with_system_msg("fread() fails");
		
		numByteSent = rdt_sendto(sockfd, response, strLength + numRead, 0, dest_addr, dest_addrlen);
		if(numByteSent < 0) 
			terminate_with_system_msg("rdt_sendto() fails");
		if(numByteSent == 0)
			return;
		if(numByteSent != strLength + numRead)
			terminate_with_user_msg("rdt_sendto() error", "sent an unexpected number of bytes");

		if(numWillRead == fileSize)
			return;		/* Finish */

		/* Send the rest of the file */
		while(1)
		{
			numRead = fread(response, 1, MAX_LEN_RESPONSE, fin);
			if(numRead != numWillRead && ferror(fin))
				terminate_with_system_msg("fread() fails");
			//response[numRead] = '\0';
			//strLength = strlen(response);
			numByteSent = rdt_sendto(sockfd, response, numRead, 0, dest_addr, dest_addrlen);
			if(numByteSent < 0) 
				terminate_with_system_msg("rdt_sendto() fails");
			if(numByteSent == 0)
				return;
			if(numByteSent != numRead)
				terminate_with_user_msg("rdt_sendto() error", "sent an unexpected number of bytes");
			if(numRead != MAX_LEN_RESPONSE)
			{
				/* We reach end-of-file. Finish! */
				break;
			}

		}		 		
	}
	else
	{
		sprintf(response, "%d\r\n", errorCode);
		strLength = strlen(response);
		numByteSent = rdt_sendto(sockfd, response, strLength, 0, dest_addr, dest_addrlen);
		if(numByteSent < 0) 
			terminate_with_system_msg("rdt_sendto() fails");
		if(numByteSent != strLength)
			terminate_with_user_msg("rdt_sendto() error", "sent an unexpected number of bytes");
	}		

}
