## Introduction
libcli provides a telnet command-line environment which can be embedded in other programs. This environment includes useful features such as automatic authentication, history, and command-line editing.

This guide should show you everything you need to embed libcli into your program. If you have any corrections, suggestions or modifications, please email [David Parrish](mailto:david+libcli@dparrish.com).

## Authentication
Two methods of authentcation are supported by libcli - internal and callback.

Internal authentication is based on a list of username / password combinations that are set up before `cli_loop()` is called. Passwords may be clear text, MD5 encrypted, or DES encrypted (requires a `{crypt}` prefix to distinguish from clear text).

Callback based authentication calls a callback with the username and password that the user enters, and must return either `CLI_OK` or `CLI_ERROR`. This can be used for checking passwords against some other database such as LDAP.

If neither `cli_set_auth_callback()` or `cli_allow_user()` have been called before `cli_loop()`, then authentication will be disabled and the user will not be prompted for a username / password combination.

Authentication for the privileged state can also be defined by a static password or by a callback. Use the `cli_set_enable_callback()` or `cli_allow_enable()` functions to set the enable password.

## Tutorial
This section will guide you through implementing libcli in a basic server.

### Create a file libclitest.c

```c
#include <libcli.h>
    
int main(int argc, char *argv[]) {
  struct sockaddr_in servaddr;
  struct cli_command *c;
  struct cli_def *cli;
  int on = 1, x, s;

  // Must be called first to setup data structures
  cli = cli_init();

  // Set the hostname (shown in the the prompt)
  cli_set_hostname(cli, "test");

  // Set the greeting
  cli_set_banner(cli, "Welcome to the CLI test program.");

  // Enable 2 username / password combinations
  cli_allow_user(cli, "fred", "nerk");
  cli_allow_user(cli, "foo", "bar");

  // Set up a few simple one-level commands
  cli_register_command(cli, NULL, "test", cmd_test, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
  cli_register_command(cli, NULL, "simple", cmd_test, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
  cli_register_command(cli, NULL, "simon", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);

  // This command takes arguments, and requires privileged mode (enable)
  cli_register_command(cli, NULL, "set", cmd_set, PRIVILEGE_PRIVILEGED, MODE_EXEC, NULL);

  // Set up 2 commands "show counters" and "show junk"
  c = cli_register_command(cli, NULL, "show", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
  // Note how we store the previous command and use it as the parent for this one.
  cli_register_command(cli, c, "junk", cmd_test, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
  // This one has some help text
  cli_register_command(cli, c, "counters", cmd_test, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show the counters that the system uses");

  // Create a socket
  s = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  // Listen on port 12345
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(12345);
  bind(s, (struct sockaddr *)&servaddr, sizeof(servaddr));

  // Wait for a connection
  listen(s, 50);

  while ((x = accept(s, NULL, 0))) {
    // Pass the connection off to libcli
    cli_loop(cli, x);
    close(x);
  }

  // Free data structures
  cli_done(cli);

  return 0;
}
```

This code snippet is all that's required to enable a libcli program. However it's not yet compilable because we haven't created the callback functions.

A few commands have been created:

* `test`
* `simple`
* `set`
* `show junk`
* `show counters`

Note that `simon` isn't on this list because `callback` was `NULL` when it was registered, so the command will not be available.

Also, the standard libcli commands `help`, `exit`, `logout`, `quit` and `history` are also available automatically.

Make this program complete by adding the callback functions

```c
int cmd_test(struct cli_def *cli, char *command, char *argv[], int argc) {
  cli_print(cli, "called %s with %s\r\n", __FUNCTION__, command);
  return CLI_OK;
}

int cmd_set(struct cli_def *cli, char *command, char *argv[], int argc) {
  if (argc < 2) {
    cli_print(cli, "Specify a variable to set\r\n");
    return CLI_OK;
  }
  cli_print(cli, "Setting %s to %s\r\n", argv[0], argv[1]);
  return CLI_OK;
}
```

2 callback functions are defined here, `cmd_test()` and `cmd_set()`. `cmd_test()` is called by many of the commands defined in the tutorial, although in reality you would usually use a callback for a single command.

`cmd_test()` simply echos the command entered back to the client. Note that it shows the full expanded command, so you can enter "`te`" at the prompt and it will print back "`called with test`".

`cmd_set()` handles the arguments given on the command line. This allows you to use a single callback to handle lots of arguments like:

* `set colour green`
* `set name David`
* `set email "I don't have an e-mail address"`
* etc...

Compile the code

```sh
gcc libclitest.c -o libclitest -lcli
```

You can now run the program with `./libclitest` and telnet to port 12345 to see your work in action.

## Function Reference

### cli\_init()
This must be called before any other cli_xxx function. It sets up the internal data structures used for command-line processing.

Returns a `struct cli_def *` which must be passed to all other `cli_xxx` functions.

### cli\_done(struct cli\_def \*cli)
This is optional, but it's a good idea to call this when you are finished with libcli. This frees memory used by libcli.

### cli\_register\_command(struct cli\_def \*cli, struct cli\_command *parent, char *command, int (*callback)(struct cli\_def *, char *, char **, int), int privilege, int mode, char *help)
Add a command to the internal command tree. Returns a `struct cli_command *`, which you can pass as parent to another call to `cli_register_command()`.

When the command has been entered by the user, callback is checked. If it is not `NULL`, then the callback is called with:

`struct cli_def *` - the handle of the cli structure. This must be passed to all cli functions, including `cli_print()`.
`char *` - the entire command which was entered. This is after command expansion.
`char **` - the list of arguments entered
`int` - the number of arguments entered
The callback must return `CLI_OK` if the command was successful, `CLI_ERROR` if processing wasn't successful and the next matching command should be tried (if any), or `CLI_QUIT` to drop the connection (e.g. on a fatal error).

If parent is `NULL`, the command is added to the top level of commands, otherwise it is a subcommand of parent.

privilege should be set to either PRIVILEGE\_PRIVILEGED or PRIVILEGE\_UNPRIVILEGED. If set to PRIVILEGE\_PRIVILEGED then the user must have entered enable before running this command.

`mode` should be set to `MODE_EXEC` for no configuration mode, `MODE_CONFIG` for generic configuration commands, or your own config level. The user can enter the generic configuration level by entering configure terminal, and can return to `MODE_EXEC` by entering exit or CTRL-Z. You can define commands to enter your own configuration levels, which should call the `cli_set_configmode()` function.

If help is provided, it is given to the user when the use the help command or press ?.

### cli\_unregister\_command(struct cli\_def \*cli, char *command)
Remove a command command and all children. There is not provision yet for removing commands at lower than the top level.

### cli\_loop(struct cli\_def \*cli, int sockfd)
The main loop of the command-line environment. This must be called with the FD of a socket open for bi-directional communication (sockfd).

### cli\_loop() handles the telnet negotiation and authentication. It returns only when the connection is finished, either by a server or client disconnect.
Returns `CLI_OK`.

### cli\_set\_auth\_callback(struct cli\_def \*cli, int (*auth\_callback)(char *, char *))
Enables or disables callback based authentication.

If `auth_callback` is not `NULL`, then authentication will be required on connection. `auth_callback` will be called with the username and password that the user enters.

`auth_callback` must return a non-zero value if authentication is successful.

If `auth_callback` is `NULL`, then callback based authentication will be disabled.

### cli\_allow\_user(struct cli\_def \*cli, char *username, char *password)
Enables internal authentication, and adds username/password to the list of allowed users.

The internal list of users will be checked before callback based authentication is tried.

### cli\_deny\_user(struct cli\_def \*cli, char *username)
Removes username/password from the list of allowed users.

If this is the last combination in the list, then internal authentication will be disabled.

### cli\_set\_banner(struct cli\_def \*cli, char *banner)
Sets the greeting that clients will be presented with when they connect. This may be a security warning for example.

If this function is not called or called with a `NULL` argument, no banner will be presented.

### cli\_set\_hostname(struct cli\_def \*cli, char *hostname)
Sets the hostname to be displayed as the first part of the prompt.

### cli\_regular(struct cli\_def \*cli, int(*callback)(struct cli\_def *))
Adds a callback function which will be called every second that a user is connected to the cli. This can be used for regular processing such as debugging, time counting or implementing idle timeouts.

Pass `NULL` as the callback function to disable this at runtime.

If the callback function does not return `CLI_OK`, then the user will be disconnected.

### cli\_file(struct cli\_def \*cli, FILE *f, int privilege, int mode)
This reads and processes every line read from f as if it were entered at the console. The privilege level will be set to privilege and mode set to mode during the processing of the file.

### cli\_print(struct cli\_def \*cli, char *format, ...)
This function should be called for any output generated by a command callback.

It takes a printf() style format string and a variable number of arguments.

Be aware that any output generated by `cli_print()` will be passed through any filter currently being applied, and the output will be redirected to the `cli_print_callback()` if one has been specified.

### cli\_error(struct cli\_def \*cli, char *format, ...)
A variant of `cli_print()` which does not have filters applied.

### cli\_print\_callback(struct cli\_def \*cli, void (*callback)(struct cli\_def *, char *))
Whenever `cli_print()` or `cli_error()` is called, the output generally goes to the user. If you specify a callback using this function, then the output will be sent to that callback. The function will be called once for each line, and it will be passed a single null-terminated string, without any newline characters.

Specifying `NULL` as the callback parameter will make libcli use the default `cli_print()` function.

### cli\_set\_enable\_callback(struct cli\_def \*cli, void (*callback)(struct cli\_def *, char *))
Just like `cli_set_auth_callback, this takes a pointer to a callback function to authorize privileged access. However this callback only takes a single string - the password.

### cli\_allow\_enable(struct cli\_def \*cli, char *password)
This will allow a static password to be used for the enable command. This static password will be checked before running any enable callbacks.

Set this to `NULL` to not have a static enable password.

### cli\_set\_configmode(struct cli\_def \*cli, int mode, char *string)
This will set the configuration mode. Once set, commands will be restricted to only ones in the selected configuration mode, plus any set to `MODE_ANY`. The previous mode value is returned.

The string passed will be used to build the prompt in the set configuration mode. e.g. if you set the string `test`, the prompt will become:

```
hostname(config-test)#
```

