#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include "helperlib.h"
#include "socket_util.h"

struct addrinfo * resolve_name_service_udp(const char *name, const char *port)
{
	struct addrinfo hint;
	struct addrinfo *servAddr;
	int rtnVal;

	/* Intialize hint */
	memset(&hint, 0, sizeof(struct addrinfo));
	hint.ai_family = AF_UNSPEC;
	if(name == NULL)
		/* Accept on any address / port */
		hint.ai_flags = AI_PASSIVE;
	hint.ai_socktype = SOCK_DGRAM;	/* Only datagram sockets */
	hint.ai_protocol = IPPROTO_UDP;
	
	rtnVal = getaddrinfo(name, port, &hint, &servAddr);
	if(rtnVal != 0)
		terminate_with_user_msg("getaddrinfo() fails", gai_strerror(rtnVal));

	return servAddr;
}

int set_up_udp_client_socket(int ai_family)
{
	return socket(ai_family, SOCK_DGRAM, IPPROTO_UDP);
}

int set_up_udp_server_socket(const char *port)
{
	struct addrinfo *serverAddr, *addr;
	int sock;

	serverAddr = resolve_name_service_udp(NULL, port);

	for(addr = serverAddr; addr != NULL; addr = addr->ai_next)
	{
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if(sock < 0)
			continue;

		/* Binding */
		if(bind(sock, addr->ai_addr, addr->ai_addrlen) < 0)
		{
			close(sock);
			continue;
		}

		return sock;
	}

	free(serverAddr);
	return -1;	
}

int get_printable_ip_addr(const struct sockaddr *addr, char **addrStr)
{
	const int MAX_LEN_OUTPUT = 1000;
	void *numericAddr;
	in_port_t port;
	char addrBuffer[INET6_ADDRSTRLEN];
	char outputBuffer[MAX_LEN_OUTPUT + 1];

	if(addr == NULL)
	{
		printf("addr is NULL");
		*addrStr = NULL;
		return -1;
	}

	switch(addr->sa_family)
	{
		case AF_INET:
			numericAddr = &((struct sockaddr_in *) addr)->sin_addr;
			port = ntohs(((struct sockaddr_in *) addr)->sin_port);
			break;
		case AF_INET6:
			numericAddr = &((struct sockaddr_in6 *) addr)->sin6_addr;
			port = ntohs(((struct sockaddr_in6 *) addr)->sin6_port);
			break;
		default:
			printf("Unknown family");
			*addrStr = NULL;
			return -1;
	}

	if(inet_ntop(addr->sa_family, numericAddr, addrBuffer, INET6_ADDRSTRLEN) == NULL)
	{
		printf("inet_ntop fails");
		*addrStr = NULL;
		return -1;
	}

	sprintf(outputBuffer, "%s-%u", addrBuffer, port);
	/* Allocate 1 more space for end-of-string character */
	*addrStr = copy_str_dynamic(outputBuffer);
			
	return 0;	
}

int is_same_ip_and_port(const struct sockaddr *addrOne, const struct sockaddr *addrTwo)
{
	int index;
	if(addrOne == NULL || addrTwo == NULL)
		return 0;

	if(addrOne->sa_family != addrTwo->sa_family)
		return 0;

	if(addrOne->sa_family == AF_INET)
	{
		/* IPv4 */
		if(((struct sockaddr_in *) addrOne)->sin_port != ((struct sockaddr_in *) addrTwo)->sin_port)
			return 0; 
		if((((struct sockaddr_in *) addrOne)->sin_addr).s_addr != (((struct sockaddr_in *) addrTwo)->sin_addr).s_addr)
			return 0;
		return 1;
	}
	
	if(addrOne->sa_family == AF_INET6)
	{
		if(((struct sockaddr_in6 *) addrOne)->sin6_port != ((struct sockaddr_in6 *) addrTwo)->sin6_port)
			return 0;

		for(index = 0; index < 16; index++)
			if((((struct sockaddr_in6 *) addrOne)->sin6_addr).s6_addr[index] != (((struct sockaddr_in6 *) addrTwo)->sin6_addr).s6_addr[index])
				return 0;

		return 1;
	}

	/* Unhandled IP family */
	return 0;
}
