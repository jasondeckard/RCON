#include "config.h"
#include "rcon.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [options] <target> <command> [args...]\n"
		"\n"
		"Send an RCON command to a Source engine server.\n"
		"\n"
		"Options:\n"
		"  -c, --config PATH   Use PATH instead of ~/.config/rcon/rcon.conf\n"
		"  -h, --help          Show this help\n"
		"\n"
		"Configuration file format (one server per line):\n"
		"  alias,IP address,port[,password]\n"
		"\n"
		"If the password is omitted, you will be prompted for it.\n",
		prog);
}

int main(int argc, char **argv)
{
	rcon_connection conn = {0};
	char *command = NULL;
	char *response = NULL;
	const char *config_path = NULL;
	const char *target = NULL;
	size_t command_len = 0;
	int i, rval;
	int arg_start = 1;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			return 0;
		}
		if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "error: %s requires a path\n", argv[i]);
				usage(argv[0]);
				return EINVAL;
			}
			config_path = argv[++i];
			continue;
		}
		if (argv[i][0] == '-') {
			fprintf(stderr, "error: unknown option %s\n", argv[i]);
			usage(argv[0]);
			return EINVAL;
		}
		/* First non-option is target */
		target = argv[i];
		arg_start = i + 1;
		break;
	}

	if (target == NULL || arg_start >= argc) {
		usage(argv[0]);
		return EINVAL;
	}

	for (i = arg_start; i < argc; i++)
		command_len += strlen(argv[i]) + 1;

	command = calloc(command_len, 1);
	if (command == NULL) {
		perror("calloc");
		return errno;
	}

	for (i = arg_start; i < argc; i++) {
		strcat(command, argv[i]);
		if (i + 1 < argc)
			strcat(command, " ");
	}

	rval = load_and_auth(target, config_path, &conn);
	if (rval) {
		fprintf(stderr, "failed to load/authenticate '%s': %s\n", target, strerror(rval));
		free(command);
		return rval;
	}

	rval = rcon_send(&conn, command, SERVERDATA_EXECCOMMAND);
	free(command);
	if (rval) {
		fprintf(stderr, "rcon_send: %s\n", strerror(rval));
		rcon_disconnect(&conn);
		return rval;
	}

	response = rcon_recv(&conn);
	rcon_disconnect(&conn);

	if (response == NULL) {
		perror("rcon_recv");
		return errno ? errno : EIO;
	}

	puts(response);
	free(response);
	return 0;
}

