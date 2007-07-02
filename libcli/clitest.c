#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "libcli.h"

#define CLITEST_PORT		8000
#define MODE_CONFIG_INT		10

#ifdef __GNUC__
# define UNUSED(d) d __attribute__ ((unused))
#else
# define UNUSED(d) d
#endif

int cmd_test(struct cli_def *cli, char *command, char *argv[], int argc)
{
    int i;
    cli_print(cli, "called %s with \"%s\"", __FUNCTION__, command);
    cli_print(cli, "%d arguments:", argc);
    for (i = 0; i < argc; i++)
	cli_print(cli, "	%s", argv[i]);

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

int check_auth(char *username, char *password)
{
    if (strcasecmp(username, "fred") != 0)
	return CLI_ERROR;
    if (strcasecmp(password, "nerk") != 0)
	return CLI_ERROR;
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

    signal(SIGCHLD, SIG_IGN);

    cli = cli_init();
    cli_set_banner(cli, "libcli test environment");
    cli_set_hostname(cli, "router");
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
    }

    cli_done(cli);
    return 0;
}
