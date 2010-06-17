// vim:sw=4 ts=8 expandtab tw=100

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "libcli.h"
#include "internal_commands.h"

int cli_int_enable(struct cli_def *cli, UNUSED(char *command), UNUSED(char *argv[]),
                   UNUSED(int argc))
{
    if (cli->privilege == PRIVILEGE_PRIVILEGED)
        return CLI_OK;

    if (!cli->enable_password && !cli->enable_callback)
    {
        /* no password required, set privilege immediately */
        cli_set_privilege(cli, PRIVILEGE_PRIVILEGED);
        cli_set_configmode(cli, MODE_EXEC, NULL);
    }
    else
    {
        /* require password entry */
        cli->state = STATE_ENABLE_PASSWORD;
    }

    return CLI_OK;
}

int cli_int_disable(struct cli_def *cli, UNUSED(char *command), UNUSED(char *argv[]),
                    UNUSED(int argc))
{
    cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
    cli_set_configmode(cli, MODE_EXEC, NULL);
    return CLI_OK;
}

int cli_int_help(struct cli_def *cli, UNUSED(char *command), UNUSED(char *argv[]), UNUSED(int argc))
{
    cli_error(cli, "\nCommands available:");
    cli_show_help(cli, cli->commands);
    return CLI_OK;
}

int cli_int_history(struct cli_def *cli, UNUSED(char *command), UNUSED(char *argv[]),
                    UNUSED(int argc))
{
    int i;

    cli_error(cli, "\nCommand history:");
    for (i = 0; i < MAX_HISTORY; i++)
    {
        if (cli->history[i])
            cli_error(cli, "%3d. %s", i, cli->history[i]);
    }

    return CLI_OK;
}

int cli_int_quit(struct cli_def *cli, UNUSED(char *command), UNUSED(char *argv[]), UNUSED(int argc))
{
    cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
    cli_set_configmode(cli, MODE_EXEC, NULL);
    return CLI_QUIT;
}

int cli_int_exit(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (cli->mode == MODE_EXEC)
        return cli_int_quit(cli, command, argv, argc);

    if (cli->mode > MODE_CONFIG)
        cli_set_configmode(cli, MODE_CONFIG, NULL);
    else
        cli_set_configmode(cli, MODE_EXEC, NULL);

    cli->service = NULL;
    return CLI_OK;
}

int cli_int_idle_timeout(struct cli_def *cli)
{
    cli_print(cli, "Idle timeout");
    return CLI_QUIT;
}

int cli_int_configure_terminal(struct cli_def *cli, UNUSED(char *command), UNUSED(char *argv[]),
                               UNUSED(int argc))
{
    cli_set_configmode(cli, MODE_CONFIG, NULL);
    return CLI_OK;
}

int cli_int_terminal_length(struct cli_def *cli, UNUSED(char *command), char *argv[], int argc)
{
    int length = 0;
    if (argc != 1)
    {
        cli_print(cli, "Terminal length is currently %d lines", cli->page_length);
        return CLI_OK;
    }

    if (strcmp(argv[0], "?") == 0)
    {
        cli_print(cli, "  [length]");
        return CLI_OK;
    }

    length = atoi(argv[0]);
    if (length < 0)
    {
        cli_print(cli, "Enter a terminal length in lines. Use 0 to disable paging.");
        return CLI_OK;
    }
    cli->page_length = length;
    return CLI_OK;
}

