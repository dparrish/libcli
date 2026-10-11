/* vim: set sw=4 tw=80 et: */

/*
 * Test driver for the Perl test suite in t/.
 *
 * The driver builds a cli from a named fixture, then runs cli_loop() on
 * fd 0, which the harness connects to one end of a socketpair.  Command
 * callbacks print what they received in a fixed format so tests can
 * assert on how a line was parsed:
 *
 *     RAN <full command name>
 *     OPTARG <name>=<value>
 *     ARG[<n>] <value>
 *
 * Usage: cli_driver [--fixture name] [--user name:password]...
 *                   [--enable password] [--hostname name] [--telnet]
 */

#include "libcli.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define DRV_MAX_USERS  16


typedef int (*drv_fixture_pt)(struct cli_def *cli);


typedef struct {
    const char               *name;
    drv_fixture_pt            setup;
} drv_fixture_t;


static int drv_cmd_print(struct cli_def *cli, const char *command,
    char *argv[], int argc);
static int drv_cmd_lines(struct cli_def *cli, const char *command,
    char *argv[], int argc);
static int drv_cmd_hostname(struct cli_def *cli, const char *command,
    char *argv[], int argc);
static int drv_fixture_basic(struct cli_def *cli);
static int drv_fixture_completion(struct cli_def *cli);
static int drv_fixture_validate(struct cli_def *cli);
static int drv_fixture_dupoptarg(struct cli_def *cli);
static int drv_fixture_buildmode(struct cli_def *cli);
static int drv_fixture_unreg_child(struct cli_def *cli);
static int drv_fixture_auth_ctx(struct cli_def *cli);
static int drv_auth_ctx(struct cli_def *cli, const char *username,
    const char *password);
static int drv_enable_ctx(struct cli_def *cli, const char *password);
static int drv_cmd_secret(struct cli_def *cli, const char *command,
    char *argv[], int argc);
static int drv_cmd_short(struct cli_def *cli, const char *command,
    char *argv[], int argc);
static int drv_validate_color(struct cli_def *cli, const char *name,
    const char *value);
static const drv_fixture_t *drv_find_fixture(const char *name);
static int drv_add_user(struct cli_def *cli, const char *spec);
static void drv_usage(const char *argv0);


static const drv_fixture_t  drv_fixtures[] = {
    { "basic", drv_fixture_basic },
    { "completion", drv_fixture_completion },
    { "validate", drv_fixture_validate },
    { "dupoptarg", drv_fixture_dupoptarg },
    { "buildmode", drv_fixture_buildmode },
    { "unreg_child", drv_fixture_unreg_child },
    { "auth_ctx", drv_fixture_auth_ctx },
    { NULL, NULL }
};


int
main(int argc, char *argv[])
{
    int                       i, rc, telnet, nusers;
    const char               *fixture, *enable, *hostname;
    struct cli_def           *cli;
    const drv_fixture_t      *fx;
    const char               *users[DRV_MAX_USERS];

    fixture = "basic";
    enable = NULL;
    hostname = "router";
    telnet = 0;
    nusers = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--telnet") == 0) {
            telnet = 1;

        } else if (i + 1 < argc && strcmp(argv[i], "--fixture") == 0) {
            fixture = argv[++i];

        } else if (i + 1 < argc && strcmp(argv[i], "--enable") == 0) {
            enable = argv[++i];

        } else if (i + 1 < argc && strcmp(argv[i], "--hostname") == 0) {
            hostname = argv[++i];

        } else if (i + 1 < argc && strcmp(argv[i], "--user") == 0) {
            if (nusers == DRV_MAX_USERS) {
                fprintf(stderr, "cli_driver: too many --user options\n");
                return 2;
            }

            users[nusers++] = argv[++i];

        } else {
            drv_usage(argv[0]);
            return 2;
        }
    }

    fx = drv_find_fixture(fixture);
    if (fx == NULL) {
        fprintf(stderr, "cli_driver: unknown fixture \"%s\"\n", fixture);
        return 2;
    }

    cli = cli_init();
    if (cli == NULL) {
        fprintf(stderr, "cli_driver: cli_init() failed\n");
        return 2;
    }

    cli_telnet_protocol(cli, telnet);
    cli_set_hostname(cli, hostname);

    for (i = 0; i < nusers; i++) {
        if (drv_add_user(cli, users[i]) != 0) {
            cli_done(cli);
            return 2;
        }
    }

    if (enable != NULL) {
        cli_allow_enable(cli, enable);
    }

    if (fx->setup(cli) != CLI_OK) {
        fprintf(stderr, "cli_driver: fixture \"%s\" setup failed\n",
                fixture);
        cli_done(cli);
        return 2;
    }

    rc = cli_loop(cli, 0);
    cli_done(cli);

    return rc == CLI_OK ? 0 : 1;
}


static int
drv_cmd_print(struct cli_def *cli, const char *command, char *argv[],
    int argc)
{
    int                       i;
    struct cli_optarg_pair   *optarg;

    cli_print(cli, "RAN %s", command);

    for (optarg = cli_get_all_found_optargs(cli);
         optarg != NULL;
         optarg = optarg->next)
    {
        cli_print(cli, "OPTARG %s=%s", optarg->name, optarg->value);
    }

    for (i = 0; i < argc; i++) {
        cli_print(cli, "ARG[%d] %s", i, argv[i]);
    }

    return CLI_OK;
}


static int
drv_cmd_lines(struct cli_def *cli, const char *command, char *argv[],
    int argc)
{
    cli_print(cli, "line 1: alpha");
    cli_print(cli, "line 2: beta");
    cli_print(cli, "line 3: gamma");

    return CLI_OK;
}


static int
drv_cmd_hostname(struct cli_def *cli, const char *command, char *argv[],
    int argc)
{
    char  *name;

    name = cli_get_optarg_value(cli, "name", NULL);
    cli_set_hostname(cli, name);

    return CLI_OK;
}


/*
 * basic: a small command tree covering subcommands, optargs, privilege
 * levels, config mode, and output for filters.
 */
static int
drv_fixture_basic(struct cli_def *cli)
{
    struct cli_command  *c;

    c = cli_register_command(cli, NULL, "show", NULL, PRIVILEGE_UNPRIVILEGED,
                             MODE_EXEC, "Show information");
    if (c == NULL) {
        return CLI_ERROR;
    }

    if (cli_register_command(cli, c, "counters", drv_cmd_print,
                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                             "Show counters") == NULL
        || cli_register_command(cli, c, "version", drv_cmd_print,
                                PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                                "Show version") == NULL
        || cli_register_command(cli, c, "lines", drv_cmd_lines,
                                PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                                "Print three lines") == NULL)
    {
        return CLI_ERROR;
    }

    c = cli_register_command(cli, NULL, "set", drv_cmd_print,
                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                             "Set a value");
    if (c == NULL) {
        return CLI_ERROR;
    }

    if (cli_register_optarg(c, "name", CLI_CMD_ARGUMENT,
                            PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                            "Name to set", NULL, NULL, NULL) == NULL
        || cli_register_optarg(c, "verbose", CLI_CMD_OPTIONAL_FLAG,
                               PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                               "Be verbose", NULL, NULL, NULL) == NULL
        || cli_register_optarg(c, "count", CLI_CMD_OPTIONAL_ARGUMENT,
                               PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                               "Repeat count", NULL, NULL, NULL) == NULL)
    {
        return CLI_ERROR;
    }

    if (cli_register_command(cli, NULL, "reload", drv_cmd_print,
                             PRIVILEGE_PRIVILEGED, MODE_EXEC,
                             "Reload the system") == NULL)
    {
        return CLI_ERROR;
    }

    c = cli_register_command(cli, NULL, "hostname", drv_cmd_hostname,
                             PRIVILEGE_PRIVILEGED, MODE_CONFIG,
                             "Set the hostname");
    if (c == NULL) {
        return CLI_ERROR;
    }

    if (cli_register_optarg(c, "name", CLI_CMD_ARGUMENT,
                            PRIVILEGE_PRIVILEGED, MODE_CONFIG,
                            "New hostname", NULL, NULL, NULL) == NULL)
    {
        return CLI_ERROR;
    }

    return CLI_OK;
}


/*
 * completion: "neighbor" has a single child with a long name, so a Tab at
 * the end of a near-full line yields one completion.  "interface" has
 * children sharing a prefix, to exercise partial completion and double
 * Tab.
 */
static int
drv_fixture_completion(struct cli_def *cli)
{
    struct cli_command  *c;

    c = cli_register_command(cli, NULL, "neighbor", NULL,
                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Neighbors");
    if (c == NULL) {
        return CLI_ERROR;
    }

    if (cli_register_command(cli, c, "advertisement", drv_cmd_print,
                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                             "Advertisement") == NULL)
    {
        return CLI_ERROR;
    }

    c = cli_register_command(cli, NULL, "interface", NULL,
                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Interface");
    if (c == NULL) {
        return CLI_ERROR;
    }

    if (cli_register_command(cli, c, "ethernet", drv_cmd_print,
                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                             "Ethernet") == NULL
        || cli_register_command(cli, c, "ethertype", drv_cmd_print,
                                PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                                "Ethertype") == NULL)
    {
        return CLI_ERROR;
    }

    return CLI_OK;
}


/*
 * validate: a command with a validated optional argument, to exercise the
 * validator success and failure paths.
 */
static int
drv_fixture_validate(struct cli_def *cli)
{
    struct cli_command  *c;

    c = cli_register_command(cli, NULL, "paint", drv_cmd_print,
                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Paint it");
    if (c == NULL) {
        return CLI_ERROR;
    }

    if (cli_register_optarg(c, "color", CLI_CMD_ARGUMENT,
                            PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                            "A primary color", NULL, drv_validate_color,
                            NULL) == NULL)
    {
        return CLI_ERROR;
    }

    return CLI_OK;
}


/*
 * dupoptarg: registers "name" twice with the same privilege and mode.
 * The duplicate registration must fail cleanly (return NULL) and leave a
 * usable cli, which the following session then exercises.
 */
static int
drv_fixture_dupoptarg(struct cli_def *cli)
{
    struct cli_command  *c;

    c = cli_register_command(cli, NULL, "set", drv_cmd_print,
                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Set");
    if (c == NULL) {
        return CLI_ERROR;
    }

    if (cli_register_optarg(c, "name", CLI_CMD_ARGUMENT,
                            PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                            "Name", NULL, NULL, NULL) == NULL)
    {
        return CLI_ERROR;
    }

    /* The duplicate; the return value is intentionally discarded. */
    cli_register_optarg(c, "name", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED,
                        MODE_EXEC, "Name again", NULL, NULL, NULL);

    return CLI_OK;
}


/*
 * buildmode: "build" has an optarg flagged CLI_CMD_ALLOW_BUILDMODE and a
 * second optarg whose name, "show", collides with a reserved buildmode
 * command.  Entering buildmode (by typing "build go") therefore fails,
 * and cli_loop reports the failure.  A plain "ping" command follows so a
 * test can confirm the cli survives the failed entry.
 */
static int
drv_fixture_buildmode(struct cli_def *cli)
{
    struct cli_command  *c;

    c = cli_register_command(cli, NULL, "build", drv_cmd_print,
                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Build");
    if (c == NULL) {
        return CLI_ERROR;
    }

    if (cli_register_optarg(c, "go",
                            CLI_CMD_ALLOW_BUILDMODE | CLI_CMD_OPTIONAL_FLAG,
                            PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                            "Enter build mode", NULL, NULL, NULL) == NULL
        || cli_register_optarg(c, "show", CLI_CMD_OPTIONAL_FLAG,
                               PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                               "Reserved name", NULL, NULL, NULL) == NULL)
    {
        return CLI_ERROR;
    }

    if (cli_register_command(cli, NULL, "ping", drv_cmd_print,
                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                             "Ping") == NULL)
    {
        return CLI_ERROR;
    }

    return CLI_OK;
}


/*
 * unreg_child: registers "show counters", then unregisters the child via
 * cli_unregister_all().  cli_free_command does not clear the parent's
 * children pointer, so "show" is left pointing at freed memory; the
 * following session then walks into it.
 */
static int
drv_fixture_unreg_child(struct cli_def *cli)
{
    struct cli_command  *show, *counters;

    show = cli_register_command(cli, NULL, "show", NULL,
                                PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show");
    if (show == NULL) {
        return CLI_ERROR;
    }

    counters = cli_register_command(cli, show, "counters", drv_cmd_print,
                                    PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                                    "Counters");
    if (counters == NULL) {
        return CLI_ERROR;
    }

    cli_unregister_all(cli, counters);

    return CLI_OK;
}


/*
 * auth_ctx: login and enable go through the cli-aware callbacks, and two
 * commands read hidden input with cli_read_password().
 *
 *     login   alice/pw1 is accepted; the third failure ends the session
 *     enable  "en" is accepted, "bye" ends the session
 *     secret  reads a password and its confirmation, prints both
 *             upper-cased (so an echo of the typed text would show)
 *     short   reads into a 4-byte buffer
 */
static int  drv_auth_failures;


static int
drv_fixture_auth_ctx(struct cli_def *cli)
{
    cli_set_auth_callback_ctx(cli, drv_auth_ctx);
    cli_set_enable_callback_ctx(cli, drv_enable_ctx);
    cli_set_context(cli, &drv_auth_failures);

    if (cli_register_command(cli, NULL, "secret", drv_cmd_secret,
                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                             "Read a password twice") == NULL
        || cli_register_command(cli, NULL, "short", drv_cmd_short,
                                PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
                                "Read into a small buffer") == NULL
        || cli_register_command(cli, NULL, "reload", drv_cmd_print,
                                PRIVILEGE_PRIVILEGED, MODE_EXEC,
                                "Reload the system") == NULL)
    {
        return CLI_ERROR;
    }

    return CLI_OK;
}


static int
drv_auth_ctx(struct cli_def *cli, const char *username, const char *password)
{
    int  *failures;

    failures = cli_get_context(cli);

    if (strcmp(username, "alice") == 0 && strcmp(password, "pw1") == 0) {
        cli_print(cli, "AUTH %s", username);
        return CLI_OK;
    }

    if (++*failures >= 3) {
        cli_print(cli, "TOO MANY FAILURES");
        return CLI_QUIT;
    }

    return CLI_ERROR;
}


static int
drv_enable_ctx(struct cli_def *cli, const char *password)
{
    if (strcmp(password, "en") == 0) {
        cli_print(cli, "ENABLE OK");
        return CLI_OK;
    }

    if (strcmp(password, "bye") == 0) {
        cli_print(cli, "ENABLE QUIT");
        return CLI_QUIT;
    }

    return CLI_ERROR;
}


static int
drv_cmd_secret(struct cli_def *cli, const char *command, char *argv[],
    int argc)
{
    int     rc1, rc2;
    char   *p;
    char    a[64], b[64];

    rc1 = cli_read_password(cli, "New password: ", a, sizeof(a));
    cli_print(cli, "RC1 %d", rc1);

    if (rc1 != CLI_OK) {
        return rc1 == CLI_QUIT ? CLI_QUIT : CLI_OK;
    }

    rc2 = cli_read_password(cli, "Confirm password: ", b, sizeof(b));
    cli_print(cli, "RC2 %d", rc2);

    if (rc2 != CLI_OK) {
        return rc2 == CLI_QUIT ? CLI_QUIT : CLI_OK;
    }

    for (p = a; *p; p++) {
        *p = toupper((unsigned char) *p);
    }

    for (p = b; *p; p++) {
        *p = toupper((unsigned char) *p);
    }

    cli_print(cli, "GOT [%s] [%s]", a, b);

    return CLI_OK;
}


static int
drv_cmd_short(struct cli_def *cli, const char *command, char *argv[],
    int argc)
{
    int   rc;
    char  buf[4];

    rc = cli_read_password(cli, "Password: ", buf, sizeof(buf));
    cli_print(cli, "RC %d LEN %d", rc, (int) strlen(buf));

    return CLI_OK;
}


static int
drv_validate_color(struct cli_def *cli, const char *name, const char *value)
{
    if (value != NULL
        && (strcmp(value, "red") == 0 || strcmp(value, "green") == 0
            || strcmp(value, "blue") == 0))
    {
        return CLI_OK;
    }

    return CLI_ERROR;
}


static const drv_fixture_t *
drv_find_fixture(const char *name)
{
    const drv_fixture_t  *fx;

    for (fx = drv_fixtures; fx->name != NULL; fx++) {
        if (strcmp(fx->name, name) == 0) {
            return fx;
        }
    }

    return NULL;
}


/* spec is "name:password"; the password may itself contain ':' */
static int
drv_add_user(struct cli_def *cli, const char *spec)
{
    char    *name, *colon;
    size_t   len;

    colon = strchr(spec, ':');
    if (colon == NULL) {
        fprintf(stderr, "cli_driver: --user wants name:password\n");
        return -1;
    }

    len = (size_t) (colon - spec);

    name = malloc(len + 1);
    if (name == NULL) {
        fprintf(stderr, "cli_driver: out of memory\n");
        return -1;
    }

    memcpy(name, spec, len);
    name[len] = '\0';

    cli_allow_user(cli, name, colon + 1);
    free(name);

    return 0;
}


static void
drv_usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [--fixture name] [--user name:password]...\n"
            "       [--enable password] [--hostname name] [--telnet]\n",
            argv0);
}
