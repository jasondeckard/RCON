#include "rcon.h"

const uint32_t SERVERDATA_AUTH = 3;
const uint32_t SERVERDATA_AUTH_RESPONSE = 2;
const uint32_t SERVERDATA_EXECCOMMAND = 2;
const uint32_t SERVERDATA_RESPONSE_VALUE = 0;

/* 
 * Create a socket and connect to the target server identified with
 * rcon_init(), and send the specified password.
 *
 * Return zero on success and errno on failure.
 */
int rcon_auth(const char *password)
{
	uint32_t id;
	struct rcon_packet *packet = NULL;
	int rval;
	uint32_t type;

	if (rcon_is_connected == true)
		return EISCONN;

	rval = connect(rcon_sockfd,
			(const struct sockaddr *) &rcon_sockaddr,
		       	sizeof(rcon_sockaddr));
	if (rval == -1)
		return errno;
	else
		rcon_is_connected = true;

	packet = rcon_create_packet(password, SERVERDATA_AUTH);
	if (packet == NULL)
		return errno;

	rval = rcon_send_packet((const struct rcon_packet *) packet);
	rcon_free_packet(packet);
	if (rval)
		return rval;

	for(;;) {
		packet = rcon_recv_packet();
		id = packet->id;
		type = packet->type;
		rcon_free_packet(packet);

		if (type == SERVERDATA_AUTH_RESPONSE) {
			if (id == -1)
				rval = EACCES;
			else
				rval = 0;

			break;
		}
	}

	return rval;
}


/*
 * Form a valid RCON protocol message from the provided body and type.  Returns
 * a pointer to the created message on success, which must be freed with
 * rcon_free_packet().  On failure, NULL is returned and errno is set.
 */
struct rcon_packet * rcon_create_packet(const char *body, uint32_t message_type)
{
	struct rcon_packet *packet = (struct rcon_packet *) calloc(1, sizeof(struct rcon_packet));
	if (packet == NULL)
		return NULL;

	packet->body = (char *) calloc(strlen(body) + 1, sizeof(char));
	if (packet->body == NULL) {
		free(packet);
		return NULL;
	}

	packet->size = strlen(body) + 10;	/* sizeof id and type, plus two nulls. */
	packet->id = ++rcon_packet_id;
	packet->type = message_type;
	strncpy(packet->body, body, strlen(body));
	packet->empty = '\0';

	return packet;
}


/*
 * Disconnect from the server.  Return zero on success and errno on failure.
 */
int rcon_disconnect(void)
{
	int rval;

	if (rcon_is_connected == false)
		return ENOTCONN;

	rval = close(rcon_sockfd);
	if (rval == -1)
		return errno;

	rcon_is_connected = false;
	return 0;
}


/*
 * Identify the target server using the provided address and port.  This must
 * be called prior to calling rcon_auth().
 *
 * Return zero on success and errno on failure.
 */
int rcon_init(const char *address, uint16_t port)
{
	int rval;
	struct timeval timeout;

	if (address == NULL)
		return EDESTADDRREQ;

	rcon_packet_id = 0;
	rcon_is_connected = false;

	bzero(&rcon_sockaddr, sizeof(rcon_sockaddr));

	rcon_sockaddr.sin_family = AF_INET;
	rcon_sockaddr.sin_addr.s_addr = inet_addr(address);
	rcon_sockaddr.sin_port = htons(port);

	rcon_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (rcon_sockfd == -1)
		return errno;

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	rval = setsockopt(rcon_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
	if (rval == -1)
		return errno;

	return 0;
}


/* 
 * Free memory allocated to the specified rcon_packet.
 */
void rcon_free_packet(struct rcon_packet *packet)
{
	if (packet) {
		if (packet->body)
			free(packet->body);

		free(packet);
	}
}


/*
 * Read one or more RCON protocol messages from the socket and return them as
 * a single string.  On failure, NULL is returned and errno is set.
 *
 * rcon_recv will allocate a buffer for storing the returned response, and
 * this buffer should be freed by the caller.
 */
char * rcon_recv(void)
{
	size_t b_size = 0;
	size_t m_size = 1;
	char *message = NULL;
	struct rcon_packet *packet = NULL;

	for(;;) {
		packet = rcon_recv_packet();

		if (packet == NULL)
			return message;
		
		b_size = packet->size - 10;
		message = (char *) realloc((void *) message, m_size + b_size);

		if (m_size == 1)
			message[0] = '\0';

		strncat(message, (const char *) packet->body, b_size);
		rcon_free_packet(packet);
		m_size += b_size;
	}
}


/* Pull a single RCON protocol message from the socket.  A pointer to the
 * packet is returned on success, and the packet must later be freed with
 * rcon_free_packet().  On failure, NULL is returned and errno is set.
 *
 * If no response is received from the server after two seconds, NULL is
 * returned and errno is set to ETIME.
 */
struct rcon_packet * rcon_recv_packet(void)
{
	size_t body_len;
	ssize_t bytes_recd = 0;
	struct rcon_packet *packet = NULL;
	ssize_t rval;

	if (rcon_is_connected == false) {
		errno = ENOTCONN;
		return NULL;
	}

	packet = (struct rcon_packet *) calloc(1, sizeof(struct rcon_packet));
	if (packet == NULL)
		return NULL;

	while (bytes_recd < sizeof(packet->size)) {
		rval = recv(rcon_sockfd, &(packet->size) + bytes_recd, sizeof(packet->size) - bytes_recd, 0);
		if (rval == -1) {
			free(packet);
			return NULL;
		}

		bytes_recd += rval;
	}

	bytes_recd = 0;
	while (bytes_recd < sizeof(packet->id)) {
		rval = recv(rcon_sockfd, &(packet->id) + bytes_recd, sizeof(packet->id) - bytes_recd, 0);
		if (rval == -1) {
			free(packet);
			return NULL;
		}

		bytes_recd += rval;
	}

	bytes_recd = 0;
	while (bytes_recd < sizeof(packet->type)) {
		rval = recv(rcon_sockfd, &(packet->type) + bytes_recd, sizeof(packet->type) - bytes_recd, 0);
		if (rval == -1) {
			free(packet);
			return NULL;
		}

		bytes_recd += rval;
	}

	body_len = packet->size - 9;
	packet->body = (char *) calloc(body_len, sizeof(char));
	if (packet->body == NULL) {
		free(packet);
		return NULL;
	}

	bytes_recd = 0;
	while (bytes_recd < body_len) {
		rval = recv(rcon_sockfd, packet->body + bytes_recd, body_len - bytes_recd, 0);
		if (rval == -1) {
			rcon_free_packet(packet);
			return NULL;
		}

		bytes_recd += rval;
	}

	bytes_recd = 0;
	while (bytes_recd < sizeof(packet->empty)) {
		rval = recv(rcon_sockfd, &(packet->empty) + bytes_recd, sizeof(packet->empty) - bytes_recd, 0);
		if (rval == -1) {
			rcon_free_packet(packet);
			return NULL;
		}

		bytes_recd += rval;
	}

	return packet;
}


/*
 * Sends the provided message to the server as a Source RCON protocol message
 * of the indicated type.  Zero is returned on success, and errno is returned
 * on failure.
 */
int rcon_send(const char *message, uint32_t message_type)
{
	int rval = 0;
	struct rcon_packet *packet = rcon_create_packet(message, message_type);

	if (packet) {
		rval = rcon_send_packet(packet);
		rcon_free_packet(packet);
	} else {
		rval = errno;
	}

	return rval;
}


/*
 * Send the provided packet to the server.  Zero is returned on error, and
 * errno is returned on failure.
 */
int rcon_send_packet(const struct rcon_packet *packet)
{
	size_t body_len;
	ssize_t bytes_sent;
	ssize_t rval;

	if (rcon_is_connected == false)
		return ENOTCONN;

	if (packet == NULL)
		return EINVAL;

	bytes_sent = 0;
	while (bytes_sent < sizeof(uint32_t) * 3) {
		rval = send(rcon_sockfd,
			(const void *) packet + bytes_sent,
			sizeof(uint32_t) * 3 - bytes_sent,
			0);

		if (rval == -1)
			return errno;

		bytes_sent += rval;
	}

	bytes_sent = 0;
	body_len = strlen(packet->body) + 1;
	while (bytes_sent < body_len) {
		rval = send(rcon_sockfd,
			(const void *) packet->body + bytes_sent,
			body_len - bytes_sent,
			0);

		if (rval == -1)
			return errno;

		bytes_sent += rval;
	}

	bytes_sent = 0;
	while (bytes_sent < sizeof(packet->empty)) {
		rval = send(rcon_sockfd,
			(const void *) &(packet->empty) + bytes_sent,
			sizeof(packet->empty) - bytes_sent,
			0);

		if (rval == -1)
			return errno;

		bytes_sent += rval;
	}

	return 0;
}
