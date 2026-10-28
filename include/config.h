#ifndef CONFIG_H
#define CONFIG_H

#include "rcon.h"
#include <stdio.h>

#define RCON_DEFAULT_CONFIG_PATH_SUFFIX "/.config/rcon/rcon.conf"
#define RCON_MAX_PASSWORD_LEN 128

/* Load server details for the named target and authenticate.
 * config_path may be NULL to use the default ~/.config/rcon/rcon.conf.
 * On success returns 0; on failure returns an errno value.
 * The connection is left authenticated on success.
 */
int load_and_auth(const char *target, const char *config_path, rcon_connection *conn);

/* Prompt for a password with echo disabled. Caller must free the result.
 * Returns NULL on error or empty input.
 */
char *get_password(void);

#endif /* CONFIG_H */

