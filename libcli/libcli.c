#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <regex.h>
#include "libcli.h"
// vim:sw=8 ts=8

enum cli_states
{
	STATE_LOGIN,
	STATE_PASSWORD,
	STATE_NORMAL,
	STATE_ENABLE_PASSWORD,
	STATE_ENABLE,
};

struct unp
{
	char *username;
	char *password;
	struct unp *next;
};

int cli_run_command(struct cli_def *cli, char *command);
int cli_match_filter_init(struct cli_def *cli, int argc, char **argv, struct cli_filter *filt);
int cli_range_filter_init(struct cli_def *cli, int argc, char **argv, struct cli_filter *filt);
int cli_count_filter_init(struct cli_def *cli, int argc, char **argv, struct cli_filter *filt);
int cli_match_filter(struct cli_def *cli, char *string, void *data);
int cli_range_filter(struct cli_def *cli, char *string, void *data);
int cli_count_filter(struct cli_def *cli, char *string, void *data);

char *cli_command_name(struct cli_def *cli, struct cli_command *command)
{
	int l;
	static char *name = NULL;
	char *o;

	if (name) free(name);
	name = calloc(1,1);

	while (command)
	{
		o = name;
		name = calloc(strlen(command->command) + strlen(o) + 2, 1);
		sprintf(name, "%s %s", command->command, o);
		command = command->parent;
		free(o);
	}
	l = strlen(name);
	if (l) name[l - 1] = 0;
	return name;
}

void cli_set_auth_callback(struct cli_def *cli, int (*auth_callback)(char *, char *))
{
	cli->auth_callback = auth_callback;
}

void cli_set_enable_callback(struct cli_def *cli, int (*enable_callback)(char *))
{
	cli->enable_callback = enable_callback;
}

void cli_allow_user(struct cli_def *cli, char *username, char *password)
{
	struct unp *u, *n;
	n = malloc(sizeof(struct unp));
	n->username = strdup(username);
	n->password = strdup(password);
	n->next = NULL;

	if (!cli->users)
		cli->users = n;
	else
	{
		for (u = cli->users; u && u->next; u = u->next);
		if (u) u->next = n;
	}
}

void cli_allow_enable(struct cli_def *cli, char *password)
{
	if (cli->enable_password)
		free(cli->enable_password);

	cli->enable_password = strdup(password);
}

void cli_deny_user(struct cli_def *cli, char *username)
{
	struct unp *u, *p = NULL;
	if (!cli->users) return;
	for (u = cli->users; u; u = u->next)
	{
		if (strcmp(username, u->username) == 0)
		{
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

void cli_set_banner(struct cli_def *cli, char *banner)
{
	if (cli->banner)
	{
		free(cli->banner);
		cli->banner = 0;
	}

	if (banner && *banner)
		cli->banner = strdup(banner);
}

void cli_set_hostname(struct cli_def *cli,char *hostname)
{
	if (cli->hostname)
	{
		free(cli->hostname);
		cli->hostname = 0;
	}

	if (hostname && *hostname)
		cli->hostname = strdup(hostname);
}

void cli_set_promptchar(struct cli_def *cli, char *promptchar)
{
	if (cli->promptchar) free(cli->promptchar);
	cli->promptchar = strdup(promptchar);
}

int cli_set_privilege(struct cli_def *cli, int privilege)
{
	int old = cli->privilege;
	cli->privilege = privilege;

	if (privilege != old)
		cli_set_promptchar(cli, privilege == PRIVILEGE_PRIVILEGED ? "# " : "> ");

	return old;
}

void cli_set_modestring(struct cli_def *cli, char *modestring)
{
	if (cli->modestring)
	{
		free(cli->modestring);
		cli->modestring = NULL;
	}
	if (modestring) cli->modestring = strdup(modestring);
}

int cli_set_configmode(struct cli_def *cli, int mode, char *config_desc)
{
	int old = cli->mode;
	cli->mode = mode;

	if (mode != old)
	{
		if (!cli->mode)
		{
			// Not config mode
			cli_set_modestring(cli, NULL);
		}
		else if (config_desc && *config_desc)
		{
			char string[64];
			snprintf(string, sizeof(string), "(config-%s)", config_desc);
			cli_set_modestring(cli, string);
		}
		else
		{
			cli_set_modestring(cli, "(config)");
		}
	}

	return old;
}

int cli_build_shortest(struct cli_def *cli, struct cli_command *commands)
{
	struct cli_command *c, *p;

	for (c = commands; c; c = c->next)
	{
		for (c->unique_len = 1; c->unique_len <= strlen(c->command); c->unique_len++)
		{
			int foundmatch = 0;
			for (p = commands; p; p = p->next)
			{
				if (c == p) continue;
				if (strncmp(p->command, c->command, c->unique_len) == 0) foundmatch++;
			}
			if (!foundmatch) break;
		}
		if (c->children) cli_build_shortest(cli, c->children);
	}
	return CLI_OK;
}

struct cli_command *cli_register_command(struct cli_def *cli, struct cli_command *parent, char *command, int (*callback)(struct cli_def *cli, char *, char **, int), int privilege, int mode, char *help)
{
	struct cli_command *c, *p;

	if (!command) return NULL;
	if (!(c = calloc(sizeof(struct cli_command), 1))) return NULL;

	c->callback = callback;
	c->next = NULL;
	c->command = strdup(command);
	c->parent = parent;
	c->privilege = privilege;
	c->mode = mode;
	if (help) c->help = strdup(help);

	if (parent)
	{
		if (!parent->children)
		{
			parent->children = c;
		}
		else
		{
			for (p = parent->children; p && p->next; p = p->next);
			if (p)
				p->next = c;
		}
	}
	else
	{
		if (!cli->commands)
		{
			cli->commands = c;
		}
		else
		{
			for (p = cli->commands; p && p->next; p = p->next);
			if (p)
				p->next = c;
		}
	}

	cli_build_shortest(cli, (parent) ? parent : cli->commands);
	return c;
}

int cli_unregister_command(struct cli_def *cli, char *command)
{
	struct cli_command *c, *p = NULL;

	if (!command) return -1;
	if (!cli->commands) return CLI_OK;

	for (c = cli->commands; c; c = c->next)
	{
		if (strcmp(c->command, command) == 0)
		{
			if (p)
				p->next = c->next;
			else
				cli->commands = c->next;

			free(c->command);
			free(c);
			return CLI_OK;
		}
		p = c;
	}

	return CLI_OK;
}

int cli_show_help(struct cli_def *cli, struct cli_command *c)
{
	struct cli_command *p;
	for (p = c; p; p = p->next)
	{
		if (p->command && p->callback && cli->privilege >= p->privilege &&
				(p->mode == cli->mode || p->mode == MODE_ANY))
		{
			cli_print(cli, "  %-20s %s", cli_command_name(cli, p), p->help ? : "");
		}
		if (p->children)
		{
			cli_show_help(cli, p->children);
		}
	}
	return CLI_OK;
}

int cli_int_enable(struct cli_def *cli, char * command, char *argv[], int argc)
{

	if (cli->privilege == PRIVILEGE_PRIVILEGED)
		return CLI_OK;

	if (!cli->enable_password && !cli->enable_callback)
	{
		// No password required, set privilege immediately
		cli_set_privilege(cli, PRIVILEGE_PRIVILEGED);
		cli_set_configmode(cli, MODE_EXEC, NULL);
	}
	else
	{
		// Require password entry
		cli->state = STATE_ENABLE_PASSWORD;
	}
	return CLI_OK;
}

int cli_int_disable(struct cli_def *cli, char * command, char *argv[], int argc)
{
	cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
	cli_set_configmode(cli, MODE_EXEC, NULL);
	return CLI_OK;
}

int cli_int_help(struct cli_def *cli, char *command, char *argv[], int argc)
{
	cli_print(cli, "\nCommands available:");
	cli_show_help(cli, cli->commands);
	return CLI_OK;
}

int cli_int_history(struct cli_def *cli, char *command, char *argv[], int argc)
{
	int i;

	cli_print(cli, "\nCommand history:");
	for (i = 0; i < MAX_HISTORY; i++)
	{
		if (cli->history[i])
			cli_print(cli, "%3d. %s", i, cli->history[i]);
	}
	return CLI_OK;
}

int cli_int_quit(struct cli_def *cli, char *command, char *argv[], int argc)
{
	cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
	cli_set_configmode(cli, MODE_EXEC, NULL);
	return CLI_QUIT;
}

int cli_int_configure_terminal(struct cli_def *cli, char *command, char *argv[], int argc)
{
	cli_set_configmode(cli, MODE_CONFIG, NULL);
	return CLI_OK;
}

int cli_int_exit_conf(struct cli_def *cli, char *command, char *argv[], int argc)
{
	cli_set_configmode(cli, MODE_EXEC, NULL);
	return CLI_OK;
}

struct cli_def *cli_init()
{
	struct cli_def *cli;
	struct cli_command *c;

	if (!(cli = calloc(sizeof(struct cli_def), 1)))
		return NULL;

	cli_register_command(cli, NULL, "help", cli_int_help, PRIVILEGE_UNPRIVILEGED, MODE_ANY, "Show available commands");
	cli_register_command(cli, NULL, "quit", cli_int_quit, PRIVILEGE_UNPRIVILEGED, MODE_ANY, "Disconnect");
	cli_register_command(cli, NULL, "logout", cli_int_quit, PRIVILEGE_UNPRIVILEGED, MODE_ANY, "Disconnect");
	cli_register_command(cli, NULL, "exit", cli_int_quit, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Disconnect");
	cli_register_command(cli, NULL, "history", cli_int_history, PRIVILEGE_UNPRIVILEGED, MODE_ANY, "Show a list of previously run commands");
	cli_register_command(cli, NULL, "enable", cli_int_enable, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Turn on privileged commands");
	cli_register_command(cli, NULL, "disable", cli_int_disable, PRIVILEGE_PRIVILEGED, MODE_EXEC, "Turn off privileged commands");

	c = cli_register_command(cli, NULL, "configure", NULL, PRIVILEGE_PRIVILEGED, MODE_EXEC, "Enter configuration mode");
	cli_register_command(cli, c, "terminal", cli_int_configure_terminal, PRIVILEGE_PRIVILEGED, MODE_EXEC, "Configure from the terminal");

	cli_register_command(cli, NULL, "exit", cli_int_exit_conf, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Exit from configure mode");

	cli->privilege = cli->mode = -1;
	cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
	cli_set_configmode(cli, MODE_EXEC, NULL);
	return cli;
}

int cli_done(struct cli_def *cli)
{
	struct unp *u = cli->users, *n;

	while (cli->commands) cli_unregister_command(cli, cli->commands->command);
	if (cli->banner) free(cli->banner);

	while (u)
	{
		if (u->username) free(u->username);
		if (u->password) free(u->password);
		n = u->next;
		free(u);
		u = n;
	}

	free(cli);
	return CLI_OK;
}

int cli_add_history(struct cli_def *cli, char *cmd)
{
	int i;
	for (i = 0; i < MAX_HISTORY; i++)
	{
		if (!cli->history[i])
		{
			cli->history[i] = strdup(cmd);
			return CLI_OK;
		}
	}
	// No space found, drop one off the beginning of the list
	free(cli->history[0]);
	for (i = 0; i < MAX_HISTORY; i++)
		cli->history[i] = cli->history[i+1];
	cli->history[MAX_HISTORY - 1] = strdup(cmd);
	return CLI_OK;
}

int cli_parse_line(char *line, char *words[], int max_words)
{
	int nwords = 0;
	char *p = line;
	char *word_start = line;
	int inquote = 0;

	while (nwords < max_words - 1)
	{
		if (!*p || *p == inquote || (word_start && !inquote && (isspace(*p) || *p == '|')))
		{
			if (word_start)
			{
				int len = p - word_start;

				memcpy(words[nwords] = malloc(len + 1), word_start, len);
				words[nwords++][len] = 0;
			}

			if (!*p)
				break;

			if (inquote)
				p++; // skip over trailing quote

			inquote = 0;
			word_start = 0;
		}
		else if (*p == '"' || *p == '\'')
		{
		    	inquote = *p++;
			word_start = p;
		}
		else
		{
			if (!word_start)
			{
				if (*p == '|')
				{
				    	words[nwords++] = strdup("|");
				}
				else if (!isspace(*p))
					word_start = p;
			}

			p++;
		}
	}

	return nwords;
}

int cli_find_command(struct cli_def *cli, struct cli_command *commands, int num_words, char *words[], int start_word, int filters[])
{
	struct cli_command *c;
	int c_words = num_words;

	if (filters[0])
		c_words = filters[0];

	// Deal with ? for help
	if (!words[start_word]) { return CLI_ERROR; }
	if (words[start_word][strlen(words[start_word])-1] == '?')
	{
		int l = strlen(words[start_word])-1;

		for (c = commands; c; c = c->next)
		{
			if (strncasecmp(c->command, words[start_word], l) == 0 && (c->callback || c->children) && cli->privilege >= c->privilege && (c->mode == cli->mode || c->mode == MODE_ANY))
				cli_print(cli, "  %-20s %s", c->command, c->help ? : "");
		}

		return CLI_OK;
	}

	for (c = commands; c; c = c->next)
	{
		if (strncasecmp(c->command, words[start_word], c->unique_len) == 0 && strncasecmp(c->command, words[start_word], strlen(words[start_word])) == 0 && cli->privilege >= c->privilege && (c->mode == cli->mode || c->mode == MODE_ANY))
		{
			int rc = CLI_OK;
			int f;
			struct cli_filter **filt = &cli->filters;

			// Found a word!
			if (!c->children)
			{
				// Last word
				if (!c->callback)
				{
					cli_print(cli, "No callback for \"%s\"", cli_command_name(cli, c));
					return CLI_ERROR;
				}
			}
			else
			{
				if (start_word == c_words - 1)
				{
					cli_print(cli, "Incomplete command");
					return CLI_ERROR;
				}
				return cli_find_command(cli, c->children, num_words, words, start_word + 1, filters);
			}

			if (!c->callback)
			{
				cli_print(cli, "Internal server error processing \"%s\"", cli_command_name(cli, c));
				return CLI_ERROR;
			}

			for (f = 0; rc == CLI_OK && filters[f]; f++)
			{
				int n = num_words;
				char **argv;
				int argc;
				int len;

				if (filters[f+1])
					n = filters[f+1];

				if (filters[f] == n - 1)
				{
					cli_print(cli, "Missing filter");
					return CLI_ERROR;
				}

				argv = words + filters[f] + 1;
				argc = n - (filters[f] + 1);
				len = strlen(argv[0]);
				if (argv[argc-1][strlen(argv[argc-1])-1] == '?')
				{
				    	if (argc == 1)
					{
						cli_print(cli, "  %-20s %s", "begin",   "Begin with lines that match");
						cli_print(cli, "  %-20s %s", "between", "Between lines that match");
						cli_print(cli, "  %-20s %s", "count",   "Count of lines");
						cli_print(cli, "  %-20s %s", "exclude", "Exclude lines that match");
						cli_print(cli, "  %-20s %s", "include", "Include lines that match");
						cli_print(cli, "  %-20s %s", "grep",    "Include lines that match regex (options: -v, -i, -e)");
						cli_print(cli, "  %-20s %s", "egrep",   "Include lines that match extended regex");
					}
					else
					{
						if (argv[0][0] != 'c') // count
							cli_print(cli, "  WORD");

						if (argc > 2 || argv[0][0] == 'c') // count
							cli_print(cli, "  <cr>");
					}

					return CLI_OK;
				}

				if (argv[0][0] == 'b' && len < 3) // [beg]in, [bet]ween
				{
					cli_print(cli, "Ambiguous filter \"%s\" (begin, between)", argv[0]);
					return CLI_ERROR;
				}

				if (argv[0][0] == 'e' && len < 2) // [eg]rep, [ex]clude
				{
					cli_print(cli, "Ambiguous filter \"%s\" (egrep, exclude)", argv[0]);
					return CLI_ERROR;
				}

				*filt = calloc(sizeof(struct cli_filter), 1);

				if (!strncmp("include", argv[0], len) ||
				    !strncmp("exclude", argv[0], len) ||
				    !strncmp("grep", argv[0], len) ||
				    !strncmp("egrep", argv[0], len))
					rc = cli_match_filter_init(cli, argc, argv, *filt);
				else if (!strncmp("begin", argv[0], len) ||
					 !strncmp("between", argv[0], len))
					rc = cli_range_filter_init(cli, argc, argv, *filt);
				else if (!strncmp("count", argv[0], len))
					rc = cli_count_filter_init(cli, argc, argv, *filt);
				else
				{
					cli_print(cli, "Invalid filter \"%s\"", argv[0]);
					rc = CLI_ERROR;
				}

				if (rc == CLI_OK)
				{
				    	filt = &(*filt)->next;
				}
				else
				{
					free(*filt);
					*filt = 0;
				}
			}

			if (rc == CLI_OK)
				rc = c->callback(cli, cli_command_name(cli, c), words + start_word + 1, c_words - start_word - 1);

			while (cli->filters)
			{
				struct cli_filter *filt = cli->filters;

				// call one last time to clean up
				filt->filter(cli, NULL, filt->data);
				cli->filters = filt->next;
				free(filt);
			}

			return rc;
		}
	}

	cli_print(cli, "Invalid %s \"%s\"", commands->parent ? "argument" : "command", words[start_word]);
	return CLI_ERROR;
}

int cli_run_command(struct cli_def *cli, char *command)
{
	int num_words, r, i, f;
	char *words[128] = {0};
	int filters[128] = {0};

	if (!command) return CLI_ERROR;
	while (isspace(*command))
		command++;

	if (!*command) return CLI_OK;

	num_words = cli_parse_line(command, words, sizeof(words)/sizeof(words[0]));
	for (i = f = 0; i < num_words && f < sizeof(filters)/sizeof(filters[0])-1; i++)
	{
		if (words[i][0] == '|')
			filters[f++] = i;
	}

	filters[f] = 0;

	if (num_words)
		r = cli_find_command(cli, cli->commands, num_words, words, 0, filters);
	else
		r = CLI_ERROR;

	for (i = 0; i < num_words; i++)
		free(words[i]);

	if (r == CLI_QUIT)
		return r;

	return CLI_OK;
}

int cli_get_completions(char *command, char **completions, int max_completions)
{
	return 0;
}

void cli_clear_line(int sockfd, char *cmd, int l, int cursor)
{
	int i;
	if (cursor < l) for (i = 0; i < (l - cursor); i++) write(sockfd, " ", 1);
	for (i = 0; i < l; i++) cmd[i] = '\b';
	for (; i < l * 2; i++) cmd[i] = ' ';
	for (; i < l * 3; i++) cmd[i] = '\b';
	write(sockfd, cmd, i);
	memset(cmd, 0, i);
	l = cursor = 0;
}

void cli_reprompt(struct cli_def *cli)
{
	if (!cli) return;
	cli->showprompt = 1;
}

void cli_regular(struct cli_def *cli, int (*callback)(struct cli_def *cli))
{
	if (!cli) return;
	cli->regular_callback = callback;
}

#define DES_PREFIX "{crypt}"	/* to distinguish b/w clear text and DES crypted */
#define MD5_PREFIX "$1$"

static int pass_matches(char *pass, char *try)
{
    int des;
    if ((des = !strncasecmp(pass, DES_PREFIX, sizeof(DES_PREFIX)-1)))
    	pass += sizeof(DES_PREFIX)-1;

    if (des || !strncmp(pass, MD5_PREFIX, sizeof(MD5_PREFIX)-1))
    	try = crypt(try, pass);

    return !strcmp(pass, try);
}

#define CTRL(c) (c - '@')

static int show_prompt(struct cli_def *cli, int sockfd)
{
	int len = 0;

	if (cli->hostname)
		len += write(sockfd, cli->hostname, strlen(cli->hostname));

	if (cli->modestring)
		len += write(sockfd, cli->modestring, strlen(cli->modestring));

	return len + write(sockfd, cli->promptchar, strlen(cli->promptchar));
}

int cli_loop(struct cli_def *cli, int sockfd)
{
	int n;
	unsigned char c;
	char *cmd, *oldcmd = NULL;
	int l, oldl = 0, is_telnet_option = 0, skip = 0, esc = 0;
	int cursor = 0, insertmode = 1;
	char *username = NULL, *password = NULL;
	char *negotiate =
		"\xFF\xFB\x03"
		"\xFF\xFB\x01"
		"\xFF\xFD\x03"
		"\xFF\xFD\x01";

	cli->state = STATE_LOGIN;

	memset(cli->history, 0, MAX_HISTORY);
	write(sockfd, negotiate, strlen(negotiate));

	cmd = malloc(4096);

	cli->client = fdopen(sockfd, "w+");
	setbuf(cli->client, NULL);
	if (cli->banner)
		cli_print(cli, "%s", cli->banner);

	// Start off in unprivileged mode
	cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
	cli_set_configmode(cli, MODE_EXEC, NULL);

	// No auth required?
	if (!cli->users && !cli->auth_callback) cli->state = STATE_NORMAL;

	while (1)
	{
		signed int in_history = 0;
		int lastchar = 0;
		struct timeval tm;

		cli->showprompt = 1;

		if (oldcmd)
		{
			l = cursor = oldl;
			oldcmd[l] = 0;
			cli->showprompt = 1;
			oldcmd = NULL;
			oldl = 0;
		}
		else
		{
			memset(cmd, 0, 4096);
			l = 0;
			cursor = 0;
		}

		tm.tv_sec = 1;
		tm.tv_usec = 0;

		while (1)
		{
			int sr;
			fd_set r;
			if (cli->showprompt)
			{
				if (cli->state != STATE_ENABLE_PASSWORD)
					write(sockfd, "\r\n", 2);

				switch (cli->state)
				{
					case STATE_LOGIN:
						write(sockfd, "Username: ", strlen("Username: "));
						break;
					case STATE_PASSWORD:
						write(sockfd, "Password: ", strlen("Password: "));
						break;
					case STATE_NORMAL:
					case STATE_ENABLE:
						show_prompt(cli, sockfd);
						write(sockfd, cmd, l);
						if (cursor < l)
						{
							int n = l - cursor;
							while (n--)
								write(sockfd, "\b", 1);
						}
						break;
					case STATE_ENABLE_PASSWORD:
						write(sockfd, "Password: ", strlen("Password: "));
						break;

				}
				cli->showprompt = 0;
			}

			FD_ZERO(&r);
			FD_SET(sockfd, &r);

			if ((sr = select(sockfd + 1, &r, NULL, NULL, &tm)) < 0)
			{
				// Select Error
				if (errno == EINTR) continue;
				perror("read");
				l = -1;
				break;
			}
			if (sr == 0)
			{
				// Timeout every second
				if (cli->regular_callback && cli->regular_callback(cli) != CLI_OK)
					break;
				tm.tv_sec = 1;
				tm.tv_usec = 0;
				continue;
			}
			if ((n = read(sockfd, &c, 1)) < 0)
			{
				if (errno == EINTR) continue;
				perror("read");
				l = -1;
				break;
			}
			if (n == 0) { l = -1; break; }
			if (skip) { skip--; continue; }

			if (c == 255 && !is_telnet_option)
			{
				is_telnet_option++;
				continue;
			}
			if (is_telnet_option)
			{
				if (c >= 251 && c <= 254)
				{
					is_telnet_option = c;
					continue;
				}
				if (c != 255)
				{
					is_telnet_option = 0;
					continue;
				}
				is_telnet_option = 0;
			}

			// Handle ANSI arrows
			if (esc)
			{
				if (esc == '[')
				{
					// Remap to readline control codes
					switch (c)
					{
						case 'A':
							// Up
							c = CTRL('P');
							break;
						case 'B':
							// Down
							c = CTRL('N');
							break;
						case 'C':
							// Right
							c = CTRL('F');
							break;
						case 'D':
							// Left
							c = CTRL('B');
							break;
						default:
							c = 0;
					}
					esc = 0;
				}
				else
				{
					esc = (c == '[') ? c : 0;
					continue;
				}
			}

			if (c == 0) continue;
			if (c == '\n') continue;
			if (c == '\r') {
				if ((cli->state == STATE_NORMAL) || (cli->state == STATE_ENABLE))
					write(sockfd, "\r\n", 2);
				break;
			}

			if (c == 27) { esc = 1; continue; }
			if (c == CTRL('C')) { write(sockfd, "\a", 1); continue; }

			// Back word, backspace/delete
			if (c == CTRL('W') || c == CTRL('H') || c == 0x7f)
			{
				int back = 0;

				// Word
				if (c == CTRL('W'))
				{
					int nc = cursor;

					if (l == 0 || cursor == 0) continue;

					while (nc && cmd[nc - 1] == ' ')
					{
						nc--;
							back++;
					}

					while (nc && cmd[nc - 1] != ' ')
					{
						nc--;
							back++;
					}
				}
				// Char
				else
				{
					if (l == 0 || cursor == 0)
					{
						write(sockfd, "\a", 1);
						continue;
					}

					back = 1;
				}

				if (back)
				{
					while (back--)
					{
						if (l == cursor)
						{
							cmd[--cursor] = 0;
							if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD)
								write(sockfd, "\b \b", 3);
						}
						else
						{
							int i;
							cursor--;
							if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD)
							{
								for (i = cursor; i <= l; i++) cmd[i] = cmd[i+1];
								write(sockfd, "\b", 1);
								write(sockfd, cmd + cursor, strlen(cmd + cursor));
								write(sockfd, " ", 1);
								for (i = 0; i <= strlen(cmd + cursor); i++)
									write(sockfd, "\b", 1);
							}
						}
						l--;
					}

					continue;
				}
			}

			// Redraw
			if (c == CTRL('L'))
			{
				int i;
				int cursorback = l - cursor;
				write(sockfd, "\r\n", 2);
				show_prompt(cli, sockfd);
				write(sockfd, cmd, l);
				for (i = 0; i < cursorback; i++) write(sockfd, "\b", 1);
				continue;
			}

			// Clear line
			if (c == CTRL('U'))
			{
				if (cli->state == STATE_PASSWORD || cli->state == STATE_ENABLE_PASSWORD)
				{
					memset(cmd, 0, l);
				}
				else
				{
					cli_clear_line(sockfd, cmd, l, cursor);
				}
				l = 0; cursor = 0;
				continue;
			}

			// EOT
			if (c == CTRL('D'))
			{
				strcpy(cmd, "quit");
				l = cursor = strlen(cmd);
				write(sockfd, "quit\r\n", l + 2);
				break;
			}

			// disable
			if (c == CTRL('Z') && cli->state == STATE_ENABLE)
			{
				cli_clear_line(sockfd, cmd, l, cursor);
				cli_set_configmode(cli, MODE_EXEC, NULL);
				cli->showprompt = 1;
				continue;
			}

			// Tab completion
			if ((cli->state == STATE_NORMAL || cli->state == STATE_ENABLE) && c == CTRL('I'))
			{
				char *completions[128];
				int num_completions = 0;

				if (cursor != l) continue;

				if (l > 0)
					num_completions = cli_get_completions(cmd, completions, 128);

				if (num_completions == 0)
				{
					write(sockfd, "\a", 1);
				}
				else if (num_completions == 1)
				{
					// Single completion
					int i;
					for (i = l; i > 0; i--, cursor--)
					{
						write(sockfd, "\b", 1);
						if (i == ' ') break;
					}
					strcpy((cmd + i), completions[0]);
					l += strlen(completions[0]);
					cursor = l;
					write(sockfd, completions[0], strlen(completions[0]));
				}
				else if (lastchar == CTRL('I'))
				{
					// double tab
					int i;
					write(sockfd, "\r\n", 2);
					for (i = 0; i < num_completions; i++)
					{
						write(sockfd, completions[i], strlen(completions[i]));
						if (i % 4 == 3)
							write(sockfd, "\r\n", 2);
						else
							write(sockfd, "		", 1);
					}
					if (i % 4 != 3) write(sockfd, "\r\n", 2);
					cli->showprompt = 1;
				}
				else
				{
					// More than one completion
					lastchar = c;
					write(sockfd, "\a", 1);
				}
				continue;
			}

			// History
			if (c == CTRL('P') || c == CTRL('N'))
			{
				int history_found = 0;

				if (cli->state != STATE_NORMAL && cli->state != STATE_ENABLE) continue;

				if (c == CTRL('P')) // Up
				{
					in_history--;
					if (in_history < 0)
					{
						for (in_history = MAX_HISTORY-1; in_history >= 0; in_history--)
						{
							if (cli->history[in_history]) { history_found = 1; break; }
						}
					}
					else
					{
						if (cli->history[in_history]) history_found = 1;
					}
				}
				else // Down
				{
					in_history++;
					if (in_history >= MAX_HISTORY || !cli->history[in_history])
					{
						int i = 0;
						for (i = 0; i < MAX_HISTORY; i++)
							if (cli->history[i]) { in_history = i; history_found = 1; break; }
					}
					else
					{
						if (cli->history[in_history]) history_found = 1;
					}
				}
				if (history_found && cli->history[in_history])
				{
					// Show history item
					cli_clear_line(sockfd, cmd, l, cursor);
					memset(cmd, 0, 4096);
					strncpy(cmd, cli->history[in_history], 4095);
					l = cursor = strlen(cmd);
					write(sockfd, cmd, l);
				}

				continue;
			}

			// Left/right cursor motion
			if (c == CTRL('B') || c == CTRL('F'))
			{
				if (c == CTRL('B')) // Left
				{
					if (cursor)
					{
						write(sockfd, "\b", 1);
						cursor--;
					}
				}
				else // Right
				{
					if (cursor < l)
					{
						write(sockfd, &cmd[cursor], 1);
						cursor++;
					}
				}

				continue;
			}

			// Start of line
			if (c == CTRL('A'))
			{
				if (cursor)
				{
					write(sockfd, "\r", 1);
					show_prompt(cli, sockfd);
					cursor = 0;
				}

				continue;
			}

			// End of line
			if (c == CTRL('E'))
			{
				if (cursor < l)
				{
					write(sockfd, &cmd[cursor], l - cursor);
					cursor = l;
				}

				continue;
			}

			// Normal character typed
			if (cursor == l)
			{
				// End of text
				cmd[cursor] = c;
				if (l < 4095)
				{
					l++;
					cursor++;
				}
				else
				{
					write(sockfd, "\a", 1);
					continue;
				}
			}
			else
			{
				// Middle of text
				if (insertmode)
				{
					int i;
					// Move everything one character to the right
					if (l >= 4094) l--;
					for (i = l; i >= cursor; i--)
						cmd[i + 1] = cmd[i];
					// Write what we've just added
					cmd[cursor] = c;

					write(sockfd, &cmd[cursor], l - cursor + 1);
					for (i = 0; i < (l - cursor + 1); i++)
						write(sockfd, "\b", 1);
					l++;
				}
				else
				{
					cmd[cursor] = c;
				}
				cursor++;
			}

			// ?
			if ((cli->state == STATE_NORMAL || cli->state == STATE_ENABLE) && c == 63 && cursor == l)
			{
				write(sockfd, "\r\n", 2);
				oldcmd = cmd;
				oldl = cursor = l - 1;
				break;
			}

			if (cli->state != STATE_PASSWORD && cli->state != STATE_ENABLE_PASSWORD) write(sockfd, &c, 1);
			oldcmd = NULL;
			oldl = 0;
			lastchar = c;
		}

		if (l < 0) break;

		if (strcasecmp(cmd, "quit") == 0) break;
		if (cli->state == STATE_LOGIN)
		{
			if (l == 0) continue;

			// Require login
			if (username) free(username);
			username = strdup(cmd);
			cli->state = STATE_PASSWORD;
			cli->showprompt = 1;
		}
		else if (cli->state == STATE_PASSWORD)
		{
			// Require password
			int allowed = 0;

			if (password) free(password);
			password = strdup(cmd);
			if (cli->auth_callback)
			{
				if (cli->auth_callback(username, password))
				{
					allowed++;
				}
			}

			if (!allowed)
			{
				struct unp *u;
				for (u = cli->users; u; u = u->next)
				{
					if (!strcmp(u->username, username)
					    && pass_matches(u->password, password))
					{
						allowed++;
						break;
					}
				}
			}

			if (allowed)
			{
				cli_print(cli, "");
				cli->state = STATE_NORMAL;
			}
			else
			{
				cli_print(cli, "\n\nAccess denied");
				free(username); username = NULL;
				free(password); password = NULL;
				cli->state = STATE_LOGIN;
			}
			cli->showprompt = 1;
		}
		else if (cli->state == STATE_ENABLE_PASSWORD)
		{
			int allowed = 0;
			if (cli->enable_password)
			{
				// Check stored static enable password
				if (pass_matches(cli->enable_password, cmd))
					allowed++;
			}
			if (!allowed && cli->enable_callback)
			{
				// Check callback
				if (cli->enable_callback(cmd))
				{
					allowed++;
				}
			}
			if (allowed)
			{
				cli->state = STATE_ENABLE;
				cli_set_privilege(cli, PRIVILEGE_PRIVILEGED);
			}
			else
			{
				cli_print(cli, "\n\nAccess denied");
				cli->state = STATE_NORMAL;
			}
		}
		else
		{
			if (l == 0) continue;
			if (cmd[l - 1] != '?' && strcasecmp(cmd, "history") != 0) cli_add_history(cli, cmd);
			if (cli_run_command(cli, cmd) == CLI_QUIT)
				break;
		}
	}

	{
		// Cleanup
		int i;
		for (i = 0; i < MAX_HISTORY; i++)
			if (cli->history[i]) free(cli->history[i]);

		if (username) free(username);
		if (password) free(password);
		if (cmd) free(cmd);
	}

	cli->client = NULL;
	return CLI_OK;
}

int cli_file(struct cli_def *cli, FILE *fh, int privilege, int mode)
{
	int oldpriv = cli_set_privilege(cli, privilege);
	int oldmode = cli_set_configmode(cli, mode, NULL);
	char buf[4096];

	while (1)
	{
		char *p;
		char *cmd;
		char *end;

		if (fgets(buf, sizeof(buf), fh) <= 0)
			break; // End of file

		if ((p = strpbrk(buf, "#!\r\n")))
			*p = 0;

		cmd = buf;
		while (isspace(*cmd))
			cmd++;

		if (!*cmd)
			continue;

		for (p = end = cmd; *p; p++)
			if (!isspace(*p))
				end = p;

		*++end = 0;
		if (strcasecmp(cmd, "quit") == 0)
			break;

		if (cli_run_command(cli, cmd) == CLI_QUIT)
			break;
	}

	cli_set_privilege(cli, oldpriv);
	cli_set_configmode(cli, oldmode, NULL /* didn't save desc */);

	return CLI_OK;
}

void cli_print(struct cli_def *cli, char *format, ...)
{
	static char *buffer = NULL;
	static int size = 0;
	char *p;
	int n;
	va_list ap;

	va_start(ap, format);
	while (!buffer || (n = vsnprintf(buffer, size, format, ap)) >= size)
	{
		if (!(p = realloc(buffer, size += 4096)))
			return;

		buffer = p;
	}
	va_end(ap);

	if (n < 0) // vsnprintf failed
		return;

	p = buffer;
	do {
		char *next = strchr(p, '\n');
		struct cli_filter *f = cli->filters;
		int print = 1;

		if (next)
			*next++ = 0;

		while (print && f)
		{
			print = (f->filter(cli, p, f->data) == CLI_OK);
			f = f->next;
		}

		if (print)
		{
			if (cli->print_callback)
				cli->print_callback(cli, p);
			else if (cli->client)
				fprintf(cli->client, "%s\r\n", p);
		}

		p = next;
	} while (p);
}

static char *join_words(int argc, char **argv)
{
	char *p;
	int len = 0;
	int i;

	for (i = 0; i < argc; i++)
	{
		if (i)
			len += 1;

		len += strlen(argv[i]);
	}

	p = malloc(len + 1);
	p[0] = 0;

	for (i = 0; i < argc; i++)
	{
		if (i)
			strcat(p, " ");

		strcat(p, argv[i]);
	}

	return p;
}

struct cli_match_filter_state {
	int flags;
#define MATCH_REGEX		1
#define MATCH_INVERT		2
	union {
		char *string;
		regex_t re;
	} match;
};

int cli_match_filter_init(struct cli_def *cli, int argc, char **argv, struct cli_filter *filt)
{
	struct cli_match_filter_state *state;
	int rflags;
	int i;
	char *p;

	if (argc < 2)
	{
		if (cli->client)
			fprintf(cli->client, "Match filter requires an argument\r\n");

		return CLI_ERROR;
	}

	filt->filter = cli_match_filter;
	filt->data = state = calloc(sizeof(struct cli_match_filter_state), 1);

	if (argv[0][0] == 'i' || // include/exclude
	    (argv[0][0] == 'e' && argv[0][1] == 'x'))
	{
		if (argv[0][0] == 'e')
			state->flags = MATCH_INVERT;

		state->match.string = join_words(argc-1, argv+1);
		return CLI_OK;
	}

	state->flags = MATCH_REGEX;

	// grep/egrep
	rflags = REG_NOSUB;
	if (argv[0][0] == 'e') // egrep
		rflags |= REG_EXTENDED;

	i = 1;
	while (i < argc - 1 && argv[i][0] == '-' && argv[i][1])
	{
		int last = 0;
		p = &argv[i][1];

		if (strspn(p, "vie") != strlen(p))
			break;

		while (*p)
		{
			switch (*p++)
			{
			case 'v':
				state->flags |= MATCH_INVERT;
				break;

			case 'i':
				rflags |= REG_ICASE;
				break;

			case 'e':
			    	last++;
			    	break;
			}
		}

		i++;
		if (last)
			break;
	}

	p = join_words(argc-i, argv+i);
	if ((i = regcomp(&state->match.re, p, rflags)))
	{
		if (cli->client)
			fprintf(cli->client, "Invalid pattern \"%s\"\r\n", p);

	    	free(p);
		return CLI_ERROR;
	}

	free(p);
	return CLI_OK;
}

int cli_match_filter(struct cli_def *cli, char *string, void *data)
{
	struct cli_match_filter_state *state = data;
	int r = CLI_ERROR;

    	if (!string) // clean up
	{
	    	if (state->flags & MATCH_REGEX)
			regfree(&state->match.re);
		else
			free(state->match.string);

		free(state);
		return CLI_OK;
	}

	if (state->flags & MATCH_REGEX)
	{
	    	if (!regexec(&state->match.re, string, 0, NULL, 0))
			r = CLI_OK;
	}
	else
	{
		if (strstr(string, state->match.string))
			r = CLI_OK;
	}

	if (state->flags & MATCH_INVERT)
	{
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

int cli_range_filter_init(struct cli_def *cli, int argc, char **argv, struct cli_filter *filt)
{
	struct cli_range_filter_state *state;
	char *from = 0;
	char *to = 0;

	if (!strncmp(argv[0], "bet", 3)) // between
	{
		if (argc < 3)
		{
			if (cli->client)
				fprintf(cli->client, "Between filter requires 2 arguments\r\n");

			return CLI_ERROR;
		}

		from = strdup(argv[1]);
		to = join_words(argc-2, argv+2);
	}
	else // begin
	{
		if (argc < 2)
		{
			if (cli->client)
				fprintf(cli->client, "Begin filter requires an argument\r\n");

			return CLI_ERROR;
		}

	    	from = join_words(argc-1, argv+1);
	}

	filt->filter = cli_range_filter;
	filt->data = state = calloc(sizeof(struct cli_range_filter_state), 1);

	state->from = from;
	state->to = to;

	return CLI_OK;
}

int cli_range_filter(struct cli_def *cli, char *string, void *data)
{
	struct cli_range_filter_state *state = data;
	int r = CLI_ERROR;

    	if (!string) // clean up
	{
		free(state->from);
		free(state->to);
		free(state);
		return CLI_OK;
	}

	if (!state->matched)
		state->matched = !!strstr(string, state->from);

	if (state->matched)
	{
	    	r = CLI_OK;
		if (state->to && strstr(string, state->to))
			state->matched = 0;
	}

	return r;
}

int cli_count_filter_init(struct cli_def *cli, int argc, char **argv, struct cli_filter *filt)
{
	if (argc > 1)
	{
		if (cli->client)
			fprintf(cli->client, "Count filter does not take arguments\r\n");

		return CLI_ERROR;
	}

	filt->filter = cli_count_filter;
	filt->data = calloc(sizeof(int), 1);

	return CLI_OK;
}

int cli_count_filter(struct cli_def *cli, char *string, void *data)
{
	int *count = data;

    	if (!string) // clean up
	{
		// print count
		if (cli->client)
			fprintf(cli->client, "%d\r\n", *count);

		free(count);
		return CLI_OK;
	}

	while (isspace(*string))
		string++;

	if (*string)
		(*count)++;  // only count non-blank lines

	return CLI_ERROR; // no output
}

void cli_print_callback(struct cli_def *cli, void (*callback)(struct cli_def *, char *))
{
	cli->print_callback = callback;
}
