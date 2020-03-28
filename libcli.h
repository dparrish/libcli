#ifndef __LIBCLI_H__
#define __LIBCLI_H__

// vim:sw=4 tw=120 et

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>

#define LIBCLI_VERSION_MAJOR 1
#define LIBCLI_VERISON_MINOR 10
#define LIBCLI_VERISON_REVISION 3
#define LIBCLI_VERSION ((LIBCLI_VERSION_MAJOR << 16) | (LIBCLI_VERSION_MINOR << 8) | LIBCLI_VERSION_REVISION)

#define CLI_OK 0
#define CLI_ERROR -1
#define CLI_QUIT -2
#define CLI_ERROR_ARG -3
#define CLI_AMBIGUOUS -4
#define CLI_UNRECOGNIZED -5
#define CLI_MISSING_ARGUMENT -6
#define CLI_MISSING_VALUE -7
#define CLI_BUILDMODE_START -8
#define CLI_BUILDMODE_ERROR -9
#define CLI_BUILDMODE_EXTEND -10
#define CLI_BUILDMODE_CANCEL -11
#define CLI_BUILDMODE_EXIT -12

#define MAX_HISTORY 256

#define PRIVILEGE_UNPRIVILEGED 0
#define PRIVILEGE_PRIVILEGED 15
#define MODE_ANY -1
#define MODE_EXEC 0
#define MODE_CONFIG 1

#define LIBCLI_HAS_ENABLE 1

#define PRINT_PLAIN 0
#define PRINT_FILTERED 0x01
#define PRINT_BUFFERED 0x02

#define CLI_MAX_LINE_LENGTH 4096
#define CLI_MAX_LINE_WORDS 128

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
  struct cli_optarg_pair *found_optargs;
  int transient_mode;
  int disallow_buildmode;
  struct cli_pipeline *pipeline;
  struct cli_buildmode *buildmode;
};

struct cli_filter {
  int (*filter)(struct cli_def *cli, const char *string, void *data);
  void *data;
  struct cli_filter *next;
};

enum command_types {
  CLI_ANY_COMMAND,
  CLI_REGULAR_COMMAND,
  CLI_FILTER_COMMAND,
  CLI_BUILDMODE_COMMAND,
};

struct cli_command {
  char *command;
  int (*callback)(struct cli_def *, const char *, char **, int);
  unsigned int unique_len;
  char *help;
  int privilege;
  int mode;
  struct cli_command *previous;
  struct cli_command *next;
  struct cli_command *children;
  struct cli_command *parent;
  struct cli_optarg *optargs;
  int (*filter)(struct cli_def *cli, const char *string, void *data);
  int (*init)(struct cli_def *cli, int, char **, struct cli_filter *filt);
  int command_type;
  int flags;
};

struct cli_comphelp {
  int comma_separated;
  char **entries;
  int num_entries;
};

enum optarg_flags {
  CLI_CMD_OPTIONAL_FLAG = 1 << 0,
  CLI_CMD_OPTIONAL_ARGUMENT = 1 << 1,
  CLI_CMD_ARGUMENT = 1 << 2,
  CLI_CMD_ALLOW_BUILDMODE = 1 << 3,
  CLI_CMD_OPTION_MULTIPLE = 1 << 4,
  CLI_CMD_OPTION_SEEN = 1 << 5,
  CLI_CMD_TRANSIENT_MODE = 1 << 6,
  CLI_CMD_DO_NOT_RECORD = 1 << 7,
  CLI_CMD_REMAINDER_OF_LINE = 1 << 8,
  CLI_CMD_HYPHENATED_OPTION = 1 << 9,
  CLI_CMD_SPOT_CHECK = 1 << 10,
};

struct cli_optarg {
  char *name;
  int flags;
  char *help;
  int mode;
  int privilege;
  unsigned int unique_len;
  int (*get_completions)(struct cli_def *, const char *, const char *, struct cli_comphelp *);
  int (*validator)(struct cli_def *, const char *, const char *);
  int (*transient_mode)(struct cli_def *, const char *, const char *);
  struct cli_optarg *next;
};

struct cli_optarg_pair {
  char *name;
  char *value;
  struct cli_optarg_pair *next;
};

struct cli_pipeline_stage {
  struct cli_command *command;
  struct cli_optarg_pair *found_optargs;
  char **words;
  int num_words;
  int status;
  int first_unmatched;
  int first_optarg;
  int stage_num;
  char *error_word;
};

struct cli_pipeline {
  char *cmdline;
  char *words[CLI_MAX_LINE_WORDS];
  int num_words;
  int num_stages;
  struct cli_pipeline_stage stage[CLI_MAX_LINE_WORDS];
  struct cli_pipeline_stage *current_stage;
};

struct cli_buildmode {
  struct cli_command *command;
  struct cli_optarg_pair *found_optargs;
  char *cname;
  int mode;
  int transient_mode;
  char *mode_text;
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
void cli_print(struct cli_def *cli, const char *format, ...) __attribute__((format(printf, 2, 3)));
void cli_bufprint(struct cli_def *cli, const char *format, ...) __attribute__((format(printf, 2, 3)));
void cli_vabufprint(struct cli_def *cli, const char *format, va_list ap);
void cli_error(struct cli_def *cli, const char *format, ...) __attribute__((format(printf, 2, 3)));
void cli_print_callback(struct cli_def *cli, void (*callback)(struct cli_def *, const char *));
void cli_free_history(struct cli_def *cli);
void cli_set_idle_timeout(struct cli_def *cli, unsigned int seconds);
void cli_set_idle_timeout_callback(struct cli_def *cli, unsigned int seconds, int (*callback)(struct cli_def *));
void cli_dump_optargs_and_args(struct cli_def *cli, const char *text, char *argv[], int argc);

// Enable or disable telnet protocol negotiation.
// Note that this is enabled by default and must be changed before cli_loop() is run.
void cli_telnet_protocol(struct cli_def *cli, int telnet_protocol);

// Set/get user context
void cli_set_context(struct cli_def *cli, void *context);
void *cli_get_context(struct cli_def *cli);

void cli_free_comphelp(struct cli_comphelp *comphelp);
int cli_add_comphelp_entry(struct cli_comphelp *comphelp, const char *entry);
void cli_set_transient_mode(struct cli_def *cli, int transient_mode);
struct cli_command *cli_register_filter(struct cli_def *cli, const char *command,
                                        int (*init)(struct cli_def *cli, int, char **, struct cli_filter *filt),
                                        int (*filter)(struct cli_def *, const char *, void *), int privilege, int mode,
                                        const char *help);
int cli_unregister_filter(struct cli_def *cli, const char *command);
struct cli_optarg *cli_register_optarg(struct cli_command *cmd, const char *name, int flags, int priviledge, int mode,
                                       const char *help,
                                       int (*get_completions)(struct cli_def *cli, const char *, const char *,
                                                              struct cli_comphelp *),
                                       int (*validator)(struct cli_def *cli, const char *, const char *),
                                       int (*transient_mode)(struct cli_def *, const char *, const char *));
int cli_optarg_addhelp(struct cli_optarg *optarg, const char *helpname, const char *helptext);
char *cli_find_optarg_value(struct cli_def *cli, char *name, char *find_after);
struct cli_optarg_pair *cli_get_all_found_optargs(struct cli_def *cli);
int cli_unregister_optarg(struct cli_command *cmd, const char *name);
char *cli_get_optarg_value(struct cli_def *cli, const char *name, char *find_after);
int cli_set_optarg_value(struct cli_def *cli, const char *name, const char *value, int allow_multiple);
void cli_unregister_all_optarg(struct cli_command *c);
void cli_unregister_all_filters(struct cli_def *cli);
void cli_unregister_all_commands(struct cli_def *cli);
void cli_unregister_all(struct cli_def *cli, struct cli_command *command);

/*
 * Expose some previous internal routines.  Just in case someone was using those
 * with an explicit reference, the original routines (cli_int_*) internally point
 * to the newly public routines.
 */
int cli_help(struct cli_def *cli, const char *command, char *argv[], int argc);
int cli_history(struct cli_def *cli, const char *command, char *argv[], int argc);
int cli_exit(struct cli_def *cli, const char *command, char *argv[], int argc);
int cli_quit(struct cli_def *cli, const char *command, char *argv[], int argc);
int cli_enable(struct cli_def *cli, const char *command, char *argv[], int argc);
int cli_disable(struct cli_def *cli, const char *command, char *argv[], int argc);

#ifdef __cplusplus
}
#endif

#endif
