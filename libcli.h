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
#define LIBCLI_VERSION_MINOR 10
#define LIBCLI_VERSION_REVISION 8
#define LIBCLI_VERSION ((LIBCLI_VERSION_MAJOR << 16) | (LIBCLI_VERSION_MINOR << 8) | LIBCLI_VERSION_REVISION)

// for backward compatability
#define LIBCLI_VERISON_MINOR    LIBCLI_VERSION_MINOR
#define LIBCLI_VERISON_REVISION LIBCLI_VERSION_REVISION


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
#define CLI_INCOMPLETE_COMMAND -13

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
//  char *commandname;  // temporary buffer for cli_command_name() to prevent leak
  char *buffer;
  unsigned buf_size;
  struct timeval timeout_tm;
  time_t idle_timeout;
  int (*idle_timeout_callback)(struct cli_def *);
  time_t last_action;
  time_t last_regular;
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
  char *full_command_name;
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

/**
 * @brief      cli object constructor
 *
 * @return     new cli object or NULL in case of error
 */
struct cli_def *cli_init(void);

/**
 * @brief      terminating a cli object
 *
 * @param      cli   target cli object
 *
 * @return     returns CLI_OK in case of successful termination
 */
int cli_done(struct cli_def *cli);

/**
 * @brief      main function to register one or a set of commands into a cli
 *             object
 *
 * @param      cli        target cli object
 * @param      parent     parent of target command; use this if you want to make
 *                        tree-style commands structures; this parent is
 *                        actually the return value of another previous
 *                        successful registration
 * @param[in]  command    target to-be-registered command
 * @param[in]  callback   function pointer which is paired to the target command
 * @param[in]  privilege  access privilege of the target command; can be one of
 *                        the PRIVILEGE_UNPRIVILEGED or PRIVILEGE_PRIVILEGED
 * @param[in]  mode       the mode in which the command should be visible; can
 *                        be predefined {MODE_ANY, MODE_EXEC, MODE_CONFIG} or
 *                        any other user-defined mode; which user enters a
 *                        specific mode just the commands in that mode will be
 *                        shown to the user plus some other common commands such
 *                        as 'exit' or 'quit' or 'help', etc.
 * @param[in]  help       your instruction help for the target command; this is
 *                        going to be shown to the user when he/she types 'help'
 *                        in a mode
 *
 * @return     return NULL in case of error or a cli_command object; if you want
 *             the current registered command to be the parent of another
 *             command, you should feed this return value as the 'parent' of
 *             another registration
 */
struct cli_command *cli_register_command(struct cli_def *cli, struct cli_command *parent, const char *command,
                                         int (*callback)(struct cli_def *, const char *, char **, int), int privilege,
                                         int mode, const char *help);
/**
 * @brief      function to un-register (remove) a command from commands tree;
 *             note that its children will be inaccessible obviously
 *
 * @param      cli      target cli object which the command should be removed
 * @param[in]  command  to-be-removed command
 *
 * @return     return CLI_OK in case of success or -1 in case of error
 */
int cli_unregister_command(struct cli_def *cli, const char *command);

/**
 * @brief      if you want to run a command in your program (compile-time), you
 *             can use this function
 *
 * @param      cli      target cli object
 * @param[in]  command  target command to be run
 *
 * @return     CLI_OK or CLI_ERROR
 */
int cli_run_command(struct cli_def *cli, const char *command);

/**
 * @brief      main loop function in which the code is waiting for user to enter
 *             something; this is called when you set up everything and
 *             registered your commands; there should be a socket file
 *             descriptor related to the user; see clitest.c file for more
 *             example
 *
 * @param      cli     target cli object
 * @param[in]  sockfd  target socket file descriptor of a user
 *
 * @return     this function should not return normally; but if there is an
 *             error it returns CLI_ERROR; if some normal command like 'exit' or
 *             'quit' happend, it returns CLI_OK
 */
int cli_loop(struct cli_def *cli, int sockfd);

/**
 * @brief      function to execute cli commands from a file in a specific
 *             privilege and mode; privilege and mode of the cli will not
 *             changed after using this line
 *
 * @param      cli        target cli object
 * @param      fh         opened file stream pointer
 * @param[in]  privilege  target privilege for executing commands in the file
 * @param[in]  mode       target mode for executing commands in the file
 *
 * @return     only returns CLI_OK
 */
int cli_file(struct cli_def *cli, FILE *fh, int privilege, int mode);

/**
 * @brief      function to set another function to check whether a
 *             username/password pair is authenticated to this cli or not;
 *             auth_callback should be implemented by the developer; when user
 *             enters username/password on the cli, the username/password is
 *             going to be passed to auth_callback function; then auth_callback
 *             should make the decision whether the passed username/password is
 *             authenticated; if auth_callback returns CLI_OK, then the user is
 *             authenticated; else it should return CLI_ERROR
 *
 * @note       username/password authentication is always prompted to user at
 *             first
 *
 * @param      cli            target cli object
 * @param[in]  auth_callback  username/passwork authenticator function pointer;
 *                            it should be implemented by developer; see
 *                            clitest.c for an example
 */
void cli_set_auth_callback(struct cli_def *cli, int (*auth_callback)(const char *, const char *));

/**
 * @brief      in Cisco-like cli, user can configure different parts of a device
 *             through cli; to limit this just to admin users (which has a
 *             specific password), Cisco defined an "enable" terminology which
 *             enables access of a user to a more layers, such as configuring
 *             whole device; this is done when user writes "enable" on the
 *             prompt of cli; if you want to set a password for entering to this
 *             mode use this function; then when user enters a password,
 *             function 'enable_callback' is going to be called with the entered
 *             password, so you (developer) can check it and return CLI_OK if
 *             the entered password is correct or CLI_ERROR in case of wrong
 *             password
 *
 * @param      cli              target cli object
 * @param[in]  enable_callback  enable password checker function pointer; should
 *                              be implemented by developer; this is like
 *                              auth_callback in cli_set_auth_callback but just
 *                              one argument password
 *
 * @see        cli_set_auth_callback, auth_callback
 */
void cli_set_enable_callback(struct cli_def *cli, int (*enable_callback)(const char *));

/**
 * @brief      function to add a username/password pair; this is a dynamic
 *             version of using cli_set_auth_callback
 *
 * @param      cli       target cli object
 * @param[in]  username  new username
 * @param[in]  password  new password
 */
void cli_allow_user(struct cli_def *cli, const char *username, const char *password);

/**
 * @brief      function to change enable mode password
 *
 * @param      cli       target cli object
 * @param[in]  password  new password
 */
void cli_allow_enable(struct cli_def *cli, const char *password);

/**
 * @brief      function to remove a user from list of users
 *
 * @param      cli       target cli object
 * @param[in]  username  target username
 */
void cli_deny_user(struct cli_def *cli, const char *username);

/**
 * @brief      function to set a greeting when a connection is made
 *
 * @param      cli     target cli object
 * @param[in]  banner  new banner
 */
void cli_set_banner(struct cli_def *cli, const char *banner);

/**
 * @brief      function to set a hostname for the prompt; hostname is going to
 *             be shown at start of each line (like Linux terminal)
 *
 * @param      cli       target cli object
 * @param[in]  hostname  target hostname
 */
void cli_set_hostname(struct cli_def *cli, const char *hostname);

/**
 * @brief      if you want to show extra characters at each line (similar to
 *             hostname), use this function
 *
 * @param      cli         target cli object
 * @param[in]  promptchar  target promptchar characters
 */
void cli_set_promptchar(struct cli_def *cli, const char *promptchar);

/**
 * @brief      when the prompt enters a mode, you can set what to be shown to
 *             the user as a string title for that mode; this can be set via
 *             this function; this is going to be shown at each line (like
 *             hostname) when you are in that mode
 *
 * @param      cli         target cli object
 * @param[in]  modestring  target to-be-shown string in the current mode
 */
void cli_set_modestring(struct cli_def *cli, const char *modestring);

/**
 * @brief      function to set privilege of the cli object; 'privilege' takes
 *             two values PRIVILEGE_UNPRIVILEGED and PRIVILEGE_PRIVILEGED
 *
 * @param      cli        target cli object
 * @param[in]  privilege  target
 *
 * @return     return value is the previous privilege
 */
int cli_set_privilege(struct cli_def *cli, int privilege);

/**
 * @brief      use this function if you want to change the mode of the cli
 *             (prompt) into a new mode; use 'config_desc' if you want to show
 *             something to user in the prompt at each line for that mode
 *
 * @param      cli          target cli object
 * @param[in]  mode         target mode
 * @param[in]  config_desc  target to-be-shown name of mode in the prompt
 *
 * @return     previous mode is returned
 */
int cli_set_configmode(struct cli_def *cli, int mode, const char *config_desc);

/**
 * @brief      function to re-prompt the cli to the user
 *
 * @param      cli   target cli object
 */
void cli_reprompt(struct cli_def *cli);

/**
 * @brief      if you want to run a specific function regularly, call this
 *             function before using cli_loop; the default repetition time is 1
 *             seconds; use cli_regular_interval function to set another period
 *
 * @param      cli       target cli object
 * @param[in]  callback  target function which you want to call it regularly
 */
void cli_regular(struct cli_def *cli, int (*callback)(struct cli_def *cli));

/**
 * @brief      function to set time interval for cli_regular function
 *
 * @param      cli      target cli object
 * @param[in]  seconds  target period time in seconds
 */
void cli_regular_interval(struct cli_def *cli, int seconds);

/**
 * @brief      function to print something in printf-style to user
 *
 * @param      cli        target cli object
 * @param[in]  format     target printf-style format identifier
 * @param[in]  <unnamed>  related printf-style continuation
 */
void cli_print(struct cli_def *cli, const char *format, ...) __attribute__((format(printf, 2, 3)));

/**
 * @brief      same as cli_print function but to print all things in the buffer
 *
 * @param      cli        target cli object
 * @param[in]  format     target printf-style format identifier
 * @param[in]  <unnamed>  related printf-style continuation
 */
void cli_bufprint(struct cli_def *cli, const char *format, ...) __attribute__((format(printf, 2, 3)));

/**
 * @brief      same as cli_bufprint but it takes va_list as argument; see c
 *             variable argument concept for more information on va_list
 *
 * @param      cli     target cli object
 * @param[in]  format  target printf-style format identifier
 * @param[in]  ap      related va_list argument pointers
 */
void cli_vabufprint(struct cli_def *cli, const char *format, va_list ap);

/**
 * @brief      function to print something in the output as error
 *
 * @param      cli        target cli object
 * @param[in]  format     target printf-style format identifier
 * @param[in]  <unnamed>  related printf-style continuation
 */
void cli_error(struct cli_def *cli, const char *format, ...) __attribute__((format(printf, 2, 3)));

/**
 * @brief      by default cli_print uses fprintf function to print into the
 *             output; if you want to use another function use this method
 *             instead of cli_print
 *
 * @param      cli       target cli object
 * @param[in]  callback  callback function which is called for printing (to the
 *                       console probably)
 */
void cli_print_callback(struct cli_def *cli, void (*callback)(struct cli_def *, const char *));

/**
 * @brief      function to remove all commands history of the prompt
 *
 * @param      cli   target cli object
 */
void cli_free_history(struct cli_def *cli);

/**
 * @brief      if you want to quit the cli prompt if user does not enter
 *             anything for a specific time period, use this function; you can
 *             also specify a function before quitting via
 *             cli_set_idle_timeout_callback; the default value is 0 which means
 *             the cli never quit (infinite idle timeout)
 *
 * @param      cli      target cli object
 * @param[in]  seconds  target idle timeout in seconds
 */
void cli_set_idle_timeout(struct cli_def *cli, unsigned int seconds);

/**
 * @brief      same as cli_set_idle_timeout but it also calls another callback
 *             function after timeout (probably for cleaning stuff, printing,
 *             etc.)
 *
 * @param      cli       target cli object
 * @param[in]  seconds   target idle timeout in seconds
 * @param[in]  callback  callback function to be called after timeout and before
 *                       quitting
 */
void cli_set_idle_timeout_callback(struct cli_def *cli, unsigned int seconds, int (*callback)(struct cli_def *));

/**
 * @brief      function to print all arguments and optional arguments given to a
 *             command by user in the output
 *
 * @param      cli   target cli object
 * @param[in]  text  an arbitrary string which is going to be printed first
 *                   (optional)
 * @param      argv  extra given arguments by user in prompt
 * @param[in]  argc  number of elements in argv
 */
void cli_dump_optargs_and_args(struct cli_def *cli, const char *text, char *argv[], int argc);

/**
 * @brief      function to enable or disable telnet protocol negotiation; this
 *             is enabled by default and must be changed before cli_loop()
 *
 * @note       this is enabled by default and must be changed before cli_loop()
 *             is run
 *
 * @param      cli              target cli object
 * @param[in]  telnet_protocol  if 0 then telnet is disabled; else it is enabled
 */
void cli_telnet_protocol(struct cli_def *cli, int telnet_protocol);

/**
 * @brief      function to set a context in cli object; a context is just a void
 *             pointer place holder for whatever developer wants to store at the
 *             cli main structure; generally it does not mean anything but just
 *             a storage for a void pointer
 *
 * @param      cli      target cli object
 * @param      context  to-be-stored context
 */
void cli_set_context(struct cli_def *cli, void *context);

/**
 * @brief      function to get a stored context set by cli_set_context
 *
 * @param      cli   target cli object
 *
 * @return     previously saved context
 */
void *cli_get_context(struct cli_def *cli);

/**
 * @brief      function to free help texts container object
 *
 * @param      comphelp  help texts container object
 */
void cli_free_comphelp(struct cli_comphelp *comphelp);

/**
 * @brief      function to add a help text into help texts container object
 *
 * @param      comphelp  help texts container object
 * @param[in]  entry     target help
 *
 * @return     CLI_OK or CLI_ERROR
 */
int cli_add_comphelp_entry(struct cli_comphelp *comphelp, const char *entry);

/**
 * @brief      when a command is executed or evaluated, cli can be set to enter
 *             a transient mode
 *
 * @param      cli             target cli object
 * @param[in]  transient_mode  target transient mode
 */
void cli_set_transient_mode(struct cli_def *cli, int transient_mode);

/**
 * @brief      function to register a filter for cli
 *
 * @param      cli        target cli object
 * @param[in]  command    target filter command
 * @param[in]  init       callback function to initialize filter
 * @param[in]  filter     callback function to __do__ the filtering
 * @param[in]  privilege  privilege of filter command
 * @param[in]  mode       mode of filter command
 * @param[in]  help       help of filter command
 *
 * @return     pointer to filter command object
 */
struct cli_command *cli_register_filter(struct cli_def *cli, const char *command,
                                        int (*init)(struct cli_def *cli, int, char **, struct cli_filter *filt),
                                        int (*filter)(struct cli_def *, const char *, void *), int privilege, int mode,
                                        const char *help);

/**
 * @brief      function to unregister a filter command
 *
 * @param      cli      target cli object
 * @param[in]  command  target filter command
 *
 * @return     CLI_OK or CLI_ERROR
 */
int cli_unregister_filter(struct cli_def *cli, const char *command);

/**
 * @brief      function to register an optional argument to a command
 *
 * @param      cmd              target command object
 * @param[in]  name             name of optional argument
 * @param[in]  flags            target flags for the optional argument
 * @param[in]  priviledge       target priviledge for the optional argument
 * @param[in]  mode             target mode for the optional argument
 * @param[in]  help             to-be-shown help for the optional arguement
 * @param[in]  get_completions  callback function which tells how the
 *                              completions for the optional argument should be
 *                              addressed
 * @param[in]  validator        callback function which validates the optional
 *                              argument; validator should check whether the
 *                              entered optional argument has everything
 *                              necessary for execution
 * @param[in]  transient_mode   callback function to be called when cli is in
 *                              transient mode
 *
 * @return     CLI_OK or CLI_ERROR
 */
struct cli_optarg *cli_register_optarg(struct cli_command *cmd, const char *name, int flags, int priviledge, int mode,
                                       const char *help,
                                       int (*get_completions)(struct cli_def *cli, const char *, const char *,
                                                              struct cli_comphelp *),
                                       int (*validator)(struct cli_def *cli, const char *, const char *),
                                       int (*transient_mode)(struct cli_def *, const char *, const char *));

/**
 * @brief      function to add help for an optional argument
 *
 * @param      optarg    target optinal argument object
 * @param[in]  helpname  name of help
 * @param[in]  helptext  text of help
 *
 * @return     CLI_OK or CLI_ERROR
 */
int cli_optarg_addhelp(struct cli_optarg *optarg, const char *helpname, const char *helptext);

/**
 * @brief      function to find an optional argument value; if 'find_after' is
 *             not NULL then first value after 'find_after' is going to be
 *             returned
 *
 * @param      cli         target cli object
 * @param      name        target optional argument name
 * @param      find_after  if 'find_after' is not NULL then first value after
 *                         'find_after' is going to be returned
 *
 * @return     NULL or a value of an optional argument
 */
char *cli_find_optarg_value(struct cli_def *cli, char *name, char *find_after);

/**
 * @brief      function to find get all optional arguments
 *
 * @param      cli   target cli object
 *
 * @return     NULL or a list of optional arguments
 */
struct cli_optarg_pair *cli_get_all_found_optargs(struct cli_def *cli);

/**
 * @brief      function to unregister an optional argument from a command
 *
 * @param      cmd   target command
 * @param[in]  name  target optional argument name
 *
 * @return     CLI_OK or CLI_ERROR
 */
int cli_unregister_optarg(struct cli_command *cmd, const char *name);

/**
 * @brief      function to get value of an optional argument; if 'find_after' is
 *             not NULL then first value after 'find_after' is going to be
 *             returned
 *
 * @param      cli         target cli object
 * @param[in]  name        target optional argument name
 * @param      find_after  if 'find_after' is not NULL then first value after
 *                         'find_after' is going to be returned
 *
 * @return     NULL or a value of an optional argument
 */
char *cli_get_optarg_value(struct cli_def *cli, const char *name, char *find_after);

/**
 * @brief      function to set a value for an optional argument; if
 *             'allow_multiple' is 0 then the found optional argument cannot be
 *             set
 *
 * @param      cli             target cli object
 * @param[in]  name            target optional argument name
 * @param[in]  value           to-be-set value
 * @param[in]  allow_multiple  if 'allow_multiple' is 0 then the found optional
 *                             argument cannot be set
 *
 * @return     CLI_OK or CLI_ERROR
 */
int cli_set_optarg_value(struct cli_def *cli, const char *name, const char *value, int allow_multiple);

/**
 * @brief      function to unregister all optional arguments from a command
 *
 * @param      c     target command object
 */
void cli_unregister_all_optarg(struct cli_command *c);

/**
 * @brief      function to unregister all filters from a cli
 *
 * @param      cli   target cli object
 */
void cli_unregister_all_filters(struct cli_def *cli);

/**
 * @brief      function to unregister all commands from a cli
 *
 * @param      cli   target cli object
 */
void cli_unregister_all_commands(struct cli_def *cli);

/**
 * @brief      function to unregister all instances of a specific command and
 *             its children (in commands tree) from a cli
 *
 * @param      cli      target cli object
 * @param      command  target command
 */
void cli_unregister_all(struct cli_def *cli, struct cli_command *command);

/*
 * Expose some previous internal routines.  Just in case someone was using those
 * with an explicit reference, the original routines (cli_int_*) internally point
 * to the newly public routines.
 */


/**
 * @brief      function to show the help screen
 *
 * @param      cli      target cli object
 * @param[in]  command  this is UNUSED
 * @param      argv     this is UNUSED
 * @param[in]  argc     this is UNUSED
 *
 * @return     always returns CLI_OK
 */
int cli_help(struct cli_def *cli, const char *command, char *argv[], int argc);

/**
 * @brief      function to show the command history screen
 *
 * @param      cli      target cli object
 * @param[in]  command  this is UNUSED
 * @param      argv     this is UNUSED
 * @param[in]  argc     this is UNUSED
 *
 * @return     always returns CLI_OK
 */
int cli_history(struct cli_def *cli, const char *command, char *argv[], int argc);

/**
 * @brief      function to exit from current mode based on mode depth
 *
 * @param      cli      target cli object
 * @param[in]  command  this is UNUSED
 * @param      argv     this is UNUSED
 * @param[in]  argc     this is UNUSED
 *
 * @return     always returns CLI_OK
 */
int cli_exit(struct cli_def *cli, const char *command, char *argv[], int argc);

/**
 * @brief      function to quit the prompt
 *
 * @param      cli      target cli object
 * @param[in]  command  this is UNUSED
 * @param      argv     this is UNUSED
 * @param[in]  argc     this is UNUSED
 *
 * @return     always returns CLI_QUIT
 */
int cli_quit(struct cli_def *cli, const char *command, char *argv[], int argc);

/**
 * @brief      function to activate enable mode
 *
 * @param      cli      target cli object
 * @param[in]  command  this is UNUSED
 * @param      argv     this is UNUSED
 * @param[in]  argc     this is UNUSED
 *
 * @return     always returns CLI_QUIT
 */
int cli_enable(struct cli_def *cli, const char *command, char *argv[], int argc);

/**
 * @brief      function to de-activate enable mode
 *
 * @param      cli      target cli object
 * @param[in]  command  this is UNUSED
 * @param      argv     this is UNUSED
 * @param[in]  argc     this is UNUSED
 *
 * @return     always returns CLI_QUIT
 */
int cli_disable(struct cli_def *cli, const char *command, char *argv[], int argc);

#ifdef __cplusplus
}
#endif

#endif
