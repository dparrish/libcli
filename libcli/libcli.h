#ifndef __LIBCLI_H__
#define __LIBCLI_H__

#define CLI_OK		0
#define CLI_ERROR	-1
#define CLI_QUIT	-2

struct cli_command
{
    char *command;
    int (*callback)(FILE *, char *, char **, int);
    int unique_len;
    char *help;
    struct cli_command *next;
    struct cli_command *children;
    struct cli_command *parent;
};

int cli_init();
int cli_done();
struct cli_command *cli_register_command(struct cli_command *parent, char *command, int (*callback)(FILE *, char *, char **, int), char *help);
int cli_unregister_command(char *command);
int cli_loop(int sockfd, char *prompt);
void cli_set_auth_callback(int (*auth_callback)(char *, char *));
void cli_allow_user(char *username, char *password);
void cli_deny_user(char *username);
void cli_set_banner(char *banner);

#endif
