/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <string.h>
#include <stdlib.h>

#include "../moonfish.h"
#include "tools.h"
#include "https.h"

static void moonfish_parse_chat(char *line, char ***command, int *count)
{
	int length;
	
	*command = NULL;
	*count = 0;
	
	while (*line == ' ') line++;
	if (*line == 0) return;
	if (*line == '@') {
		line++;
		while (*line != ' ' && *line != 0) line++;
		while (*line == ' ') line++;
	}
	if (*line == ':') {
		line++;
		while (*line != ' ' && *line != 0) line++;
		while (*line == ' ') line++;
	}
	
	while (*line != 0) {
		
		*command = realloc(*command, sizeof **command * (*count + 1));
		if (*line == ':') {
			(*command)[*count] = strdup(line + 1);
			if ((*command)[*count] == NULL) {
				perror("strdup");
				exit(1);
			}
			(*count)++;
			break;
		}
		
		length = 0;
		while (line[length] != ' ' && line[length] != 0) length++;
		(*command)[*count] = strndup(line, length);
		if ((*command)[*count] == NULL) {
			perror("strndup");
			exit(1);
		}
		line += length;
		
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

static void moonfish_read_chat(struct tls *tls, char *username, char **channel, char **message)
{
	static int under_count = 0;
	
	char *line;
	char **command;
	int count;
	int i;
	
	for (;;) {
		
		line = moonfish_read_line(tls);
		if (line == NULL) {
			fprintf(stderr, "IRC connection closed unexpectedly\n");
			exit(1);
		}
		
		moonfish_parse_chat(line, &command, &count);
		free(line);
		
		if (count == 0) continue;
		
		if (!strcmp(command[0], "433")) {
			
			moonfish_free_chat(command, count);
			
			under_count++;
			
			if (under_count > 16) {
				fprintf(stderr, "tried too many nicknames\n");
				exit(1);
			}
			
			moonfish_write_text(tls, "NICK ");
			moonfish_write_text(tls, username);
			for (i = 0 ; i < under_count ; i++) moonfish_write_text(tls, "_");
			moonfish_write_text(tls, "\r\n");
			
			continue;
		}
		
		if (!strcmp(command[0], "PING")) {
			
			if (count < 2) {
				moonfish_free_chat(command, count);
				moonfish_write_text(tls, "PONG\r\n");
				continue;
			}
			
			moonfish_write_text(tls, "PONG ");
			moonfish_write_text(tls, command[1]);
			moonfish_write_text(tls, "\r\n");
			moonfish_free_chat(command, count);
			continue;
		}
		
		if (!strcmp(command[0], "PRIVMSG")) {
			
			if (count < 3) {
				fprintf(stderr, "malformed IRC message\n");
				exit(1);
			}
			
			*channel = strdup(command[1]);
			if (*channel == NULL) {
				perror("strdup");
				exit(1);
			}
			
			*message = strdup(command[2]);
			if (*message == NULL) {
				perror("strdup");
				exit(1);
			}
			
			moonfish_free_chat(command, count);
			break;
		}
		
		moonfish_free_chat(command, count);
	}
}

static void moonfish_chat(char **command, char **options, char *host, char *port, char *username, char *channels)
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
	
	moonfish_spawn(command, &in, &out, NULL);
	fprintf(in, "uci\n");
	moonfish_wait(out, "uciok");
	
	for (;;) {
		value = strchr(*options, '=');
		if (value == NULL) break;
		fprintf(in, "setoption name %.*s value %s\n", (int) (value - *options), *options, value + 1);
		options++;
	}
	
	fprintf(in, "isready\n");
	moonfish_wait(out, "readyok");
	
	fprintf(in, "ucinewgame\n");
	
	tls = moonfish_connect(host, port);
	
	/* todo: validate password */
	password = getenv("moonfish_chat_password");
	if (password != NULL && *password != 0) {
		moonfish_write_text(tls, "PASS ");
		moonfish_write_text(tls, password);
		moonfish_write_text(tls, "\r\n");
	}
	
	moonfish_write_text(tls, "USER ");
	moonfish_write_text(tls, username);
	moonfish_write_text(tls, " 0 * ");
	moonfish_write_text(tls, username);
	moonfish_write_text(tls, "\r\nNICK ");
	moonfish_write_text(tls, username);
	moonfish_write_text(tls, "\r\n");
	
	moonfish_write_text(tls, "JOIN ");
	moonfish_write_text(tls, channels);
	moonfish_write_text(tls, "\r\n");
	
	names = strdup("");
	if (names == NULL) {
		perror("strdup");
		exit(1);
	}
	
	channel = NULL;
	message = NULL;
	
	for (;;) {
		
		if (channel != NULL) free(channel);
		if (message != NULL) free(message);
		
		moonfish_read_chat(tls, username, &channel, &message);
		
		if (!strcmp(message, "!chess reset")) {
			
			moonfish_chess(&chess);
			fprintf(in, "ucinewgame\n");
			
			moonfish_write_text(tls, "PRIVMSG ");
			moonfish_write_text(tls, channel);
			moonfish_write_text(tls, " :The board has been reset.\r\n");
			
			continue;
		}
		
		if (moonfish_from_san(&chess, &move, message)) continue;
		
		moonfish_to_uci(&chess, &move, name);
		chess = move.chess;
		
		if (moonfish_finished(&chess)) {
			
			moonfish_to_fen(&chess, fen);
			moonfish_chess(&chess);
			fprintf(in, "ucinewgame\n");
			
			for (i = 0 ; fen[i] != 0 ; i++) {
				if (fen[i] == ' ') fen[i] = '_';
			}
			moonfish_write_text(tls, "PRIVMSG ");
			moonfish_write_text(tls, channel);
			moonfish_write_text(tls, " :https://lichess.org/export/fen.gif?fen=");
			moonfish_write_text(tls, fen);
			moonfish_write_text(tls, "\r\n");
			
			moonfish_write_text(tls, "PRIVMSG ");
			moonfish_write_text(tls, channel);
			moonfish_write_text(tls, " :Game over!\r\n");
			
			continue;
		}
		
		names = realloc(names, strlen(names) + strlen(name) + 2);
		if (names == NULL) {
			perror("realloc");
			exit(1);
		}
		
		strcat(names, " ");
		strcat(names, name);
		
		fprintf(in, "isready\n");
		moonfish_wait(out, "readyok");
		fprintf(in, "position startpos moves%s\n", names);
		
		fprintf(in, "isready\n");
		moonfish_wait(out, "readyok");
		fprintf(in, "go movetime 5000\n");
		
		name0 = moonfish_wait(out, "bestmove");
		names = realloc(names, strlen(names) + strlen(name0) + 2);
		if (names == NULL) {
			perror("realloc");
			exit(1);
		}
		
		strcat(names, " ");
		strcat(names, name0);
		
		if (moonfish_from_uci(&chess, &move, name0)) {
			fprintf(stderr, "malformed move '%s' by the bot\n", name0);
			exit(1);
		}
		
		moonfish_to_san(&chess, &move, name, 0);
		chess = move.chess;
		moonfish_to_fen(&chess, fen);
		
		for (i = 0 ; fen[i] != 0 ; i++) {
			if (fen[i] == ' ') fen[i] = '_';
		}
		
		moonfish_write_text(tls, "PRIVMSG ");
		moonfish_write_text(tls, channel);
		moonfish_write_text(tls, " :");
		moonfish_write_text(tls, name);
		moonfish_write_text(tls, "\r\n");
		
		moonfish_write_text(tls, "PRIVMSG ");
		moonfish_write_text(tls, channel);
		moonfish_write_text(tls, " :https://lichess.org/export/fen.gif?fen=");
		moonfish_write_text(tls, fen);
		moonfish_write_text(tls, "\r\n");
		
		if (moonfish_finished(&chess)) {
			
			moonfish_chess(&chess);
			fprintf(in, "ucinewgame\n");
			
			moonfish_write_text(tls, "PRIVMSG ");
			moonfish_write_text(tls, channel);
			moonfish_write_text(tls, " :Game over!\r\n");
		}
	}
}

int main(int argc, char **argv)
{
	static char *format = "<UCI-options> [--] <cmd> <args>...";
	static struct moonfish_arg args[] = {
		{"N", "host", "<name>", "irc.libera.chat", "network host name (default: 'irc.libera.chat')"},
		{"P", "port", "<port>", "6697", "network port (default: '6697')"},
		{"M", "nick", "<nickname>", "moonfish", "the bot's nickname (default: 'moonfish')"},
		{"C", "channel", "<channels>", "#moonfish", "channels to join (default: '#moonfish')"},
		{NULL, NULL, NULL, NULL, NULL},
	};
	
	char **options, **command;
	
	/* todo: validate nickname & channels */
	options = moonfish_args(args, format, argc, argv);
	
	command = options;
	for (;;) {
		if (*command == NULL) moonfish_usage(args, format, argv[0]);
		if (strchr(*command, '=') == NULL) break;
		command++;
	}
	
	if (!strcmp(*command, "--")) command++;
	moonfish_chat(command, options, args[0].value, args[1].value, args[2].value, args[3].value);
	return 1;
}
