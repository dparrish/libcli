#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "libcli.h"

#define CLITEST_PORT		8000

int cmd_test(struct cli_def *cli, FILE *client, char *command, char *argv[], int argc)
{
    int i;
    cli_print(cli, client, "called %s with \"%s\"", __FUNCTION__, command);
    cli_print(cli, client, "%d arguments:", argc);
    for (i = 0; i < argc; i++)
    {
	cli_print(cli, client, "	%s", argv[i]);
    }
    return CLI_OK;
}

int cmd_set(struct cli_def *cli, FILE *client, char *command, char *argv[], int argc)
{
    if (argc < 2)
    {
	cli_print(cli, client, "Specify a variable to set");
	return CLI_OK;
    }
    cli_print(cli, client, "Setting \"%s\" to \"%s\"", argv[0], argv[1]);
    return CLI_OK;
}

int check_auth(char *username, char *password)
{
    if (!strcasecmp(username, "fred") && !strcasecmp(password, "nerk"))
	return 1;
    return 0;
}

int main(int argc, char *argv[])
{
    struct cli_command *c;
    struct cli_def *cli;
    int s, x;
    struct sockaddr_in servaddr;
    int on = 1;

    cli = cli_init();
    cli_set_banner(cli, "libcli test environment");
    cli_register_command(cli, NULL, "test", cmd_test, NULL);
    cli_register_command(cli, NULL, "sex", NULL, NULL);
    cli_register_command(cli, NULL, "simple", NULL, NULL);
    cli_register_command(cli, NULL, "simon", NULL, NULL);
    cli_register_command(cli, NULL, "set", cmd_set, NULL);
    c = cli_register_command(cli, NULL, "show", NULL, NULL);
    cli_register_command(cli, c, "counters", cmd_test, "Show the counters that the system uses");
    cli_register_command(cli, c, "junk", cmd_test, NULL);

    cli_set_auth_callback(cli, check_auth);

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
	perror("socket");
	return 1;
    }
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(CLITEST_PORT);
    if (bind(s, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
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
	cli_loop(cli, x, "cli> ");
	close(x);
    }

    cli_done(cli);
    return 0;
}

