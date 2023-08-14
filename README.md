Libcli provides a shared C library for including a Cisco-like command-line
interface into other software.

It’s a telnet interface which supports command-line editing, history,
authentication and callbacks for a user-definable function tree.

To compile:

```sh
$ make
```

To cross-compile specify a triplet in variable `CROSS_COMPILE`:

```sh
$ CROSS_COMPILE=machine-vendor-operatingsystem- make
````

To install:

```sh
$ make install
````

Note - as of version 1.10.5 you have a compile time decision on using select()
or poll() in cli_loop().  The default is to use the legacy 'select()' call.
If built with 'CFLAGS=-DLIBCLI_USE_POLL make' then the poll() system call will
be used instead.  One additional check is being made now in cli_loop() to 
ensure that the passed file descriptor is in range.  If not, an error message
will be sent and the cli_loop() will exit in the child process with CLI_ERROR.

This will install `libcli.so` into `/usr/local/lib`. If you want to change the
location, edit Makefile.

There is a test application built called clitest. Run this and telnet to port
8000.

By default, a single username and password combination is enabled.

```
Username: fred
Password: nerk
```

Get help by entering `help` or hitting `?`.

libcli provides support for using the arrow keys for command-line editing. Up
and Down arrows will cycle through the command history, and Left & Right can be
used for editing the current command line.

libcli also works out the shortest way of entering a command, so if you have a
command `show users | grep foobar` defined, you can enter `sh us | g foobar` if that
is the shortest possible way of doing it.

Enter `sh?` at the command line to get a list of commands starting with `sh`

A few commands are defined in every libcli program:

* `help`
* `quit`
* `exit`
* `logout`
* `history`

Use in your own code:

First of all, make sure you `#include <libcli.h>` in your C code, and link with
`-lcli`.

If you have any trouble with this, have a look at clitest.c for a
demonstration.

Start your program off with a `cli_init()`.
This sets up the internal data structures required.

When a user connects, they are presented with a greeting if one is set using the
`cli_set_banner(banner)` function.

By default, the command-line session is not authenticated, which means users
will get full access as soon as they connect. As this may not be always the best
thing, 2 methods of authentication are available.

First, you can add username / password combinations with the
`cli_allow_user(username, password)` function. When a user connects, they can
connect with any of these username / password combinations.

Secondly, you can add a callback using the `cli_set_auth_callback(callback)`
function. This function is passed the username and password as `char *`, and must
return `CLI_OK` if the user is to have access and `CLI_ERROR` if they are not.

The library itself will take care of prompting the user for credentials.

Commands are built using a tree-like structure. You define commands with the
`cli_register_command(parent, command, callback, privilege, mode, help)` function.

`parent` is a `cli_command *` reference to a previously added command. Using a
parent you can build up complex commands.

e.g. to provide commands `show users`, `show sessions` and `show people`, use
the following sequence:

```c
cli_command *c = cli_register_command(NULL, "show", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL);
cli_register_command(c, "sessions", fn_sessions, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show the sessions connected");
cli_register_command(c, "users", fn_users, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show the users connected");
cli_register_command(c, "people", fn_people, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show a list of the people I like");
```

If callback is `NULL`, the command can be used as part of a tree, but cannot be
individually run. 

If you decide later that you don't want a command to be run, you can call
`cli_unregister_command(command)`.
You can use this to build dynamic command trees.

It is possible to carry along a user-defined context to all command callbacks
using `cli_set_context(cli, context)` and `cli_get_context(cli)` functions.


You are responsible for accepting a TCP connection, and for creating a
process or thread to run the cli.  Once you are ready to process the
connection, call `cli_loop(cli, sock)` to interact with the user on the
given socket.  Note that as mentioned above, if the select() call is used and 
sock is out of range (>= FD_SETSIZE) then cli_loop() will display an error in
both the parent process and to the remote TCP connection before exiting that routine.

This function will return when the user exits the cli, either by breaking the
connection or entering `quit`.

Call `cli_done()` to free the data structures.

