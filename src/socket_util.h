#ifndef _SOCKET_UTIL_H_
#define _SOCKET_UTIL_H_

/* resolve_name_service_udp(...)
 *	+ Based on the given server name (or IP address) and the port number (
 *		or service), return all the IP addresses (for UDP socket)
 *		associated with the given information.
 *
 * Return value:
 *	+ The pointer to the first node of a linked list of struct addrinfo
 *		elements.
 */
struct addrinfo * resolve_name_service_udp(const char *name, const char *port);

/* set_up_udp_client_socket(...)
 *	+ Create a UDP socket with the given ai_family (AF_INET or AF_INET6)
 *
 * Return value:
 *	+ A non-negative integer socket id.
 *	+ -1 upon error
 */
int set_up_udp_client_socket(int ai_family);

/* set_up_udp_client_socket(...)
 *	+ Create a UDP socket. Bind it to an IP address (selected from
 *		the result returned from resolve_name_service_udp()) and
 *		the given port.
 *
 * Return value:
 *	+ A non-negative integer socket id.
 *	+ -1 upon error.
 */ 
int set_up_udp_server_socket(const char *port);

/* get_printable_ip_addr(...)
 *	+ Get a printable version of the IP address.
 *
 * Return value:
 *	+ 0 upon success.
 *	+ -1 upon error. In this case, *addrStr will be set to NULL
 *
 * Note: *addrStr will be a pointer that points to a dynamically allocated memory
 *		region. The user has to free it himself after using the function.
 */ 
int get_printable_ip_addr(const struct sockaddr *addr, char **addrStr);

/* is_same_ip_and_port(...)
 *	+ Decide if the two struct sockaddr represent the same source (i.e. they
 *		have the same IP address and the same port)
 *
 * Return value:
 *	+ 0 if not.
 *	+ 1 otherwise.
 */
int is_same_ip_and_port(const struct sockaddr *addrOne, const struct sockaddr *addrTwo);
#endif /* ifndef _SOCKET_UTIL_H_ */
