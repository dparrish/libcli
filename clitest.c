#include <stdio.h>
#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "libcli.h"

#define CLITEST_PORT                8000
#define MODE_CONFIG_INT             10

#ifdef __GNUC__
# define UNUSED(d) d __attribute__ ((unused))
#else
# define UNUSED(d) d
#endif

unsigned int regular_count = 0;
unsigned int debug_regular = 0;

#ifdef WIN32
typedef int socklen_t;

int winsock_init()
{
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    // Start up sockets
    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0)
    {
        // Tell the user that we could not find a usable WinSock DLL.
        return 0;
    }

    /*
     * Confirm that the WinSock DLL supports 2.2
     * Note that if the DLL supports versions greater than 2.2 in addition to
     * 2.2, it will still return 2.2 in wVersion since that is the version we
     * requested.
     * */
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        // Tell the user that we could not find a usable WinSock DLL.
        WSACleanup();
        return 0;
    }
    return 1;
}
#endif

int cmd_test(struct cli_def *cli, char *command, char *argv[], int argc)
{
    int i;
    cli_print(cli, "called %s with \"%s\"", __FUNCTION__, command);
    cli_print(cli, "%d arguments:", argc);
    for (i = 0; i < argc; i++)
        cli_print(cli, "        %s", argv[i]);

    return CLI_OK;
}

int cmd_set(struct cli_def *cli, UNUSED(char *command), char *argv[],
    int argc)
{
    if (argc < 2 || strcmp(argv[0], "?") == 0)
    {
        cli_print(cli, "Specify a variable to set");
        return CLI_OK;
    }

    if (strcmp(argv[1], "?") == 0)
    {
        cli_print(cli, "Specify a value");
        return CLI_OK;
    }

    if (strcmp(argv[0], "regular_interval") == 0)
    {
        unsigned int sec = 0;
        if (!argv[1] && !&argv[1])
        {
            cli_print(cli, "Specify a regular callback interval in seconds");
            return CLI_OK;
        }
        sscanf(argv[1], "%d", &sec);
        if (sec < 1)
        {
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

int cmd_config_int(struct cli_def *cli, UNUSED(char *command), char *argv[],
    int argc)
{
    if (argc < 1)
    {
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

int cmd_config_int_exit(struct cli_def *cli, UNUSED(char *command),
    UNUSED(char *argv[]), UNUSED(int argc))
{
    cli_set_configmode(cli, MODE_CONFIG, NULL);
    return CLI_OK;
}

int cmd_show_regular(struct cli_def *cli, UNUSED(char *command), char *argv[], int argc)
{
    cli_print(cli, "cli_regular() has run %u times", regular_count);
    return CLI_OK;
}

int cmd_debug_regular(struct cli_def *cli, UNUSED(char *command), char *argv[], int argc)
{
    debug_regular = !debug_regular;
    cli_print(cli, "cli_regular() debugging is %s", debug_regular ? "enabled" : "disabled");
    return CLI_OK;
}

int check_auth(char *username, char *password)
{
    if (strcasecmp(username, "fred") != 0)
        return CLI_ERROR;
    if (strcasecmp(password, "nerk") != 0)
        return CLI_ERROR;
    return CLI_OK;
}

int regular_callback(struct cli_def *cli)
{
    regular_count++;
    if (debug_regular)
    {
        cli_print(cli, "Regular callback - %u times so far", regular_count);
        cli_reprompt(cli);
    }
    return CLI_OK;
}

int check_enable(char *password)
{
    return !strcasecmp(password, "topsecret");
}

void pc(UNUSED(struct cli_def *cli), char *string)
{
    printf("%s\n", string);
}

int main()
{
    struct cli_command *c;
    struct cli_def *cli;
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

    cli = cli_init();
    cli_set_banner(cli, "libcli test environment");
    cli_set_hostname(cli, "router");
    cli_regular(cli, regular_callback);
    cli_regular_interval(cli, 5); // Defaults to 1 second
    cli_set_idle_timeout(cli, 60); // 60 second idle timeout
    cli_register_command(cli, NULL, "test", cmd_test, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, NULL);

    cli_register_command(cli, NULL, "simple", NULL, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, NULL);

    cli_register_command(cli, NULL, "simon", NULL, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, NULL);

    cli_register_command(cli, NULL, "set", cmd_set, PRIVILEGE_PRIVILEGED,
        MODE_EXEC, NULL);

    c = cli_register_command(cli, NULL, "show", NULL, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, NULL);

    cli_register_command(cli, c, "regular", cmd_show_regular, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, "Show the how many times cli_regular has run");

    cli_register_command(cli, c, "counters", cmd_test, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, "Show the counters that the system uses");

    cli_register_command(cli, c, "junk", cmd_test, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, NULL);

    cli_register_command(cli, NULL, "interface", cmd_config_int,
        PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Configure an interface");

    cli_register_command(cli, NULL, "exit", cmd_config_int_exit,
        PRIVILEGE_PRIVILEGED, MODE_CONFIG_INT,
        "Exit from interface configuration");

    cli_register_command(cli, NULL, "address", cmd_test, PRIVILEGE_PRIVILEGED,
        MODE_CONFIG_INT, "Set IP address");

    c = cli_register_command(cli, NULL, "debug", NULL, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, NULL);

    cli_register_command(cli, c, "regular", cmd_debug_regular, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, "Enable cli_regular() callback debugging");

    cli_set_auth_callback(cli, check_auth);
    cli_set_enable_callback(cli, check_enable);
    // Test reading from a file
    {
        FILE *fh;

        if ((fh = fopen("clitest.txt", "r")))
        {
            // This sets a callback which just displays the cli_print() text to stdout
            cli_print_callback(cli, pc);
            cli_file(cli, fh, PRIVILEGE_UNPRIVILEGED, MODE_EXEC);
            cli_print_callback(cli, NULL);
            fclose(fh);
        }
    }

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return 1;
    }
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(CLITEST_PORT);
    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }

    if (listen(s, 50) < 0)
    {
        perror("listen");
        return 1;
    }

    printf("Listening on port %d\n", CLITEST_PORT);
    while ((x = accept(s, NULL, 0)))
    {
#ifndef WIN32
        int pid = fork();
        if (pid < 0)
        {
            perror("fork");
            return 1;
        }

        /* parent */
        if (pid > 0)
        {
            socklen_t len = sizeof(addr);
            if (getpeername(x, (struct sockaddr *) &addr, &len) >= 0)
                printf(" * accepted connection from %s\n", inet_ntoa(addr.sin_addr));

            close(x);
            continue;
        }

        /* child */
        close(s);
        cli_loop(cli, x);
        exit(0);
#else
        cli_loop(cli, x);
        shutdown(x, SD_BOTH);
        close(x);
#endif
    }

    cli_done(cli);
    return 0;
}
