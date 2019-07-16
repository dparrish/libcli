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

// vim:sw=4 tw=120 et

#ifdef __GNUC__
#define UNUSED(d) d __attribute__((unused))
#else
#define UNUSED(d) d
#endif

#define MATCH_REGEX 1
#define MATCH_INVERT 2

#ifdef WIN32
/*
 * Stupid windows has multiple namespaces for filedescriptors, with different
 * read/write functions required for each ..
 */
int read(int fd, void *buf, unsigned int count) {
  return recv(fd, buf, count, 0);
}

int write(int fd, const void *buf, unsigned int count) {
  return send(fd, buf, count, 0);
}

int vasprintf(char **strp, const char *fmt, va_list args) {
  int size;
  va_list argCopy;

  // do initial vsnprintf on a copy of the va_list
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

/*
 * Dummy definitions to allow compilation on Windows
 */
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
  STATE_ENABLE
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

/* free and zero (to avoid double-free) */
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
static int cli_int_add_optarg_value(struct cli_def *cli, const char *name, const char *value, int allow_multiple);
static void cli_free_command(struct cli_command *cmd);
static int cli_int_unregister_command_core(struct cli_def *cli, const char *command, int command_type);
static int cli_int_unregister_buildmode_command(struct cli_def *cli, const char *command) __attribute__((unused));
static struct cli_command *cli_int_register_buildmode_command(
    struct cli_def *cli, struct cli_command *parent, const char *command,
    int (*callback)(struct cli_def *cli, const char *, char **, int), int privilege, int mode, const char *help);
static int cli_int_buildmode_cmd_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_flag_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_flag_multiple_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_cancel_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_exit_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_show_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_unset_cback(struct cli_def *cli, const char *command, char *argv[], int argc);
static int cli_int_buildmode_unset_completor(struct cli_def *cli, const char *name, const char *word,
                                             struct cli_comphelp *comphelp);
static int cli_int_buildmode_unset_validator(struct cli_def *cli, const char *name, const char *value);
static int cli_int_execute_buildmode(struct cli_def *cli);
static void cli_int_free_found_optargs(struct cli_optarg_pair **optarg_pair);
// static void cli_int_unset_optarg_value(struct cli_def *cli, const char *name ) ;
static struct cli_pipeline *cli_int_generate_pipeline(struct cli_def *cli, const char *command);
static int cli_int_validate_pipeline(struct cli_def *cli, struct cli_pipeline *pipeline);
static int cli_int_execute_pipeline(struct cli_def *cli, struct cli_pipeline *pipeline);
inline void cli_int_show_pipeline(struct cli_def *cli, struct cli_pipeline *pipeline);
static struct cli_pipeline *cli_int_free_pipeline(struct cli_pipeline *pipeline);
static struct cli_command *cli_register_command_core(struct cli_def *cli, struct cli_command *parent,
                                                     struct cli_command *c);

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

struct cli_command *cli_register_command_core(struct cli_def *cli, struct cli_command *parent, struct cli_command *c) {
  struct cli_command *p;

  if (!c) return NULL;

  if (parent) {
    if (!parent->children) {
      parent->children = c;
    } else {
      for (p = parent->children; p && p->next; p = p->next)
        ;
      if (p) p->next = c;
    }
  } else {
    if (!cli->commands) {
      cli->commands = c;
    } else {
      for (p = cli->commands; p && p->next; p = p->next)
        ;
      if (p) p->next = c;
    }
  }
  return c;
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

  c->parent = parent;
  c->privilege = privilege;
  c->mode = mode;
  if (help && !(c->help = strdup(help))) {
    free(c->command);
    free(c);
    return NULL;
  }

  if (!cli_register_command_core(cli, parent, c)) {
    cli_free_command(c);
    c = NULL;
  }
  return c;
}

static void cli_free_command(struct cli_command *cmd) {
  struct cli_command *c, *p;

  for (c = cmd->children; c;) {
    p = c->next;
    cli_free_command(c);
    c = p;
  }

  free(cmd->command);
  if (cmd->help) free(cmd->help);
  if (cmd->optargs) cli_unregister_all_optarg(cmd);
  free(cmd);
}

int cli_int_unregister_command_core(struct cli_def *cli, const char *command, int command_type) {
  struct cli_command *c, *p = NULL;

  if (!command) return -1;
  if (!cli->commands) return CLI_OK;

  for (c = cli->commands; c; c = c->next) {
    if ((strcmp(c->command, command) == 0) && (c->command_type == command_type)) {
      if (p)
        p->next = c->next;
      else
        cli->commands = c->next;

      cli_free_command(c);
      return CLI_OK;
    }
    p = c;
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

int cli_int_enable(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  if (cli->privilege == PRIVILEGE_PRIVILEGED) return CLI_OK;

  if (!cli->enable_password && !cli->enable_callback) {
    /* no password required, set privilege immediately */
    cli_set_privilege(cli, PRIVILEGE_PRIVILEGED);
    cli_set_configmode(cli, MODE_EXEC, NULL);
  } else {
    /* require password entry */
    cli->state = STATE_ENABLE_PASSWORD;
  }

  return CLI_OK;
}

int cli_int_disable(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
  cli_set_configmode(cli, MODE_EXEC, NULL);
  return CLI_OK;
}

int cli_int_help(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  cli_error(cli, "\nCommands available:");
  cli_show_help(cli, cli->commands);
  return CLI_OK;
}

int cli_int_history(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  int i;

  cli_error(cli, "\nCommand history:");
  for (i = 0; i < MAX_HISTORY; i++) {
    if (cli->history[i]) cli_error(cli, "%3d. %s", i, cli->history[i]);
  }

  return CLI_OK;
}

int cli_int_quit(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
  cli_set_configmode(cli, MODE_EXEC, NULL);
  return CLI_QUIT;
}

int cli_int_exit(struct cli_def *cli, const char *command, char *argv[], int argc) {
  if (cli->mode == MODE_EXEC) return cli_int_quit(cli, command, argv, argc);

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

  cli_register_command(cli, 0, "help", cli_int_help, PRIVILEGE_UNPRIVILEGED, MODE_ANY, "Show available commands");
  cli_register_command(cli, 0, "quit", cli_int_quit, PRIVILEGE_UNPRIVILEGED, MODE_ANY, "Disconnect");
  cli_register_command(cli, 0, "logout", cli_int_quit, PRIVILEGE_UNPRIVILEGED, MODE_ANY, "Disconnect");
  cli_register_command(cli, 0, "exit", cli_int_exit, PRIVILEGE_UNPRIVILEGED, MODE_ANY, "Exit from current mode");
  cli_register_command(cli, 0, "history", cli_int_history, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
                       "Show a list of previously run commands");
  cli_register_command(cli, 0, "enable", cli_int_enable, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                       "Turn on privileged commands");
  cli_register_command(cli, 0, "disable", cli_int_disable, PRIVILEGE_PRIVILEGED, MODE_EXEC,
                       "Turn off privileged commands");

  c = cli_register_command(cli, 0, "configure", 0, PRIVILEGE_PRIVILEGED, MODE_EXEC, "Enter configuration mode");
  cli_register_command(cli, c, "terminal", cli_int_configure_terminal, PRIVILEGE_PRIVILEGED, MODE_EXEC,
                       "Configure from the terminal");

  // and now the built in filters
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

  c = cli_register_filter(cli, "count", cli_count_filter_init, cli_count_filter, PRIVILEGE_UNPRIVILEGED, MODE_ANY,
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
    if ((c->command_type == command_type) || (command_type == CLI_ANY_COMMAND)) {
      // handle case where we're deleting *first* command
      if (c == cli->commands) cli->commands = p;
      // Unregister all child commands
      if (c->children) cli_unregister_tree(cli, c->children, command_type);
      if (c->optargs) cli_unregister_all_optarg(c);
      if (c->command) free(c->command);
      if (c->help) free(c->help);
      free(c);
    }
    c = p;
  }
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
    if (!*p || *p == inquote || (word_start && !inquote && (isspace(*p) || *p == '|'))) {
      if (word_start) {
        int len = p - word_start;

        memcpy(words[nwords] = malloc(len + 1), word_start, len);
        words[nwords++][len] = 0;
      }

      if (!*p) break;

      if (inquote) p++; /* skip over trailing quote */

      inquote = 0;
      word_start = 0;
    } else if (*p == '"' || *p == '\'') {
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

  // split command into pipeline stages,

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

  if (!(pipeline = cli_int_generate_pipeline(cli, command))) goto out;

  stage = &pipeline->stage[pipeline->num_stages - 1];

  // check to see if either *no* input, or if the lastchar is a tab.
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
    n = c->next;

    if (c->command_type != command_type) continue;
    if (cli->privilege < c->privilege) continue;

    if (c->mode != cli->mode && c->mode != MODE_ANY) continue;

    if (stage->words[i] && strncasecmp(c->command, stage->words[i], strlen(stage->words[i]))) continue;

    // special case for 'buildmode' - skip if the argument for this command was seen, unless MULTIPLE flag is set
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

      // if we have no more children, we've matched the *command* - remember this
      if (!c->children) {
        break;
      }

      i++;
      continue;
    }

    if ((lastchar == '?')) {
      if (asprintf(&strptr, "  %-20s %s", c->command, (c->help) ? c->help : "") != -1) {
        cli_add_comphelp_entry(comphelp, strptr);
        free_z(strptr);
      }
    } else {
      cli_add_comphelp_entry(comphelp, c->command);
    }
  }

out:
  if (c) {
    // advance past first word of stage
    i++;
    stage->first_unmatched = i;
    if (c->optargs) {

      cli_int_parse_optargs(cli, stage, c, lastchar, comphelp);
    } else if (lastchar == '?') {
      // special case for getting help with no defined optargs....
      comphelp->num_entries = -1;
    }
  }

  pipeline = cli_int_free_pipeline(pipeline);
}

static void cli_clear_line(int sockfd, char *cmd, int l, int cursor) {
  // use cmd as our buffer, and overwrite contents as needed
  // Backspace to beginning
  memset((char *)cmd, '\b', cursor);
  _write(sockfd, cmd, cursor);

  // overwrite existing cmd with spaces
  memset((char *)cmd, ' ', l);
  _write(sockfd, cmd, l);

  // and backspace again to beginning
  memset((char *)cmd, '\b', l);
  _write(sockfd, cmd, l);

  // null cmd buffer
  memset((char *)cmd, 0, l);

  // reset pointers to beginning
  cursor = l = 0;
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

#define DES_PREFIX "{crypt}" /* to distinguish clear text from DES crypted */
#define MD5_PREFIX "$1$"

static int pass_matches(const char *pass, const char *try) {
  int des;
  if ((des = !strncasecmp(pass, DES_PREFIX, sizeof(DES_PREFIX) - 1))) pass += sizeof(DES_PREFIX) - 1;

#ifndef WIN32
  /*
   * TODO - find a small crypt(3) function for use on windows
   */
  if (des || !strncmp(pass, MD5_PREFIX, sizeof(MD5_PREFIX) - 1)) try = crypt(try, pass);
#endif

  return !strcmp(pass, try);
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

  /* start off in unprivileged mode */
  cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
  cli_set_configmode(cli, MODE_EXEC, NULL);

  /* no auth required? */
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
       * ensure our transient mode is reset to the starting mode on *each* loop traversal
       * transient mode is valid only while a command is being evaluated/executed
       */
      cli->transient_mode = cli->mode;

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
        /* select error */
        if (errno == EINTR) continue;

        perror("select");
        l = -1;
        break;
      }

      if (sr == 0) {
        /* timeout every second */
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

      /* handle ANSI arrows */
      if (esc) {
        if (esc == '[') {
          /* remap to readline control codes */
          switch (c) {
            case 'A': /* Up */
              c = CTRL('P');
              break;

            case 'B': /* Down */
              c = CTRL('N');
              break;

            case 'C': /* Right */
              c = CTRL('F');
              break;

            case 'D': /* Left */
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

      /* back word, backspace/delete */
      if (c == CTRL('W') || c == CTRL('H') || c == 0x7f) {
        int back = 0;

        if (c == CTRL('W')) /* word */
        {
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
        } else /* char */
        {
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
                // back up one space, then write current buffer followed by a space
                _write(sockfd, "\b", 1);
                _write(sockfd, cmd + cursor, l - cursor);
                _write(sockfd, " ", 1);

                // move everything one char left
                memmove(cmd + cursor - 1, cmd + cursor, l - cursor);

                // set former last char to null
                cmd[l - 1] = 0;

                // and reposition cursor
                for (i = l; i >= cursor; i--) _write(sockfd, "\b", 1);
              }
              cursor--;
            }
            l--;
          }

          continue;
        }
      }

      /* redraw */
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

      /* clear line */
      if (c == CTRL('U')) {
        if (cli->state == STATE_PASSWORD || cli->state == STATE_ENABLE_PASSWORD)
          memset(cmd, 0, l);
        else
          cli_clear_line(sockfd, cmd, l, cursor);

        l = cursor = 0;
        continue;
      }

      /* kill to EOL */
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

      /* EOT */
      if (c == CTRL('D')) {
        if (cli->state == STATE_PASSWORD || cli->state == STATE_ENABLE_PASSWORD) break;

        if (l) continue;

        l = -1;
        break;
      }

      /* disable */
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

      /* TAB completion */
      if (c == CTRL('I')) {
        struct cli_comphelp comphelp = {0};

        if (cli->state == STATE_LOGIN || cli->state == STATE_PASSWORD || cli->state == STATE_ENABLE_PASSWORD) continue;

        if (cursor != l) continue;

        cli_get_completions(cli, cmd, c, &comphelp);
        if (comphelp.num_entries == 0) {
          _write(sockfd, "\a", 1);
        } else if (lastchar == CTRL('I')) {
          // double tab
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
          if ((comphelp.entries[0][0] != '[') && (comphelp.entries[0][0] != '<')) {
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
            // and now forget the tab, since we just found a single match
            lastchar = '\0';
          } else {
            // Yes, we had a match, but it wasn't required - remember the tab in case the user double tabs....
            lastchar = CTRL('I');
          }
        } else if (comphelp.num_entries > 1) {
          /* More than one completion
           * Show as many characters as we can until the completions start to differ
           */
          lastchar = c;
          int i, j, k = 0;
          char *tptr = comphelp.entries[0];

          /* quickly try to see where our entries differ
           * corner cases
           * - if all entries are optional, don't show
           *   *any* options unless user has provided a letter.
           * - if any entry starts with '<' then don't fill in
           *   anything.
           */

          // skip a leading '['
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

          /*
           * ok, try to show minimum match string if we have a
           * non-zero k and the first letter of the last word
           * is not '['
           */
          if (k && (comphelp.entries[comphelp.num_entries - 1][0] != '[')) {
            for (; l > 0; l--, cursor--) {
              if (cmd[l - 1] == ' ' || cmd[l - 1] == '|' || (comphelp.comma_separated && cmd[l - 1] == ',')) break;
              _write(sockfd, "\b", 1);
            }
            strncpy((cmd + l), tptr, k);
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

      /* '?' at end of line - generate applicable 'help' messages */
      if ((c == '?') && (cursor == l)) {
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

      /* history */
      if (c == CTRL('P') || c == CTRL('N')) {
        int history_found = 0;

        if (cli->state == STATE_LOGIN || cli->state == STATE_PASSWORD || cli->state == STATE_ENABLE_PASSWORD) continue;

        if (c == CTRL('P'))  // Up
        {
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
        } else  // Down
        {
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

      /* left/right cursor motion */
      if (c == CTRL('B') || c == CTRL('F')) {
        if (c == CTRL('B')) /* Left */
        {
          if (cursor) {
            if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) _write(sockfd, "\b", 1);

            cursor--;
          }
        } else /* Right */
        {
          if (cursor < l) {
            if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) _write(sockfd, &cmd[cursor], 1);

            cursor++;
          }
        }

        continue;
      }

      /* start of line */
      if (c == CTRL('A')) {
        if (cursor) {
          if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) {
            _write(sockfd, "\r", 1);
            show_prompt(cli, sockfd);
          }

          cursor = 0;
        }

        continue;
      }

      /* end of line */
      if (c == CTRL('E')) {
        if (cursor < l) {
          if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD)
            _write(sockfd, &cmd[cursor], l - cursor);

          cursor = l;
        }

        continue;
      }

      /* normal character typed */
      if (cursor == l) {
        /* append to end of line if not at end-of-buffer */
        if (l < CLI_MAX_LINE_LENGTH - 1) {
          cmd[cursor] = c;
          l++;
          cursor++;
        } else {
          // end-of-buffer, ensure null terminated
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

        // IMPORTANT - if at end of buffer, set last char to NULL and don't
        // change length otherwise bump length by 1
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

      /* require login */
      free_z(username);
      if (!(username = strdup(cmd))) return 0;
      cli->state = STATE_PASSWORD;
      cli->showprompt = 1;
    } else if (cli->state == STATE_PASSWORD) {
      /* require password */
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
        /* check stored static enable password */
        if (pass_matches(cli->enable_password, cmd)) allowed++;
      }

      if (!allowed && cli->enable_callback) {
        /* check callback */
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
          // unable to enter buildmode successfully
          cli_print(cli, "Failure entering build mode for '%s'", cli->buildmode->cname);
          cli_int_free_buildmode(cli);
          continue;
        case CLI_BUILDMODE_CANCEL:
          // called if user enters 'cancel'
          cli_print(cli, "Canceling build mode for '%s'", cli->buildmode->cname);
          cli_int_free_buildmode(cli);
          break;
        case CLI_BUILDMODE_EXIT:
          // called when user enters exit - rebuild *entire* command line
          // recall all located optargs
          cli->found_optargs = cli->buildmode->found_optargs;
          rc = cli_int_execute_buildmode(cli);
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

    // Update the last_action time now as the last command run could take a
    // long time to return
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

    if (fgets(buf, CLI_MAX_LINE_LENGTH - 1, fh) == NULL) break; /* end of file */

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
  cli_set_configmode(cli, oldmode, NULL /* didn't save desc */);

  return CLI_OK;
}

static void _print(struct cli_def *cli, int print_mode, const char *format, va_list ap) {
  int n;
  char *p = NULL;

  if (!cli) return;  // sanity check

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
  // valid search flags starts with a hyphen, then any number of i,v, or e characters

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

          case 'e':  // implies next term is search string, so stop processing flags
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
    /*
     * No regex functions in windows, so return an error
     */
    return CLI_ERROR;
  }
#endif

  return CLI_OK;
}

int cli_match_filter(UNUSED(struct cli_def *cli), const char *string, void *data) {
  struct cli_match_filter_state *state = data;
  int r = CLI_ERROR;

  if (!string)  // clean up
  {
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

  // Do not have to check from/to since we would not have gotten here if we were
  // missing a required argument
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

  if (!string)  // clean up
  {
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

  if (!string)  // clean up
  {
    // print count
    if (cli->client) fprintf(cli->client, "%d\r\n", *count);

    free(count);
    return CLI_OK;
  }

  while (isspace(*string)) string++;

  if (*string) (*count)++;  // only count non-blank lines

  return CLI_ERROR;  // no output
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
  // filters are all registered at the top level
  if (!cli_register_command_core(cli, NULL, c)) {
    cli_free_command(c);
    c = NULL;
  }
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
      if (find_after && (find_after == optarg_pair->value)) {
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

int cli_register_optarg(struct cli_command *cmd, const char *name, int flags, int privilege, int mode, const char *help,
                        int (*get_completions)(struct cli_def *cli, const char *, const char *, struct cli_comphelp *),
                        int (*validator)(struct cli_def *cli, const char *, const char *),
                        int (*transient_mode)(struct cli_def *cli, const char *, const char *)) {
  struct cli_optarg *optarg;
  struct cli_optarg *lastopt = NULL;
  struct cli_optarg *ptr = NULL;
  int retval = CLI_ERROR;

  // name must not already exist with this priv/mode
  for (ptr = cmd->optargs, lastopt = NULL; ptr; lastopt = ptr, ptr = ptr->next) {
    if (!(strcmp(name, ptr->name)) && (ptr->mode == mode) && (ptr->privilege == privilege)) {
      return CLI_ERROR;
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
  }
  return retval;
}

int cli_unregister_optarg(struct cli_command *cmd, const char *name) {
  struct cli_optarg *ptr;
  struct cli_optarg *lastptr;
  int retval = CLI_ERROR;
  // iterate looking for this option name, stoping at end or if name matches
  for (lastptr = NULL, ptr = cmd->optargs; ptr && strcmp(ptr->name, name); lastptr = ptr, ptr = ptr->next) {
    ;
  }

  // if ptr, then we found the optarg to delete
  if (ptr) {
    if (lastptr) {
      // not first optarg
      lastptr->next = ptr->next;
      ptr->next = NULL;
    } else {
      // first optarg
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

int cli_int_add_optarg_value(struct cli_def *cli, const char *name, const char *value, int allow_multiple) {
  struct cli_optarg_pair *optarg_pair, **anchor;
  int rc = CLI_ERROR;

  for (optarg_pair = cli->found_optargs, anchor = &cli->found_optargs; optarg_pair;
       anchor = &optarg_pair->next, optarg_pair = optarg_pair->next) {
    // break if we found this name *and* allow_multiple is false
    if (!strcmp(optarg_pair->name, name) && !allow_multiple) {
      break;
    }
  }
  // if we *didn't* find this, then allocate a new entry before proceeding
  if (!optarg_pair) {
    optarg_pair = (struct cli_optarg_pair *)calloc(1, sizeof(struct cli_optarg_pair));
    *anchor = optarg_pair;
  }
  // set the value
  if (optarg_pair) {
    // name is null only if we didn't find it
    if (!optarg_pair->name) optarg_pair->name = strdup(name);

    // value may be overwritten, so free any old value.
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

  printf("cli_get_optarg_value entry - looking for <%s> after <%p>\n", name, (void *)find_after);
  for (optarg_pair = cli->found_optargs; !value && optarg_pair; optarg_pair = optarg_pair->next) {
    printf("  Checking %s with value %s <%p> \n", optarg_pair->name, optarg_pair->value, (void *)optarg_pair->value);

    // check next entry if this isn't our name
    if (strcasecmp(optarg_pair->name, name)) continue;

    // did we have a find_after, then ignore anything up until our find_after match
    if ((find_after) && (optarg_pair->value == find_after)) {
      find_after = NULL;
      continue;
    } else if (!find_after) {
      value = optarg_pair->value;
    }
  }
  printf("cli_get_optarg_value exit - returning <%s><%p>\n", name, value);
  return value;
}

void cli_int_free_buildmode(struct cli_def *cli) {

  if (!cli || !cli->buildmode) return;
  //  cli_unregister_tree(cli, cli->commands, CLI_BUILDMODE_COMMAND);
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

  if ((!cli) || !(buildmode = (struct cli_buildmode *)calloc(1, sizeof(struct cli_buildmode)))) {
    cli_error(cli, "Unable to build buildmode mode for command %s", stage->command->command);
    return CLI_BUILDMODE_ERROR;
  }

  // clean up any shrapnel from earlier - shouldn't be any but....
  if (cli->buildmode) {
    cli_int_free_buildmode(cli);
  }

  // assign it so cli_int_register_buildmode_command() has something to work with
  cli->buildmode = buildmode;
  cli->buildmode->mode = cli->mode;
  cli->buildmode->transient_mode = cli->transient_mode;
  if (mode_text) cli->buildmode->mode_text = strdup(mode_text);

  // need this to verify we have all *required* arguments
  cli->buildmode->command = stage->command;

  // build new *limited* list of commands from this commands optargs
  for (optarg = stage->command->optargs; optarg; optarg = optarg->next) {
    // don't allow anything that could redefine our mode or buildmode mode, or redefine exit/cancel
    if (!strcmp(optarg->name, "cancel") || (!strcmp(optarg->name, "exit"))) {
      cli_error(cli, "Unable to build buildmode mode from optarg named %s", optarg->name);
      return CLI_BUILDMODE_ERROR;
    }
    if (optarg->flags & (CLI_CMD_ALLOW_BUILDMODE | CLI_CMD_TRANSIENT_MODE)) continue;
    if (optarg->mode != cli->mode && optarg->mode != cli->transient_mode)
      continue;
    else if (optarg->flags & (CLI_CMD_OPTIONAL_ARGUMENT | CLI_CMD_ARGUMENT)) {
      if ((c = cli_int_register_buildmode_command(cli, NULL, optarg->name, cli_int_buildmode_cmd_cback,
                                                  optarg->privilege, cli->mode, optarg->help))) {
        cli_register_optarg(c, optarg->name, CLI_CMD_ARGUMENT | (optarg->flags & CLI_CMD_OPTION_MULTIPLE),
                            optarg->privilege, cli->mode, optarg->help, optarg->get_completions, optarg->validator,
                            NULL);
      } else {
        return CLI_BUILDMODE_ERROR;
      }
    } else {
      if (optarg->flags & CLI_CMD_OPTION_MULTIPLE) {
        if (!cli_int_register_buildmode_command(cli, NULL, optarg->name, cli_int_buildmode_flag_multiple_cback,
                                                optarg->privilege, cli->mode, optarg->help)) {
          return CLI_BUILDMODE_ERROR;
        }
      } else {
        if (!cli_int_register_buildmode_command(cli, NULL, optarg->name, cli_int_buildmode_flag_cback,
                                                optarg->privilege, cli->mode, optarg->help)) {
          return CLI_BUILDMODE_ERROR;
        }
      }
    }
  }
  cli->buildmode->cname = strdup(cli_command_name(cli, stage->command));
  // and lastly two 'always there' commands to cancel current mode and to execute the command
  cli_int_register_buildmode_command(cli, NULL, "cancel", cli_int_buildmode_cancel_cback, PRIVILEGE_UNPRIVILEGED,
                                     cli->mode, "Cancel command");
  cli_int_register_buildmode_command(cli, NULL, "exit", cli_int_buildmode_exit_cback, PRIVILEGE_UNPRIVILEGED, cli->mode,
                                     "Exit and execute command");
  cli_int_register_buildmode_command(cli, NULL, "show", cli_int_buildmode_show_cback, PRIVILEGE_UNPRIVILEGED, cli->mode,
                                     "Show current settings");
  c = cli_int_register_buildmode_command(cli, NULL, "unset", cli_int_buildmode_unset_cback, PRIVILEGE_UNPRIVILEGED,
                                         cli->mode, "Unset a setting");
  cli_register_optarg(c, "setting", CLI_CMD_ARGUMENT | CLI_CMD_DO_NOT_RECORD, PRIVILEGE_UNPRIVILEGED, cli->mode,
                      "setting to clear", cli_int_buildmode_unset_completor, cli_int_buildmode_unset_validator, NULL);

  return CLI_BUILDMODE_START;
}

int cli_int_unregister_buildmode_command(struct cli_def *cli, const char *command) {
  return cli_int_unregister_command_core(cli, command, CLI_BUILDMODE_COMMAND);
}

struct cli_command *cli_int_register_buildmode_command(struct cli_def *cli, struct cli_command *parent,
                                                       const char *command,
                                                       int (*callback)(struct cli_def *cli, const char *, char **, int),
                                                       int privilege, int mode, const char *help) {
  struct cli_command *c;

  if (!command) return NULL;
  if (!(c = calloc(sizeof(struct cli_command), 1))) return NULL;

  c->callback = callback;
  c->next = NULL;
  if (!(c->command = strdup(command))) {
    free(c);
    return NULL;
  }

  c->command_type = CLI_BUILDMODE_COMMAND;
  c->parent = parent;
  c->privilege = privilege;
  c->mode = mode;
  if (help && !(c->help = strdup(help))) {
    free(c->command);
    free(c);
    return NULL;
  }
  // buildmode commmands are all registered at the top level
  if (!cli_register_command_core(cli, NULL, c)) {
    cli_free_command(c);
    c = NULL;
  }
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
    rc = cli_run_command(cli, cmdline);
  }
  free_z(cmdline);
  cli_int_free_buildmode(cli);
  return rc;
}

char *cli_int_buildmode_extend_cmdline(char *cmdline, char *word) {
  char *tptr = NULL;
  char *cptr = NULL;
  size_t oldlen = strlen(cmdline);
  size_t wordlen = strlen(word);
  int add_quotes = 0;

  /*
   * Allocate enough space to hold the old string, a space, and the new string (including null terminator).
   * Also include enough space for a quote around the string if it contains a whitespace character
   */
  if ((tptr = (char *)realloc(cmdline, oldlen + 1 + wordlen + 1 + 2))) {
    strcat(tptr, " ");
    for (cptr = word; *cptr; cptr++) {
      if (isspace(*cptr)) {
        add_quotes = 1;
        break;
      }
    }
    if (add_quotes) strcat(tptr, "'");
    strcat(tptr, word);
    if (add_quotes) strcat(tptr, "'");
  }
  return tptr;
}

int cli_int_buildmode_cmd_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  int rc = CLI_BUILDMODE_EXTEND;

  if (argc) {
    cli_error(cli, "Extra arguments on command line, command ignored.");
    rc = CLI_ERROR;
  }
  return rc;
}

// a 'flag' callback has no optargs, so we need to set it ourself based on *this* command
int cli_int_buildmode_flag_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  int rc = CLI_BUILDMODE_EXTEND;

  if (argc) {
    cli_error(cli, "Extra arguments on command line, command ignored.");
    rc = CLI_ERROR;
  }
  if (cli_int_add_optarg_value(cli, command, command, 0)) {
    cli_error(cli, "Problem setting value for optional flag %s", command);
    rc = CLI_ERROR;
  }
  return rc;
}

// a 'flag' callback has no optargs, so we need to set it ourself based on *this* command
int cli_int_buildmode_flag_multiple_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  int rc = CLI_BUILDMODE_EXTEND;

  if (argc) {
    cli_error(cli, "Extra arguments on command line, command ignored.");
    rc = CLI_ERROR;
  }
  if (cli_int_add_optarg_value(cli, command, command, CLI_CMD_OPTION_MULTIPLE)) {
    cli_error(cli, "Problem setting value for optional flag %s", command);
    rc = CLI_ERROR;
  }

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

int cli_int_buildmode_exit_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  int rc = CLI_BUILDMODE_EXIT;

  if (argc > 0) {
    cli_error(cli, "Extra arguments on command line, command ignored.");
    rc = CLI_ERROR;
  }
  return rc;
}

int cli_int_buildmode_show_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  struct cli_optarg_pair *optarg_pair;
  if (cli && cli->buildmode) {
    for (optarg_pair = cli->found_optargs; optarg_pair; optarg_pair = optarg_pair->next) {
      // only show vars that are also current 'commands'
      struct cli_command *c = cli->commands;
      for (; c; c = c->next) {
        if (c->command_type != CLI_BUILDMODE_COMMAND) continue;
        if (!strcmp(c->command, optarg_pair->name)) {
          cli_print(cli, "  %-20s = %s", optarg_pair->name, optarg_pair->value);
          break;
        }
      }
    }
  }
  return CLI_OK;
}

int cli_int_buildmode_unset_cback(struct cli_def *cli, const char *command, char *argv[], int argc) {
  // iterate over our 'set' variables to see if that variable is also a 'valid' command right now

  struct cli_command *c;

  // is this 'optarg' to remove one of the current commands?
  for (c = cli->commands; c; c = c->next) {
    if (c->command_type != CLI_BUILDMODE_COMMAND) continue;
    if (cli->privilege < c->privilege) continue;
    if ((cli->buildmode->mode != c->mode) && (cli->buildmode->transient_mode != c->mode) && (c->mode != MODE_ANY))
      continue;
    if (strcmp(c->command, argv[0])) continue;
    // Ok, go fry anything by this name

    cli_int_unset_optarg_value(cli, argv[0]);
    break;
  }

  return CLI_OK;
}

/* Generate a list of variables that *have* been set */
int cli_int_buildmode_unset_completor(struct cli_def *cli, const char *name, const char *word,
                                      struct cli_comphelp *comphelp) {
  return CLI_OK;
}

int cli_int_buildmode_unset_validator(struct cli_def *cli, const char *name, const char *value) {
  return CLI_OK
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
      // command matched but from another mode,
      // remember it if we fail to find correct command
      again_config = c;
    } else if (c->mode == MODE_ANY) {
      // command matched but for any mode,
      // remember it if we fail to find correct command
      again_any = c;
    }
  }

  // drop out of config submode if we have matched command on MODE_CONFIG
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
  for (i = 0; i < pipeline->num_stages; i++) {
    // in 'buildmode' we only have one pipeline, but we need to recall if we had started with any optargs

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

    // and save our found optargs for later use

    if (cli->buildmode)
      cli->buildmode->found_optargs = cli->found_optargs;
    else
      pipeline->stage[i].found_optargs = cli->found_optargs;

    if (rc != CLI_OK) break;
  }
  cli->pipeline = NULL;

  return rc;
}

struct cli_pipeline *cli_int_free_pipeline(struct cli_pipeline *pipeline) {
  int i;
  if (pipeline) {
    for (i = 0; i < pipeline->num_stages; i++) cli_int_free_found_optargs(&pipeline->stage[i].found_optargs);
    for (i = 0; i < pipeline->num_words; i++) free_z(pipeline->words[i]);
    free_z(pipeline->cmdline);
    free_z(pipeline);
    pipeline = NULL;
  }
  return pipeline;
}

void cli_int_show_pipeline(struct cli_def *cli, struct cli_pipeline *pipeline) {
  int i, j;
  struct cli_pipeline_stage *stage;
  char **word;
  struct cli_optarg_pair *optarg_pair;

  for (i = 0, word = pipeline->words; i < pipeline->num_words; i++, word++) printf("[%s] ", *word);
  printf("\n");
  printf("#stages=%d, #words=%d\n", pipeline->num_stages, pipeline->num_words);

  for (i = 0; i < pipeline->num_stages; i++) {
    stage = &(pipeline->stage[i]);
    printf("  #%d(%d words) first_unmatched=%d: ", i, stage->num_words, stage->first_unmatched);
    for (j = 0; j < stage->num_words; j++) {
      printf(" [%s]", stage->words[j]);
    }
    printf("\n");

    if (stage->command) {
      printf("  Command: %s\n", stage->command->command);
    }
    for (optarg_pair = stage->found_optargs; optarg_pair; optarg_pair = optarg_pair->next) {
      printf("    %s: %s\n", optarg_pair->name, optarg_pair->value);
    }
  }
}

/*
 * Take an array of words and return a pipeline, using '|' to split command into different 'stages'.
 * Pipeline is broken down by '|' characters and within each p
 */
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
        // can't allow filters in buildmode commands
        cli_int_free_pipeline(pipeline);
        return NULL;
      }
      stage->stage_num = pipeline->num_stages;
      stage++;
      stage->num_words = 0;
      pipeline->num_stages++;
      stage->words = word + 1;  // first word of the next stage is one past where we are (possibly NULL)
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

static char DELIM_OPT_START[] = "[";
static char DELIM_OPT_END[] = "]";
static char DELIM_ARG_START[] = "<";
static char DELIM_ARG_END[] = ">";
static char DELIM_NONE[] = "";
static char BUILDMODE_YES[] = " (enter buildmode)";
static char BUILDMODE_NO[] = "";

static void cli_get_optarg_comphelp(struct cli_def *cli, struct cli_optarg *optarg, struct cli_comphelp *comphelp,
                                    int num_candidates, const char lastchar, const char *anchor_word,
                                    const char *next_word) {
  int help_insert = 0;
  char *delim_start = DELIM_NONE;

  char *delim_end = DELIM_NONE;
  char *allow_buildmode = BUILDMODE_NO;
  int (*get_completions)(struct cli_def *, const char *, const char *, struct cli_comphelp *) = NULL;
  char *tptr = NULL;

  // if we've already seen a value by this exact name, skip it, unless the multiple flag is set
  if (cli_find_optarg_value(cli, optarg->name, NULL) && !(optarg->flags & (CLI_CMD_OPTION_MULTIPLE))) return;

  get_completions = optarg->get_completions;
  if (optarg->flags & CLI_CMD_OPTIONAL_FLAG) {
    if (!(anchor_word && !strncmp(anchor_word, optarg->name, strlen(anchor_word)))) {
      delim_start = DELIM_OPT_START;
      delim_end = DELIM_OPT_END;
      get_completions = NULL;  // no point, completor of field is the name itself
    }
  } else if (optarg->flags & CLI_CMD_HYPHENATED_OPTION) {
    delim_start = DELIM_OPT_START;
    delim_end = DELIM_OPT_END;
  } else if (optarg->flags & CLI_CMD_ARGUMENT) {
    delim_start = DELIM_ARG_START;
    delim_end = DELIM_ARG_END;
  } else if (optarg->flags & CLI_CMD_OPTIONAL_ARGUMENT) {
    /*
     * optional args can match against the name the value.
     * Here 'anchor_word' is the name, and 'next_word' is what we're matching against.
     * So if anchor_word==next_word we're looking at the 'name' of the optarg, otherwise we
     * know the name and are going against the value.
     */
    if (anchor_word != next_word) {
      // matching against optional argument 'value'
      help_insert = 0;
      if (!get_completions) {
        delim_start = DELIM_ARG_START;
        delim_end = DELIM_ARG_END;
      }
    } else {
      // matching against optional argument 'name'
      help_insert = 1;
      get_completions = NULL;  // matching against the name, not the following field value
      if (!(anchor_word && !strncmp(anchor_word, optarg->name, strlen(anchor_word)))) {
        delim_start = DELIM_OPT_START;
        delim_end = DELIM_OPT_END;
      }
    }
  }

  // Fill in with help text or completor value(s) as indicated
  if ((lastchar == '?') && (asprintf(&tptr, "%s%s%s", delim_start, optarg->name, delim_end) != -1)) {
    if (optarg->flags & CLI_CMD_ALLOW_BUILDMODE) allow_buildmode = BUILDMODE_YES;
    if (help_insert && (asprintf(&tptr, "  %-20s enter '%s' to %s%s", tptr, optarg->name,
                                 (optarg->help) ? optarg->help : "", allow_buildmode) != -1)) {
      cli_add_comphelp_entry(comphelp, tptr);
      free_z(tptr);
    } else if (asprintf(&tptr, "  %-20s %s%s", tptr, (optarg->help) ? optarg->help : "", allow_buildmode) != -1) {
      cli_add_comphelp_entry(comphelp, tptr);
      free_z(tptr);
    }
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
  int w_idx;   // word_index
  int w_incr;  // word_increment
  struct cli_optarg *candidates[CLI_MAX_LINE_WORDS];
  int num_candidates = 0;
  int c_idx;  // candidate_idx
  char *value;
  int is_last_word = 0;
  int (*validator)(struct cli_def *, const char * name, const char * value);

  /*
   * Tab completion and help are *only* allowed at end of string, but we need to process the entire command to
   * know what has already been found.  There should be no ambiguities before the 'last' word.
   * Note specifically that for tab completions and help the *last* word can be a null pointer.
   */
  stage->error_word = NULL;

  /* start our optarg and word pointers at the beginning
   * optarg will be incremented *only* when an argument is identified
   * w_idx will be incremented either by 1 (optflag or argument) or 2 (optional argument)
   */
  w_idx = stage->first_unmatched;
  optarg = cmd->optargs;
  num_candidates = 0;
  while (optarg && w_idx < stage->num_words && (num_candidates <= 1)) {
    num_candidates = 0;
    w_incr = 1;  // assume we're only incrementing by a word - if we match an optional argument bump to 2

    /* the initial loop here is to identify candidates based matching *this* word in order against:
     *    an exact match of the word to the optinal flag/argument name (yield exactly one match and exit the loop)
     *    a partial match for optional flag/argument name
     *    candidate an argument.
     */

    for (oaptr = optarg; oaptr; oaptr = oaptr->next) {
      // Skip this option unless it matches privileges, MODE_ANY, the current mode, or the transient_mode
      if (cli->privilege < oaptr->privilege) continue;
      if ((oaptr->mode != cli->mode) && (oaptr->mode != cli->transient_mode) && (oaptr->mode != MODE_ANY)) continue;

      /* Two special cases - a hphenated option and an 'exact' match optional flag or optional argument
       * If our word starts with a '-' and we have a CMD_CLI_HYPHENATED_OPTION or an exact match for an
       * optional flag/argument name trumps anything and will be the *only* candidate
       * Otherwise if the word is 'blank', could be an argument, or matches 'enough' of an option/flag it is a candidate
       * Once we accept an argument as a candidate, we're done looking for candidates as straight arguments are required
       */
      if (stage->words[w_idx] && (oaptr->flags & (CLI_CMD_OPTIONAL_FLAG | CLI_CMD_OPTIONAL_ARGUMENT)) &&
          !strcmp(oaptr->name, stage->words[w_idx])) {
        candidates[0] = oaptr;
        num_candidates = 1;
        break;
      } else if (stage->words[w_idx] && stage->words[w_idx][0] == '-' && (oaptr->flags & (CLI_CMD_HYPHENATED_OPTION))) {
        candidates[0] = oaptr;
        num_candidates = 1;
        break;
      } else if (!stage->words[w_idx] || (oaptr->flags & CLI_CMD_ARGUMENT) ||
                 !strncasecmp(oaptr->name, stage->words[w_idx], strlen(stage->words[w_idx]))) {
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
    if ((num_candidates > 1) && ((lastchar == '\0') || (w_idx < (stage->num_words - 1)))) {
      stage->error_word = stage->words[w_idx];
      stage->status = CLI_AMBIGUOUS;
      cli_error(cli, "Ambiguous option/argument for command %s", stage->command->command);
      return;
    }

    /*
     * So now we could have one or more candidates.  We need to call get help/completions *only* if this is the
     * 'last-word'
     * Remember that last word for optinal arguments is last or next to last....
     */
    if (lastchar != '\0') {
      int called_comphelp = 0;
      for (c_idx = 0; c_idx < num_candidates; c_idx++) {
        oaptr = candidates[c_idx];

        // need to know *which* word we're trying to complete for optional_args, hence the difference calls
        if (((oaptr->flags & (CLI_CMD_OPTIONAL_FLAG | CLI_CMD_ARGUMENT)) && (w_idx == (stage->num_words - 1))) ||
            ((oaptr->flags & (CLI_CMD_OPTIONAL_ARGUMENT | CLI_CMD_HYPHENATED_OPTION)) &&
             (w_idx == (stage->num_words - 1)))) {
          cli_get_optarg_comphelp(cli, oaptr, comphelp, num_candidates, lastchar, stage->words[w_idx],
                                  stage->words[w_idx]);
          called_comphelp = 1;
        } else if ((oaptr->flags & CLI_CMD_OPTIONAL_ARGUMENT) && (w_idx == (stage->num_words - 2))) {
          cli_get_optarg_comphelp(cli, oaptr, comphelp, num_candidates, lastchar, stage->words[w_idx],
                                  stage->words[w_idx + 1]);
          called_comphelp = 1;
        }
      }
      // if we were 'end-of-word' and looked for completions/help, return to user
      if (called_comphelp) {
        stage->status = CLI_OK;
        return;
      }
    }

    // set some values for use later - makes code much easier to read
    if (((oaptr->flags & (CLI_CMD_OPTIONAL_FLAG | CLI_CMD_ARGUMENT)) && (w_idx == (stage->num_words - 1))) ||
        ((oaptr->flags & CLI_CMD_OPTIONAL_ARGUMENT) && (w_idx == (stage->num_words - 2)))) {
      is_last_word = 1;
    }
    value = stage->words[w_idx];
    oaptr = candidates[0];
    validator = oaptr->validator;

    if ((oaptr->flags & CLI_CMD_OPTIONAL_ARGUMENT)) {
      w_incr = 2;
      if (!stage->words[w_idx + 1] && lastchar == '\0') {
        // hit a optional argument that does not have a value with it
        cli_error(cli, "Optional argument %s requires a value", stage->words[w_idx]);
        stage->error_word = stage->words[w_idx];
        stage->status = CLI_MISSING_VALUE;
        return;
      }
      value = stage->words[w_idx + 1];
    }

    /*
     * Ok, so we're not at end of string and doing help/completions.
     * So see if our value is 'valid', to save it, and see if we have any extra processing to
     * do such as a transient mode check or enter build mode.
     */

    if (!validator || ((*validator)(cli, oaptr->name, value) == CLI_OK)) {
      if (oaptr->flags & CLI_CMD_DO_NOT_RECORD) {
        // we want completion and validation, but then leave this 'value' to be seen - used *only* by buildmode
        // as argv[0] with argc=1
        break;
      } else {
        // need to combine remaining words if the CLI_CMD_REMAINDER_OF_LINE flag it set, then we're done processing
        int set_value_return = 0;

        if (oaptr->flags & CLI_CMD_REMAINDER_OF_LINE) {
          char *combined = NULL;
          combined = join_words(stage->num_words - w_idx, stage->words + w_idx);
          set_value_return = cli_int_add_optarg_value(cli, oaptr->name, combined, 0);
          free_z(combined);
        } else {
          set_value_return = cli_int_add_optarg_value(cli, oaptr->name, value, oaptr->flags & CLI_CMD_OPTION_MULTIPLE);
        }

        if (set_value_return != CLI_OK) {
          cli_error(cli, "%sProblem setting value for command argument %s", lastchar == '\0' ? "" : "\n",
                    stage->words[w_idx]);
          cli_reprompt(cli);
          stage->error_word = stage->words[w_idx];
          stage->status = CLI_ERROR;
          return;
        }
      }
    } else {
      cli_error(cli, "%sProblem parsing command setting %s with value %s", lastchar == '\0' ? "" : "\n", oaptr->name,
                stage->words[w_idx]);
      cli_reprompt(cli);
      stage->error_word = stage->words[w_idx];
      stage->status = CLI_ERROR;
      return;
    }

    // if this optarg can set the transient mode, then evaluate it if we're not at last word
    if (oaptr->transient_mode && (oaptr->transient_mode(cli, oaptr->name, value))) {
      stage->error_word = stage->words[w_idx];
      stage->status = CLI_ERROR;
      return;
    }

    // only do buildmode optargs if we're a executing a command, parsing command (stage 0), and this is the last word
    if ((stage->status == CLI_OK) && (oaptr->flags & CLI_CMD_ALLOW_BUILDMODE) && is_last_word) {
      stage->status = cli_int_enter_buildmode(cli, stage, value);
      return;
    }

    /*
     * Optional flags and arguments can appear multiple times, but true arguments only once.  Advance our optarg
     * starting point when we see a true argument
     */
    if (oaptr->flags & CLI_CMD_ARGUMENT) {
      // advance pass this argument entry
      optarg = oaptr->next;
    }

    w_idx += w_incr;
    stage->first_unmatched = w_idx;
  }

  // Ok, if we're evaluating the command for execution, ensure we have all required arguments.
  if (lastchar == '\0') {
    for (; optarg; optarg = optarg->next) {
      if (cli->privilege < optarg->privilege) continue;
      if ((optarg->mode != cli->mode) && (optarg->mode != cli->transient_mode) && (optarg->mode != MODE_ANY)) continue;
      if (optarg->flags & CLI_CMD_DO_NOT_RECORD) continue;
      if (optarg->flags & CLI_CMD_ARGUMENT) {
        cli_error(cli, "Incomplete command, missing required argument '%s'", optarg->name);
        stage->status = CLI_MISSING_ARGUMENT;
        return;
      }
    }
  }
  return;
}
