#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "libcli.h"

#define CLITEST_PORT		8000

int cmd_test(FILE *client, char *command, char *argv[], int argc)
{
    int i;
    fprintf(client, "called %s with \"%s\"\r\n", __FUNCTION__, command);
    fprintf(client, "%d arguments:\r\n", argc);
    for (i = 0; i < argc; i++)
    {
	fprintf(client, "	%s\r\n", argv[i]);
    }
    return CLI_OK;
}

int cmd_set(FILE *client, char *command, char *argv[], int argc)
{
    if (argc < 2)
    {
	fprintf(client, "Specify a variable to set\r\n");
	return CLI_OK;
    }
    fprintf(client, "Setting \"%s\" to \"%s\"\r\n", argv[0], argv[1]);
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
    int s, x;
    struct sockaddr_in servaddr;
    int on = 1;

    cli_init();
    cli_set_banner("libcli test environment");
    cli_register_command(NULL, "test", cmd_test, NULL);
    cli_register_command(NULL, "sex", NULL, NULL);
    cli_register_command(NULL, "simple", NULL, NULL);
    cli_register_command(NULL, "simon", NULL, NULL);
    cli_register_command(NULL, "set", cmd_set, NULL);
    c = cli_register_command(NULL, "show", NULL, NULL);
    cli_register_command(c, "counters", cmd_test, "Show the counters that the system uses");
    cli_register_command(c, "junk", cmd_test, NULL);

    cli_set_auth_callback(check_auth);

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

    while ((x = accept(s, NULL, 0)))
    {
	cli_loop(x, "cli> ");
	close(x);
    }

    cli_done();
    return 0;
}

