#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <malloc.h>
#include <string.h>
#include "libcli.h"

#define MAX_HISTORY 256

struct cli_def cli = {0};
FILE *cli_client = NULL;
char *cli_history[MAX_HISTORY];

char *cli_command_name(struct cli_command *command)
{
    int l;
    char *name = calloc(1, 1), *o;
    while (command) {
	o = name;
	name = calloc(strlen(command->command) + strlen(o) + 1, 1);
	sprintf(name, "%s %s", command->command, o);
	command = command->parent;
	free(o);
    }
    l = strlen(name);
    if (l) name[l - 1] = 0;
    return name;
}

void cli_set_auth_callback(int (*auth_callback)(char *, char *))
{
    cli.auth_callback = auth_callback;
}

void cli_allow_user(char *username, char *password)
{
    struct unp *u, *n;
    n = malloc(sizeof(struct unp));
    n->username = strdup(username);
    n->password = strdup(password);
    n->next = NULL;

    if (!cli.users)
	cli.users = n;
    else
    {
	for (u = cli.users; u && u->next; u = u->next);
	if (u) u->next = n;
    }
}

void cli_deny_user(char *username)
{
    struct unp *u, *p = NULL;
    if (!cli.users) return;
    for (u = cli.users; u; u = u->next)
    {
	if (strcmp(username, u->username) == 0)
	{
	    if (p)
		p->next = u->next;
	    else
		cli.users = u->next;
	    free(u->username);
	    free(u->password);
	    free(u);
	    break;
	}
	p = u;
    }
}

void cli_set_banner(char *banner)
{
    if (cli.banner) free(cli.banner);
    cli.banner = strdup(banner);
}

int cli_build_shortest(struct cli_command *commands)
{
    struct cli_command *c, *p;

    for (c = commands; c; c = c->next)
    {
	for (c->unique_len = 1; c->unique_len <= strlen(c->command); c->unique_len++)
	{
	    int foundmatch = 0;
	    for (p = cli.commands; p; p = p->next)
	    {
		if (c == p) continue;
		if (strncmp(p->command, c->command, c->unique_len) == 0) foundmatch++;
	    }
	    if (!foundmatch) break;
	}
	if (c->children) cli_build_shortest(c->children);
    }
    return CLI_OK;
}

struct cli_command *cli_register_command(struct cli_command *parent, char *command, int (*callback)(FILE *, char *, char **, int), char *help)
{
    struct cli_command *c, *p;

    if (!command) return NULL;
    if (!(c = calloc(sizeof(struct cli_command), 1))) return NULL;

    c->callback = callback;
    c->next = NULL;
    c->command = strdup(command);
    c->parent = parent;
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
	    {
		p->next = c;
	    }
	    else
	    {
		fprintf(cli_client, "Can't find end of linked list!\r\n");
		return NULL;
	    }
	}
    }
    else
    {
	if (!cli.commands)
	{
	    cli.commands = c;
	}
	else
	{
	    for (p = cli.commands; p && p->next; p = p->next);
	    if (p)
	    {
		p->next = c;
	    }
	    else
	    {
		fprintf(cli_client, "Can't find end of linked list!\r\n");
		return NULL;
	    }
	}
    }

    cli_build_shortest((parent) ? parent : cli.commands);
    return c;
}

int cli_unregister_command(char *command)
{
    struct cli_command *c, *p = NULL;

    if (!command) return -1;
    if (!cli.commands) return CLI_OK;

    for (c = cli.commands; c; c = c->next)
    {
	if (strcmp(c->command, command) == 0)
	{
	    if (p)
		p->next = c->next;
	    else
		cli.commands = c->next;
	    free(c->command);
	    free(c);
	    return CLI_OK;
	}
	p = c;
    }

    return CLI_OK;
}

int cli_show_help(struct cli_command *c)
{
    struct cli_command *p;
    for (p = c; p; p = p->next)
    {
	if (p->command && p->callback)
	{
	    fprintf(cli_client, "%-20s%s\r\n", cli_command_name(p), p->help ? : "");
	}
	if (p->children)
	{
	    cli_show_help(p->children);
	}
    }
    return CLI_OK;
}

int cli_int_help(FILE *client, char *command, char *argv[], int argc)
{
    fprintf(client, "\r\nCommands available:\r\n");
    cli_show_help(cli.commands);
    return CLI_OK;
}

int cli_int_history(FILE *client, char *command, char *argv[], int argc)
{
    int i;

    fprintf(client, "\r\nCommand history:\r\n");
    for (i = 0; i < MAX_HISTORY; i++)
    {
	if (cli_history[i])
	    fprintf(client, "%3d. %s\r\n", i, cli_history[i]);
    }
    return CLI_OK;
}

int cli_int_quit(FILE *client, char *command, char *argv[], int argc)
{
    return CLI_QUIT;
}

int cli_init()
{
    cli_register_command(NULL, "help", cli_int_help, "Disconnect");
    cli_register_command(NULL, "quit", cli_int_quit, "Disconnect");
    cli_register_command(NULL, "logout", cli_int_quit, "Disconnect");
    cli_register_command(NULL, "exit", cli_int_quit, "Disconnect");
    cli_register_command(NULL, "history", cli_int_history, "Show a list of previously run commands");
    return CLI_OK;
}

int cli_done()
{
    while (cli.commands) cli_unregister_command(cli.commands->command);
    return CLI_OK;
}

char *cli_trim_leading(char *value)
{
	char *p;

	for (p = value; *p; p++)
		if (*p > 32 && *p < 127)
			break;

	return p;
}

char *cli_trim_trailing(char *value)
{
	int i;

	for (i = strlen(value) - 1; i > 0; i--)
	{
		if (value[i] > 32 && value[i] < 127) break;
		value[i] = 0;
	}

	return value;
}

int cli_add_history(char *cmd)
{
    int i;
    for (i = 0; i < MAX_HISTORY; i++)
    {
	if (!cli_history[i])
	{
	    cli_history[i] = strdup(cmd);
	    return CLI_OK;
	}
    }
    // No space found, drop one off the beginning of the list
    free(cli_history[i]);
    for (i = 0; i < MAX_HISTORY; i++)
	cli_history[i] = cli_history[i+1];
    cli_history[i] = strdup(cmd);
    return CLI_OK;
}

int cli_parse_line(char *line, char *words[], int max_words)
{
	int word_counter = 0, index;
	char *p = line;
	char *word_start = NULL;
	char quotes = 0;
	int quotes_level = 0;

	while (word_counter < max_words)
	{
		if (*p == '\n' || *p == '\r')
			return word_counter;
		if (*p == ' ' || *p == '\t' || *p == 0)
		{
			if (word_start && !quotes)
			{
				char *lastchar;
				words[word_counter] = (char *)calloc((p - word_start) + 1, 1);
				memcpy(words[word_counter], word_start, p - word_start);

				// Chop trailing quote
				lastchar = words[word_counter] + strlen(words[word_counter]) - 1;
				if (*lastchar == '"') *lastchar = 0;

				word_start = NULL;
				word_counter++;
			}
			if (*p == 0)
				break;
			p++;
			continue;
		}
		if (word_start == NULL)
			word_start = p;

		if ((*p == '"' || *p == '(' || *p == '[') && quotes_level == 0)
		{
			quotes = *p;
			quotes_level++;
			if (p == word_start) word_start++;
			p++;
			continue;
		}

		if ((*p == '"' && quotes == '"') || (*p == ')' && quotes == '(') || (*p == ']' && quotes == '['))
		{
			quotes_level--;
			if (quotes_level == 0)
				quotes = 0;
			p++;
			continue;
		}

		p++;
	}
	for (index = word_counter; index < max_words; index++)
	{
		words[index] = NULL;
	}

	return word_counter;
}

int cli_find_command(struct cli_command *commands, int num_words, char *words[], int start_word)
{
    struct cli_command *c;

    // Deal with ? for help
    if (!words[start_word]) { fprintf(cli_client, "Incomplete command\r\n"); return CLI_ERROR; }
    if (words[start_word][strlen(words[start_word])-1] == '?')
    {
	int l = strlen(words[start_word])-1;

	for (c = commands; c; c = c->next)
	{
	    if (strncasecmp(c->command, words[start_word], l) == 0 && (c->callback || c->children))
	    {
		fprintf(cli_client, "%-20s%s\r\n", c->command, c->help ? : "");
	    }
	}

	return CLI_OK;
    }

    for (c = commands; c; c = c->next)
    {
	if (strncasecmp(c->command, words[start_word], c->unique_len) == 0 && strncasecmp(c->command, words[start_word], strlen(words[start_word])) == 0)
	{
	    // Found a word!
	    if (!c->children)
	    {
		// Last word
		if (!c->callback)
		{
		    fprintf(cli_client, "No callback for \"%s\"\r\n", cli_command_name(c));
		    return CLI_ERROR;
		}
	    }
	    else
	    {
		if (start_word == num_words)
		{
		    fprintf(cli_client, "Incomplete command\r\n");
		    return CLI_ERROR;
		}
		return cli_find_command(c->children, num_words, words, start_word + 1);
	    }

	    if (!c->callback)
	    {
		fprintf(cli_client, "Internal server error processing \"%s\"\r\n", cli_command_name(c));
		return CLI_ERROR;
	    }
//	    printf("Found command \"%s\"\n", cli_command_name(c));
	    return c->callback(cli_client, cli_command_name(c), words + start_word + 1, num_words - start_word - 1);
	}
    }
    fprintf(cli_client, "Invalid command \"%s\"\r\n", words[start_word]);
    return CLI_ERROR;
}

int cli_run_command(char *command)
{
    int num_words, r, i;
    char *words[128] = {0};

    if (!command) return CLI_ERROR;
    command = cli_trim_trailing(cli_trim_leading(command));
    if (!*command) return CLI_OK;

    num_words = cli_parse_line(command, words, 128);
    if (!num_words) return CLI_ERROR;

    r = cli_find_command(cli.commands, num_words, words, 0);
    for (i = 0; i < num_words; i++)
	free(words[i]);

    if (r == CLI_QUIT)
	return r;

    return CLI_OK;
}

int cli_get_help(char *command)
{
    return 0;
}

int cli_get_completions(char *command, char **completions, int max_completions)
{
    return 0;
}

void cli_clear_line(int sockfd, char *cmd, int l, int cursor)
{
    int i;
    if (cursor < l) for (i = 0; i < (l - cursor); i++) write(sockfd, " ", 1);
    for (i = 0; i < l; i++) cmd[i] = '\x08';
    for (; i < l * 2; i++) cmd[i] = '\x20';
    for (; i < l * 3; i++) cmd[i] = '\x08';
    write(sockfd, cmd, i);
    memset(cmd, 0, i);
    l = cursor = 0;
}

int cli_loop(int sockfd, char *prompt)
{
    int n;
    unsigned char c;
    char *cmd, *oldcmd = NULL;
    int l, oldl = 0, is_telnet_option = 0, skip = 0, esc = 0;
    int cursor = 0, insertmode = 1;
    int state = 0;
    char *username = NULL, *password = NULL;

    char *negotiate =
	"\xFF\xFB\x03"
	"\xFF\xFB\x01"
	"\xFF\xFD\x03"
	"\xFF\xFD\x01";

    memset(cli_history, 0, MAX_HISTORY);
    write(sockfd, negotiate, strlen(negotiate));

    cmd = malloc(4096);

    cli_client = fdopen(sockfd, "w+");

    setbuf(cli_client, NULL);
    if (cli.banner)
	fprintf(cli_client, "%s\r\n", cli.banner);

    if (!cli.users && !cli.auth_callback) state = 2;

    while (1)
    {
	signed int in_history = 0;
	int lastchar = 0;
	int showit = 1;

	if (oldcmd)
	{
	    l = cursor = oldl;
	    oldcmd[l] = 0;
	    showit = 1;
	    oldcmd = NULL;
	    oldl = 0;
	}
	else
	{
	    memset(cmd, 0, 4096);
	    l = 0;
	    cursor = 0;
	}

	while (1)
	{

	    if (showit)
	    {
		write(sockfd, "\r\n", 2);
		switch (state)
		{
		    case 0: write(sockfd, "Username: ", strlen("Username: "));
			    break;
		    case 1: write(sockfd, "Password: ", strlen("Password: "));
			    break;
		    case 2: write(sockfd, prompt, strlen(prompt));
			    write(sockfd, cmd, l);
			    break;
		}
		showit = 0;
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

	    // Handle Escape codes
	    if (esc)
	    {
		// Handle arrow keys
		if (esc == 91)
		{
		    if (state == 2 && (c == 65 || c == 66))
		    {
			if (c == 65)
			{
			    // Up arrow
			    in_history--;
			    if (in_history < 0)
			    {
				int i = 0;
				for (i = MAX_HISTORY; i >= 0; i--)
				    if (cli_history[i]) { in_history = i; break; }
			    }
			}
			else if (c == 66)
			{
			    // Down arrow
			    in_history++;
			    if (in_history >= MAX_HISTORY || !cli_history[in_history])
			    {
				int i = 0;
				for (i = 0; i < MAX_HISTORY; i++)
				    if (cli_history[i]) { in_history = i; break; }
			    }
			}
			if (cli_history[in_history])
			{
			    // Show history item
			    cli_clear_line(sockfd, cmd, l, cursor);
			    memset(cmd, 0, 4096);
			    strcpy(cmd, cli_history[in_history]);
			    l = cursor = strlen(cmd);
			    write(sockfd, cmd, l);
			}
		    }
		    else if (state == 2 && c == 68)
		    {
			// Left arrow
			if (cursor != 0)
			{
			    cursor--;
			    write(sockfd, "\x08", 1);
			}
		    }
		    else if (state == 2 && c == 67)
		    {
			// Right arrow
			if (cursor < l)
			{
			    write(sockfd, &cmd[cursor], 1);
			    cursor++;
			}
		    }
		}
		else
		{
		    if (c == 91) { esc = c; continue; }
		}
		esc = 0;
		continue;
	    }

	    if (c == '\n') continue;
	    if (c == '\r') { if (state == 2) write(sockfd, "\r\n", 2); break; }
	    if (c == 0) continue;

	    if (c == 3) { write(sockfd, "\x07", 1); continue; }
	    if (c == 27) { esc = 1; continue; }

	    // Backspace
	    if (c == 127 || c == 8)
	    {
		if (l == 0 || cursor == 0)
		{
		    write(sockfd, "\x07", 2);
		}
		else
		{
		    if (l == cursor)
		    {
			cursor--;
			cmd[l] = 0;
			if (state != 1) write(sockfd, "\x08\x20\x08", 3);
		    }
		    else
		    {
			int i;
			cursor--;
			if (state != 1)
			{
			    for (i = cursor; i <= l; i++) cmd[i] = cmd[i+1];
			    write(sockfd, "\x08\x08", 1);
			    write(sockfd, cmd + cursor, strlen(cmd + cursor));
			    write(sockfd, " ", 1);
			    for (i = 0; i <= strlen(cmd + cursor); i++)
				write(sockfd, "\x08", 1);
			}
		    }
		    l--;
		}
		continue;
	    }

	    // Ctrl-L
	    if (c == 12)
	    {
		int i;
		int totallen = l + strlen(prompt);
		int cursorback = l - cursor;
		for (i = 0; i < totallen; i++) write(sockfd, "\x08", 1);
		for (i = 0; i < totallen; i++) write(sockfd, " ", 1);
		for (i = 0; i < totallen; i++) write(sockfd, "\x08", 1);
		write(sockfd, prompt, strlen(prompt));
		write(sockfd, cmd, l);
		for (i = 0; i < cursorback; i++) write(sockfd, "\x08", 1);
		continue;
	    }

	    // Ctrl-U
	    if (c == 21)
	    {
		cli_clear_line(sockfd, cmd, l, cursor);
		l = 0; cursor = 0;
		continue;
	    }

	    // Ctrl-D
	    if (c == 4)
	    {
		strcpy(cmd, "quit");
		l = cursor = strlen(cmd);
		write(sockfd, "quit\r\n", l + 2);
		break;
	    }

	    // Tab completion
	    if (state == 2 && c == 9)
	    {
		char *completions[128];
		int num_completions = 0;

		if (cursor != l) continue;

		if (l > 0) num_completions = cli_get_completions(cmd, completions, 128);
		if (num_completions == 0)
		{
		    c = 7;
		    write(sockfd, &c, 1);
		}
		else if (num_completions == 1)
		{
		    // Single completion
		    int i;
		    for (i = l; i > 0; i--, cursor--)
		    {
			write(sockfd, "\x08", 1);
			if (i == ' ') break;
		    }
		    strcpy((cmd + i), completions[0]);
		    l += strlen(completions[0]);
		    cursor = l;
		    write(sockfd, completions[0], strlen(completions[0]));
		}
		else if (lastchar == 9)
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
			    write(sockfd, "	", 1);
		    }
		    if (i % 4 != 3) write(sockfd, "\r\n", 2);
		    showit = 1;
		}
		else
		{
		    // More than one completion
		    lastchar = c;
		    c = 7;
		    write(sockfd, &c, 1);
		}
		continue;
	    }

	    // Normal character typed
	    if (cursor == l)
	    {
		// End of text
		cmd[cursor] = c;
		l++;
	    }
	    else
	    {
		// Middle of text
		if (insertmode)
		{
		    int i;
		    // Move everything one character to the right
		    for (i = l; i >= cursor; i--)
			cmd[i + 1] = cmd[i];
		    // Write what we've just added
		    cmd[cursor] = c;

		    write(sockfd, &cmd[cursor], l - cursor + 1);
		    for (i = 0; i < (l - cursor + 1); i++)
			write(sockfd, "\x08", 1);
		    l++;
		}
		else
		{
		    cmd[cursor] = c;
		}
	    }
	    cursor++;

	    // ?
	    if (state == 2 && c == 63 && cursor == l)
	    {
		write(sockfd, "\r\n", 2);
		oldcmd = cmd;
		oldl = cursor = l - 1;
		break;
	    }

	    if (state != 1) write(sockfd, &c, 1);
	    oldcmd = NULL;
	    oldl = 0;
	    lastchar = c;
	}

	if (l < 0) break;

	if (strcasecmp(cmd, "quit") == 0) break;

	if (state == 0)
	{
	    if (l == 0) continue;

	    // Require login
	    if (username) free(username);
	    username = strdup(cmd);
	    state++;
	    showit = 1;
	}
	else if (state == 1)
	{
	    // Require password
	    int allowed = 0;

	    if (l == 0) { state = 0; continue; }

	    if (password) free(password);
	    password = strdup(cmd);
	    if (cli.auth_callback)
	    {
		if (cli.auth_callback(username, password))
		{
		    allowed++;
		}
	    }

	    if (!allowed)
	    {
		struct unp *u;
		for (u = cli.users; u; u = u->next)
		{
		    if (strcmp(username, u->username) == 0 && strcmp(password, u->password) == 0)
		    {
			allowed++;
			break;
		    }
		}
	    }

	    if (allowed)
	    {
		state = 2;
	    }
	    else
	    {
		fprintf(cli_client, "Access denied\r\n");
		free(username); username = NULL;
		free(password); password = NULL;
		state = 0;
	    }
	    showit = 1;
	}
	else
	{
	    if (l == 0) continue;
	    if (cmd[l-1] != '?' && strcasecmp(cmd, "history") != 0) cli_add_history(cmd);
//	    printf("Command: %s\n", cmd);
	    if (cli_run_command(cmd) == CLI_QUIT)
		break;
	}
    }

    {
	// Cleanup
	int i;
	for (i = 0; i < MAX_HISTORY; i++)
	    if (cli_history[i]) free(cli_history[i]);
	if (username) free(username);
	if (password) free(password);
	if (cmd) free(cmd);
    }

    return CLI_OK;
}

