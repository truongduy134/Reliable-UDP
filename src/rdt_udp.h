#ifndef _RDT_UDP_H_
#define _RDT_UDP_H_

/* This contains API for a reliable-data-transfer implementation
		of UDP */
/* rdt_sendto(...)
 * 
 *	Reliable-data-transfer version of sendto() 
 */
ssize_t rdt_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

/* rdt_recvfrom(...)
 *
 *	Reliable-data-transfer version of recvfrom()
 *
 * Note: expect_src_addr indicates the expected source to receive the packet from. If expect_src_addr == NULL, receive and handle packet
 *		from any addresses. If expect_src_addr != NULL, those packets from different addresses from the specified address
 *		will be ignored!
 */
ssize_t rdt_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, const struct sockaddr *expect_src_addr);

/* rdt_recv_data(...)
 *
 *	Extract data from the packet received by calling rdt_recvfrom(...) (Reliable-data-transfer version of recvfrom())
 *
 * Note: expect_src_addr indicates the expected source to receive the packet from. If expect_src_addr == NULL, receive and handle packet
 *		from any addresses. If expect_src_addr != NULL, those packets from different addresses from the specified address
 *		will be ignored!
 */
ssize_t rdt_recv_data(int sockfd, void *data, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, const struct sockaddr *expect_src_addr);

/* set_max_packet_size(...)
 *	
 *	Set the maximum packet / segment size
 */
void set_max_packet_size(size_t newSize);

void initialize_rdt_recv();
void initialize_rdt_send();

#endif /* ifndef _RDT_UDP_H_ */
