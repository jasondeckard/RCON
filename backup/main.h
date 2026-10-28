#ifndef __MAIN_H__
#define __MAIN_H__ 1

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdbool.h>
#include <termios.h>

extern char * find_config(FILE *file, const char *target);
extern char * get_password(void);
extern int load_config(const char *target);
extern FILE * open_config(void);
extern int parse_config(char *config_entry);

#endif /* __MAIN_H__ */
