#ifndef __LIBCLI_H__
#define __LIBCLI_H__

// vim:sw=4 tw=120 et

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

#define CLI_OK                  0
#define CLI_ERROR               -1
#define CLI_QUIT                -2
#define CLI_ERROR_ARG           -3

#define MAX_HISTORY             256

#define PRIVILEGE_UNPRIVILEGED  0
#define PRIVILEGE_PRIVILEGED    15
#define MODE_ANY                -1
#define MODE_EXEC               0
#define MODE_CONFIG             1

#define LIBCLI_HAS_ENABLE       1

#define PRINT_PLAIN             0
#define PRINT_FILTERED          0x01
#define PRINT_BUFFERED          0x02
#define PRINT_NONL              0x04

#define CLI_MAX_LINE_LENGTH     4096
#define CLI_MAX_LINE_WORDS      128

struct cli_def {
    int completion_callback;
    struct cli_command *commands;
    int (*auth_callback)(const char *, const char *);
    int (*regular_callback)(struct cli_def *cli);
    int (*enable_callback)(const char *);
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
    void (*print_callback)(struct cli_def *cli, const char *string);
    FILE *client;
    /* internal buffers */
    void *conn;
    void *service;
    char *commandname;  // temporary buffer for cli_command_name() to prevent leak
    char *buffer;
    unsigned buf_size;
    struct timeval timeout_tm;
    time_t idle_timeout;
    int (*idle_timeout_callback)(struct cli_def *);
    time_t last_action;
    int telnet_protocol;
    void *user_context;
};

struct cli_filter {
    int (*filter)(struct cli_def *cli, const char *string, void *data);
    void *data;
    struct cli_filter *next;
};

struct cli_command {
    char *command;
    int (*callback)(struct cli_def *, const char *, char **, int);
    unsigned int unique_len;
    char *help;
    int privilege;
    int mode;
    struct cli_command *next;
    struct cli_command *children;
    struct cli_command *parent;
};

struct cli_def *cli_init(void);
int cli_done(struct cli_def *cli);
struct cli_command *cli_register_command(struct cli_def *cli, struct cli_command *parent, const char *command,
                                         int (*callback)(struct cli_def *, const char *, char **, int), int privilege,
                                         int mode, const char *help);
int cli_unregister_command(struct cli_def *cli, const char *command);
int cli_run_command(struct cli_def *cli, const char *command);
int cli_loop(struct cli_def *cli, int sockfd);
int cli_file(struct cli_def *cli, FILE *fh, int privilege, int mode);
void cli_set_auth_callback(struct cli_def *cli, int (*auth_callback)(const char *, const char *));
void cli_set_enable_callback(struct cli_def *cli, int (*enable_callback)(const char *));
void cli_allow_user(struct cli_def *cli, const char *username, const char *password);
void cli_allow_enable(struct cli_def *cli, const char *password);
void cli_deny_user(struct cli_def *cli, const char *username);
void cli_set_banner(struct cli_def *cli, const char *banner);
void cli_set_hostname(struct cli_def *cli, const char *hostname);
void cli_set_promptchar(struct cli_def *cli, const char *promptchar);
void cli_set_modestring(struct cli_def *cli, const char *modestring);
int cli_set_privilege(struct cli_def *cli, int privilege);
int cli_set_configmode(struct cli_def *cli, int mode, const char *config_desc);
void cli_reprompt(struct cli_def *cli);
void cli_regular(struct cli_def *cli, int (*callback)(struct cli_def *cli));
void cli_regular_interval(struct cli_def *cli, int seconds);
void cli_print(struct cli_def *cli, const char *format, ...) __attribute__((format (printf, 2, 3)));
void cli_print_nonl(struct cli_def *cli, const char *format, ...) __attribute__((format (printf, 2, 3)));
void cli_bufprint(struct cli_def *cli, const char *format, ...) __attribute__((format (printf, 2, 3)));
void cli_vabufprint(struct cli_def *cli, const char *format, va_list ap);
void cli_error(struct cli_def *cli, const char *format, ...) __attribute__((format (printf, 2, 3)));
void cli_print_callback(struct cli_def *cli, void (*callback)(struct cli_def *, const char *));
void cli_free_history(struct cli_def *cli);
void cli_set_idle_timeout(struct cli_def *cli, unsigned int seconds);
void cli_set_idle_timeout_callback(struct cli_def *cli, unsigned int seconds, int (*callback)(struct cli_def *));

// Enable or disable telnet protocol negotiation.
// Note that this is enabled by default and must be changed before cli_loop() is run.
void cli_telnet_protocol(struct cli_def *cli, int telnet_protocol);

// Set/get user context
void cli_set_context(struct cli_def *cli, void *context);
void *cli_get_context(struct cli_def *cli);

#ifdef __cplusplus
}
#endif

#define CLI_EV_WAIT 1
#define CLI_EV_QUIT 2
#define CLI_EV_TRY_ACTION 3

typedef struct cc_rbuf_s {
    int             nx;
    int             lx;
    int             oldlx;
    int             is_telnet_option;
    int             skip;
    int             esc;
    int             cursor;
    int             insertmode;
    char           *cmd;
    char           *oldcmd;
    char           *username;
    char           *password;
    signed int      in_history;
    int             lastchar;
    struct timeval  tm;
} cc_rbuf_t;

extern cc_rbuf_t cc_rbuf_default;

struct cc_server_ctx_s;

typedef struct cc_ev_client_s {
    struct event    *cc_ev;
    int              cc_ev_fd;
} cc_ev_client_t;

#define CC_EVENT_MODE_SELECT    0
#define CC_EVENT_MODE_EVPOLL    1
typedef struct cc_ev_server_s {
    int                    cc_ev_mode;
    struct event_base     *cc_ev_base;
    struct evconnlistener *cc_ev_listener;
    int                    cc_ev_epollfd;
} cc_ev_server_t;

#define CC_CLIENTS_MAX 5
typedef struct cc_client_s {
    unsigned int            cc_client_id;
    struct cli_def         *cc_cli_ctx;
    cc_rbuf_t               cc_ri;      /* read information */

    cc_ev_client_t          cc_ev_clnt;
    struct cc_server_ctx_s *cc_svr_ctx;
} cc_client_t;

typedef struct cli_def * (*cc_cli_ctx_alloc_cb)(void);

typedef struct cc_server_ctx_s {
    cc_ev_server_t         sc_ev_srvr;
    cc_cli_ctx_alloc_cb    sc_cli_ctx_alloc_cb;
    cc_client_t           *sc_client_list[CC_CLIENTS_MAX+1];
} cc_server_ctx_t;

int
cc_conn_cli_setup(cc_client_t *client_ctx,
                  int          sockfd);
int cc_conn_input_read (cc_client_t *client_ctx,
                        int          sockfd);
int cc_conn_cli_action(cc_client_t *client_ctx,
                       int          sockfd);

int
cc_client_add (cc_server_ctx_t *svr_ctx,
               cc_client_t **client);
void
cc_client_remove (cc_client_t **cc_client);

unsigned char
cli_help_required (struct cli_def *cli,
                   const char     *command,
                   char           *argv[],
                   int             argc);
#endif
