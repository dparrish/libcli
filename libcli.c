// vim:sw=2 tw=120 et

#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#if !defined(__APPLE__) && !defined(__FreeBSD__)
#include <malloc.h>
#endif
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifndef WIN32
#include <regex.h>
#endif
#include "libcli.h"

#ifdef __GNUC__
#define UNUSED(d) d __attribute__((unused))
#else
#define UNUSED(d) d
#endif

#define MATCH_REGEX 1
#define MATCH_INVERT 2

#ifdef WIN32
// Stupid windows has multiple namespaces for filedescriptors, with different read/write functions required for each ..
int read(int fd, void *buf, unsigned int count) {
  return recv(fd, buf, count, 0);
}

int write(int fd, const void *buf, unsigned int count) {
  return send(fd, buf, count, 0);
}

int vasprintf(char **strp, const char *fmt, va_list args) {
  int size;
  va_list argCopy;

  // Do initial vsnprintf on a copy of the va_list
  va_copy(argCopy, args);
  size = vsnprintf(NULL, 0, fmt, argCopy);
  va_end(argCopy);
  if ((*strp = malloc(size + 1)) == NULL) {
    return -1;
  }

  size = vsnprintf(*strp, size + 1, fmt, args);
  return size;
}

int asprintf(char **strp, const char *fmt, ...) {
  va_list args;
  int size;

  va_start(args, fmt);
  size = vasprintf(strp, fmt, args);

  va_end(args);
  return size;
}

int fprintf(FILE *stream, const char *fmt, ...) {
  va_list args;
  int size;
  char *buf;

  va_start(args, fmt);
  size = vasprintf(&buf, fmt, args);
  if (size < 0) {
    goto out;
  }
  size = write(stream->_file, buf, size);
  free(buf);

out:
  va_end(args);
  return size;
}

// Dummy definitions to allow compilation on Windows
int regex_dummy() {
  return 0;
};
#define regfree(...) regex_dummy()
#define regexec(...) regex_dummy()
#define regcomp(...) regex_dummy()
#define regex_t int
#define REG_NOSUB 0
#define REG_EXTENDED 0
#define REG_ICASE 0
#endif

enum cli_states {
  STATE_LOGIN,
  STATE_PASSWORD,
  STATE_NORMAL,
  STATE_ENABLE_PASSWORD,
  STATE_ENABLE,
};

struct unp {
  char *username;
  char *password;
  struct unp *next;
};

struct cli_filter_cmds {
  const char *cmd;
  const char *help;
};

// Free and zero (to avoid double-free)
#define free_z(p) \
  do {            \
    if (p) {      \
      free(p);    \
      (p) = 0;    \
    }             \
  } while (0)

// Forward defines of *INTERNAL* library function as static here
static int cli_search_flags_validator(struct cli_def *cli, const char *word, const char *value);
static int cli_match_filter_init(struct cli_def *cli, int argc, char **argv, struct cli_filter *filt);
static int cli_range_filter_init(struct cli_def *cli, int argc, char **argv, struct cli_filter *filt);
static int cli_count_filter_init(struct cli_def *cli, int argc, char **argv, struct cli_filter *filt);
static int cli_match_filter(struct cli_def *cli, const char *string, void *data);
static int cli_range_filter(struct cli_def *cli, const char *string, void *data);
static int cli_count_filter(struct cli_def *cli, const char *string, void *data);
static void cli_int_parse_optargs(struct cli_def *cli, struct cli_pipeline_stage *stage, struct cli_command *cmd,
                                  char lastchar, struct cli_comphelp *comphelp);
static int cli_int_enter_buildmode(struct cli_def *cli, struct cli_pipeline_stage *stage, char *mode_text);
static char *cli_int_buildmode_extend_cmdline(char *, char *word);
static void cli_int_free_buildmode(struct cli_def *cli);
static void cli_free_command(struct cli_def *cli, struct cli_command *cmd);
static int cli_int_unregister_command_core(struct cli_def *cli, const char *command, int command_type);
static int cli_int_unregister_buildmode_command(struct cli_def *cli, const char *command) __attribute__((unused));
static struct cli_command *cli_int_register_buildmode_command(struct cli_def *cli, struct cli_command *parent,
                                                              const char *command,
                                                              int (*callback)(struct cli_def *cli, const char *,
                                                                              char **, int),
                                                              int flags, int privilege, int mode, const char *help);
static void cli_int_buildmode_reset_unset_help(struct cli_def *cli);
static int cli_int_buildmode_cmd_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_flag_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_flag_multiple_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_cancel_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_execute_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_show_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_unset_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_unset_completor(struct cli_def *cli, const char *name, const char *word,
                                             struct cli_comphelp *comphelp);
static int cli_int_buildmode_unset_validator(struct cli_def *cli, const char *name, const char *value);
static int cli_int_execute_buildmode(struct cli_def *cli);
static void cli_int_free_found_optargs(struct cli_optarg_pair **optarg_pair);
static void cli_int_unset_optarg_value(struct cli_def *cli, const char *name);
static struct cli_pipeline *cli_int_generate_pipeline(struct cli_def *cli, const char *command);
static int cli_int_validate_pipeline(struct cli_def *cli, struct cli_pipeline *pipeline);
static int cli_int_execute_pipeline(struct cli_def *cli, struct cli_pipeline *pipeline);
inline void cli_int_show_pipeline(struct cli_def *cli, struct cli_pipeline *pipeline);
static void cli_int_free_pipeline(struct cli_pipeline *pipeline);
static void cli_register_command_core(struct cli_def *cli, struct cli_command *parent, struct cli_command *c);
static void cli_int_wrap_help_line(char *nameptr, char *helpptr, struct cli_comphelp *comphelp);

static char DELIM_OPT_START[] = "[";
static char DELIM_OPT_END[] = "]";
static char DELIM_ARG_START[] = "<";
static char DELIM_ARG_END[] = ">";
static char DELIM_NONE[] = "";

static ssize_t _write(int fd, const void *buf, size_t count) {
  size_t written = 0;
  ssize_t thisTime = 0;
  while (count != written) {
    thisTime = write(fd, (char *)buf + written, count - written);
    if (thisTime == -1) {
      if (errno == EINTR)
        continue;
      else
        return -1;
    }
    written += thisTime;
  }
  return written;
}

char *cli_command_name(struct cli_def *cli, struct cli_command *command) {
  char *name;
  char *o;

  if (cli->commandname) {
    free(cli->commandname);
    cli->commandname = NULL;
  }
  name = cli->commandname;

  if (!(name = calloc(1, 1))) return NULL;

  while (command) {
    o = name;
    if (asprintf(&name, "%s%s%s", command->command, *o ? " " : "", o) == -1) {
      fprintf(stderr, "Couldn't allocate memory for command_name: %s", strerror(errno));
      free(o);
      return NULL;
    }
    command = command->parent;
    free(o);
  }
  cli->commandname = name;
  return name;
}

void cli_set_auth_callback(struct cli_def *cli, int (*auth_callback)(const char *, const char *)) {
  cli->auth_callback = auth_callback;
}

void cli_set_enable_callback(struct cli_def *cli, int (*enable_callback)(const char *)) {
  cli->enable_callback = enable_callback;
}

void cli_allow_user(struct cli_def *cli, const char *username, const char *password) {
  struct unp *u, *n;
  if (!(n = malloc(sizeof(struct unp)))) {
    fprintf(stderr, "Couldn't allocate memory for user: %s", strerror(errno));
    return;
  }
  if (!(n->username = strdup(username))) {
    fprintf(stderr, "Couldn't allocate memory for username: %s", strerror(errno));
    free(n);
    return;
  }
  if (!(n->password = strdup(password))) {
    fprintf(stderr, "Couldn't allocate memory for password: %s", strerror(errno));
    free(n->username);
    free(n);
    return;
  }
  n->next = NULL;

  if (!cli->users) {
    cli->users = n;
  } else {
    for (u = cli->users; u && u->next; u = u->next)
      ;
    if (u) u->next = n;
  }
}

void cli_allow_enable(struct cli_def *cli, const char *password) {
  free_z(cli->enable_password);
  if (!(cli->enable_password = strdup(password))) {
    fprintf(stderr, "Couldn't allocate memory for enable password: %s", strerror(errno));
  }
}

void cli_deny_user(struct cli_def *cli, const char *username) {
  struct unp *u, *p = NULL;
  if (!cli->users) return;
  for (u = cli->users; u; u = u->next) {
    if (strcmp(username, u->username) == 0) {
      if (p)
        p->next = u->next;
      else
        cli->users = u->next;
      free(u->username);
      free(u->password);
      free(u);
      break;
    }
    p = u;
  }
}

void cli_set_banner(struct cli_def *cli, const char *banner) {
  free_z(cli->banner);
  if (banner && *banner) cli->banner = strdup(banner);
}

void cli_set_hostname(struct cli_def *cli, const char *hostname) {
  free_z(cli->hostname);
  if (hostname && *hostname) cli->hostname = strdup(hostname);
}

void cli_set_promptchar(struct cli_def *cli, const char *promptchar) {
  free_z(cli->promptchar);
  cli->promptchar = strdup(promptchar);
}

static int cli_build_shortest(struct cli_def *cli, struct cli_command *commands) {
  struct cli_command *c, *p;
  char *cp, *pp;
  unsigned len;

  for (c = commands; c; c = c->next) {
    c->unique_len = strlen(c->command);
    if ((c->mode != MODE_ANY && c->mode != cli->mode) || c->privilege > cli->privilege) continue;

    c->unique_len = 1;
    for (p = commands; p; p = p->next) {
      if (c == p) continue;
      if (c->command_type != p->command_type) continue;
      if ((p->mode != MODE_ANY && p->mode != cli->mode) || p->privilege > cli->privilege) continue;

      cp = c->command;
      pp = p->command;
      len = 1;

      while (*cp && *pp && *cp++ == *pp++) len++;

      if (len > c->unique_len) c->unique_len = len;
    }

    if (c->children) cli_build_shortest(cli, c->children);
  }

  return CLI_OK;
}

int cli_set_privilege(struct cli_def *cli, int priv) {
  int old = cli->privilege;
  cli->privilege = priv;

  if (priv != old) {
    cli_set_promptchar(cli, priv == PRIVILEGE_PRIVILEGED ? "# " : "> ");
    cli_build_shortest(cli, cli->commands);
  }

  return old;
}

void cli_set_modestring(struct cli_def *cli, const char *modestring) {
  free_z(cli->modestring);
  if (modestring) cli->modestring = strdup(modestring);
}

int cli_set_configmode(struct cli_def *cli, int mode, const char *config_desc) {
  int old = cli->mode;
  cli->mode = mode;

  if (mode != old) {
    if (!cli->mode) {
      // Not config mode
      cli_set_modestring(cli, NULL);
    } else if (config_desc && *config_desc) {
      char string[64];
      snprintf(string, sizeof(string), "(config-%s)", config_desc);
      cli_set_modestring(cli, string);
    } else {
      cli_set_modestring(cli, "(config)");
    }

    cli_build_shortest(cli, cli->commands);
  }

  return old;
}

void cli_register_command_core(struct cli_def *cli, struct cli_command *parent, struct cli_command *c) {
  struct cli_command *p = NULL;

  if (!c) return;

  c->parent = parent;

  /*
   * Figure out we have a chain, or would be the first element on it.
   * If we'd be the first element, assign as such.
   * Otherwise find the lead element so we can trace it below.
   */

  if (parent) {
    if (!parent->children) {
      parent->children = c;
    } else {
      p = parent->children;
    }
  } else {
    if (!cli->commands) {
      cli->commands = c;
    } else {
      p = cli->commands;
    }
  }

  /*
   * If we have a chain (p is not null), run down to the last element and place this command at the end
   */
  for (; p && p->next; p = p->next)
    ;

  if (p) {
    p->next = c;
    c->previous = p;
  }
  return;
}

struct cli_command *cli_register_command(struct cli_def *cli, struct cli_command *parent, const char *command,
                                         int (*callback)(struct cli_def *cli, const char *, char **, int),
                                         int privilege, int mode, const char *help) {
  struct cli_command *c;

  if (!command) return NULL;
  if (!(c = calloc(sizeof(struct cli_command), 1))) return NULL;
  c->command_type = CLI_REGULAR_COMMAND;
  c->callback = callback;
  c->next = NULL;
  if (!(c->command = strdup(command))) {
    free(c);
    return NULL;
  }

  c->privilege = privilege;
  c->mode = mode;
  if (help && !(c->help = strdup(help))) {
    free(c->command);
    free(c);
    return NULL;
  }

  cli_register_command_core(cli, parent, c);
  return c;
}

static void cli_free_command(struct cli_def *cli, struct cli_command *cmd) {
  struct cli_command *c, *p;

  for (c = cmd->children; c;) {
    p = c->next;
    cli_free_command(cli, c);
    c = p;
  }

  free(cmd->command);
  if (cmd->help) free(cmd->help);
  if (cmd->optargs) cli_unregister_all_optarg(cmd);

  /*
   * Ok, update the pointers of anyone who pointed to us.
   * We have 3 pointers to worry about - parent, previous, and next.
   * We don't have to worry about children since they've been cleared above.
   * If both cli->command points to us we need to update cli->command to point to whatever command is 'next'.
   * Otherwise ensure that any item before/behind us points around us.
   *
   * Important - there is no provision for deleting a discrete subcommand.
   * For example, suppose we define foo, then bar with foo as the parent, then baz with bar as the parent.  We cannot
   * delete 'bar' and have a new chain of foo -> baz.
   * The above freeing of children prevents this in the first place.
   */

  if (cmd == cli->commands) {
    cli->commands = cmd->next;
    if (cmd->next) {
      cmd->next->parent = NULL;
      cmd->next->previous = NULL;
    }
  } else {
    if (cmd->previous) {
      cmd->previous->next = cmd->next;
    }
    if (cmd->next) {
      cmd->next->previous = cmd->previous;
    }
  }
  free(cmd);
}

int cli_int_unregister_command_core(struct cli_def *cli, const char *command, int command_type) {
  struct cli_command *c, *p = NULL;

  if (!command) return -1;
  if (!cli->commands) return CLI_OK;

  for (c = cli->commands; c;) {
    p = c->next;
    if (strcmp(c->command, command) == 0 && c->command_type == command_type) {
      cli_free_command(cli, c);
      return CLI_OK;
    }
    c = p;
  }

  return CLI_OK;
}

int cli_unregister_command(struct cli_def *cli, const char *command) {
  return cli_int_unregister_command_core(cli, command, CLI_REGULAR_COMMAND);
}

int cli_show_help(struct cli_def *cli, struct cli_command *c) {
  struct cli_command *p;

  for (p = c; p; p = p->next) {
    if (p->command && p->callback && cli->privilege >= p->privilege && (p->mode == cli->mode || p->mode == MODE_ANY)) {
      cli_error(cli, "  %-20s %s", cli_command_name(cli, p), (p->help != NULL ? p->help : ""));
    }

    if (p->children) cli_show_help(cli, p->children);
  }

  return CLI_OK;
}

int cli_enable(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  if (cli->privilege == PRIVILEGE_PRIVILEGED) return CLI_OK;

  if (!cli->enable_password && !cli->enable_callback) {
    // No password required, set privilege immediately.
    cli_set_privilege(cli, PRIVILEGE_PRIVILEGED);
    cli_set_configmode(cli, MODE_EXEC, NULL);
  } else {
    // Require password entry
    cli->state = STATE_ENABLE_PASSWORD;
  }

  return CLI_OK;
}

int cli_disable(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
  cli_set_configmode(cli, MODE_EXEC, NULL);
  return CLI_OK;
}

int cli_help(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  cli_error(cli, "\nCommands available:");
  cli_show_help(cli, cli->commands);
  return CLI_OK;
}

int cli_history(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  int i;

  cli_error(cli, "\nCommand history:");
  for (i = 0; i < MAX_HISTORY; i++) {
    if (cli->history[i]) cli_error(cli, "%3d. %s", i, cli->history[i]);
  }

  return CLI_OK;
}

int cli_quit(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
  cli_set_configmode(cli, MODE_EXEC, NULL);
  return CLI_QUIT;
}

int cli_exit(struct cli_def *cli, const char *command, char *argv[], int argc) {
  if (cli->mode == MODE_EXEC) return cli_quit(cli, command, argv, argc);

  if (cli->mode > MODE_CONFIG)
    cli_set_configmode(cli, MODE_CONFIG, NULL);
  else
    cli_set_configmode(cli, MODE_EXEC, NULL);

  cli->service = NULL;
  return CLI_OK;
}

int cli_int_idle_timeout(struct cli_def *cli) {
  cli_print(cli, "Idle timeout");
  return CLI_QUIT;
}

int cli_int_configure_terminal(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]),
                               UNUSED(int argc)) {
  cli_set_configmode(cli, MODE_CONFIG, NULL);
  return CLI_OK;
}

struct cli_def *cli_init() {
  struct cli_def *cli;
  struct cli_command *c;

  if (!(cli = calloc(sizeof(struct cli_def), 1))) return 0;

  cli->buf_size = 1024;
  if (!(cli->buffer = calloc(cli->buf_size, 1))) {
    free_z(cli);
    return 0;
  }
  cli->telnet_protocol = 1;

  cli_register_command(cli, 0, "help", cli_help, PRIVILEGE_UNPRIVILEGED, MODE_ANY, "Show available commands");
  cli_register_command(cli, 0, "quit", cli_quit, PRIVILEGE_UNPRIVILEGED, MODE_ANY, "Disconnect");
  cli_register_command(cli, 0, "logout", cli_quit, PRIVILEGE_UNPRIVILEGED, MODE_ANY, "Disconnect");
  cli_register_command(cli, 0, "exit", cli_exit, PRIVILEGE_UNPRIVILEGED, MODE_ANY, "Exit from current mode");
  cli_register_command(cli, 0, "history", cli_history, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                       "Show a list of previously run commands");
  cli_register_command(cli, 0, "enable", cli_enable, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Turn on privileged commands");
  cli_register_command(cli, 0, "disable", cli_disable, PRIVILEGE_PRIVILEGED, MODE_EXEC, "Turn off privileged commands");

  c = cli_register_command(cli, 0, "configure", 0, PRIVILEGE_PRIVILEGED, MODE_EXEC, "Enter configuration mode");
  cli_register_command(cli, c, "terminal", cli_int_configure_terminal, PRIVILEGE_PRIVILEGED, MODE_EXEC,
                       "Conlfigure from the terminal");

  // And now the built in filters
  c = cli_register_filter(cli, "begin", cli_range_filter_init, cli_range_filter, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                          "Begin with lines that match");
  cli_register_optarg(c, "range_start", CLI_CMD_ARGUMENT | CLI_CMD_REMAINDER_OF_LINE, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                      "Begin showing lines that match", NULL, NULL, NULL);

  c = cli_register_filter(cli, "between", cli_range_filter_init, cli_range_filter, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                          "Between lines that match");
  cli_register_optarg(c, "range_start", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                      "Begin showing lines that match", NULL, NULL, NULL);
  cli_register_optarg(c, "range_end", CLI_CMD_ARGUMENT | CLI_CMD_REMAINDER_OF_LINE, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                      "Stop showing lines that match", NULL, NULL, NULL);

  cli_register_filter(cli, "count", cli_count_filter_init, cli_count_filter, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                      "Count of lines");

  c = cli_register_filter(cli, "exclude", cli_match_filter_init, cli_match_filter, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                          "Exclude lines that match");
  cli_register_optarg(c, "search_pattern", CLI_CMD_ARGUMENT | CLI_CMD_REMAINDER_OF_LINE, PRIVILEGE_UNPRIVILEGED,
                      MODE_ANY, "Search pattern", NULL, NULL, NULL);

  c = cli_register_filter(cli, "include", cli_match_filter_init, cli_match_filter, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                          "Include lines that match");
  cli_register_optarg(c, "search_pattern", CLI_CMD_ARGUMENT | CLI_CMD_REMAINDER_OF_LINE, PRIVILEGE_UNPRIVILEGED,
                      MODE_ANY, "Search pattern", NULL, NULL, NULL);

  c = cli_register_filter(cli, "grep", cli_match_filter_init, cli_match_filter, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                          "Include lines that match regex (options: -v, -i, -e)");
  cli_register_optarg(c, "search_flags", CLI_CMD_HYPHENATED_OPTION, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                      "Search flags (-[ivx]", NULL, cli_search_flags_validator, NULL);
  cli_register_optarg(c, "search_pattern", CLI_CMD_ARGUMENT | CLI_CMD_REMAINDER_OF_LINE, PRIVILEGE_UNPRIVILEGED,
                      MODE_ANY, "Search pattern", NULL, NULL, NULL);

  c = cli_register_filter(cli, "egrep", cli_match_filter_init, cli_match_filter, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                          "Include lines that match extended regex");
  cli_register_optarg(c, "search_flags", CLI_CMD_HYPHENATED_OPTION, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                      "Search flags (-[ivx]", NULL, cli_search_flags_validator, NULL);
  cli_register_optarg(c, "search_pattern", CLI_CMD_ARGUMENT | CLI_CMD_REMAINDER_OF_LINE, PRIVILEGE_UNPRIVILEGED,
                      MODE_ANY, "Search pattern", NULL, NULL, NULL);

  cli->privilege = cli->mode = -1;
  cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
  cli_set_configmode(cli, MODE_EXEC, 0);

  // Default to 1 second timeout intervals
  cli->timeout_tm.tv_sec = 1;
  cli->timeout_tm.tv_usec = 0;

  // Set default idle timeout callback, but no timeout
  cli_set_idle_timeout_callback(cli, 0, cli_int_idle_timeout);
  return cli;
}

void cli_unregister_tree(struct cli_def *cli, struct cli_command *command, int command_type) {
  struct cli_command *c, *p = NULL;

  if (!command) command = cli->commands;

  for (c = command; c;) {
    p = c->next;
    if (c->command_type == command_type || command_type == CLI_ANY_COMMAND) {
      if (c == cli->commands) cli->commands = c->next;
      // Unregister all child commands
      cli_free_command(cli, c);
    }
    c = p;
  }
}

void cli_unregister_all(struct cli_def *cli, struct cli_command *command) {
  cli_unregister_tree(cli, command, CLI_REGULAR_COMMAND);
}

int cli_done(struct cli_def *cli) {
  if (!cli) return CLI_OK;
  struct unp *u = cli->users, *n;

  cli_free_history(cli);

  // Free all users
  while (u) {
    if (u->username) free(u->username);
    if (u->password) free(u->password);
    n = u->next;
    free(u);
    u = n;
  }

  if (cli->buildmode) cli_int_free_buildmode(cli);
  cli_unregister_tree(cli, cli->commands, CLI_ANY_COMMAND);
  free_z(cli->commandname);
  free_z(cli->modestring);
  free_z(cli->banner);
  free_z(cli->promptchar);
  free_z(cli->hostname);
  free_z(cli->buffer);
  free_z(cli);

  return CLI_OK;
}

static int cli_add_history(struct cli_def *cli, const char *cmd) {
  int i;
  for (i = 0; i < MAX_HISTORY; i++) {
    if (!cli->history[i]) {
      if (i == 0 || strcasecmp(cli->history[i - 1], cmd))
        if (!(cli->history[i] = strdup(cmd))) return CLI_ERROR;
      return CLI_OK;
    }
  }
  // No space found, drop one off the beginning of the list
  free(cli->history[0]);
  for (i = 0; i < MAX_HISTORY - 1; i++) cli->history[i] = cli->history[i + 1];
  if (!(cli->history[MAX_HISTORY - 1] = strdup(cmd))) return CLI_ERROR;
  return CLI_OK;
}

void cli_free_history(struct cli_def *cli) {
  int i;
  for (i = 0; i < MAX_HISTORY; i++) {
    if (cli->history[i]) free_z(cli->history[i]);
  }
}

static char *cli_int_return_newword(const char *start, const char *end) {
  int len = end - start;
  char *to = NULL;
  char *newword = NULL;

  // allocate space (including terminal NULL, then go through and deal with escaping characters as we copy them

  if (!(newword = calloc(len + 1, 1))) return 0;
  to = newword;
  while (start != end) {
    if (*start == '\\')
      start++;
    else
      *to++ = *start++;
  }
  return newword;
}

static int cli_parse_line(const char *line, char *words[], int max_words) {
  int nwords = 0;
  const char *p = line;
  const char *word_start = 0;
  int inquote = 0;

  while (*p) {
    if (!isspace(*p)) {
      word_start = p;
      break;
    }
    p++;
  }

  while (nwords < max_words - 1) {
    if (*p == '\\' && *(p + 1)) {
      p += 2;
    }

    /*
     * a 'word' terminates at:
     *   - end-of-string, whitespace (if not inside quotes)
     *   - start of quoted section (if word_start != NULL)
     *   - end of a quoted section
     *   - whitespace/pipe unless inside quotes
     */

    if (!*p || *p == inquote || (word_start && !inquote && (isspace(*p) || *p == '|'))) {
      // if we have a word start, extract from there to this character dealing with escapes
      if (word_start) {
        if (!(words[nwords++] = cli_int_return_newword(word_start, p))) return 0;
      }

      // now figure out how to proceed

      // if at end_of_string we're done
      if (!*p) break;

      // found matching quote, eat it
      if (inquote) p++;  // Skip over trailing quote if we have one
      inquote = 0;
      word_start = 0;
    } else if (!inquote && (*p == '"' || *p == '\'')) {
      if (word_start && word_start != p) {
        if (!(words[nwords++] = cli_int_return_newword(word_start, p))) return 0;
      }
      inquote = *p++;
      word_start = p;
    } else {
      if (!word_start) {
        if (*p == '|') {
          if (!(words[nwords++] = strdup("|"))) return 0;
        } else if (!isspace(*p))
          word_start = p;
      }

      p++;
    }
  }

  return nwords;
}

static char *join_words(int argc, char **argv) {
  char *p;
  int len = 0;
  int i;

  for (i = 0; i < argc; i++) {
    if (i) len += 1;

    len += strlen(argv[i]);
  }

  p = malloc(len + 1);
  p[0] = 0;

  for (i = 0; i < argc; i++) {
    if (i) strcat(p, " ");

    strcat(p, argv[i]);
  }

  return p;
}

int cli_run_command(struct cli_def *cli, const char *command) {
  int rc = CLI_ERROR;
  struct cli_pipeline *pipeline;

  // Split command into pipeline stages
  pipeline = cli_int_generate_pipeline(cli, command);

  // cli_int_validate_pipeline will deal with buildmode command setup, and return CLI_BUILDMODE_START if found.
  if (pipeline) rc = cli_int_validate_pipeline(cli, pipeline);

  if (rc == CLI_OK) {
    rc = cli_int_execute_pipeline(cli, pipeline);
  }
  cli_int_free_pipeline(pipeline);
  return rc;
}

void cli_get_completions(struct cli_def *cli, const char *command, char lastchar, struct cli_comphelp *comphelp) {
  struct cli_command *c = NULL;
  struct cli_command *n = NULL;

  int i;
  int command_type;
  struct cli_pipeline *pipeline = NULL;
  struct cli_pipeline_stage *stage;
  char *delim_start = DELIM_NONE;
  char *delim_end = DELIM_NONE;

  if (!(pipeline = cli_int_generate_pipeline(cli, command))) goto out;

  stage = &pipeline->stage[pipeline->num_stages - 1];

  // Check to see if either *no* input, or if the lastchar is a tab.
  if ((!stage->words[0] || (command[strlen(command) - 1] == ' ')) && (stage->words[stage->num_words - 1]))
    stage->num_words++;

  if (cli->buildmode)
    command_type = CLI_BUILDMODE_COMMAND;
  else if (pipeline->num_stages == 1)
    command_type = CLI_REGULAR_COMMAND;
  else
    command_type = CLI_FILTER_COMMAND;

  for (c = cli->commands, i = 0; c && i < stage->num_words; c = n) {
    char *strptr = NULL;
    char *nameptr = NULL;
    n = c->next;

    if (c->command_type != command_type) continue;
    if (cli->privilege < c->privilege) continue;
    if (c->mode != cli->mode && c->mode != MODE_ANY) continue;
    if (stage->words[i] && strncasecmp(c->command, stage->words[i], strlen(stage->words[i]))) continue;

    // Special case for 'buildmode' - skip if the argument for this command was seen, unless MULTIPLE flag is set
    if (cli->buildmode) {
      struct cli_optarg *optarg;
      for (optarg = cli->buildmode->command->optargs; optarg; optarg = optarg->next) {
        if (!strcmp(optarg->name, c->command)) break;
      }
      if (optarg && cli_find_optarg_value(cli, optarg->name, NULL) && !(optarg->flags & (CLI_CMD_OPTION_MULTIPLE)))
        continue;
    }
    if (i < stage->num_words - 1) {
      if (stage->words[i] && (strlen(stage->words[i]) < c->unique_len) && strcmp(stage->words[i], c->command)) continue;

      n = c->children;

      // If we have no more children, we've matched the *command* - remember this
      if (!c->children) break;

      i++;
      continue;
    }

    if (lastchar == '?') {
      delim_start = DELIM_NONE;
      delim_end = DELIM_NONE;

      // Note that buildmode commands need to see if that command is some optinal value

      if (command_type == CLI_BUILDMODE_COMMAND) {
        if (c->flags & (CLI_CMD_OPTIONAL_FLAG | CLI_CMD_OPTIONAL_ARGUMENT)) {
          delim_start = DELIM_OPT_START;
          delim_end = DELIM_OPT_END;
        }
      }
      if (asprintf(&nameptr, "%s%s%s", delim_start, c->command, delim_end) != -1) {
        if (asprintf(&strptr, "  %-20s", nameptr) != -1) {
          cli_int_wrap_help_line(strptr, c->help, comphelp);
          free_z(strptr);
        }
        free(nameptr);
      }
    } else {
      cli_add_comphelp_entry(comphelp, c->command);
    }
  }

out:
  if (c) {
    // Advance past first word of stage
    i++;
    stage->command = c;
    stage->first_unmatched = i;
    if (c->optargs) {
      cli_int_parse_optargs(cli, stage, c, lastchar, comphelp);
    } else if (lastchar == '?') {
      // Special case for getting help with no defined optargs....
      comphelp->num_entries = -1;
    }
    if  (stage->status) {
      // if we had an error here we need to redraw the commandline 
      cli_reprompt(cli);
    }
  }

  cli_int_free_pipeline(pipeline);
}

static void cli_clear_line(int sockfd, char *cmd, int l, int cursor) {
  // Use cmd as our buffer, and overwrite contents as needed.
  // Backspace to beginning
  memset((char *)cmd, '\b', cursor);
  _write(sockfd, cmd, cursor);

  // Overwrite existing cmd with spaces
  memset((char *)cmd, ' ', l);
  _write(sockfd, cmd, l);

  // ..and backspace again to beginning
  memset((char *)cmd, '\b', l);
  _write(sockfd, cmd, l);

  // Null cmd buffer
  memset((char *)cmd, 0, l);
}

void cli_reprompt(struct cli_def *cli) {
  if (!cli) return;
  cli->showprompt = 1;
}

void cli_regular(struct cli_def *cli, int (*callback)(struct cli_def *cli)) {
  if (!cli) return;
  cli->regular_callback = callback;
}

void cli_regular_interval(struct cli_def *cli, int seconds) {
  if (seconds < 1) seconds = 1;
  cli->timeout_tm.tv_sec = seconds;
  cli->timeout_tm.tv_usec = 0;
}

#define DES_PREFIX "{crypt}"  // To distinguish clear text from DES crypted
#define MD5_PREFIX "$1$"

static int pass_matches(const char *pass, const char *attempt) {
  int des;
  if ((des = !strncasecmp(pass, DES_PREFIX, sizeof(DES_PREFIX) - 1))) pass += sizeof(DES_PREFIX) - 1;

#ifndef WIN32
  // TODO(dparrish): Find a small crypt(3) function for use on windows
  if (des || !strncmp(pass, MD5_PREFIX, sizeof(MD5_PREFIX) - 1)) attempt = crypt(attempt, pass);
#endif

  return !strcmp(pass, attempt);
}

#define CTRL(c) (c - '@')

static int show_prompt(struct cli_def *cli, int sockfd) {
  int len = 0;

  if (cli->hostname) len += write(sockfd, cli->hostname, strlen(cli->hostname));

  if (cli->modestring) len += write(sockfd, cli->modestring, strlen(cli->modestring));
  if (cli->buildmode) {
    len += write(sockfd, "[", 1);
    len += write(sockfd, cli->buildmode->cname, strlen(cli->buildmode->cname));
    len += write(sockfd, "...", 3);
    if (cli->buildmode->mode_text) len += write(sockfd, cli->buildmode->mode_text, strlen(cli->buildmode->mode_text));
    len += write(sockfd, "]", 1);
  }
  return len + write(sockfd, cli->promptchar, strlen(cli->promptchar));
}

int cli_loop(struct cli_def *cli, int sockfd) {
  int n, l, oldl = 0, is_telnet_option = 0, skip = 0, esc = 0, cursor = 0;
  char *cmd = NULL, *oldcmd = 0;
  char *username = NULL, *password = NULL;

  cli_build_shortest(cli, cli->commands);
  cli->state = STATE_LOGIN;

  cli_free_history(cli);
  if (cli->telnet_protocol) {
    static const char *negotiate =
        "\xFF\xFB\x03"
        "\xFF\xFB\x01"
        "\xFF\xFD\x03"
        "\xFF\xFD\x01";
    _write(sockfd, negotiate, strlen(negotiate));
  }

  if ((cmd = malloc(CLI_MAX_LINE_LENGTH)) == NULL) return CLI_ERROR;

#ifdef WIN32
  /*
   * OMG, HACK
   */
  if (!(cli->client = fdopen(_open_osfhandle(sockfd, 0), "w+"))) return CLI_ERROR;
  cli->client->_file = sockfd;
#else
  if (!(cli->client = fdopen(sockfd, "w+"))) {
    free(cmd);
    return CLI_ERROR;
  }
#endif

  setbuf(cli->client, NULL);
  if (cli->banner) cli_error(cli, "%s", cli->banner);

  // Set the last action now so we don't time immediately
  if (cli->idle_timeout) time(&cli->last_action);

  // Start off in unprivileged mode
  cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
  cli_set_configmode(cli, MODE_EXEC, NULL);

  // No auth required?
  if (!cli->users && !cli->auth_callback) cli->state = STATE_NORMAL;

  while (1) {
    signed int in_history = 0;
    unsigned char lastchar = '\0';
    unsigned char c = '\0';
    struct timeval tm;

    cli->showprompt = 1;

    if (oldcmd) {
      l = cursor = oldl;
      oldcmd[l] = 0;
      cli->showprompt = 1;
      oldcmd = NULL;
      oldl = 0;
    } else {
      memset(cmd, 0, CLI_MAX_LINE_LENGTH);
      l = 0;
      cursor = 0;
    }

    memcpy(&tm, &cli->timeout_tm, sizeof(tm));

    while (1) {
      int sr;
      fd_set r;

      /*
       * Ensure our transient mode is reset to the starting mode on *each* loop traversal transient mode is valid only
       * while a command is being evaluated/executed.  Also explicitly set the disallow_buildmode flag based on whether
       * or not cli->buildmode is NULL or not.  The cli->buildmode flag can be changed during process, but the
       * enable/disable needs to be set before any processing is entered.
       */
      cli->transient_mode = cli->mode;
      cli->disallow_buildmode = (cli->buildmode) ? 1 : 0;

      if (cli->showprompt) {
        if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) _write(sockfd, "\r\n", 2);

        switch (cli->state) {
          case STATE_LOGIN:
            _write(sockfd, "Username: ", strlen("Username: "));
            break;

          case STATE_PASSWORD:
            _write(sockfd, "Password: ", strlen("Password: "));
            break;

          case STATE_NORMAL:
          case STATE_ENABLE:
            show_prompt(cli, sockfd);
            _write(sockfd, cmd, l);
            if (cursor < l) {
              int n = l - cursor;
              while (n--) _write(sockfd, "\b", 1);
            }
            break;

          case STATE_ENABLE_PASSWORD:
            _write(sockfd, "Password: ", strlen("Password: "));
            break;
        }

        cli->showprompt = 0;
      }

      FD_ZERO(&r);
      FD_SET(sockfd, &r);

      if ((sr = select(sockfd + 1, &r, NULL, NULL, &tm)) < 0) {
        if (errno == EINTR) continue;
        perror("select");
        l = -1;
        break;
      }

      if (sr == 0) {
        // Timeout every second
        if (cli->regular_callback && cli->regular_callback(cli) != CLI_OK) {
          l = -1;
          break;
        }

        if (cli->idle_timeout) {
          if (time(NULL) - cli->last_action >= cli->idle_timeout) {
            if (cli->idle_timeout_callback) {
              // Call the callback and continue on if successful
              if (cli->idle_timeout_callback(cli) == CLI_OK) {
                // Reset the idle timeout counter
                time(&cli->last_action);
                continue;
              }
            }
            // Otherwise, break out of the main loop
            l = -1;
            break;
          }
        }

        memcpy(&tm, &cli->timeout_tm, sizeof(tm));
        continue;
      }

      if ((n = read(sockfd, &c, 1)) < 0) {
        if (errno == EINTR) continue;

        perror("read");
        l = -1;
        break;
      }

      if (cli->idle_timeout) time(&cli->last_action);

      if (n == 0) {
        l = -1;
        break;
      }

      if (skip) {
        skip--;
        continue;
      }

      if (c == 255 && !is_telnet_option) {
        is_telnet_option++;
        continue;
      }

      if (is_telnet_option) {
        if (c >= 251 && c <= 254) {
          is_telnet_option = c;
          continue;
        }

        if (c != 255) {
          is_telnet_option = 0;
          continue;
        }

        is_telnet_option = 0;
      }

      // Handle ANSI arrows
      if (esc) {
        if (esc == '[') {
          // Remap to readline control codes
          switch (c) {
            case 'A':  // Up
              c = CTRL('P');
              break;

            case 'B':  // Down
              c = CTRL('N');
              break;

            case 'C':  // Right
              c = CTRL('F');
              break;

            case 'D':  // Left
              c = CTRL('B');
              break;

            default:
              c = 0;
          }

          esc = 0;
        } else {
          esc = (c == '[') ? c : 0;
          continue;
        }
      }

      if (c == 0) continue;
      if (c == '\n') continue;

      if (c == '\r') {
        if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) _write(sockfd, "\r\n", 2);
        break;
      }

      if (c == 27) {
        esc = 1;
        continue;
      }

      if (c == CTRL('C')) {
        _write(sockfd, "\a", 1);
        continue;
      }

      // Back word, backspace/delete
      if (c == CTRL('W') || c == CTRL('H') || c == 0x7f) {
        int back = 0;

        if (c == CTRL('W')) {
          // Word
          int nc = cursor;

          if (l == 0 || cursor == 0) continue;

          while (nc && cmd[nc - 1] == ' ') {
            nc--;
            back++;
          }

          while (nc && cmd[nc - 1] != ' ') {
            nc--;
            back++;
          }
        } else {
          // Char
          if (l == 0 || cursor == 0) {
            _write(sockfd, "\a", 1);
            continue;
          }

          back = 1;
        }

        if (back) {
          while (back--) {
            if (l == cursor) {
              cmd[--cursor] = 0;
              if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) _write(sockfd, "\b \b", 3);
            } else {
              int i;
              if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) {
                // Back up one space, then write current buffer followed by a space
                _write(sockfd, "\b", 1);
                _write(sockfd, cmd + cursor, l - cursor);
                _write(sockfd, " ", 1);

                // Move everything one char left
                memmove(cmd + cursor - 1, cmd + cursor, l - cursor);

                // Set former last char to null
                cmd[l - 1] = 0;

                // And reposition cursor
                for (i = l; i >= cursor; i--) _write(sockfd, "\b", 1);
              }
              cursor--;
            }
            l--;
          }

          continue;
        }
      }

      // Redraw
      if (c == CTRL('L')) {
        int i;
        int cursorback = l - cursor;

        if (cli->state == STATE_PASSWORD || cli->state == STATE_ENABLE_PASSWORD) continue;

        _write(sockfd, "\r\n", 2);
        show_prompt(cli, sockfd);
        _write(sockfd, cmd, l);

        for (i = 0; i < cursorback; i++) _write(sockfd, "\b", 1);

        continue;
      }

      // Clear line
      if (c == CTRL('U')) {
        if (cli->state == STATE_PASSWORD || cli->state == STATE_ENABLE_PASSWORD)
          memset(cmd, 0, l);
        else
          cli_clear_line(sockfd, cmd, l, cursor);

        l = cursor = 0;
        continue;
      }

      // Kill to EOL
      if (c == CTRL('K')) {
        if (cursor == l) continue;

        if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) {
          int cptr;
          for (cptr = cursor; cptr < l; cptr++) _write(sockfd, " ", 1);

          for (cptr = cursor; cptr < l; cptr++) _write(sockfd, "\b", 1);
        }

        memset(cmd + cursor, 0, l - cursor);
        l = cursor;
        continue;
      }

      // EOT
      if (c == CTRL('D')) {
        if (cli->state == STATE_PASSWORD || cli->state == STATE_ENABLE_PASSWORD) break;

        if (l) continue;

        l = -1;
        break;
      }

      // Disable
      if (c == CTRL('Z')) {
        if (cli->mode != MODE_EXEC) {
          if (cli->buildmode) cli_int_free_buildmode(cli);
          cli_clear_line(sockfd, cmd, l, cursor);
          cli_set_configmode(cli, MODE_EXEC, NULL);
          l = cursor = 0;
          cli->showprompt = 1;
        }

        continue;
      }

      // TAB completion
      if (c == CTRL('I')) {
        struct cli_comphelp comphelp = {0};

        if (cli->state == STATE_LOGIN || cli->state == STATE_PASSWORD || cli->state == STATE_ENABLE_PASSWORD) continue;
        if (cursor != l) continue;

        cli_get_completions(cli, cmd, c, &comphelp);
        if (comphelp.num_entries == 0) {
          _write(sockfd, "\a", 1);
        } else if (lastchar == CTRL('I')) {
          // Double tab
          int i;
          for (i = 0; i < comphelp.num_entries; i++) {
            if (i % 4 == 0)
              _write(sockfd, "\r\n", 2);
            else
              _write(sockfd, " ", 1);
            _write(sockfd, comphelp.entries[i], strlen(comphelp.entries[i]));
          }
          _write(sockfd, "\r\n", 2);
          cli->showprompt = 1;
        } else if (comphelp.num_entries == 1) {
          // Single completion - show *unless* the optional/required 'prefix' is present
          if (comphelp.entries[0][0] != '[' && comphelp.entries[0][0] != '<') {
            for (; l > 0; l--, cursor--) {
              if (cmd[l - 1] == ' ' || cmd[l - 1] == '|' || (comphelp.comma_separated && cmd[l - 1] == ',')) break;
              _write(sockfd, "\b", 1);
            }
            strcpy((cmd + l), comphelp.entries[0]);
            l += strlen(comphelp.entries[0]);
            cmd[l++] = ' ';
            cursor = l;
            _write(sockfd, comphelp.entries[0], strlen(comphelp.entries[0]));
            _write(sockfd, " ", 1);
            // And now forget the tab, since we just found a single match
            lastchar = '\0';
          } else {
            // Yes, we had a match, but it wasn't required - remember the tab in case the user double tabs....
            lastchar = CTRL('I');
          }
        } else if (comphelp.num_entries > 1) {
          /*
           * More than one completion.
           * Show as many characters as we can until the completions start to differ.
           */
          lastchar = c;
          int i, j, k = 0;
          char *tptr = comphelp.entries[0];

          /*
           * Quickly try to see where our entries differ.
           * Corner cases:
           * - If all entries are optional, don't show *any* options unless user has provided a letter.
           * - If any entry starts with '<' then don't fill in anything.
           */

          // Skip a leading '['
          k = strlen(tptr);
          if (*tptr == '[')
            tptr++;
          else if (*tptr == '<')
            k = 0;

          for (i = 1; k != 0 && i < comphelp.num_entries; i++) {
            char *wptr = comphelp.entries[i];

            if (*wptr == '[')
              wptr++;
            else if (*wptr == '<')
              k = 0;

            for (j = 0; (j < k) && (j < (int)strlen(wptr)); j++) {
              if (strncasecmp(tptr + j, wptr + j, 1)) break;
            }
            k = j;
          }

          // Try to show minimum match string if we have a non-zero k and the first letter of the last word is not '['.
          if (k && comphelp.entries[comphelp.num_entries - 1][0] != '[') {
            for (; l > 0; l--, cursor--) {
              if (cmd[l - 1] == ' ' || cmd[l - 1] == '|' || (comphelp.comma_separated && cmd[l - 1] == ',')) break;
              _write(sockfd, "\b", 1);
            }
            strncpy(cmd + l, tptr, k);
            l += k;
            cursor = l;
            _write(sockfd, tptr, k);

          } else {
            _write(sockfd, "\a", 1);
          }
        }
        cli_free_comphelp(&comphelp);
        continue;
      }

      // '?' at end of line - generate applicable 'help' messages
      if (c == '?' && cursor == l) {
        struct cli_comphelp comphelp = {0};
        int i;
        int show_cr = 1;

        if (cli->state == STATE_LOGIN || cli->state == STATE_PASSWORD || cli->state == STATE_ENABLE_PASSWORD) continue;
        if (cursor != l) continue;

        cli_get_completions(cli, cmd, c, &comphelp);
        if (comphelp.num_entries == 0) {
          _write(sockfd, "\a", 1);
        } else if (comphelp.num_entries > 0) {
          cli->showprompt = 1;
          _write(sockfd, "\r\n", 2);
          for (i = 0; i < (int)comphelp.num_entries; i++) {
            if (comphelp.entries[i][2] != '[') show_cr = 0;
            cli_error(cli, "%s", comphelp.entries[i]);
          }
          if (show_cr) cli_error(cli, "  <cr>");
        }

        cli_free_comphelp(&comphelp);

        if (comphelp.num_entries >= 0) continue;
      }

      // History
      if (c == CTRL('P') || c == CTRL('N')) {
        int history_found = 0;

        if (cli->state == STATE_LOGIN || cli->state == STATE_PASSWORD || cli->state == STATE_ENABLE_PASSWORD) continue;

        if (c == CTRL('P')) {
          // Up
          in_history--;
          if (in_history < 0) {
            for (in_history = MAX_HISTORY - 1; in_history >= 0; in_history--) {
              if (cli->history[in_history]) {
                history_found = 1;
                break;
              }
            }
          } else {
            if (cli->history[in_history]) history_found = 1;
          }
        } else {
          // Down
          in_history++;
          if (in_history >= MAX_HISTORY || !cli->history[in_history]) {
            int i = 0;
            for (i = 0; i < MAX_HISTORY; i++) {
              if (cli->history[i]) {
                in_history = i;
                history_found = 1;
                break;
              }
            }
          } else {
            if (cli->history[in_history]) history_found = 1;
          }
        }
        if (history_found && cli->history[in_history]) {
          // Show history item
          cli_clear_line(sockfd, cmd, l, cursor);
          memset(cmd, 0, CLI_MAX_LINE_LENGTH);
          strncpy(cmd, cli->history[in_history], CLI_MAX_LINE_LENGTH - 1);
          l = cursor = strlen(cmd);
          _write(sockfd, cmd, l);
        }

        continue;
      }

      // Left/right cursor motion
      if (c == CTRL('B') || c == CTRL('F')) {
        if (c == CTRL('B')) {
          // Left
          if (cursor) {
            if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) _write(sockfd, "\b", 1);

            cursor--;
          }
        } else {
          // Right
          if (cursor < l) {
            if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) _write(sockfd, &cmd[cursor], 1);

            cursor++;
          }
        }

        continue;
      }

      if (c == CTRL('A')) {
        // Start of line
        if (cursor) {
          if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) {
            _write(sockfd, "\r", 1);
            show_prompt(cli, sockfd);
          }

          cursor = 0;
        }

        continue;
      }

      if (c == CTRL('E')) {
        // End of line
        if (cursor < l) {
          if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD)
            _write(sockfd, &cmd[cursor], l - cursor);

          cursor = l;
        }

        continue;
      }

      if (cursor == l) {
        // Normal character typed.
        // Append to end of line if not at end-of-buffer.
        if (l < CLI_MAX_LINE_LENGTH - 1) {
          cmd[cursor] = c;
          l++;
          cursor++;
        } else {
          // End-of-buffer, ensure null terminated
          cmd[cursor] = 0;
          _write(sockfd, "\a", 1);
          continue;
        }
      } else {
        // Middle of text
        int i;
        // Move everything one character to the right
        memmove(cmd + cursor + 1, cmd + cursor, l - cursor);

        // Insert new character
        cmd[cursor] = c;

        // IMPORTANT - if at end of buffer, set last char to NULL and don't change length, otherwise bump length by 1
        if (l == CLI_MAX_LINE_LENGTH - 1) {
          cmd[l] = 0;
        } else {
          l++;
        }

        // Write buffer, then backspace to where we were
        _write(sockfd, cmd + cursor, l - cursor);

        for (i = 0; i < (l - cursor); i++) _write(sockfd, "\b", 1);
        cursor++;
      }

      if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) {
        if (c == '?' && cursor == l) {
          _write(sockfd, "\r\n", 2);
          oldcmd = cmd;
          oldl = cursor = l - 1;
          break;
        }
        _write(sockfd, &c, 1);
      }

      oldcmd = 0;
      oldl = 0;
      lastchar = c;
    }

    if (l < 0) break;

    if (cli->state == STATE_LOGIN) {
      if (l == 0) continue;

      // Require login
      free_z(username);
      if (!(username = strdup(cmd))) return 0;
      cli->state = STATE_PASSWORD;
      cli->showprompt = 1;
    } else if (cli->state == STATE_PASSWORD) {
      // Require password
      int allowed = 0;

      free_z(password);
      if (!(password = strdup(cmd))) return 0;
      if (cli->auth_callback) {
        if (cli->auth_callback(username, password) == CLI_OK) allowed++;
      }

      if (!allowed) {
        struct unp *u;
        for (u = cli->users; u; u = u->next) {
          if (!strcmp(u->username, username) && pass_matches(u->password, password)) {
            allowed++;
            break;
          }
        }
      }

      if (allowed) {
        cli_error(cli, " ");
        cli->state = STATE_NORMAL;
      } else {
        cli_error(cli, "\n\nAccess denied");
        free_z(username);
        free_z(password);
        cli->state = STATE_LOGIN;
      }

      cli->showprompt = 1;
    } else if (cli->state == STATE_ENABLE_PASSWORD) {
      int allowed = 0;
      if (cli->enable_password) {
        // Check stored static enable password
        if (pass_matches(cli->enable_password, cmd)) allowed++;
      }

      if (!allowed && cli->enable_callback) {
        // Check callback
        if (cli->enable_callback(cmd)) allowed++;
      }

      if (allowed) {
        cli->state = STATE_ENABLE;
        cli_set_privilege(cli, PRIVILEGE_PRIVILEGED);
      } else {
        cli_error(cli, "\n\nAccess denied");
        cli->state = STATE_NORMAL;
      }
    } else {
      int rc;
      if (l == 0) continue;
      if (cmd[l - 1] != '?' && strcasecmp(cmd, "history") != 0) cli_add_history(cli, cmd);

      rc = cli_run_command(cli, cmd);
      switch (rc) {
        case CLI_BUILDMODE_ERROR:
          // Unable to enter buildmode successfully
          cli_print(cli, "Failure entering build mode for '%s'", cli->buildmode->cname);
          cli_int_free_buildmode(cli);
          continue;
        case CLI_BUILDMODE_CANCEL:
          // Called if user enters 'cancel'
          cli_print(cli, "Canceling build mode for '%s'", cli->buildmode->cname);
          cli_int_free_buildmode(cli);
          break;
        case CLI_BUILDMODE_EXIT:
          // Called when user enters exit - rebuild *entire* command line.
          // Recall all located optargs
          cli->found_optargs = cli->buildmode->found_optargs;
          rc = cli_int_execute_buildmode(cli);
          break;
        case CLI_QUIT:
          break;
        case CLI_BUILDMODE_START:
        case CLI_BUILDMODE_EXTEND:
        default:
          break;
      }

      // Process is done if we get a CLI_QUIT,
      if (rc == CLI_QUIT) break;
    }

    // Update the last_action time now as the last command run could take a long time to return
    if (cli->idle_timeout) time(&cli->last_action);
  }

  cli_free_history(cli);
  free_z(username);
  free_z(password);
  free_z(cmd);

  fclose(cli->client);
  cli->client = 0;
  return CLI_OK;
}

int cli_file(struct cli_def *cli, FILE *fh, int privilege, int mode) {
  int oldpriv = cli_set_privilege(cli, privilege);
  int oldmode = cli_set_configmode(cli, mode, NULL);
  char buf[CLI_MAX_LINE_LENGTH];

  while (1) {
    char *p;
    char *cmd;
    char *end;

    // End of file
    if (fgets(buf, CLI_MAX_LINE_LENGTH - 1, fh) == NULL) break;

    if ((p = strpbrk(buf, "#\r\n"))) *p = 0;

    cmd = buf;
    while (isspace(*cmd)) cmd++;

    if (!*cmd) continue;

    for (p = end = cmd; *p; p++)
      if (!isspace(*p)) end = p;

    *++end = 0;
    if (strcasecmp(cmd, "quit") == 0) break;

    if (cli_run_command(cli, cmd) == CLI_QUIT) break;
  }

  cli_set_privilege(cli, oldpriv);
  cli_set_configmode(cli, oldmode, NULL);

  return CLI_OK;
}

static void _print(struct cli_def *cli, int print_mode, const char *format, va_list ap) {
  int n;
  char *p = NULL;

  if (!cli) return;

  n = vasprintf(&p, format, ap);
  if (n < 0) return;
  if (cli->buffer) free(cli->buffer);
  cli->buffer = p;
  cli->buf_size = n;

  p = cli->buffer;
  do {
    char *next = strchr(p, '\n');
    struct cli_filter *f = (print_mode & PRINT_FILTERED) ? cli->filters : 0;
    int print = 1;

    if (next)
      *next++ = 0;
    else if (print_mode & PRINT_BUFFERED)
      break;

    while (print && f) {
      print = (f->filter(cli, p, f->data) == CLI_OK);
      f = f->next;
    }
    if (print) {
      if (cli->print_callback)
        cli->print_callback(cli, p);
      else if (cli->client)
        fprintf(cli->client, "%s\r\n", p);
    }

    p = next;
  } while (p);

  if (p && *p) {
    if (p != cli->buffer) memmove(cli->buffer, p, strlen(p));
  } else
    *cli->buffer = 0;
}

void cli_bufprint(struct cli_def *cli, const char *format, ...) {
  va_list ap;

  va_start(ap, format);
  _print(cli, PRINT_BUFFERED | PRINT_FILTERED, format, ap);
  va_end(ap);
}

void cli_vabufprint(struct cli_def *cli, const char *format, va_list ap) {
  _print(cli, PRINT_BUFFERED, format, ap);
}

void cli_print(struct cli_def *cli, const char *format, ...) {
  va_list ap;

  va_start(ap, format);
  _print(cli, PRINT_FILTERED, format, ap);
  va_end(ap);
}

void cli_error(struct cli_def *cli, const char *format, ...) {
  va_list ap;

  va_start(ap, format);
  _print(cli, PRINT_PLAIN, format, ap);
  va_end(ap);
}

struct cli_match_filter_state {
  int flags;
  union {
    char *string;
    regex_t re;
  } match;
};

int cli_search_flags_validator(struct cli_def *cli, const char *word, const char *value) {
  // Valid search flags starts with a hyphen, then any number of i, v, or e characters.

  if ((*value++ == '-') && (*value) && (strspn(value, "vie") == strlen(value))) return CLI_OK;
  return CLI_ERROR;
}

int cli_match_filter_init(struct cli_def *cli, int argc, char **argv, struct cli_filter *filt) {
  struct cli_match_filter_state *state;
  char *search_pattern = cli_get_optarg_value(cli, "search_pattern", NULL);
  char *search_flags = cli_get_optarg_value(cli, "search_flags", NULL);

  filt->filter = cli_match_filter;
  filt->data = state = calloc(sizeof(struct cli_match_filter_state), 1);
  if (!state) return CLI_ERROR;

  if (!strcmp(cli->pipeline->current_stage->words[0], "include")) {
    state->match.string = strdup(search_pattern);
  } else if (!strcmp(cli->pipeline->current_stage->words[0], "exclude")) {
    state->match.string = strdup(search_pattern);
    state->flags = MATCH_INVERT;
#ifndef WIN32
  } else {
    int rflags = REG_NOSUB;
    if (!strcmp(cli->pipeline->current_stage->words[0], "grep")) {
      state->flags = MATCH_REGEX;
    } else if (!strcmp(cli->pipeline->current_stage->words[0], "egrep")) {
      state->flags = MATCH_REGEX;
      rflags |= REG_EXTENDED;
    }
    if (search_flags) {
      char *p = search_flags++;
      while (*p) {
        switch (*p++) {
          case 'v':
            state->flags |= MATCH_INVERT;
            break;

          case 'i':
            rflags |= REG_ICASE;
            break;

          case 'e':
            // Implies next term is search string, so stop processing flags
            break;
        }
      }
    }
    if (regcomp(&state->match.re, search_pattern, rflags)) {
      if (cli->client) fprintf(cli->client, "Invalid pattern \"%s\"\r\n", search_pattern);
      return CLI_ERROR;
    }
  }
#else
  } else {
    // No regex functions in windows, so return an error.
    return CLI_ERROR;
  }
#endif

  return CLI_OK;
}

int cli_match_filter(UNUSED(struct cli_def *cli), const char *string, void *data) {
  struct cli_match_filter_state *state = data;
  int r = CLI_ERROR;

  if (!string) {
    if (state->flags & MATCH_REGEX)
      regfree(&state->match.re);
    else
      free(state->match.string);

    free(state);
    return CLI_OK;
  }

  if (state->flags & MATCH_REGEX) {
    if (!regexec(&state->match.re, string, 0, NULL, 0)) r = CLI_OK;
  } else {
    if (strstr(string, state->match.string)) r = CLI_OK;
  }

  if (state->flags & MATCH_INVERT) {
    if (r == CLI_OK)
      r = CLI_ERROR;
    else
      r = CLI_OK;
  }

  return r;
}

struct cli_range_filter_state {
  int matched;
  char *from;
  char *to;
};

int cli_range_filter_init(struct cli_def *cli, int argc, char **argv, struct cli_filter *filt) {
  struct cli_range_filter_state *state;
  char *from = strdup(cli_get_optarg_value(cli, "range_start", NULL));
  char *to = strdup(cli_get_optarg_value(cli, "range_end", NULL));

  // Do not have to check from/to since we would not have gotten here if we were missing a required argument.
  filt->filter = cli_range_filter;
  filt->data = state = calloc(sizeof(struct cli_range_filter_state), 1);
  if (state) {
    state->from = from;
    state->to = to;
    return CLI_OK;
  } else {
    free_z(from);
    free_z(to);
    return CLI_ERROR;
  }
}

int cli_range_filter(UNUSED(struct cli_def *cli), const char *string, void *data) {
  struct cli_range_filter_state *state = data;
  int r = CLI_ERROR;

  if (!string) {
    free_z(state->from);
    free_z(state->to);
    free_z(state);
    return CLI_OK;
  }

  if (!state->matched) state->matched = !!strstr(string, state->from);

  if (state->matched) {
    r = CLI_OK;
    if (state->to && strstr(string, state->to)) state->matched = 0;
  }

  return r;
}

int cli_count_filter_init(struct cli_def *cli, int argc, UNUSED(char **argv), struct cli_filter *filt) {
  if (argc > 1) {
    if (cli->client) fprintf(cli->client, "Count filter does not take arguments\r\n");

    return CLI_ERROR;
  }

  filt->filter = cli_count_filter;
  if (!(filt->data = calloc(sizeof(int), 1))) return CLI_ERROR;

  return CLI_OK;
}

int cli_count_filter(struct cli_def *cli, const char *string, void *data) {
  int *count = data;

  if (!string) {
    // Print count
    if (cli->client) fprintf(cli->client, "%d\r\n", *count);

    free(count);
    return CLI_OK;
  }

  while (isspace(*string)) string++;

  // Only count non-blank lines
  if (*string) (*count)++;

  return CLI_ERROR;
}

void cli_print_callback(struct cli_def *cli, void (*callback)(struct cli_def *, const char *)) {
  cli->print_callback = callback;
}

void cli_set_idle_timeout(struct cli_def *cli, unsigned int seconds) {
  if (seconds < 1) seconds = 0;
  cli->idle_timeout = seconds;
  time(&cli->last_action);
}

void cli_set_idle_timeout_callback(struct cli_def *cli, unsigned int seconds, int (*callback)(struct cli_def *)) {
  cli_set_idle_timeout(cli, seconds);
  cli->idle_timeout_callback = callback;
}

void cli_telnet_protocol(struct cli_def *cli, int telnet_protocol) {
  cli->telnet_protocol = !!telnet_protocol;
}
void cli_set_context(struct cli_def *cli, void *context) {
  cli->user_context = context;
}

void *cli_get_context(struct cli_def *cli) {
  return cli->user_context;
}

struct cli_command *cli_register_filter(struct cli_def *cli, const char *command,
                                        int (*init)(struct cli_def *cli, int, char **, struct cli_filter *),
                                        int (*filter)(struct cli_def *, const char *, void *), int privilege, int mode,
                                        const char *help) {
  struct cli_command *c;

  if (!command) return NULL;
  if (!(c = calloc(sizeof(struct cli_command), 1))) return NULL;

  c->command_type = CLI_FILTER_COMMAND;
  c->init = init;
  c->filter = filter;
  c->next = NULL;
  if (!(c->command = strdup(command))) {
    free(c);
    return NULL;
  }

  c->privilege = privilege;
  c->mode = mode;
  if (help && !(c->help = strdup(help))) {
    free(c->command);
    free(c);
    return NULL;
  }

  // Filters are all registered at the top level.
  cli_register_command_core(cli, NULL, c);
  return c;
}

int cli_unregister_filter(struct cli_def *cli, const char *command) {
  return cli_int_unregister_command_core(cli, command, CLI_FILTER_COMMAND);
}

void cli_int_free_found_optargs(struct cli_optarg_pair **optarg_pair) {
  struct cli_optarg_pair *c;

  if (!optarg_pair || !*optarg_pair) return;

  for (c = *optarg_pair; c;) {
    *optarg_pair = c->next;
    free_z(c->name);
    free_z(c->value);
    free_z(c);
    c = *optarg_pair;
  }
}

char *cli_find_optarg_value(struct cli_def *cli, char *name, char *find_after) {
  char *value = NULL;
  struct cli_optarg_pair *optarg_pair;
  if (!name || !cli->found_optargs) return NULL;

  for (optarg_pair = cli->found_optargs; optarg_pair && !value; optarg_pair = optarg_pair->next) {
    if (strcmp(optarg_pair->name, name) == 0) {
      if (find_after && find_after == optarg_pair->value) {
        find_after = NULL;
        continue;
      }
      value = optarg_pair->value;
    }
  }
  return value;
}

static void cli_optarg_build_shortest(struct cli_optarg *optarg) {
  struct cli_optarg *c, *p;
  char *cp, *pp;
  unsigned int len;

  for (c = optarg; c; c = c->next) {
    c->unique_len = 1;
    for (p = optarg; p; p = p->next) {
      if (c == p) continue;
      cp = c->name;
      pp = p->name;
      len = 1;
      while (*cp && *pp && *cp++ == *pp++) len++;
      if (len > c->unique_len) c->unique_len = len;
    }
  }
}

void cli_free_optarg(struct cli_optarg *optarg) {
  free_z(optarg->help);
  free_z(optarg->name);
  free_z(optarg);
}

int cli_optarg_addhelp(struct cli_optarg *optarg, const char *helpname, const char *helptext) {
  char *tstr;

  // put a vertical tab (\v), the new helpname, a horizontal tab (\t), and then the new help text
  if ((!optarg) || (asprintf(&tstr, "%s\v%s\t%s", optarg->help, helpname, helptext) == -1)) {
    return CLI_ERROR;
  } else {
    free(optarg->help);
    optarg->help = tstr;
  }
  return CLI_OK;
}

struct cli_optarg *cli_register_optarg(struct cli_command *cmd, const char *name, int flags, int privilege, int mode,
                                       const char *help,
                                       int (*get_completions)(struct cli_def *cli, const char *, const char *,
                                                              struct cli_comphelp *),
                                       int (*validator)(struct cli_def *cli, const char *, const char *),
                                       int (*transient_mode)(struct cli_def *cli, const char *, const char *)) {
  struct cli_optarg *optarg = NULL;
  struct cli_optarg *lastopt = NULL;
  struct cli_optarg *ptr = NULL;
  int retval = CLI_ERROR;

  // Name must not already exist with this priv/mode
  for (ptr = cmd->optargs, lastopt = NULL; ptr; lastopt = ptr, ptr = ptr->next) {
    if (!strcmp(name, ptr->name) && ptr->mode == mode && ptr->privilege == privilege) {
      goto CLEANUP;
    }
  }
  if (!(optarg = calloc(sizeof(struct cli_optarg), 1))) goto CLEANUP;
  if (!(optarg->name = strdup(name))) goto CLEANUP;
  if (help && !(optarg->help = strdup(help))) goto CLEANUP;

  optarg->mode = mode;
  optarg->privilege = privilege;
  optarg->get_completions = get_completions;
  optarg->validator = validator;
  optarg->transient_mode = transient_mode;
  optarg->flags = flags;

  if (lastopt)
    lastopt->next = optarg;
  else
    cmd->optargs = optarg;
  cli_optarg_build_shortest(cmd->optargs);
  retval = CLI_OK;

CLEANUP:
  if (retval != CLI_OK) {
    cli_free_optarg(optarg);
    optarg = NULL;
  }
  return optarg;
}

int cli_unregister_optarg(struct cli_command *cmd, const char *name) {
  struct cli_optarg *ptr;
  struct cli_optarg *lastptr;
  int retval = CLI_ERROR;
  // Iterate looking for this option name, stopping at end or if name matches
  for (lastptr = NULL, ptr = cmd->optargs; ptr && strcmp(ptr->name, name); lastptr = ptr, ptr = ptr->next)
    ;

  // If ptr, then we found the optarg to delete
  if (ptr) {
    if (lastptr) {
      // Not first optarg
      lastptr->next = ptr->next;
      ptr->next = NULL;
    } else {
      // First optarg
      cmd->optargs = ptr->next;
      ptr->next = NULL;
    }
    cli_free_optarg(ptr);
    cli_optarg_build_shortest(cmd->optargs);
    retval = CLI_OK;
  }
  return retval;
}

void cli_unregister_all_optarg(struct cli_command *c) {
  struct cli_optarg *o, *p;

  for (o = c->optargs; o; o = p) {
    p = o->next;
    cli_free_optarg(o);
  }
}

void cli_int_unset_optarg_value(struct cli_def *cli, const char *name) {
  struct cli_optarg_pair **p, *c;
  for (p = &cli->found_optargs, c = *p; *p;) {
    c = *p;

    if (!strcmp(c->name, name)) {
      *p = c->next;
      free_z(c->name);
      free_z(c->value);
      free_z(c);
    } else {
      p = &(*p)->next;
    }
  }
}

int cli_set_optarg_value(struct cli_def *cli, const char *name, const char *value, int allow_multiple) {
  struct cli_optarg_pair *optarg_pair, **anchor;
  int rc = CLI_ERROR;

  for (optarg_pair = cli->found_optargs, anchor = &cli->found_optargs; optarg_pair;
       anchor = &optarg_pair->next, optarg_pair = optarg_pair->next) {
    // Break if we found this name *and* allow_multiple is false
    if (!strcmp(optarg_pair->name, name) && !allow_multiple) {
      break;
    }
  }
  // If we *didn't* find this, then allocate a new entry before proceeding
  if (!optarg_pair) {
    optarg_pair = (struct cli_optarg_pair *)calloc(1, sizeof(struct cli_optarg_pair));
    *anchor = optarg_pair;
  }
  // Set the value
  if (optarg_pair) {
    // Name is null only if we didn't find it
    if (!optarg_pair->name) optarg_pair->name = strdup(name);

    // Value may be overwritten, so free any old value.
    if (optarg_pair->value) free_z(optarg_pair->value);
    optarg_pair->value = strdup(value);

    rc = CLI_OK;
  }
  return rc;
}

struct cli_optarg_pair *cli_get_all_found_optargs(struct cli_def *cli) {
  if (cli) return cli->found_optargs;
  return NULL;
}

char *cli_get_optarg_value(struct cli_def *cli, const char *name, char *find_after) {
  char *value = NULL;
  struct cli_optarg_pair *optarg_pair;

  for (optarg_pair = cli->found_optargs; !value && optarg_pair; optarg_pair = optarg_pair->next) {
    // Check next entry if this isn't our name
    if (strcasecmp(optarg_pair->name, name)) continue;

    // Did we have a find_after, then ignore anything up until our find_after match
    if (find_after && optarg_pair->value == find_after) {
      find_after = NULL;
      continue;
    } else if (!find_after) {
      value = optarg_pair->value;
    }
  }
  return value;
}

void cli_int_free_buildmode(struct cli_def *cli) {
  if (!cli || !cli->buildmode) return;
  cli_unregister_tree(cli, cli->commands, CLI_BUILDMODE_COMMAND);
  cli->mode = cli->buildmode->mode;
  free_z(cli->buildmode->cname);
  free_z(cli->buildmode->mode_text);
  cli_int_free_found_optargs(&cli->buildmode->found_optargs);
  free_z(cli->buildmode);
}

int cli_int_enter_buildmode(struct cli_def *cli, struct cli_pipeline_stage *stage, char *mode_text) {
  struct cli_optarg *optarg;
  struct cli_command *c;
  struct cli_buildmode *buildmode;
  struct cli_optarg *buildmodeOptarg = NULL;
  int rc = CLI_BUILDMODE_START;

  if (!cli || !(buildmode = (struct cli_buildmode *)calloc(1, sizeof(struct cli_buildmode)))) {
    cli_error(cli, "Unable to build buildmode mode for command %s", stage->command->command);
    rc = CLI_BUILDMODE_ERROR;
    goto out;
  }

  // Clean up any shrapnel from earlier - shouldn't be any but....
  if (cli->buildmode) cli_int_free_buildmode(cli);

  // Assign it so cli_int_register_buildmode_command() has something to work with
  cli->buildmode = buildmode;
  cli->buildmode->mode = cli->mode;
  cli->buildmode->transient_mode = cli->transient_mode;
  if (mode_text) cli->buildmode->mode_text = strdup(mode_text);

  // Need this to verify we have all *required* arguments
  cli->buildmode->command = stage->command;

  // Build new *limited* list of commands from this commands optargs
  // Currently we only allow a single entry point to a buildmode, so advance t that
  // optarg and proceed from there.
  for (buildmodeOptarg = stage->command->optargs;
       buildmodeOptarg && !(buildmodeOptarg->flags & CLI_CMD_ALLOW_BUILDMODE); buildmodeOptarg = buildmodeOptarg->next)
    ;

  // Now start at this argument and flesh out the rest of the commands available for this buildmode
  for (optarg = buildmodeOptarg; optarg; optarg = optarg->next) {
    // Don't allow anything that could redefine our mode or buildmode mode, or redefine exit/cancel/show/unset
    if (!strcmp(optarg->name, "cancel") || !strcmp(optarg->name, "execute") || !strcmp(optarg->name, "show") ||
        !strcmp(optarg->name, "unset")) {
      cli_error(cli, "Default buildmode command conflicts with optarg named %s", optarg->name);
      rc = CLI_BUILDMODE_ERROR;
      goto out;
    }
    if (optarg->flags & (CLI_CMD_ALLOW_BUILDMODE | CLI_CMD_TRANSIENT_MODE | CLI_CMD_SPOT_CHECK)) continue;
    // accept the first optarg allowing buildmode, but reject any subsequent one
    if (optarg->flags & CLI_CMD_ALLOW_BUILDMODE && (optarg != buildmodeOptarg)) continue;
    if (optarg->mode != cli->mode && optarg->mode != cli->transient_mode)
      continue;
    else if (optarg->flags & (CLI_CMD_OPTIONAL_ARGUMENT | CLI_CMD_ARGUMENT)) {
      if ((c = cli_int_register_buildmode_command(cli, NULL, optarg->name, cli_int_buildmode_cmd_cback, optarg->flags,
                                                  optarg->privilege, cli->mode, optarg->help))) {
        cli_register_optarg(c, optarg->name, CLI_CMD_ARGUMENT | (optarg->flags & CLI_CMD_OPTION_MULTIPLE),
                            optarg->privilege, cli->mode, optarg->help, optarg->get_completions, optarg->validator,
                            NULL);
      } else {
        rc = CLI_BUILDMODE_ERROR;
        goto out;
      }
    } else {
      if (optarg->flags & CLI_CMD_OPTION_MULTIPLE) {
        if (!cli_int_register_buildmode_command(cli, NULL, optarg->name, cli_int_buildmode_flag_multiple_cback,
                                                optarg->flags, optarg->privilege, cli->mode, optarg->help)) {
          rc = CLI_BUILDMODE_ERROR;
          goto out;
        }
      } else {
        if (!cli_int_register_buildmode_command(cli, NULL, optarg->name, cli_int_buildmode_flag_cback, optarg->flags,
                                                optarg->privilege, cli->mode, optarg->help)) {
          rc = CLI_BUILDMODE_ERROR;
          goto out;
        }
      }
    }
  }
  cli->buildmode->cname = strdup(cli_command_name(cli, stage->command));
  // Now add the four 'always there' commands to cancel current mode and to execute the command, show settings, and
  // unset
  cli_int_register_buildmode_command(cli, NULL, "cancel", cli_int_buildmode_cancel_cback, 0, PRIVILEGE_UNPRIVILEGED,
                                     cli->mode, "Cancel command");
  cli_int_register_buildmode_command(cli, NULL, "execute", cli_int_buildmode_execute_cback, 0, PRIVILEGE_UNPRIVILEGED,
                                     cli->mode, "Execute command");
  cli_int_register_buildmode_command(cli, NULL, "show", cli_int_buildmode_show_cback, 0, PRIVILEGE_UNPRIVILEGED,
                                     cli->mode, "Show current settings");
  c = cli_int_register_buildmode_command(cli, NULL, "unset", cli_int_buildmode_unset_cback, 0, PRIVILEGE_UNPRIVILEGED,
                                         cli->mode, "Unset a setting");
  cli_register_optarg(c, "setting", CLI_CMD_ARGUMENT | CLI_CMD_DO_NOT_RECORD, PRIVILEGE_UNPRIVILEGED, cli->mode,
                      "setting to clear", cli_int_buildmode_unset_completor, cli_int_buildmode_unset_validator, NULL);

out:
  // And lastly set the initial help menu for the unset command
  cli_int_buildmode_reset_unset_help(cli);
  if (rc != CLI_BUILDMODE_START) cli_int_free_buildmode(cli);
  return rc;
}

int cli_int_unregister_buildmode_command(struct cli_def *cli, const char *command) {
  return cli_int_unregister_command_core(cli, command, CLI_BUILDMODE_COMMAND);
}

struct cli_command *cli_int_register_buildmode_command(struct cli_def *cli, struct cli_command *parent,
                                                       const char *command,
                                                       int (*callback)(struct cli_def *cli, const char *, char **, int),
                                                       int flags, int privilege, int mode, const char *help) {
  struct cli_command *c;

  if (!command) return NULL;
  if (!(c = calloc(sizeof(struct cli_command), 1))) return NULL;

  c->flags = flags;
  c->callback = callback;
  c->next = NULL;
  if (!(c->command = strdup(command))) {
    free(c);
    return NULL;
  }

  c->command_type = CLI_BUILDMODE_COMMAND;
  c->privilege = privilege;
  c->mode = mode;
  if (help && !(c->help = strndup(help, strchrnul(help, '\v') - help))) {
    free(c->command);
    free(c);
    return NULL;
  }

  // Buildmode commmands are all registered at the top level
  cli_register_command_core(cli, NULL, c);
  return c;
}

int cli_int_execute_buildmode(struct cli_def *cli) {
  struct cli_optarg *optarg = NULL;
  int rc = CLI_OK;
  char *cmdline;
  char *value = NULL;

  cmdline = strdup(cli_command_name(cli, cli->buildmode->command));
  for (optarg = cli->buildmode->command->optargs; rc == CLI_OK && optarg; optarg = optarg->next) {
    value = NULL;
    do {
      if (cli->privilege < optarg->privilege) continue;
      if ((optarg->mode != cli->buildmode->mode) && (optarg->mode != cli->buildmode->transient_mode) &&
          (optarg->mode != MODE_ANY))
        continue;

      value = cli_get_optarg_value(cli, optarg->name, value);
      if (!value && optarg->flags & CLI_CMD_ARGUMENT) {
        cli_error(cli, "Missing required argument %s", optarg->name);
        rc = CLI_MISSING_ARGUMENT;
      } else if (value) {
        if (optarg->flags & (CLI_CMD_OPTIONAL_FLAG | CLI_CMD_ARGUMENT)) {
          if (!(cmdline = cli_int_buildmode_extend_cmdline(cmdline, value))) {
            cli_error(cli, "Unable to append to building commandlne");
            rc = CLI_ERROR;
          }
        } else {
          if (!(cmdline = cli_int_buildmode_extend_cmdline(cmdline, optarg->name))) {
            cli_error(cli, "Unable to append to building commandlne");
            rc = CLI_ERROR;
          }
          if (!(cmdline = cli_int_buildmode_extend_cmdline(cmdline, value))) {
            cli_error(cli, "Unable to append to building commandlne");
            rc = CLI_ERROR;
          }
        }
      }
    } while (rc == CLI_OK && value && optarg->flags & CLI_CMD_OPTION_MULTIPLE);
  }

  if (rc == CLI_OK) {
    cli_int_free_buildmode(cli);
    cli_add_history(cli, cmdline);
    // disallow processing of buildmode so we don't wind up in a potential loop
    // main loop will also set as required
    cli->disallow_buildmode = 1;
    rc = cli_run_command(cli, cmdline);
  }
  free_z(cmdline);
  return rc;
}

char *cli_int_buildmode_extend_cmdline(char *cmdline, char *word) {
  char *tptr = NULL;
  char *cptr = NULL;
  size_t oldlen = strlen(cmdline);
  size_t wordlen = strlen(word);
  char quoteChar[2] = "";

  // by default we don't add quotes, but if the word is empty, we *must* to preserve that empty string
  // we also need to quote it if there is a space in the word, or any unescaped quotes, so we'll have
  // to walk the word....
  cptr = word;
  if (!wordlen) {
    quoteChar[0] = '"';
  }
  for (cptr = word; *cptr; cptr++) {
    if (*cptr == '\\' && *(cptr + 1)) {
      cptr++;  // skip over escapes blindly
    } else if ((*cptr == ' ') && (quoteChar[0] == '\0')) {
      // if we found a space we need quotes, select double unless we've already selected something
      quoteChar[0] = '"';
    } else if (*cptr == '"') {
      // if our first unescaped quote is a double, then we wrap the string in single quotes
      quoteChar[0] = '\'';
      break;
    } else if (*cptr == '\'') {
      // if our first unescaped quote is a single, then we wrap the string in double quotes
      quoteChar[0] = '"';
      break;
    }
  }

  // Allocate enough space to hold the old string, a space, possible quote, the new string,
  // another possible quote, and the final null terminator).

  if ((tptr = (char *)realloc(cmdline, oldlen + 1 + 1 + wordlen + 1 + 1))) {
    strcat(tptr, " ");
    strcat(tptr, quoteChar);
    strcat(tptr, word);
    strcat(tptr, quoteChar);
  }
  return tptr;
}

// Any time we set or unset a buildmode setting, we need to regerate the 'help' menu for the unset command
void cli_int_buildmode_reset_unset_help(struct cli_def *cli) {
  struct cli_command *cmd;

  // find the buildmode unset command
  for (cmd = cli->commands; cmd; cmd = cmd->next) {
    if ((cmd->command_type == CLI_BUILDMODE_COMMAND) && !strcmp(cmd->command, "unset")) break;
  }

  if (cmd) {
    struct cli_optarg *optarg;
    for (optarg = cmd->optargs; optarg && strcmp(optarg->name, "setting"); optarg = optarg->next)
      ;

    if (optarg) {
      char *endOfMainHelp;
      struct cli_optarg_pair *optarg_pair;
      /*
       * This will ensure that any previously added help is not propogated - this left over space will be freed by the
       * cli_optarg_addhelp() calls a few lines down
       */
      if ((endOfMainHelp = strchr(optarg->help, '\v'))) *endOfMainHelp = '\0';

      for (optarg_pair = cli->found_optargs; optarg_pair; optarg_pair = optarg_pair->next) {
        // Only show vars that are also current 'commands'
        struct cli_command *c = cli->commands;
        for (; c; c = c->next) {
          if (c->command_type != CLI_BUILDMODE_COMMAND) continue;
          if (!strcmp(c->command, optarg_pair->name)) {
            char *tmphelp;
            if (asprintf(&tmphelp, "unset %s", optarg_pair->name) >= 0) {
              cli_optarg_addhelp(optarg, optarg_pair->name, tmphelp);
              free_z(tmphelp);
            }
          }
        }
      }
    }
  }
}

int cli_int_buildmode_cmd_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  int rc = CLI_BUILDMODE_EXTEND;

  if (argc) {
    cli_error(cli, "Extra arguments on command line, command ignored.");
    rc = CLI_ERROR;
  }
  cli_int_buildmode_reset_unset_help(cli);
  return rc;
}

// A 'flag' callback has no optargs, so we need to set it ourself based on *this* command
int cli_int_buildmode_flag_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  int rc = CLI_BUILDMODE_EXTEND;

  if (argc) {
    cli_error(cli, "Extra arguments on command line, command ignored.");
    rc = CLI_ERROR;
  }
  if (cli_set_optarg_value(cli, command, command, 0)) {
    cli_error(cli, "Problem setting value for optional flag %s", command);
    rc = CLI_ERROR;
  }
  cli_int_buildmode_reset_unset_help(cli);
  return rc;
}

// A 'flag' callback has no optargs, so we need to set it ourself based on *this* command
int cli_int_buildmode_flag_multiple_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  int rc = CLI_BUILDMODE_EXTEND;

  if (argc) {
    cli_error(cli, "Extra arguments on command line, command ignored.");
    rc = CLI_ERROR;
  }
  if (cli_set_optarg_value(cli, command, command, CLI_CMD_OPTION_MULTIPLE)) {
    cli_error(cli, "Problem setting value for optional flag %s", command);
    rc = CLI_ERROR;
  }

  cli_int_buildmode_reset_unset_help(cli);
  return rc;
}

int cli_int_buildmode_cancel_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  int rc = CLI_BUILDMODE_CANCEL;

  if (argc > 0) {
    cli_error(cli, "Extra arguments on command line, command ignored.");
    rc = CLI_ERROR;
  }
  return rc;
}

int cli_int_buildmode_execute_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  int rc = CLI_BUILDMODE_EXIT;

  if (argc > 0) {
    cli_error(cli, "Extra arguments on command line, command ignored.");
    rc = CLI_ERROR;
  }
  return rc;
}

int cli_int_buildmode_show_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  struct cli_optarg_pair *optarg_pair;

  for (optarg_pair = cli->found_optargs; optarg_pair; optarg_pair = optarg_pair->next) {
    // Only show vars that are also current 'commands'
    struct cli_command *c = cli->commands;
    for (; c; c = c->next) {
      if (c->command_type != CLI_BUILDMODE_COMMAND) continue;
      if (!strcmp(c->command, optarg_pair->name)) {
        cli_print(cli, "  %-20s = %s", optarg_pair->name, optarg_pair->value);
        break;
      }
    }
  }
  return CLI_OK;
}

int cli_int_buildmode_unset_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  // Iterate over our 'set' variables to see if that variable is also a 'valid' command right now
  struct cli_command *c;

  // have to catch this one here due to how buildmode works
  if (!argv[0] || !*argv[0]) {
    cli_error(cli, "Incomplete command, missing required argument 'setting' for command  'unset'");
    return CLI_ERROR;
  }
  // Is this 'optarg' to remove one of the current commands?
  for (c = cli->commands; c; c = c->next) {
    if (c->command_type != CLI_BUILDMODE_COMMAND) continue;
    if (cli->privilege < c->privilege) continue;
    if ((cli->buildmode->mode != c->mode) && (cli->buildmode->transient_mode != c->mode) && (c->mode != MODE_ANY))
      continue;
    if (strcmp(c->command, argv[0])) continue;
    // Go fry anything by this name

    cli_int_unset_optarg_value(cli, argv[0]);
    cli_int_buildmode_reset_unset_help(cli);
    break;
  }

  return CLI_OK;
}

// Generate a list of variables that *have* been set
int cli_int_buildmode_unset_completor(struct cli_def *cli, const char *name, const char *word,
                                      struct cli_comphelp *comphelp) {
  struct cli_optarg_pair *optarg_pair;

  for (optarg_pair = cli->found_optargs; optarg_pair; optarg_pair = optarg_pair->next) {
    // Only complete vars that could be set by current 'commands'
    struct cli_command *c = cli->commands;
    for (; c; c = c->next) {
      if (c->command_type != CLI_BUILDMODE_COMMAND) continue;
      if ((!strcmp(c->command, optarg_pair->name)) && (!word || !strncmp(word, optarg_pair->name, strlen(word)))) {
        cli_add_comphelp_entry(comphelp, optarg_pair->name);
      }
    }
  }
  return CLI_OK;
}

int cli_int_buildmode_unset_validator(struct cli_def *cli, const char *name, const char *value) {
  struct cli_optarg_pair *optarg_pair;

  if (!name || !*name) {
    cli_error(cli, "No setting given to unset");
    return CLI_ERROR;
  }
  for (optarg_pair = cli->found_optargs; optarg_pair; optarg_pair = optarg_pair->next) {
    // Only complete vars that could be set by current 'commands'
    struct cli_command *c = cli->commands;
    for (; c; c = c->next) {
      if (c->command_type != CLI_BUILDMODE_COMMAND) continue;
      if (!strcmp(c->command, optarg_pair->name) && value && !strcmp(optarg_pair->name, value)) {
        return CLI_OK;
      }
    }
  }
  return CLI_ERROR;
}

void cli_set_transient_mode(struct cli_def *cli, int transient_mode) {
  cli->transient_mode = transient_mode;
}

int cli_add_comphelp_entry(struct cli_comphelp *comphelp, const char *entry) {
  int retval = CLI_ERROR;
  if (comphelp && entry) {
    char *dupelement = strdup(entry);
    char **duparray = (char **)realloc((void *)comphelp->entries, sizeof(char *) * (comphelp->num_entries + 1));
    if (dupelement && duparray) {
      comphelp->entries = duparray;
      comphelp->entries[comphelp->num_entries++] = dupelement;
      retval = CLI_OK;
    } else {
      free_z(dupelement);
      free_z(duparray);
    }
  }
  return retval;
}

void cli_free_comphelp(struct cli_comphelp *comphelp) {
  if (comphelp) {
    int idx;

    for (idx = 0; idx < comphelp->num_entries; idx++) free_z(comphelp->entries[idx]);
    free_z(comphelp->entries);
  }
}

static int cli_int_locate_command(struct cli_def *cli, struct cli_command *commands, int command_type, int start_word,
                                  struct cli_pipeline_stage *stage) {
  struct cli_command *c, *again_config = NULL, *again_any = NULL;
  int c_words = stage->num_words;

  for (c = commands; c; c = c->next) {
    if (c->command_type != command_type) continue;
    if (cli->privilege < c->privilege) continue;

    if (strncasecmp(c->command, stage->words[start_word], c->unique_len)) continue;
    if (strncasecmp(c->command, stage->words[start_word], strlen(stage->words[start_word]))) continue;

  AGAIN:
    if (c->mode == cli->mode || (c->mode == MODE_ANY && again_any != NULL)) {
      int rc = CLI_OK;

      // Found a word!
      if (!c->children) {
        // Last word
        if (!c->callback && !c->filter) {
          cli_error(cli, "No callback for \"%s\"", cli_command_name(cli, c));
          return CLI_ERROR;
        }
      } else {
        if (start_word == c_words - 1) {
          if (c->callback) goto CORRECT_CHECKS;

          cli_error(cli, "Incomplete command");
          return CLI_ERROR;
        }
        rc = cli_int_locate_command(cli, c->children, command_type, start_word + 1, stage);
        if (rc == CLI_ERROR_ARG) {
          if (c->callback) {
            rc = CLI_OK;
            goto CORRECT_CHECKS;
          } else {
            cli_error(cli, "Invalid %s \"%s\"", commands->parent ? "argument" : "command", stage->words[start_word]);
          }
        }
        return rc;
      }

      if (!c->callback && !c->filter) {
        cli_error(cli, "Internal server error processing \"%s\"", cli_command_name(cli, c));
        return CLI_ERROR;
      }

    CORRECT_CHECKS:

      if (rc == CLI_OK) {
        stage->command = c;
        stage->first_unmatched = start_word + 1;
        stage->first_optarg = stage->first_unmatched;
        cli_int_parse_optargs(cli, stage, c, '\0', NULL);
        rc = stage->status;
      }
      return rc;
    } else if (cli->mode > MODE_CONFIG && c->mode == MODE_CONFIG) {
      // Command matched but from another mode, remember it if we fail to find correct command
      again_config = c;
    } else if (c->mode == MODE_ANY) {
      // Command matched but for any mode, remember it if we fail to find correct command
      again_any = c;
    }
  }

  // Drop out of config submode if we have matched command on MODE_CONFIG
  if (again_config) {
    c = again_config;
    goto AGAIN;
  }
  if (again_any) {
    c = again_any;
    goto AGAIN;
  }

  if (start_word == 0)
    cli_error(cli, "Invalid %s \"%s\"", commands->parent ? "argument" : "command", stage->words[start_word]);

  return CLI_ERROR_ARG;
}

int cli_int_validate_pipeline(struct cli_def *cli, struct cli_pipeline *pipeline) {
  int i;
  int rc = CLI_OK;
  int command_type;

  if (!pipeline) return CLI_ERROR;
  cli->pipeline = pipeline;

  cli->found_optargs = NULL;

  // If the line is totally empty this is not an error, but we need to return
  // CLI_ERROR to avoid processing it
  if (pipeline->num_words == 0) return CLI_ERROR;

  for (i = 0; i < pipeline->num_stages; i++) {
    // And double check each stage for an empty line - this *is* an error
    if (pipeline->stage[i].num_words == 0) {
      cli_error(cli, "Empty command given");
      return CLI_ERROR;
    }

    // In 'buildmode' we only have one pipeline, but we need to recall if we had started with any optargs
    if (cli->buildmode && i == 0)
      command_type = CLI_BUILDMODE_COMMAND;
    else if (i > 0)
      command_type = CLI_FILTER_COMMAND;
    else
      command_type = CLI_REGULAR_COMMAND;

    cli->pipeline->current_stage = &pipeline->stage[i];
    if (cli->buildmode)
      cli->found_optargs = cli->buildmode->found_optargs;
    else
      cli->found_optargs = NULL;
    rc = cli_int_locate_command(cli, cli->commands, command_type, 0, &pipeline->stage[i]);

    // And save our found optargs for later use
    if (cli->buildmode)
      cli->buildmode->found_optargs = cli->found_optargs;
    else
      pipeline->stage[i].found_optargs = cli->found_optargs;

    if (rc != CLI_OK) break;
  }
  cli->pipeline = NULL;

  return rc;
}

void cli_int_free_pipeline(struct cli_pipeline *pipeline) {
  int i;
  if (!pipeline) return;
  for (i = 0; i < pipeline->num_stages; i++) cli_int_free_found_optargs(&pipeline->stage[i].found_optargs);
  for (i = 0; i < pipeline->num_words; i++) free_z(pipeline->words[i]);
  free_z(pipeline->cmdline);
  free_z(pipeline);
  pipeline = NULL;
}

void cli_int_show_pipeline(struct cli_def *cli, struct cli_pipeline *pipeline) {
  int i, j;
  struct cli_pipeline_stage *stage;
  char **word;
  struct cli_optarg_pair *optarg_pair;

  for (i = 0, word = pipeline->words; i < pipeline->num_words; i++, word++) printf("[%s] ", *word);
  fprintf(stderr, "\n");
  fprintf(stderr, "#stages=%d, #words=%d\n", pipeline->num_stages, pipeline->num_words);

  for (i = 0; i < pipeline->num_stages; i++) {
    stage = &(pipeline->stage[i]);
    fprintf(stderr, "  #%d(%d words) first_unmatched=%d: ", i, stage->num_words, stage->first_unmatched);
    for (j = 0; j < stage->num_words; j++) {
      fprintf(stderr, " [%s]", stage->words[j]);
    }
    fprintf(stderr, "\n");

    if (stage->command) {
      fprintf(stderr, "  Command: %s\n", stage->command->command);
    }
    for (optarg_pair = stage->found_optargs; optarg_pair; optarg_pair = optarg_pair->next) {
      fprintf(stderr, "    %s: %s\n", optarg_pair->name, optarg_pair->value);
    }
  }
}

// Take an array of words and return a pipeline, using '|' to split command into different 'stages'.
// Pipeline is broken down by '|' characters and within each p.
struct cli_pipeline *cli_int_generate_pipeline(struct cli_def *cli, const char *command) {
  int i;
  struct cli_pipeline_stage *stage;
  char **word;
  struct cli_pipeline *pipeline = NULL;

  cli->found_optargs = NULL;
  if (cli->buildmode) cli->found_optargs = cli->buildmode->found_optargs;
  if (!command) return NULL;
  while (*command && isspace(*command)) command++;

  if (!(pipeline = (struct cli_pipeline *)calloc(1, sizeof(struct cli_pipeline)))) return NULL;
  pipeline->cmdline = (char *)strdup(command);

  pipeline->num_words = cli_parse_line(command, pipeline->words, CLI_MAX_LINE_WORDS);

  pipeline->stage[0].num_words = 0;
  stage = &pipeline->stage[0];
  word = pipeline->words;
  stage->words = word;
  for (i = 0; i < pipeline->num_words; i++, word++) {
    if (*word[0] == '|') {
      if (cli->buildmode) {
        // Can't allow filters in buildmode commands
        cli_int_free_pipeline(pipeline);
        cli_error(cli, "\nPipelines are not allowed in buildmode");
        return NULL;
      }
      stage->stage_num = pipeline->num_stages;
      stage++;
      stage->num_words = 0;
      pipeline->num_stages++;
      stage->words = word + 1;  // First word of the next stage is one past where we are (possibly NULL)
    } else {
      stage->num_words++;
    }
  }
  stage->stage_num = pipeline->num_stages;
  pipeline->num_stages++;
  return pipeline;
}

int cli_int_execute_pipeline(struct cli_def *cli, struct cli_pipeline *pipeline) {
  int stage_num;
  int rc = CLI_OK;
  struct cli_filter **filt = &cli->filters;

  if (!pipeline | !cli) return CLI_ERROR;

  cli->pipeline = pipeline;
  for (stage_num = 1; stage_num < pipeline->num_stages; stage_num++) {
    struct cli_pipeline_stage *stage = &pipeline->stage[stage_num];
    pipeline->current_stage = stage;
    cli->found_optargs = stage->found_optargs;
    *filt = calloc(sizeof(struct cli_filter), 1);
    if (*filt) {
      if ((rc = stage->command->init(cli, stage->num_words, stage->words, *filt) != CLI_OK)) {
        break;
      }
      filt = &(*filt)->next;
    }
  }
  pipeline->current_stage = NULL;

  // Did everything init?  If so, execute, otherwise skip execution
  if ((rc == CLI_OK) && pipeline->stage[0].command->callback) {
    struct cli_pipeline_stage *stage = &pipeline->stage[0];

    pipeline->current_stage = &pipeline->stage[0];
    if (pipeline->current_stage->command->command_type == CLI_BUILDMODE_COMMAND)
      cli->found_optargs = cli->buildmode->found_optargs;
    else
      cli->found_optargs = pipeline->stage[0].found_optargs;
    rc = stage->command->callback(cli, cli_command_name(cli, stage->command), stage->words + stage->first_unmatched,
                                  stage->num_words - stage->first_unmatched);
    if (pipeline->current_stage->command->command_type == CLI_BUILDMODE_COMMAND)
      cli->buildmode->found_optargs = cli->found_optargs;
    pipeline->current_stage = NULL;
  }

  // Now teardown any filters
  while (cli->filters) {
    struct cli_filter *filt = cli->filters;
    if (filt->filter) filt->filter(cli, NULL, cli->filters->data);
    cli->filters = filt->next;
    free_z(filt);
  }
  cli->found_optargs = NULL;
  cli->pipeline = NULL;
  return rc;
}

/*
 *  Attemp quick dirty wrapping of helptext taking into account the offset from name, embedded
 *  cr/lf in helptext, and trying to split on last white-text before the margin
 */
void cli_int_wrap_help_line(char *nameptr, char *helpptr, struct cli_comphelp *comphelp) {
  int maxwidth = 80;  // temporary assumption, to be fixed later when libcli 'understands' screen dimensions
  int availwidth;
  int namewidth;
  int toprint;
  char *crlf;
  char *line;
  char emptystring[] = "";
  namewidth = strlen(nameptr);
  availwidth = maxwidth - namewidth;

  if (!helpptr) helpptr = emptystring;
  /*
   * Now we need to iterate one or more times to only print out at most
   * maxwidth - leftwidth characters of helpptr.  Note that there are no
   * tabs in helpptr, so each 'char' displays as one char
   */

  do {
    toprint = strlen(helpptr);
    if (toprint > availwidth) {
      toprint = availwidth;
      while ((toprint >= 0) && !isspace(helpptr[toprint])) toprint--;
      if (toprint < 0) {
        // if we backed up and found no whitespace, dump as much as we can
        toprint = availwidth;
      }
    }  // see if we might have an embedded carriage return or line feed
    if ((crlf = strpbrk(helpptr, "\n\r"))) {
      // crlf is a pointer - see if it is 'before' the toprint index
      if ((crlf - helpptr) < toprint) {
        // ok, crlf is before the wrap, so have line break here.
        toprint = (crlf - helpptr);
      }
    }

    if (asprintf(&line, "%*.*s%.*s", namewidth, namewidth, nameptr, toprint, helpptr) < 0) break;
    cli_add_comphelp_entry(comphelp, line);
    free_z(line);

    nameptr = emptystring;
    helpptr += toprint;
    // advance to first non whitespace
    while (helpptr && isspace(*helpptr)) helpptr++;
  } while (*helpptr);
}

static void cli_get_optarg_comphelp(struct cli_def *cli, struct cli_optarg *optarg, struct cli_comphelp *comphelp,
                                    int num_candidates, const char lastchar, const char *anchor_word,
                                    const char *next_word) {
  int help_insert = 0;
  char *delim_start = DELIM_NONE;
  char *delim_end = DELIM_NONE;
  int (*get_completions)(struct cli_def *, const char *, const char *, struct cli_comphelp *) = NULL;
  char *tptr = NULL;

  // If we've already seen a value by this exact name, skip it, unless the multiple flag is set
  if (cli_find_optarg_value(cli, optarg->name, NULL) && !(optarg->flags & (CLI_CMD_OPTION_MULTIPLE))) return;

  get_completions = optarg->get_completions;
  if (optarg->flags & CLI_CMD_OPTIONAL_FLAG) {
    if (!(anchor_word && !strncmp(anchor_word, optarg->name, strlen(anchor_word)))) {
      delim_start = DELIM_OPT_START;
      delim_end = DELIM_OPT_END;
      get_completions = NULL;  // No point, completor of field is the name itself
    }
  } else if (optarg->flags & CLI_CMD_HYPHENATED_OPTION) {
    delim_start = DELIM_OPT_START;
    delim_end = DELIM_OPT_END;
  } else if (optarg->flags & CLI_CMD_ARGUMENT) {
    delim_start = DELIM_ARG_START;
    delim_end = DELIM_ARG_END;
  } else if (optarg->flags & CLI_CMD_OPTIONAL_ARGUMENT) {
    /*
     * Optional args can match against the name or the value.
     * Here 'anchor_word' is the name, and 'next_word' is 'value' for said optional argument.
     * So if anchor_word==next_word we're looking at the 'name' of the optarg, otherwise we know the name and are going
     * against the value.
     */
    if (anchor_word != next_word) {
      // Matching against optional argument 'value'
      help_insert = 0;
      if (!get_completions) {
        delim_start = DELIM_ARG_START;
        delim_end = DELIM_ARG_END;
      }
    } else {
      // Matching against optional argument 'name'
      help_insert = 1;
      get_completions = NULL;  // Matching against the name, not the following field value
      if (!(anchor_word && !strncmp(anchor_word, optarg->name, strlen(anchor_word)))) {
        delim_start = DELIM_OPT_START;
        delim_end = DELIM_OPT_END;
      }
    }
  }

  // Fill in with help text or completor value(s) as indicated
  if (lastchar == '?') {
    /*
     *  Note - help is a bit complex, and we could optimize it.  But it isn't done often,
     *  so we're always going to do it on the fly.
     *  Help will consist of '\v' separated lines.  Each line except the first is also '\t'
     *  separated into the name/text fields.  If a line does not have a '\t' separated then the
     *  name will be the name of the optarg, and the help will be that entire line.  The *first*
     *  does get some tweaks to how the name and help is displayed.
     *  The first pass through will be indented 2 spaces on the left with the formated name occupying
     *  20 spaces (expanding if more than 20).  If the command is a 'buildmode' command the first
     *  character of the 'text' will be an asterisk.  The 'rest' of the line (assuming an 80 character '
     *  wide line for now) will be used to wrap the 'text' field honoring embedded newlines, and trying to
     *  wrap on nearest preceeding whitespace when it hits a boundary.  Subsequent lines will be indented
     *  by an additional 2 spaces, and will drop the asterisk.
     */
    char *working = NULL;
    char *nameptr = NULL;
    char *helpptr = NULL;
    char *lineptr = NULL;
    char *savelineptr = NULL;
    char *savetabptr = NULL;
    char *tname = NULL;
    int indent = 2;
    int helplen;
    char emptystring[] = "";

    /*
     * Print out actual text into a working buffer that we can then call 'strtok_r' on it.  This lets
     * us prepend some optional fields nice and easily.  At this point it is one big string, so we can
     * iterate over it making changes (strtok_r) as needed.
     */
    if (help_insert) {
      helplen = asprintf(&working, "%s%s%s%s%s", (optarg->flags & CLI_CMD_ALLOW_BUILDMODE) ? "* " : "", "type '",
                         optarg->name, "' to select ", optarg->name);
    } else {
      helplen = asprintf(&working, "%s%s", (optarg->flags & CLI_CMD_ALLOW_BUILDMODE) ? "* " : "", optarg->help);
    }

    // pull the first line
    helpptr = strtok_r(working, "\v", &savelineptr);
    nameptr = optarg->name;

    if (helplen < 0) {
      helpptr = emptystring;
      working = NULL;
    }

    // break things up into tab separated entities - always show the first entry
    do {
      char *leftcolumn;
      if (asprintf(&tname, "%s%s%s", delim_start, nameptr, delim_end) == -1) break;
      if (asprintf(&leftcolumn, "%*.*s%-20s ", indent, indent, "", tname) == -1) break;

      cli_int_wrap_help_line(leftcolumn, helpptr, comphelp);

      // clear out any delimiter settings and set indent for any subtext
      delim_start = DELIM_NONE;
      delim_end = DELIM_NONE;
      indent = 4;
      free_z(tname);
      free_z(leftcolumn);

      // we may not need to show all off the 'extra help', so loop here
      do {
        lineptr = strtok_r(NULL, "\v", &savelineptr);
        if (lineptr) {
          nameptr = strtok_r(lineptr, "\t", &savetabptr);
          helpptr = strtok_r(NULL, "\t", &savetabptr);
        }
      } while (lineptr && nameptr && helpptr && (next_word && (strncmp(next_word, nameptr, strlen(next_word)))));
    } while (lineptr && nameptr && helpptr);
    free_z(working);
  } else if (lastchar == CTRL('I')) {
    if (get_completions) {
      (*get_completions)(cli, optarg->name, next_word, comphelp);
    } else if ((!anchor_word || !strncmp(anchor_word, optarg->name, strlen(anchor_word))) &&
               (asprintf(&tptr, "%s%s%s", delim_start, optarg->name, delim_end) != -1)) {
      cli_add_comphelp_entry(comphelp, tptr);
      free_z(tptr);
    }
  }
}

static void cli_int_parse_optargs(struct cli_def *cli, struct cli_pipeline_stage *stage, struct cli_command *cmd,
                                  char lastchar, struct cli_comphelp *comphelp) {
  struct cli_optarg *optarg = NULL, *oaptr = NULL;
  int word_idx, word_incr, candidate_idx;
  struct cli_optarg *candidates[CLI_MAX_LINE_WORDS];
  char *value;
  int num_candidates = 0;
  int is_last_word = 0;
  int (*validator)(struct cli_def *, const char *name, const char *value);

  if (cli->buildmode)
    cli->found_optargs = cli->buildmode->found_optargs;
  else
    cli->found_optargs = stage->found_optargs;
  /*
   * Tab completion and help are *only* allowed at end of string, but we need to process the entire command to know what
   * has already been found.  There should be no ambiguities before the 'last' word.
   * Note specifically that for tab completions and help the *last* word can be a null pointer.
   */
  stage->error_word = NULL;

  /* Start our optarg and word pointers at the beginning.
   * optarg will be incremented *only* when an argument is identified.
   * word_idx will be incremented either by 1 (optflag or argument) or 2 (optional argument).
   */
  word_idx = stage->first_unmatched;
  optarg = cmd->optargs;
  num_candidates = 0;
  while (optarg && word_idx < stage->num_words && num_candidates <= 1) {
    num_candidates = 0;
    word_incr = 1;  // Assume we're only incrementing by a word - if we match an optional argument bump to 2

    /*
     * The initial loop here is to identify candidates based matching *this* word in order against:
     * - An exact match of the word to the optinal flag/argument name (yield exactly one match and exit the loop)
     * - A partial match for optional flag/argument name
     * - Candidate an argument.
     */

    for (oaptr = optarg; oaptr; oaptr = oaptr->next) {
      // Skip this option unless it matches privileges, MODE_ANY, the current mode, or the transient_mode
      if (cli->privilege < oaptr->privilege) continue;
      if ((oaptr->mode != cli->mode) && (oaptr->mode != cli->transient_mode) && (oaptr->mode != MODE_ANY)) continue;

      /*
       * Special cases:
       * - spot check
       * - a hyphenated option a hyphenated option
       * - an optional flag without validator, but the word matches the optarg name
       * - an optional flag with a validator *and* the word passes the validator,
       * - an optional argument where the word matches the argument name
       * a hit on any of these special cases is an automatic *only* candidate.
       *
       * Otherwise if the word is 'blank', could be an argument, or matches 'enough' of an option/flag it is a
       * candidate.
       * Once we accept an argument as a candidate, we're done looking for candidates as straight arguments are
       * required.
       */
      if (oaptr->flags & CLI_CMD_SPOT_CHECK && num_candidates == 0) {
        stage->status = (*oaptr->validator)(cli, NULL, NULL);
        if (stage->status != CLI_OK) {
          stage->error_word = stage->words[word_idx];
          cli_reprompt(cli);
          goto done;
        }
      } else if (stage->words[word_idx] && stage->words[word_idx][0] == '-' &&
                 (oaptr->flags & (CLI_CMD_HYPHENATED_OPTION))) {
        candidates[0] = oaptr;
        num_candidates = 1;
        break;
      } else if (stage->words[word_idx] && (oaptr->flags & CLI_CMD_OPTIONAL_FLAG) &&
                 ((oaptr->validator && (oaptr->validator(cli, oaptr->name, stage->words[word_idx]) == CLI_OK)) ||
                  (!oaptr->validator && !strcmp(oaptr->name, stage->words[word_idx])))) {
        candidates[0] = oaptr;
        num_candidates = 1;
        break;
      } else if (stage->words[word_idx] && (oaptr->flags & CLI_CMD_OPTIONAL_ARGUMENT) &&
                 !strcmp(oaptr->name, stage->words[word_idx])) {
        candidates[0] = oaptr;
        num_candidates = 1;
        break;
      } else if (!stage->words[word_idx] || (oaptr->flags & CLI_CMD_ARGUMENT) ||
                 !strncasecmp(oaptr->name, stage->words[word_idx], strlen(stage->words[word_idx]))) {
        candidates[num_candidates++] = oaptr;
      }
      if (oaptr->flags & CLI_CMD_ARGUMENT) {
        break;
      }
    }

    /*
     * Iterate over the list of candidates for this word.  There are several early exit cases to consider:
     * - If we have no candidates then we're done - any remaining words must be processed by the command callback
     * - If we have more than one candidate evaluating for execution punt hard after complaining.
     * - If we have more than one candidate and we're not at end-of-line (
     */
    if (num_candidates == 0) break;
    if (num_candidates > 1 && (lastchar == '\0' || word_idx < (stage->num_words - 1))) {
      stage->error_word = stage->words[word_idx];
      stage->status = CLI_AMBIGUOUS;
      cli_error(cli, "\nAmbiguous option/argument for command %s", stage->command->command);
      goto done;
    }

    /*
     * So now we could have one or more candidates.  We need to call get help/completions *only* if this is the
     * 'last-word'.
     * Remember that last word for optional arguments is last or next to last....
     */
    if (lastchar != '\0') {
      int called_comphelp = 0;
      for (candidate_idx = 0; candidate_idx < num_candidates; candidate_idx++) {
        oaptr = candidates[candidate_idx];

        // Need to know *which* word we're trying to complete for optional_args, hence the difference calls
        if (((oaptr->flags & (CLI_CMD_OPTIONAL_FLAG | CLI_CMD_ARGUMENT)) && (word_idx == (stage->num_words - 1))) ||
            (oaptr->flags & (CLI_CMD_OPTIONAL_ARGUMENT | CLI_CMD_HYPHENATED_OPTION) &&
             word_idx == (stage->num_words - 1))) {
          cli_get_optarg_comphelp(cli, oaptr, comphelp, num_candidates, lastchar, stage->words[word_idx],
                                  stage->words[word_idx]);
          called_comphelp = 1;
        } else if (oaptr->flags & CLI_CMD_OPTIONAL_ARGUMENT && word_idx == (stage->num_words - 2)) {
          cli_get_optarg_comphelp(cli, oaptr, comphelp, num_candidates, lastchar, stage->words[word_idx],
                                  stage->words[word_idx + 1]);
          called_comphelp = 1;
        }
      }
      // If we were 'end-of-word' and looked for completions/help, return to user
      if (called_comphelp) {
        stage->status = CLI_OK;
        goto done;
      }
    }

    // Set some values for use later - makes code much easier to read
    value = stage->words[word_idx];
    oaptr = candidates[0];
    validator = oaptr->validator;
    if ((oaptr->flags & (CLI_CMD_OPTIONAL_FLAG | CLI_CMD_ARGUMENT) && word_idx == (stage->num_words - 1)) ||
        (oaptr->flags & CLI_CMD_OPTIONAL_ARGUMENT && word_idx == (stage->num_words - 2))) {
      is_last_word = 1;
    }

    if (oaptr->flags & CLI_CMD_OPTIONAL_ARGUMENT) {
      word_incr = 2;
      if (!stage->words[word_idx + 1] && lastchar == '\0') {
        // Hit an optional argument that does not have a value with it
        cli_error(cli, "Optional argument %s requires a value", stage->words[word_idx]);
        stage->error_word = stage->words[word_idx];
        stage->status = CLI_MISSING_VALUE;
        goto done;
      }
      value = stage->words[word_idx + 1];
    }

    /*
     * We're not at end of string and doing help/completions.
     * So see if our value is 'valid', to save it, and see if we have any extra processing to do such as a transient
     * mode check or enter build mode.
     */

    if (!validator || (*validator)(cli, oaptr->name, value) == CLI_OK) {
      if (oaptr->flags & CLI_CMD_DO_NOT_RECORD) {
        // We want completion and validation, but then leave this 'value' to be seen - used *only* by buildmode as
        // argv[0] with argc=1
        break;
      } else {
        // Need to combine remaining words if the CLI_CMD_REMAINDER_OF_LINE flag it set, then we're done processing
        int set_value_return = 0;

        if (oaptr->flags & CLI_CMD_REMAINDER_OF_LINE) {
          char *combined = NULL;
          combined = join_words(stage->num_words - word_idx, stage->words + word_idx);
          set_value_return = cli_set_optarg_value(cli, oaptr->name, combined, 0);
          free_z(combined);
        } else {
          set_value_return = cli_set_optarg_value(cli, oaptr->name, value, oaptr->flags & CLI_CMD_OPTION_MULTIPLE);
        }

        if (set_value_return != CLI_OK) {
          cli_error(cli, "%sProblem setting value for command argument %s", lastchar == '\0' ? "" : "\n",
                    stage->words[word_idx]);
          cli_reprompt(cli);
          stage->error_word = stage->words[word_idx];
          stage->status = CLI_ERROR;
          goto done;
        }
      }
    } else {
      cli_error(cli, "%sProblem parsing command setting %s with value %s", lastchar == '\0' ? "" : "\n", oaptr->name,
                stage->words[word_idx]);
      cli_reprompt(cli);
      stage->error_word = stage->words[word_idx];
      stage->status = CLI_ERROR;
      goto done;
    }

    // If this optarg can set the transient mode, then evaluate it if we're not at last word
    if (oaptr->transient_mode && oaptr->transient_mode(cli, oaptr->name, value)) {
      stage->error_word = stage->words[word_idx];
      stage->status = CLI_ERROR;
      goto done;
    }

    // Only process CLI_CMD_ALLOW_BUILDMODE if we're not already in buildmode, parsing command (stage 0), and this is
    // the last word
    if (!cli->disallow_buildmode && (stage->status == CLI_OK) && (oaptr->flags & CLI_CMD_ALLOW_BUILDMODE) &&
        is_last_word) {
      stage->status = cli_int_enter_buildmode(cli, stage, value);
      goto done;
    }

    // Optional flags and arguments can appear multiple times, and in any order.  We only advance
    // from our starting optarg if the matching optarg is a true argument.
    if (oaptr->flags & CLI_CMD_ARGUMENT) {
      // Advance past this argument entry
      optarg = oaptr->next;
    }

    word_idx += word_incr;
    stage->first_unmatched = word_idx;
  }

  // If we're evaluating the command for execution, ensure we have all required arguments.
  if (lastchar == '\0') {
    for (; optarg; optarg = optarg->next) {
      if (cli->privilege < optarg->privilege) continue;
      if ((optarg->mode != cli->mode) && (optarg->mode != cli->transient_mode) && (optarg->mode != MODE_ANY)) continue;
      if (optarg->flags & CLI_CMD_DO_NOT_RECORD) continue;
      if (optarg->flags & CLI_CMD_ARGUMENT) {
        cli_error(cli, "Incomplete command, missing required argument '%s' for command '%s'", optarg->name,
                  cmd->command);
        stage->status = CLI_MISSING_ARGUMENT;
        goto done;
      }
    }
  }

done:
  if (cli->buildmode)
    cli->buildmode->found_optargs = cli->found_optargs;
  else
    stage->found_optargs = cli->found_optargs;
  return;
}

void cli_unregister_all_commands(struct cli_def *cli) {
  cli_unregister_tree(cli, cli->commands, CLI_REGULAR_COMMAND);
}

void cli_unregister_all_filters(struct cli_def *cli) {
  cli_unregister_tree(cli, cli->commands, CLI_FILTER_COMMAND);
}

/*
 * Several routines were declared as internal, but would be useful for external use also
 * Rename them so they can be exposed, but have original routines simply call the 'public' ones
 */

int cli_int_quit(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  return cli_quit(cli, command, argv, argc);
}

int cli_int_help(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  return cli_help(cli, command, argv, argc);
}

int cli_int_history(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  return cli_history(cli, command, argv, argc);
}

int cli_int_exit(struct cli_def *cli, const char *command, char *argv[], int argc) {
  return cli_exit(cli, command, argv, argc);
}

int cli_int_enable(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  return cli_enable(cli, command, argv, argc);
}

int cli_int_disable(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  return cli_disable(cli, command, argv, argc);
}

void cli_dump_optargs_and_args(struct cli_def *cli, const char *text, char *argv[], int argc) {
  int i;
  struct cli_optarg_pair *optargs;
  cli_print(cli, "%s: mode = %d, transient_mode = %d", text, cli->mode, cli->transient_mode);
  cli_print(cli, "Identified optargs");
  for (optargs = cli_get_all_found_optargs(cli), i = 0; optargs; optargs = optargs->next, i++)
    cli_print(cli, "%2d  %s=%s", i, optargs->name, optargs->value);
  cli_print(cli, "Extra args");
  for (i = 0; i < argc; i++) cli_print(cli, "%2d %s", i, argv[i]);
}
