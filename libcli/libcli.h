#ifndef __LIBCLI_H__
#define __LIBCLI_H__

#define CLI_OK		0
#define CLI_ERROR	-1
#define CLI_QUIT	-2
#define MAX_HISTORY	256

#include <stdio.h>

struct cli_def
{
    int completion_callback;
    struct cli_command *commands;
    int (*auth_callback)(char *, char *);
    int (*regular_callback)(struct cli_def *cli, FILE *);
    char *banner;
    struct unp *users;
    char *history[MAX_HISTORY];
    char showprompt;
    int (*filter)(struct cli_def *cli, char *string, char *params[], int num_params);
    int filter_param_i;
    char **filter_param_s;
    void *filter_data;
};

struct cli_command
{
    char *command;
    int (*callback)(struct cli_def *, FILE *, char *, char **, int);
    int unique_len;
    char *help;
    struct cli_command *next;
    struct cli_command *children;
    struct cli_command *parent;
};

struct cli_def *cli_init();
int cli_done(struct cli_def *cli);
struct cli_command *cli_register_command(struct cli_def *cli, struct cli_command *parent, char *command, int (*callback)(struct cli_def *, FILE *, char *, char **, int), char *help);
int cli_unregister_command(struct cli_def *cli, char *command);
int cli_loop(struct cli_def *cli, int sockfd, char *prompt);
void cli_set_auth_callback(struct cli_def *cli, int (*auth_callback)(char *, char *));
void cli_allow_user(struct cli_def *cli, char *username, char *password);
void cli_deny_user(struct cli_def *cli, char *username);
void cli_set_banner(struct cli_def *cli, char *banner);
void cli_reprompt(struct cli_def *cli);
void cli_regular(struct cli_def *cli, int (*callback)(struct cli_def *cli, FILE *));
void cli_print(struct cli_def *cli, FILE *client, char *format, ...);
void cli_add_filter(struct cli_def *cli, int (*filter)(struct cli_def *, char *, char **, int), char *params[], int num_params);
void cli_clear_filter(struct cli_def *cli);

#endif
