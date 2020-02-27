#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "libcli.h"

// vim:sw=4 tw=120 et

#define CLITEST_PORT 8000
#define MODE_CONFIG_INT 10

#ifdef __GNUC__
#define UNUSED(d) d __attribute__((unused))
#else
#define UNUSED(d) d
#endif

unsigned int regular_count = 0;
unsigned int debug_regular = 0;

struct my_context {
  int value;
  char *message;
};

#ifdef WIN32
typedef int socklen_t;

int winsock_init() {
  WORD wVersionRequested;
  WSADATA wsaData;
  int err;

  // Start up sockets
  wVersionRequested = MAKEWORD(2, 2);

  err = WSAStartup(wVersionRequested, &wsaData);
  if (err != 0) {
    // Tell the user that we could not find a usable WinSock DLL.
    return 0;
  }

  /*
   * Confirm that the WinSock DLL supports 2.2
   * Note that if the DLL supports versions greater than 2.2 in addition to
   * 2.2, it will still return 2.2 in wVersion since that is the version we
   * requested.
   * */
  if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
    // Tell the user that we could not find a usable WinSock DLL.
    WSACleanup();
    return 0;
  }
  return 1;
}
#endif

int cmd_test(struct cli_def *cli, const char *command, char *argv[], int argc) {
  int i;
  cli_print(cli, "called %s with \"%s\"", __func__, command);
  cli_print(cli, "%d arguments:", argc);
  for (i = 0; i < argc; i++) cli_print(cli, "        %s", argv[i]);

  return CLI_OK;
}

int cmd_set(struct cli_def *cli, UNUSED(const char *command), char *argv[], int argc) {
  if (argc < 2 || strcmp(argv[0], "?") == 0) {
    cli_print(cli, "Specify a variable to set");
    return CLI_OK;
  }

  if (strcmp(argv[1], "?") == 0) {
    cli_print(cli, "Specify a value");
    return CLI_OK;
  }

  if (strcmp(argv[0], "regular_interval") == 0) {
    unsigned int sec = 0;
    if (!argv[1] && !*argv[1]) {
      cli_print(cli, "Specify a regular callback interval in seconds");
      return CLI_OK;
    }
    sscanf(argv[1], "%u", &sec);
    if (sec < 1) {
      cli_print(cli, "Specify a regular callback interval in seconds");
      return CLI_OK;
    }
    cli->timeout_tm.tv_sec = sec;
    cli->timeout_tm.tv_usec = 0;
    cli_print(cli, "Regular callback interval is now %d seconds", sec);
    return CLI_OK;
  }

  cli_print(cli, "Setting \"%s\" to \"%s\"", argv[0], argv[1]);
  return CLI_OK;
}

int cmd_config_int(struct cli_def *cli, UNUSED(const char *command), char *argv[], int argc) {
  if (argc < 1) {
    cli_print(cli, "Specify an interface to configure");
    return CLI_OK;
  }

  if (strcmp(argv[0], "?") == 0)
    cli_print(cli, "  test0/0");

  else if (strcasecmp(argv[0], "test0/0") == 0)
    cli_set_configmode(cli, MODE_CONFIG_INT, "test");
  else
    cli_print(cli, "Unknown interface %s", argv[0]);

  return CLI_OK;
}

int cmd_config_int_exit(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  cli_set_configmode(cli, MODE_CONFIG, NULL);
  return CLI_OK;
}

int cmd_show_regular(struct cli_def *cli, UNUSED(const char *command), char *argv[], int argc) {
  cli_print(cli, "cli_regular() has run %u times", regular_count);
  return CLI_OK;
}

int cmd_debug_regular(struct cli_def *cli, UNUSED(const char *command), char *argv[], int argc) {
  debug_regular = !debug_regular;
  cli_print(cli, "cli_regular() debugging is %s", debug_regular ? "enabled" : "disabled");
  return CLI_OK;
}

int cmd_context(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc)) {
  struct my_context *myctx = (struct my_context *)cli_get_context(cli);
  cli_print(cli, "User context has a value of %d and message saying %s", myctx->value, myctx->message);
  return CLI_OK;
}

int check_auth(const char *username, const char *password) {
  if (strcasecmp(username, "fred") != 0) return CLI_ERROR;
  if (strcasecmp(password, "nerk") != 0) return CLI_ERROR;
  return CLI_OK;
}

int regular_callback(struct cli_def *cli) {
  regular_count++;
  if (debug_regular) {
    cli_print(cli, "Regular callback - %u times so far", regular_count);
    cli_reprompt(cli);
  }
  return CLI_OK;
}

int check_enable(const char *password) {
  return !strcasecmp(password, "topsecret");
}

int idle_timeout(struct cli_def *cli) {
  cli_print(cli, "Custom idle timeout");
  return CLI_QUIT;
}

void pc(UNUSED(struct cli_def *cli), const char *string) {
  printf("%s\n", string);
}

#define MODE_POLYGON_TRIANGLE 20
#define MODE_POLYGON_RECTANGLE 21

int cmd_perimeter(struct cli_def *cli, const char *command, char *argv[], int argc) {
  struct cli_optarg_pair *optargs = cli_get_all_found_optargs(cli);
  int i = 1, numSides = 0;
  int perimeter = 0;
  int verbose_count = 0;
  char *verboseArg;
  char *shapeName = NULL;

  cli_print(cli, "perimeter callback, with %d args", argc);
  for (; optargs; optargs = optargs->next) cli_print(cli, "%d, %s=%s", i++, optargs->name, optargs->value);

  verboseArg = NULL;
  while ((verboseArg = cli_get_optarg_value(cli, "verbose", verboseArg))) {
    verbose_count++;
  }
  cli_print(cli, "verbose argument was seen  %d times", verbose_count);

  shapeName = cli_get_optarg_value(cli, "shape", NULL);
  if (!shapeName) {
    cli_error(cli, "No shape name given");
    return CLI_ERROR;
  } else if (strcmp(shapeName, "triangle") == 0) {
    numSides = 3;
  } else if (strcmp(shapeName, "rectangle") == 0) {
    numSides = 4;
  } else {
    cli_error(cli, "Unrecognized shape given");
    return CLI_ERROR;
  }
  for (i = 1; i <= numSides; i++) {
    char sidename[50], *value;
    int length;
    snprintf(sidename, 50, "side_%d", i);
    value = cli_get_optarg_value(cli, sidename, NULL);
    length = strtol(value, NULL, 10);
    perimeter += length;
  }
  cli_print(cli, "Perimeter is %d", perimeter);
  return CLI_OK;
}

const char *KnownShapes[] = {"rectangle", "triangle", NULL};

int shape_completor(struct cli_def *cli, const char *name, const char *value, struct cli_comphelp *comphelp) {
  const char **shape;
  int rc = CLI_OK;
  printf("shape_completor called with <%s>\n", value);
  for (shape = KnownShapes; *shape && (rc == CLI_OK); shape++) {
    if (!value || !strncmp(*shape, value, strlen(value))) {
      rc = cli_add_comphelp_entry(comphelp, *shape);
    }
  }
  return rc;
}

int shape_validator(struct cli_def *cli, const char *name, const char *value) {
  const char **shape;

  printf("shape_validator called with <%s>\n", value);
  for (shape = KnownShapes; *shape; shape++) {
    if (!strcmp(value, *shape)) return CLI_OK;
  }
  return CLI_ERROR;
}

int verbose_validator(struct cli_def *cli, const char *name, const char *value) {
  printf("verbose_validator called\n");
  return CLI_OK;
}

// note that we're setting a 'custom' optarg tag/value pair as an example here
int shape_transient_eval(struct cli_def *cli, const char *name, const char *value) {
  printf("shape_transient_eval called with <%s>\n", value);
  if (!strcmp(value, "rectangle")) {
    cli_set_transient_mode(cli, MODE_POLYGON_RECTANGLE);
    cli_set_optarg_value(cli, "duplicateShapeValue", value, 0);
    return CLI_OK;
  } else if (!strcmp(value, "triangle")) {
    cli_set_transient_mode(cli, MODE_POLYGON_TRIANGLE);
    cli_set_optarg_value(cli, "duplicateShapeValue", value, 0);
    return CLI_OK;
  }
  cli_error(cli, "unrecognized value for setting %s -> %s", name, value);
  return CLI_ERROR;
}

const char *KnownColors[] = {"black",    "white",     "gray",      "red",        "blue",
                             "green",    "lightred",  "lightblue", "lightgreen", "darkred",
                             "darkblue", "darkgreen", "lavender",  "yellow",     NULL};

int color_completor(struct cli_def *cli, const char *name, const char *word, struct cli_comphelp *comphelp) {
  // Attempt to show matches against the following color strings
  const char **color;
  int rc = CLI_OK;
  printf("color_completor called with <%s>\n", word);
  for (color = KnownColors; *color && (rc == CLI_OK); color++) {
    if (!word || !strncmp(*color, word, strlen(word))) {
      rc = cli_add_comphelp_entry(comphelp, *color);
    }
  }
  return rc;
}

int color_validator(struct cli_def *cli, const char *name, const char *value) {
  const char **color;
  int rc = CLI_ERROR;
  printf("color_validator called for %s\n", name);
  for (color = KnownColors; *color; color++) {
    if (!strcmp(value, *color)) return CLI_OK;
  }
  return rc;
}

int side_length_validator(struct cli_def *cli, const char *name, const char *value) {
  // Verify 'value' is a positive number
  long len;
  char *endptr;
  int rc = CLI_OK;

  printf("side_length_validator called\n");
  errno = 0;
  len = strtol(value, &endptr, 10);
  if ((endptr == value) || (*endptr != '\0') || ((errno == ERANGE) && ((len == LONG_MIN) || (len == LONG_MAX))))
    return CLI_ERROR;
  return rc;
}

int transparent_validator(struct cli_def *cli, const char *name, const char *value) {
  return strcasecmp("transparent", value) ? CLI_ERROR : CLI_OK;
}

int check1_validator(struct cli_def *cli, UNUSED(const char *name), UNUSED(const char *value)) {
  char *color;
  char *transparent;

  printf("check1_validator called \n");
  color = cli_get_optarg_value(cli, "color", NULL);
  transparent = cli_get_optarg_value(cli, "transparent", NULL);

  if (!color && !transparent) {
    cli_error(cli, "\nMust supply either a color or transparent!");
    return CLI_ERROR;
  } else if (color && !strcmp(color, "black") && transparent) {
    cli_error(cli, "\nCan not have a transparent black object!");
    return CLI_ERROR;
  }
  return CLI_OK;
}

int cmd_string(struct cli_def *cli, const char *command, char *argv[], int argc) {
  int i;
  cli_print(cli, "Raw commandline was <%s>", cli->pipeline->cmdline);
  cli_print(cli, "Value for text argument is <%s>", cli_get_optarg_value(cli, "text", NULL));

  cli_print(cli, "Found %d 'extra' arguments after 'text' argument was processed", argc);
  for (i = 0; i != argc; i++) {
    cli_print(cli, "  Extra arg %d = <%s>", i + 1, argv[i]);
  }
  return CLI_OK;
}

void run_child(int x) {
  struct cli_command *c;
  struct cli_def *cli;
  struct cli_optarg *o;

  // Prepare a small user context
  char mymessage[] = "I contain user data!";
  struct my_context myctx;
  myctx.value = 5;
  myctx.message = mymessage;

  cli = cli_init();
  cli_set_banner(cli, "libcli test environment");
  cli_set_hostname(cli, "router");
  cli_telnet_protocol(cli, 1);
  cli_regular(cli, regular_callback);

  // change regular update to 5 seconds rather than default of 1 second
  cli_regular_interval(cli, 5);

  // set 60 second idle timeout
  cli_set_idle_timeout_callback(cli, 60, idle_timeout);
  cli_register_command(cli, NULL, "test", cmd_test, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
  cli_register_command(cli, NULL, "simple", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
  cli_register_command(cli, NULL, "simon", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
  cli_register_command(cli, NULL, "set", cmd_set, PRIVILEGE_PRIVILEGED, MODE_EXEC, NULL);
  c = cli_register_command(cli, NULL, "show", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
  cli_register_command(cli, c, "regular", cmd_show_regular, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                       "Show the how many times cli_regular has run");
  cli_register_command(cli, c, "counters", cmd_test, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                       "Show the counters that the system uses");
  cli_register_command(cli, c, "junk", cmd_test, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
  cli_register_command(cli, NULL, "interface", cmd_config_int, PRIVILEGE_PRIVILEGED, MODE_CONFIG,
                       "Configure an interface");
  cli_register_command(cli, NULL, "exit", cmd_config_int_exit, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT,
                       "Exit from interface configuration");
  cli_register_command(cli, NULL, "address", cmd_test, PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT, "Set IP address");
  c = cli_register_command(cli, NULL, "debug", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
  cli_register_command(cli, c, "regular", cmd_debug_regular, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                       "Enable cli_regular() callback debugging");

  // Register some commands/subcommands to demonstrate opt/arg and buildmode operations

  c = cli_register_command(
      cli, NULL, "perimeter", cmd_perimeter, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
      "Calculate perimeter of polygon\nhas embedded "
      "newline\nand_a_really_long_line_that_is_much_longer_than_80_columns_to_show_that_wrap_case");
  o = cli_register_optarg(c, "transparent", CLI_CMD_OPTIONAL_FLAG, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                          "Set transparent flag", NULL, NULL, NULL);
  cli_optarg_addhelp(o, "transparent", "(any case)set to transparent");

  cli_register_optarg(
      c, "verbose", CLI_CMD_OPTIONAL_FLAG | CLI_CMD_OPTION_MULTIPLE, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
      "Set verbose flag with some humongously long string \nwithout any embedded newlines in it to test with", NULL,
      NULL, NULL);
  o = cli_register_optarg(c, "color", CLI_CMD_OPTIONAL_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Set color",
                          color_completor, color_validator, NULL);
  cli_optarg_addhelp(o, "black", "the color 'black'");
  cli_optarg_addhelp(o, "white", "the color 'white'");
  cli_optarg_addhelp(o, "gray", "the color 'gray'");
  cli_optarg_addhelp(o, "red", "the color 'red'");
  cli_optarg_addhelp(o, "blue", "the color 'blue'");
  cli_optarg_addhelp(o, "green", "the color 'green'");
  cli_optarg_addhelp(o, "lightred", "the color 'lightred'");
  cli_optarg_addhelp(o, "lightblue", "the color 'lightblue'");
  cli_optarg_addhelp(o, "lightgreen", "the color 'lightgreen'");
  cli_optarg_addhelp(o, "darkred", "the color 'darkred'");
  cli_optarg_addhelp(o, "darkblue", "the color 'darkblue'");
  cli_optarg_addhelp(o, "darkgreen", "the color 'darkgreen'");
  cli_optarg_addhelp(o, "lavender", "the color 'lavender'");
  cli_optarg_addhelp(o, "yellow", "the color 'yellow'");

  cli_register_optarg(c, "__check1__", CLI_CMD_SPOT_CHECK, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL,
                      check1_validator, NULL);
  o = cli_register_optarg(c, "shape", CLI_CMD_ARGUMENT | CLI_CMD_ALLOW_BUILDMODE, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                          "Specify shape(shows subtext on help)", shape_completor, shape_validator,
                          shape_transient_eval);
  cli_optarg_addhelp(o, "triangle", "specify a triangle");
  cli_optarg_addhelp(o, "rectangle", "specify a rectangle");

  cli_register_optarg(c, "side_1", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_POLYGON_TRIANGLE,
                      "Specify side 1 length", NULL, side_length_validator, NULL);
  cli_register_optarg(c, "side_1", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_POLYGON_RECTANGLE,
                      "Specify side 1 length", NULL, side_length_validator, NULL);
  cli_register_optarg(c, "side_2", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_POLYGON_TRIANGLE,
                      "Specify side 2 length", NULL, side_length_validator, NULL);
  cli_register_optarg(c, "side_2", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_POLYGON_RECTANGLE,
                      "Specify side 2 length", NULL, side_length_validator, NULL);
  cli_register_optarg(c, "side_3", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_POLYGON_TRIANGLE,
                      "Specify side 3 length", NULL, side_length_validator, NULL);
  cli_register_optarg(c, "side_3", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_POLYGON_RECTANGLE,
                      "Specify side 3 length", NULL, side_length_validator, NULL);
  cli_register_optarg(c, "side_4", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_POLYGON_RECTANGLE,
                      "Specify side 4 length", NULL, side_length_validator, NULL);

  c = cli_register_command(cli, NULL, "string", cmd_string, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                           "string input argument testing");

  cli_register_optarg(c, "buildmode", CLI_CMD_OPTIONAL_FLAG | CLI_CMD_ALLOW_BUILDMODE, PRIVILEGE_UNPRIVILEGED,
                      MODE_EXEC, "flag", NULL, NULL, NULL);
  cli_register_optarg(c, "text", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "text string", NULL, NULL, NULL);

  // Set user context and its command
  cli_set_context(cli, (void *)&myctx);
  cli_register_command(cli, NULL, "context", cmd_context, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                       "Test a user-specified context");

  cli_set_auth_callback(cli, check_auth);
  cli_set_enable_callback(cli, check_enable);
  // Test reading from a file
  {
    FILE *fh;

    if ((fh = fopen("clitest.txt", "r"))) {
      // This sets a callback which just displays the cli_print() text to stdout
      cli_print_callback(cli, pc);
      cli_file(cli, fh, PRIVILEGE_UNPRIVILEGED, MODE_EXEC);
      cli_print_callback(cli, NULL);
      fclose(fh);
    }
  }
  cli_loop(cli, x);
  cli_done(cli);
}

int main() {
  int s, x;
  struct sockaddr_in addr;
  int on = 1;

#ifndef WIN32
  signal(SIGCHLD, SIG_IGN);
#endif
#ifdef WIN32
  if (!winsock_init()) {
    printf("Error initialising winsock\n");
    return 1;
  }
#endif

  if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return 1;
  }

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
    perror("setsockopt");
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(CLITEST_PORT);
  if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    return 1;
  }

  if (listen(s, 50) < 0) {
    perror("listen");
    return 1;
  }

  printf("Listening on port %d\n", CLITEST_PORT);
  while ((x = accept(s, NULL, 0))) {
#ifndef WIN32
    int pid = fork();
    if (pid < 0) {
      perror("fork");
      return 1;
    }

    /* parent */
    if (pid > 0) {
      socklen_t len = sizeof(addr);
      if (getpeername(x, (struct sockaddr *)&addr, &len) >= 0)
        printf(" * accepted connection from %s\n", inet_ntoa(addr.sin_addr));

      close(x);
      continue;
    }

    /* child */
    close(s);
    run_child(x);
    exit(0);
#else
    run_child(x);
    shutdown(x, SD_BOTH);
    close(x);
#endif
  }

  return 0;
}
