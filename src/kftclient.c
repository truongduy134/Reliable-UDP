#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include "helperlib.h"
#include "socket_util.h"
#include "dropper.h"
#include "rdt_udp.h"

#define DEBUG_FLG "-d"
#define DEBUG_FLG_CAPITAL "-D"
#define BAD_REQUEST 400
#define OK 200
#define NOT_FOUND 404
#define MILLION 1000000

int debug;
int isClient = 1;

void catch_alarm(int sig_num);
void generate_request(char *request, char *remote_file_name, int maxPacketSize);
int recv_file_content(int serverSocket, const struct sockaddr *expect_src, const char *localFileName, const size_t maxPacketSize);

int main(int argc, char *argv[])
{
	if(argc < 7)
		terminate_with_user_msg("Command-line arguments", "<server_IP_address> <server_port_number> <remote_filename> <local_filename> <max_packet_size> <loss_percent> [-d]");

	struct addrinfo *serverAddr, *addr;
	FILE * localFilePtr;
	int serverSocket;
	ssize_t numByteSent;
	int sendSuccessFlg, statusCode;
	struct timeval startTime, endTime;

	/* Set maximum packet size */
	const size_t maxPacketSize = atoi(argv[5]);
	if(maxPacketSize > 0)
		set_max_packet_size(maxPacketSize);
	else
		terminate_with_user_msg("Command-line arguments", "Max packet size must be positive");

	char request[maxPacketSize];
	double elapseTime;

	/* Check if there is a debug flag */
	if(argc == 7)
		debug = 0;
	else
	{
		if(strcmp(argv[7], DEBUG_FLG) == 0 || strcmp(argv[7], DEBUG_FLG_CAPITAL) == 0)
			debug = 1;
		else
			debug = 0;
		
	}
	
	/* Open local file for writing */
	localFilePtr = fopen(argv[4], "w");
	if(localFilePtr == NULL)
		terminate_with_system_msg("fopen() fails");

	/* Set dropper */
	if(set_dropper(atoi(argv[6])) < 0)
		terminate_with_user_msg("set_dropper() fails", argv[6]);
	
	/* Set up client socket */
	serverAddr = resolve_name_service_udp(argv[1], argv[2]);

	/* Set up handler for SIGALRM */
	set_signal_handler(SIGALRM, catch_alarm);

	sendSuccessFlg = 0;
	for(addr = serverAddr; addr != NULL; addr = addr->ai_next)
	{
		serverSocket = set_up_udp_client_socket(addr->ai_family);

		generate_request(request, argv[3], maxPacketSize);

		/* Get the start time */
		if(gettimeofday(&startTime, NULL) < 0)
			terminate_with_system_msg("gettimeofday() fails");

		numByteSent = rdt_sendto(serverSocket, request, strlen(request), 0, addr->ai_addr, addr->ai_addrlen);
		if(numByteSent < 0 || numByteSent != strlen(request))
			continue;

		/* Otherwise, sent successfully */
		sendSuccessFlg = 1;

		/* Receive file from the server */
		statusCode = recv_file_content(serverSocket, addr->ai_addr, argv[4], maxPacketSize);

		/* Get end time */
		if(gettimeofday(&endTime, NULL) < 0)
			terminate_with_system_msg("gettimeofday() fails");

		switch(statusCode)
		{
			case BAD_REQUEST:
				printf("Request sent to the server is not in the correct format. It cannot be processed!\n");
				break;
			case NOT_FOUND:
				printf("Server cannot found the file with name = %s\n", argv[3]);
				break;
			case OK:
				printf("File has successfully written\n");
				break;
			default:
				printf("Server error. File cannot be written\n");
		}
		
		elapseTime = (endTime.tv_sec - startTime.tv_sec) + (endTime.tv_usec - startTime.tv_usec) * 1.0 / MILLION;
		printf("Elapse time = %.4lf\n", elapseTime); 
		break;
	}

	if(!sendSuccessFlg)
		printf("Cannot communicate with the server\n");
	free(serverAddr);
	return 0;
}

void catch_alarm(int sig_num)
{
	if(sig_num == SIGALRM && debug)
		printf("Catch SIGALRM signal\n");
}

void generate_request(char *request, char *remote_file_name, int maxPacketSize)
{
	sprintf(request, "%s\r\n%d\r\n\r\n", remote_file_name, maxPacketSize);
}

int recv_file_content(int serverSocket, const struct sockaddr *expect_src, const char *localFileName, const size_t maxPacketSize)
{
	const char delim[] = "\r\n";
	ssize_t numByteRecv, fileLen;
	struct sockaddr_storage dummy;
	socklen_t dummyAddrLen = sizeof(struct sockaddr_storage);
	char response[maxPacketSize + 1], cpy_response[maxPacketSize + 1];
	char *lineResponse;
	int statusCode;
	FILE *fout;
	size_t numByteWrite, indexData = 0;
	
	numByteRecv = rdt_recv_data(serverSocket, response, maxPacketSize, 0, (struct sockaddr *) &dummy, &dummyAddrLen, expect_src);
	if(numByteRecv < 0)
		terminate_with_system_msg("rdt_recv_data() fails");
	memcpy(cpy_response, response, numByteRecv);
	cpy_response[numByteRecv] = '\0';
	/* Parse the first response packet to get information */
	lineResponse = strtok(cpy_response, delim);
	if(lineResponse == NULL)
		return -1;
	statusCode = atoi(lineResponse);
	indexData += strlen(lineResponse) + strlen(delim);
	if(statusCode != OK)
		return statusCode;

	/* Get file length */
	lineResponse = strtok(NULL, delim);
	if(lineResponse == NULL)
		return -1;
	fileLen = atol(lineResponse);
	indexData += strlen(lineResponse) + strlen(delim);

	/* Open a file */
	fout = fopen(localFileName, "w");
	if(fout == NULL)
		terminate_with_system_msg("fopen() fails");

	numByteWrite = fwrite(response + indexData, 1, numByteRecv - indexData, fout);
	if(numByteWrite != numByteRecv - indexData)
		terminate_with_user_msg("fwrite() error", "wrote an unexpected number of bytes");
	fileLen -= numByteRecv - indexData;

	if(debug)
		printf("Now file len = %d\n", (int) fileLen);
	while(fileLen > 0)
	{
		if(debug)
			printf("Inside Loop\n");
		dummyAddrLen = sizeof(struct sockaddr_storage);
		numByteRecv = rdt_recv_data(serverSocket, response, maxPacketSize, 0, (struct sockaddr *) &dummy, &dummyAddrLen, expect_src);
		if(numByteRecv < 0)
			terminate_with_system_msg("rdt_recv_data() fails");

		//response[numByteRecv] = '\0';
		//strLength = strlen(response);
		numByteWrite = fwrite(response, 1, numByteRecv, fout);
		if(numByteWrite != numByteRecv)
			terminate_with_user_msg("fwrite() error", "wrote an unexpected number of bytes");
		fileLen -= numByteRecv;	
		if(debug)
			printf("Now file len = %d\n", (int) fileLen);
	}
	fclose(fout);
	return statusCode;		
}
