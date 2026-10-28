#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static FILE *open_config(const char *config_path);
static char *find_config(FILE *file, const char *target);
static int parse_and_auth(char *config_entry, rcon_connection *conn);

int load_and_auth(const char *target, const char *config_path, rcon_connection *conn)
{
	FILE *file;
	char *line;
	int rval;

	if (target == NULL || conn == NULL)
		return EINVAL;

	file = open_config(config_path);
	if (file == NULL)
		return errno ? errno : ENOENT;

	line = find_config(file, target);
	fclose(file);
	if (line == NULL)
		return ENOENT;

	rval = parse_and_auth(line, conn);
	free(line);
	return rval;
}

char *get_password(void)
{
	struct termios old_term, new_term;
	char *password;
	int i = 0;
	int c;

	password = calloc(RCON_MAX_PASSWORD_LEN + 1, 1);
	if (password == NULL)
		return NULL;

	if (tcgetattr(STDIN_FILENO, &old_term) == -1) {
		free(password);
		return NULL;
	}
	new_term = old_term;
	new_term.c_lflag &= ~(ECHO | ICANON);
	if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) == -1) {
		free(password);
		return NULL;
	}

	printf("Enter password: ");
	fflush(stdout);

	while (i < RCON_MAX_PASSWORD_LEN) {
		c = getchar();
		if (c == '\n' || c == '\r' || c == EOF)
			break;
		if (c == 127 || c == '\b') { /* backspace */
			if (i > 0) {
				i--;
				printf("\b \b");
				fflush(stdout);
			}
			continue;
		}
		if (isprint(c)) {
			password[i++] = (char)c;
			putchar('*'); /* visual feedback */
			fflush(stdout);
		}
	}
	password[i] = '\0';
	printf("\n");

	tcsetattr(STDIN_FILENO, TCSANOW, &old_term);

	if (i == 0) {
		free(password);
		return NULL;
	}
	return password;
}

static FILE *open_config(const char *config_path)
{
	char *path = NULL;
	FILE *file;
	struct passwd *pw;

	if (config_path != NULL) {
		file = fopen(config_path, "r");
		return file;
	}

	pw = getpwuid(getuid());
	if (pw == NULL || pw->pw_dir == NULL) {
		errno = ENOENT;
		return NULL;
	}

	path = malloc(strlen(pw->pw_dir) + strlen(RCON_DEFAULT_CONFIG_PATH_SUFFIX) + 1);
	if (path == NULL)
		return NULL;

	strcpy(path, pw->pw_dir);
	strcat(path, RCON_DEFAULT_CONFIG_PATH_SUFFIX);

	file = fopen(path, "r");
	free(path);
	return file;
}

static char *find_config(FILE *file, const char *target)
{
	char *line = NULL;
	size_t line_len = 0;
	ssize_t nread;
	size_t target_len = strlen(target);

	while ((nread = getline(&line, &line_len, file)) != -1) {
		if (nread > 0 && line[0] != '#' && strncmp(line, target, target_len) == 0 &&
		    (line[target_len] == ',' || line[target_len] == '\0' || line[target_len] == '\n')) {
			return line; /* caller frees */
		}
		free(line);
		line = NULL;
		line_len = 0;
	}
	free(line);
	return NULL;
}

static int parse_and_auth(char *config_entry, rcon_connection *conn)
{
	char *address;
	char *port_str;
	char *password;
	char *endptr;
	long port_long;
	uint16_t port;
	int rval;
	char *owned_password = NULL;
	char *nl;

	/* Skip alias */
	if (strtok(config_entry, ",") == NULL)
		return EINVAL;

	address = strtok(NULL, ",");
	port_str = strtok(NULL, ",");
	password = strtok(NULL, ",\n"); /* strip possible newline from password */

	if (address == NULL || port_str == NULL)
		return EINVAL;

	/* Strip trailing newline/whitespace from port field */
	nl = port_str + strlen(port_str) - 1;
	while (nl >= port_str && (*nl == '\n' || *nl == '\r' || *nl == ' ' || *nl == '\t')) {
		*nl = '\0';
		nl--;
	}

	port_long = strtol(port_str, &endptr, 10);
	if (endptr == port_str || *endptr != '\0' || port_long < 1 || port_long > 65535)
		return EINVAL;
	port = (uint16_t)port_long;

	rval = rcon_init(conn, address, port);
	if (rval)
		return rval;

	if (password == NULL || password[0] == '\0') {
		owned_password = get_password();
		if (owned_password == NULL) {
			rcon_disconnect(conn);
			return EINVAL;
		}
		password = owned_password;
	}

	rval = rcon_auth(conn, password);
	if (owned_password) {
		explicit_bzero(owned_password, strlen(owned_password));
		free(owned_password);
	}

	if (rval)
		rcon_disconnect(conn);

	return rval;
}

