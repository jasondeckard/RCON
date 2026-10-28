/*
 * Implementation of Source's RCON protocol, defined in
 * https://developer.valvesoftware.com/wiki/Source_RCON_Protocol
 *
 * Usage:
 *   rcon_connection conn = {0};
 *   rcon_init(&conn, address, port);
 *   rcon_auth(&conn, password);
 *   rcon_send(&conn, command, SERVERDATA_EXECCOMMAND);
 *   char *resp = rcon_recv(&conn);
 *   // ... free(resp);
 *   rcon_disconnect(&conn);
 */

#ifndef RCON_H
#define RCON_H

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/* Packet framing constants (Source RCON) */
#define RCON_PACKET_OVERHEAD     10  /* id(4) + type(4) + body null + empty null */
#define RCON_MIN_PACKET_SIZE     10
#define RCON_RECV_TIMEOUT_SEC    1
#define RCON_MAX_BODY_LEN        4096

/* Valid Source RCON protocol message types */
extern const uint32_t SERVERDATA_AUTH;
extern const uint32_t SERVERDATA_AUTH_RESPONSE;
extern const uint32_t SERVERDATA_EXECCOMMAND;
extern const uint32_t SERVERDATA_RESPONSE_VALUE;

typedef struct rcon_connection {
	int sockfd;
	bool is_connected;
	struct sockaddr_in sockaddr;
	uint32_t packet_id;
} rcon_connection;

/* Initialize connection state and socket (does not connect yet). */
int rcon_init(rcon_connection *conn, const char *address, uint16_t port);

/* Connect and authenticate. */
int rcon_auth(rcon_connection *conn, const char *password);

/* Disconnect and clean up. */
int rcon_disconnect(rcon_connection *conn);

/* Send a command (or other message type). */
int rcon_send(rcon_connection *conn, const char *message, uint32_t message_type);

/* Receive and concatenate response body(ies). Caller must free the result. */
char *rcon_recv(rcon_connection *conn);

#endif /* RCON_H */

