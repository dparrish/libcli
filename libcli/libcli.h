#ifndef __LIBCLI_H__
#define __LIBCLI_H__

#include <stdio.h>
#include <stdarg.h>

#define CLI_OK			0
#define CLI_ERROR		-1
#define CLI_QUIT		-2
#define CLI_ERROR_ARG		-3

#define MAX_HISTORY		256

#define PRIVILEGE_UNPRIVILEGED	0
#define PRIVILEGE_PRIVILEGED	15
#define MODE_ANY		-1
#define MODE_EXEC		0
#define MODE_CONFIG		1

#define LIBCLI_HAS_ENABLE	1

#define PRINT_PLAIN		0
#define PRINT_FILTERED		0x01
#define PRINT_BUFFERED		0x02

struct cli_def {
    int completion_callback;
    struct cli_command *commands;
    int (*auth_callback)(char *, char *);
    int (*regular_callback)(struct cli_def *cli);
    int (*enable_callback)(char *);
    char *banner;
    struct unp *users;
    char *enable_password;
    char *history[MAX_HISTORY];
    char showprompt;
    char *promptchar;
    char *hostname;
    char *modestring;
    int privilege;
    int mode;
    int state;
    struct cli_filter *filters;
    void (*print_callback)(struct cli_def *cli, char *string);
    FILE *client;
    /* internal buffers */
    void *conn;
    void *service;
    char *commandname;  // temporary buffer for cli_command_name() to prevent leak
    char *buffer;
    unsigned buf_size;
    struct timeval timeout_tm;
    unsigned int idle_timeout;
    time_t last_action;
};

struct cli_filter {
    int (*filter)(struct cli_def *cli, char *string, void *data);
    void *data;
    struct cli_filter *next;
};

struct cli_command {
    char *command;
    int (*callback)(struct cli_def *, char *, char **, int);
    unsigned int unique_len;
    char *help;
    int privilege;
    int mode;
    struct cli_command *next;
    struct cli_command *children;
    struct cli_command *parent;
};

struct cli_def *cli_init();
int cli_done(struct cli_def *cli);
struct cli_command *cli_register_command(struct cli_def *cli, struct cli_command *parent, char *command, int (*callback)(struct cli_def *, char *, char **, int), int privilege, int mode, char *help);
int cli_unregister_command(struct cli_def *cli, char *command);
int cli_run_command(struct cli_def *cli, char *command);
int cli_loop(struct cli_def *cli, int sockfd);
int cli_file(struct cli_def *cli, FILE *fh, int privilege, int mode);
void cli_set_auth_callback(struct cli_def *cli, int (*auth_callback)(char *, char *));
void cli_set_enable_callback(struct cli_def *cli, int (*enable_callback)(char *));
void cli_allow_user(struct cli_def *cli, char *username, char *password);
void cli_allow_enable(struct cli_def *cli, char *password);
void cli_deny_user(struct cli_def *cli, char *username);
void cli_set_banner(struct cli_def *cli, char *banner);
void cli_set_hostname(struct cli_def *cli, char *hostname);
void cli_set_promptchar(struct cli_def *cli, char *promptchar);
void cli_set_modestring(struct cli_def *cli, char *modestring);
int cli_set_privilege(struct cli_def *cli, int privilege);
int cli_set_configmode(struct cli_def *cli, int mode, char *config_desc);
void cli_reprompt(struct cli_def *cli);
void cli_regular(struct cli_def *cli, int (*callback)(struct cli_def *cli));
void cli_regular_interval(struct cli_def *cli, int seconds);
void cli_print(struct cli_def *cli, char *format, ...) __attribute__((format (printf, 2, 3)));
void cli_bufprint(struct cli_def *cli, char *format, ...) __attribute__((format (printf, 2, 3)));
void cli_vabufprint(struct cli_def *cli, char *format, va_list ap);
void cli_error(struct cli_def *cli, char *format, ...) __attribute__((format (printf, 2, 3)));
void cli_print_callback(struct cli_def *cli, void (*callback)(struct cli_def *, char *));
void cli_free_history(struct cli_def *cli);
void cli_set_idle_timeout(struct cli_def *cli, unsigned int seconds);

#endif
