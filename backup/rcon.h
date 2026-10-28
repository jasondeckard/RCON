/*
 * Implementation of Source's RCON protocol, defined in
 * https://developer.valvesoftware.com/wiki/Source_RCON_Protocol
 *
 * If you want to use rcon.c in your own program, you should call rcon_init()
 * and rcon_auth() first, in that order.  rcon_send() and rcon_recv() should
 * be all you need to communicate with the remote server.  Remember to free
 * the buffer returned by rcon_recv(), and use rcon_disconnect() to close the
 * connection.
 *
 * If you want to connect to a different server after closing the connection
 * to the first, you should call rcon_init() again.
 */

#ifndef __RCON_H__
#define __RCON_H__ 1

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


/* Data structure to describe the Source RCON protocol message */
struct rcon_packet {
        uint32_t size;
        uint32_t id;
        uint32_t type;
        char *body;
        char empty;
};


/* Valid Source RCON protocol message types */
extern const uint32_t SERVERDATA_AUTH;
extern const uint32_t SERVERDATA_AUTH_RESPONSE;
extern const uint32_t SERVERDATA_EXECCOMMAND;
extern const uint32_t SERVERDATA_RESPONSE_VALUE;


/* Global packet ID */
uint32_t rcon_packet_id;


/* Socket information */
bool rcon_is_connected;
int rcon_sockfd;
struct sockaddr_in rcon_sockaddr;


/* Connect to the server and authenticate the client. */
extern int rcon_auth(const char *password);


/* Form a valid RCON protocol message which must later be freed by
 * rcon_free_packet().
 */
extern struct rcon_packet * rcon_create_packet(const char *body, uint32_t message_type);


/* Disconnect from the server. */
extern int rcon_disconnect(void);


/* Set target server and initialize globals. */
extern int rcon_init(const char *address, uint16_t port);


/* Free memory allocated for an RCON message. */
extern void rcon_free_packet(struct rcon_packet *packet);


/* Read the contents of one or more RCON messages from the socket.
 * Concatenate the bodies and return a single string.  Call free()
 * on the provided string when no longer needed.
 */
extern char * rcon_recv(void);


/* Receive a single packet from the socket. */
extern struct rcon_packet * rcon_recv_packet(void);


/* Create a RCON message of the specified type, using 'message' as the packet
 * body.
 */
extern int rcon_send(const char *message, uint32_t message_type);


/* Send a single packet to the socket. */
extern int rcon_send_packet(const struct rcon_packet *packet);


#endif /* __RCON_H__ */
