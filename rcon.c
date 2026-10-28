#include "rcon.h"

const uint32_t SERVERDATA_AUTH = 3;
const uint32_t SERVERDATA_AUTH_RESPONSE = 2;
const uint32_t SERVERDATA_EXECCOMMAND = 2;
const uint32_t SERVERDATA_RESPONSE_VALUE = 0;

static struct rcon_packet *rcon_create_packet(rcon_connection *conn, const char *body, uint32_t message_type);
static void rcon_free_packet(struct rcon_packet *packet);
static struct rcon_packet *rcon_recv_packet(rcon_connection *conn);
static int rcon_send_packet(rcon_connection *conn, const struct rcon_packet *packet);

struct rcon_packet {
	uint32_t size;
	uint32_t id;
	uint32_t type;
	char *body;
	char empty;
};

int rcon_init(rcon_connection *conn, const char *address, uint16_t port)
{
	struct timeval timeout;
	int rval;

	if (conn == NULL || address == NULL)
		return EINVAL;

	memset(conn, 0, sizeof(*conn));
	conn->packet_id = 0;
	conn->is_connected = false;
	conn->sockfd = -1;

	conn->sockaddr.sin_family = AF_INET;
	conn->sockaddr.sin_addr.s_addr = inet_addr(address);
	if (conn->sockaddr.sin_addr.s_addr == INADDR_NONE)
		return EINVAL;
	conn->sockaddr.sin_port = htons(port);

	conn->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (conn->sockfd == -1)
		return errno;

	timeout.tv_sec = RCON_RECV_TIMEOUT_SEC;
	timeout.tv_usec = 0;
	rval = setsockopt(conn->sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	if (rval == -1) {
		close(conn->sockfd);
		conn->sockfd = -1;
		return errno;
	}

	return 0;
}

int rcon_auth(rcon_connection *conn, const char *password)
{
	struct rcon_packet *packet;
	uint32_t id, type;
	int rval;

	if (conn == NULL || password == NULL)
		return EINVAL;
	if (conn->is_connected)
		return EISCONN;

	rval = connect(conn->sockfd, (const struct sockaddr *)&conn->sockaddr, sizeof(conn->sockaddr));
	if (rval == -1)
		return errno;
	conn->is_connected = true;

	packet = rcon_create_packet(conn, password, SERVERDATA_AUTH);
	if (packet == NULL)
		return errno;

	rval = rcon_send_packet(conn, packet);
	rcon_free_packet(packet);
	if (rval)
		return rval;

	for (;;) {
		packet = rcon_recv_packet(conn);
		if (packet == NULL)
			return errno ? errno : EIO;

		id = packet->id;
		type = packet->type;
		rcon_free_packet(packet);

		if (type == SERVERDATA_AUTH_RESPONSE) {
			return (id == (uint32_t)-1) ? EACCES : 0;
		}
	}
}

int rcon_disconnect(rcon_connection *conn)
{
	int rval;

	if (conn == NULL)
		return EINVAL;
	if (!conn->is_connected)
		return ENOTCONN;

	rval = close(conn->sockfd);
	conn->sockfd = -1;
	conn->is_connected = false;
	return (rval == -1) ? errno : 0;
}

int rcon_send(rcon_connection *conn, const char *message, uint32_t message_type)
{
	struct rcon_packet *packet;
	int rval;

	if (conn == NULL || message == NULL)
		return EINVAL;

	packet = rcon_create_packet(conn, message, message_type);
	if (packet == NULL)
		return errno;

	rval = rcon_send_packet(conn, packet);
	rcon_free_packet(packet);
	return rval;
}

char *rcon_recv(rcon_connection *conn)
{
	struct rcon_packet *packet;
	char *message = NULL;
	size_t m_size = 1;
	size_t b_size;

	if (conn == NULL || !conn->is_connected) {
		errno = ENOTCONN;
		return NULL;
	}

	message = calloc(1, 1);
	if (message == NULL)
		return NULL;

	for (;;) {
		packet = rcon_recv_packet(conn);
		if (packet == NULL) {
			/* Timeout or end of multi-packet response is normal */
			return message;
		}

		b_size = (packet->size > RCON_PACKET_OVERHEAD) ? packet->size - RCON_PACKET_OVERHEAD : 0;
		if (b_size > 0 && packet->body) {
			char *new_msg = realloc(message, m_size + b_size);
			if (new_msg == NULL) {
				rcon_free_packet(packet);
				free(message);
				return NULL;
			}
			message = new_msg;
			strncat(message, packet->body, b_size);
			m_size += b_size;
		}
		rcon_free_packet(packet);
	}
}

static struct rcon_packet *rcon_create_packet(rcon_connection *conn, const char *body, uint32_t message_type)
{
	struct rcon_packet *packet;
	size_t body_len;

	if (conn == NULL || body == NULL)
		return NULL;

	body_len = strlen(body);
	if (body_len > RCON_MAX_BODY_LEN) {
		errno = EMSGSIZE;
		return NULL;
	}

	packet = calloc(1, sizeof(*packet));
	if (packet == NULL)
		return NULL;

	packet->body = malloc(body_len + 1);
	if (packet->body == NULL) {
		free(packet);
		return NULL;
	}

	packet->size = (uint32_t)(body_len + RCON_PACKET_OVERHEAD);
	packet->id = ++conn->packet_id;
	packet->type = message_type;
	memcpy(packet->body, body, body_len + 1);
	packet->empty = '\0';

	return packet;
}

static void rcon_free_packet(struct rcon_packet *packet)
{
	if (packet) {
		free(packet->body);
		free(packet);
	}
}

static struct rcon_packet *rcon_recv_packet(rcon_connection *conn)
{
	struct rcon_packet *packet;
	ssize_t bytes_recd, rval;
	size_t body_len;

	if (conn == NULL || !conn->is_connected) {
		errno = ENOTCONN;
		return NULL;
	}

	packet = calloc(1, sizeof(*packet));
	if (packet == NULL)
		return NULL;

	/* Read size */
	bytes_recd = 0;
	while (bytes_recd < (ssize_t)sizeof(packet->size)) {
		rval = recv(conn->sockfd, ((char *)&packet->size) + bytes_recd,
			    sizeof(packet->size) - bytes_recd, 0);
		if (rval <= 0) {
			free(packet);
			return NULL;
		}
		bytes_recd += rval;
	}

	if (packet->size < RCON_MIN_PACKET_SIZE || packet->size > RCON_MAX_BODY_LEN + RCON_PACKET_OVERHEAD) {
		free(packet);
		errno = EMSGSIZE;
		return NULL;
	}

	/* Read id */
	bytes_recd = 0;
	while (bytes_recd < (ssize_t)sizeof(packet->id)) {
		rval = recv(conn->sockfd, ((char *)&packet->id) + bytes_recd,
			    sizeof(packet->id) - bytes_recd, 0);
		if (rval <= 0) {
			free(packet);
			return NULL;
		}
		bytes_recd += rval;
	}

	/* Read type */
	bytes_recd = 0;
	while (bytes_recd < (ssize_t)sizeof(packet->type)) {
		rval = recv(conn->sockfd, ((char *)&packet->type) + bytes_recd,
			    sizeof(packet->type) - bytes_recd, 0);
		if (rval <= 0) {
			free(packet);
			return NULL;
		}
		bytes_recd += rval;
	}

	/* After size, id, and type have been read, remaining bytes = size - 8.
	 * Those are body (including its null) + the final empty null.
	 * body_len (including null) = size - 9. */
	body_len = packet->size - 9;
	if (body_len == 0 || body_len > RCON_MAX_BODY_LEN + 1) {
		free(packet);
		errno = EMSGSIZE;
		return NULL;
	}

	packet->body = calloc(body_len + 1, 1);
	if (packet->body == NULL) {
		free(packet);
		return NULL;
	}

	bytes_recd = 0;
	while (bytes_recd < (ssize_t)body_len) {
		rval = recv(conn->sockfd, packet->body + bytes_recd, body_len - bytes_recd, 0);
		if (rval <= 0) {
			rcon_free_packet(packet);
			return NULL;
		}
		bytes_recd += rval;
	}

	/* Read the final empty terminator */
	bytes_recd = 0;
	while (bytes_recd < (ssize_t)sizeof(packet->empty)) {
		rval = recv(conn->sockfd, &packet->empty + bytes_recd,
			    sizeof(packet->empty) - bytes_recd, 0);
		if (rval <= 0) {
			rcon_free_packet(packet);
			return NULL;
		}
		bytes_recd += rval;
	}

	return packet;
}

static int rcon_send_packet(rcon_connection *conn, const struct rcon_packet *packet)
{
	ssize_t bytes_sent, rval;
	size_t body_len;

	if (conn == NULL || !conn->is_connected || packet == NULL)
		return (conn && !conn->is_connected) ? ENOTCONN : EINVAL;

	/* Send size + id + type (12 bytes) */
	bytes_sent = 0;
	while (bytes_sent < (ssize_t)(sizeof(uint32_t) * 3)) {
		rval = send(conn->sockfd, ((const char *)packet) + bytes_sent,
			    sizeof(uint32_t) * 3 - bytes_sent, 0);
		if (rval == -1)
			return errno;
		bytes_sent += rval;
	}

	/* Send body (including null) */
	body_len = strlen(packet->body) + 1;
	bytes_sent = 0;
	while (bytes_sent < (ssize_t)body_len) {
		rval = send(conn->sockfd, packet->body + bytes_sent, body_len - bytes_sent, 0);
		if (rval == -1)
			return errno;
		bytes_sent += rval;
	}

	/* Send empty terminator */
	bytes_sent = 0;
	while (bytes_sent < (ssize_t)sizeof(packet->empty)) {
		rval = send(conn->sockfd, &packet->empty + bytes_sent,
			    sizeof(packet->empty) - bytes_sent, 0);
		if (rval == -1)
			return errno;
		bytes_sent += rval;
	}

	return 0;
}
