/*
 * Remote Console (RCON)
 *
 * This application facilitates communication with servers adhering to the
 * Source RCON Protocol as defined on the Valve Developer website at
 * https://developer.valvesoftware.com/wiki/Source_RCON_Protocol.
 *
 * This application expects to find a configuration file named
 * ~/.config/rcon/rcon.conf.  The configuration file should contain an entry
 * for each RCON capable server you want to access with this utility, using the
 * format:
 *
 * 	name,IP address,port[,password]
 *
 * The user will be prompted for the server's RCON password at runtime if
 * a password is not provided in the configuration file.
 *
 * Call the application from the command-line with two arguments:
 *
 * 	target		The name of the server to contact.  This is the
 * 			'name' field in the configuration example above.
 *
 * 	command		Command to send to that server.
 *
 * For example:
 *
 * 	$ rcon my_server status
 *
 * will send the command "status" to the server named "my_server" in the
 * ~/.rcon configuration file.  The configuration file entry for this example
 * might be:
 *
 * 	my_server,127.0.0.1,21515
 *
 * 	or
 *
 * 	my_server,127.0.0.1,21515,P4S5W0RD
 *
 * where "P4S5W0RD" is the server's RCON password.
 */

#include "main.h"
#include "rcon.h"

int main(int argc, char **argv)
{
	size_t command_len = 0;
	char *command;			/* Command to send */
	char *response;			/* Server's response */
	int rval;
	char *target;			/* Server's alias in the config file */

	/*
	 * Process command line.
	 */
	if (argc < 3) {
		puts("usage: rcon <target> <command>");
		return EINVAL;
	}

	target = argv[1];

	for (int i = 2; i < argc; ++i)
		command_len += strlen(argv[i]) + 1;

	command = (char *) calloc(command_len, sizeof(char));
	if (command == NULL) {
		perror("calloc");
		return errno;
	}

	/* Concatenate args as a single string. */
	for (int i=2; i < argc; ++i) {
		strcat(command, argv[i]);
		if (i + 1 < argc)
			strcat(command, " ");
	}

	/*
	 * Process configuration file.
	 */
	rval = load_config(target);
	if (rval) {
		free(command);
		fprintf(stderr, "load_config: %s\n", strerror(rval));
		return rval;
	}

	/*
	 * Transaction.
	 */
	rval = rcon_send(command, SERVERDATA_EXECCOMMAND);
	free(command);
	if (rval) {
		errno = rval;
		perror("rcon_send");
		return rval;
	}

	response = rcon_recv();
	if (response == NULL) {
		rval = errno;
		free(response);
		perror("rcon_recv()");
		return rval;
	}

	puts(response);
	free(response);

	return 0;
}


/*
 * Searches the provided file for the named target.  On success,
 * a pointer to the configuration line is returned, which must be
 * freed later.  On failure, NULL is returned.
 */
char * find_config(FILE *file, const char *target)
{
	bool eof = false;
	char *line = NULL;
	size_t line_len = 0;
	int rval;

	while (eof == false) {
		/* Let getline() allocate the buffer. */
		rval = getline(&line, &line_len, file);
		
		if (rval == -1)
			eof = true;

		if (line_len) {
			if (strncmp(line, target, strlen(target)) == 0) {
				break;
			}
		}

		free(line);
		line = NULL;
		line_len = 0;
	}

	return line;
}


/*
 * Prompt the user for a password, and return it.  The caller must free the
 * provided string.
 */
char * get_password(void)
{
	char input;
	size_t max_length = 128;
	char *password = calloc(max_length, sizeof(char));
	struct termios term;

	printf("Enter password: ");

	/* Disable terminal echo */
	tcgetattr(fileno(stdin), &term);
	term.c_lflag &= ~ECHO;
	tcsetattr(fileno(stdin), 0, &term);

	for (int i = 0; i < max_length; ++i) {
		input = getchar();
		if (input == '\n') {
			printf("\n");
			break;
		}

		password[i] = input;
	}

	/* Enable terminal echo */
	term.c_lflag |= ECHO;
	tcsetattr(fileno(stdin), 0, &term);

	return password;
}


/*
 * Obtain the entry for the provided target from the configuration file, and
 * call rcon_init() with the details.  Returns zero on success, and errno on
 * failure.
 *
 * The configuration file is expected as ~/.config/rcon/rcon.conf and contains one line
 * for each server entry using this format:
 *
 * 	target_name,IP address,port,password
 */
int load_config(const char *target)
{
	FILE *file;
	char *line;
	size_t rval;

	file = open_config();
	if (file == NULL)
		return errno;

	line = find_config(file, target);
	if (line == NULL)
		return errno;
	
	rval = parse_config(line);

	free(line);

	return rval;
}


/*
 * Provides a file pointer to ~/.config/rcon/rcon.conf
 *
 * Returns NULL on failure.
 */
FILE * open_config(void)
{
	FILE *file;
	char *home_dir;
	char *path;

	errno = 0; /* Per getpwuid man page */
	home_dir = getpwuid(getuid())->pw_dir;
	path = calloc(strlen(home_dir) + 24, sizeof(char));
	if (path == NULL)
		return NULL;

	strcpy(path, home_dir);
	strcat(path, "/.config/rcon/rcon.conf");

	file = fopen(path, "r");
	free(path);

	return file;
}


/*
 * Parse the provided configuration entry and call rcon_init().  The user is
 * prompted for a password if one is not provided in the configuration line.
 *
 * Returns zero on success and errno on failure.
 */
int parse_config(char *config_entry)
{
	char *address;
	uint16_t port;
	char *password;
	int last;
	int rval;

	strtok(config_entry, ",");	/* Ignore the name entry */
	address = strtok(NULL, ",");
	port = atoi(strtok(NULL, ","));
	password = strtok(NULL, ",");

	/* strip newline from password, if we have one. */
	if (password) {
		last = strlen(password) - 1;
		if (password[last] == '\n')
			password[last] = '\0';
	}

	rval = rcon_init(address, port);
	if (rval) {
		return rval;
	}

	if (password == NULL) {
		password = get_password();
		rval = rcon_auth(password);
		free(password);
	} else {
		rval = rcon_auth(password);
	}

	return rval;
}
