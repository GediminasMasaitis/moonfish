/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <string.h>
#include <stdlib.h>

#include "../moonfish.h"
#include "tools.h"
#include "https.h"

static void moonfish_parse_chat(char *argv0, char *line, char ***command, int *count)
{
	int length;
	
	*command = NULL;
	*count = 0;
	
	while (*line == ' ') line++;
	if (*line == 0) return;
	if (*line == '@')
	{
		line++;
		while (*line != ' ' && *line != 0) line++;
		while (*line == ' ') line++;
	}
	if (*line == ':')
	{
		line++;
		while (*line != ' ' && *line != 0) line++;
		while (*line == ' ') line++;
	}
	
	while (*line != 0)
	{
		*command = realloc(*command, sizeof **command * (*count + 1));
		if (*line == ':')
		{
			(*command)[*count] = strdup(line + 1);
			if ((*command)[*count] == NULL)
			{
				perror(argv0);
				exit(1);
			}
			(*count)++;
			break;
		}
		
		length = 0;
		while (line[length] != ' ' && line[length] != 0) length++;
		(*command)[*count] = strndup(line, length);
		line += length;
		if ((*command)[*count] == NULL)
		{
			perror(argv0);
			exit(1);
		}
		
		while (*line == ' ') line++;
		(*count)++;
	}
}

static void moonfish_free_chat(char **command, int count)
{
	int i;
	if (count == 0) return;
	for (i = 0 ; i < count ; i++) free(command[i]);
	free(command);
}

static void moonfish_read_chat(char *argv0, struct tls *tls, char *username, char **channel, char **message)
{
	static int under_count = 0;
	
	char *line;
	char **command;
	int count;
	int i;
	
	for (;;)
	{
		line = moonfish_read_line(argv0, tls);
		if (line == NULL)
		{
			fprintf(stderr, "%s: IRC connection closed unexpectedly\n", argv0);
			exit(1);
		}
		
		moonfish_parse_chat(argv0, line, &command, &count);
		free(line);
		
		if (count == 0) continue;
		
		if (!strcmp(command[0], "433"))
		{
			moonfish_free_chat(command, count);
			
			under_count++;
			
			if (under_count > 16)
			{
				fprintf(stderr, "%s: Tried too many nicknames\n", argv0);
				exit(1);
			}
			
			moonfish_write_text(argv0, tls, "NICK ");
			moonfish_write_text(argv0, tls, username);
			for (i = 0 ; i < under_count ; i++) moonfish_write_text(argv0, tls, "_");
			moonfish_write_text(argv0, tls, "\r\n");
			
			continue;
		}
		
		if (!strcmp(command[0], "PING"))
		{
			if (count < 2)
			{
				moonfish_free_chat(command, count);
				moonfish_write_text(argv0, tls, "PONG\r\n");
				continue;
			}
			
			moonfish_write_text(argv0, tls, "PONG ");
			moonfish_write_text(argv0, tls, command[1]);
			moonfish_write_text(argv0, tls, "\r\n");
			moonfish_free_chat(command, count);
			continue;
		}
		
		if (!strcmp(command[0], "PRIVMSG"))
		{
			if (count < 3)
			{
				fprintf(stderr, "%s: Invalid IRC message\n", argv0);
				exit(1);
			}
			
			*channel = strdup(command[1]);
			if (*channel == NULL)
			{
				perror(argv0);
				exit(1);
			}
			
			*message = strdup(command[2]);
			if (*message == NULL)
			{
				perror(argv0);
				exit(1);
			}
			
			moonfish_free_chat(command, count);
			break;
		}
		
		moonfish_free_chat(command, count);
	}
}

static void moonfish_chat(char *argv0, char **command, char **options, char *host, char *port, char *username, char *channels)
{
	struct tls *tls;
	char *channel;
	char *message;
	struct moonfish_chess chess;
	struct moonfish_move move;
	char name[12];
	FILE *in, *out;
	char *value;
	char *names, *name0;
	char fen[128];
	char *password;
	int i;
	
	moonfish_chess(&chess);
	
	moonfish_spawn(argv0, command, &in, &out, NULL);
	fprintf(in, "uci\n");
	moonfish_wait(out, "uciok");
	
	for (;;)
	{
		value = strchr(*options, '=');
		if (value == NULL) break;
		fprintf(in, "setoption name %.*s value %s\n", (int) (value - *options), *options, value + 1);
		options++;
	}
	
	fprintf(in, "isready\n");
	moonfish_wait(out, "readyok");
	
	fprintf(in, "ucinewgame\n");
	
	tls = moonfish_connect(argv0, host, port);
	
	/* todo: validate password */
	password = getenv("moonfish_chat_password");
	if (password != NULL && *password != 0)
	{
		moonfish_write_text(argv0, tls, "PASS ");
		moonfish_write_text(argv0, tls, password);
		moonfish_write_text(argv0, tls, "\r\n");
	}
	
	moonfish_write_text(argv0, tls, "USER ");
	moonfish_write_text(argv0, tls, username);
	moonfish_write_text(argv0, tls, " 0 * ");
	moonfish_write_text(argv0, tls, username);
	moonfish_write_text(argv0, tls, "\r\nNICK ");
	moonfish_write_text(argv0, tls, username);
	moonfish_write_text(argv0, tls, "\r\n");
	
	moonfish_write_text(argv0, tls, "JOIN ");
	moonfish_write_text(argv0, tls, channels);
	moonfish_write_text(argv0, tls, "\r\n");
	
	names = strdup("");
	if (names == NULL)
	{
		perror(argv0);
		exit(1);
	}
	
	channel = NULL;
	message = NULL;
	
	for (;;)
	{
		if (channel != NULL) free(channel);
		if (message != NULL) free(message);
		
		moonfish_read_chat(argv0, tls, username, &channel, &message);
		
		if (!strcmp(message, "!chess reset"))
		{
			moonfish_chess(&chess);
			fprintf(in, "ucinewgame\n");
			
			moonfish_write_text(argv0, tls, "PRIVMSG ");
			moonfish_write_text(argv0, tls, channel);
			moonfish_write_text(argv0, tls, " :The board has been reset.\r\n");
			
			continue;
		}
		
		if (moonfish_from_san(&chess, &move, message))
			continue;
		
		moonfish_play(&chess, &move);
		if (moonfish_finished(&chess))
		{
			moonfish_to_fen(&chess, fen);
			moonfish_chess(&chess);
			fprintf(in, "ucinewgame\n");
			
			for (i = 0 ; fen[i] != 0 ; i++)
				if (fen[i] == ' ') fen[i] = '_';
			
			moonfish_write_text(argv0, tls, "PRIVMSG ");
			moonfish_write_text(argv0, tls, channel);
			moonfish_write_text(argv0, tls, " :https://lichess.org/export/fen.gif?fen=");
			moonfish_write_text(argv0, tls, fen);
			moonfish_write_text(argv0, tls, "\r\n");
			
			moonfish_write_text(argv0, tls, "PRIVMSG ");
			moonfish_write_text(argv0, tls, channel);
			moonfish_write_text(argv0, tls, " :Game over!\r\n");
			
			continue;
		}
		
		moonfish_to_uci(name, &move);
		
		names = realloc(names, strlen(names) + strlen(name) + 2);
		if (names == NULL)
		{
			perror(argv0);
			exit(1);
		}
		
		strcat(names, " ");
		strcat(names, name);
		
		fprintf(in, "isready\n");
		moonfish_wait(out, "readyok");
		fprintf(in, "position startpos moves");
		fprintf(in, names);
		fprintf(in, "\n");
		
		fprintf(in, "isready\n");
		moonfish_wait(out, "readyok");
		fprintf(in, "go movetime 5000\n");
		
		name0 = moonfish_wait(out, "bestmove");
		names = realloc(names, strlen(names) + strlen(name0) + 2);
		if (names == NULL)
		{
			perror(argv0);
			exit(1);
		}
		
		strcat(names, " ");
		strcat(names, name0);
		
		moonfish_from_uci(&chess, &move, name0);
		moonfish_to_san(&chess, name, &move);
		moonfish_play(&chess, &move);
		moonfish_to_fen(&chess, fen);
		
		for (i = 0 ; fen[i] != 0 ; i++)
			if (fen[i] == ' ') fen[i] = '_';
		
		moonfish_write_text(argv0, tls, "PRIVMSG ");
		moonfish_write_text(argv0, tls, channel);
		moonfish_write_text(argv0, tls, " :");
		moonfish_write_text(argv0, tls, name);
		moonfish_write_text(argv0, tls, "\r\n");
		
		moonfish_write_text(argv0, tls, "PRIVMSG ");
		moonfish_write_text(argv0, tls, channel);
		moonfish_write_text(argv0, tls, " :https://lichess.org/export/fen.gif?fen=");
		moonfish_write_text(argv0, tls, fen);
		moonfish_write_text(argv0, tls, "\r\n");
		
		if (moonfish_finished(&chess))
		{
			moonfish_chess(&chess);
			fprintf(in, "ucinewgame\n");
			
			moonfish_write_text(argv0, tls, "PRIVMSG ");
			moonfish_write_text(argv0, tls, channel);
			moonfish_write_text(argv0, tls, " :Game over!\r\n");
		}
	}
}

int main(int argc, char **argv)
{
	static char *format = "<UCI-options> <cmd> <args>...";
	static struct moonfish_arg args[] =
	{
		{"N", "host", "<name>", "irc.libera.chat", "network host name (default: 'irc.libera.chat')"},
		{"P", "port", "<port>", "6697", "network port (default: '6697')"},
		{"M", "nick", "<nickname>", "moonfish", "the bot's nickname (default: 'moonfish')"},
		{"C", "channel", "<channels>", "#moonfish", "channels to join (default: '#moonfish')"},
		{NULL, NULL, NULL, NULL, NULL},
	};
	
	char **options, **command;
	
	/* todo: validate nickname & channels*/
	options = moonfish_args(args, format, argc, argv);
	
	command = options;
	for (;;)
	{
		if (*command == NULL) moonfish_usage(args, format, argv[0]);
		if (strchr(*command, '=') == NULL) break;
		command++;
	}
	
	moonfish_chat(argv[0], command, options, args[0].value, args[1].value, args[2].value, args[3].value);
	fprintf(stderr, "%s: Unreachable\n", argv[0]);
	return 1;
}
