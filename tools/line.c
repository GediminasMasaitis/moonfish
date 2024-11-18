/* moonfish's license: 0BSD */
/* copyright 2025 zamfofex */

/* note: declarations whose names start with 'moonline' are based on linenoise, but are heavily modified */
/* <https://github.com/antirez/linenoise> BSD 2-Clause */

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

struct moonline_state {
	struct termios termios;
	char *history[256];
	char line[2048];
	char *prompt;
	void *data;
	int completing;
	int completion_index;
	int position;
	int width;
	int history_index;
};

static void moonfish_highlight(void *data, char *line, int offset, int count);
static void moonfish_lock(void *data);
static void moonfish_unlock(void *data);

static void moonline_refresh(struct moonline_state *state)
{
	int offset;
	int count0, count1;
	
	offset = 0;
	count0 = strlen(state->prompt);
	count1 = strlen(state->line);
	
	if (count0 + count1 >= state->width) {
		offset = count0 + count1 - state->width + 1;
	}
	if (state->position - offset < (state->width - count0) / 2) {
		offset = state->position - (state->width - count0) / 2;
	}
	if (offset < 0) offset = 0;
	
	moonfish_lock(state->data);
	printf("\r%s", state->prompt);
	moonfish_highlight(state->data, state->line, offset, state->width - count0 - 1);
	printf("\x1B[0m\x1B[0K\r\x1B[%dC", count0 + state->position - offset);
	moonfish_unlock(state->data);
	fflush(stdout);
}

static void moonline_refresh_with_completions(struct moonline_state *state, char **completions)
{
	static char line[sizeof state->line];
	
	int count;
	int position;
	
	count = 0;
	while (completions[count] != NULL) count++;
	
	if (state->completion_index >= count) {
		moonline_refresh(state);
		return;
	}
	
	position = state->position;
	memcpy(line, state->line, sizeof line);
	
	state->line[0] = 0;
	strncat(state->line, completions[state->completion_index], sizeof line - 1);
	state->position = strlen(state->line);
	moonline_refresh(state);
	
	state->position = position;
	memcpy(state->line, line, sizeof line);
}

static char **moonfish_completions(void *data, char *line);

static void moonline_insert(struct moonline_state *state, char ch)
{
	int length;
	
	if (ch < 0x20) return;
	if (ch > 0x7E) return;
	
	length = strlen(state->line);
	if (length == sizeof state->line - 1) return;
	memmove(state->line + state->position + 1, state->line + state->position, length - state->position + 1);
	state->line[state->position] = ch;
	state->position++;
	moonline_refresh(state);
}

static char moonline_complete(struct moonline_state *state, char ch)
{
	char **completions;
	int count;
	
	completions = moonfish_completions(state->data, state->line);
	
	if (completions[0] == 0) {
		fprintf(stderr, "\x7");
		state->completing = 0;
		return ch;
	}
	
	count = 0;
	while (completions[count] != NULL) count++;
	
	switch (ch) {
	default:
		if (state->completion_index < count) {
			state->line[0] = 0;
			strncat(state->line, completions[state->completion_index], sizeof state->line - 1);
			state->position = strlen(state->line);
		}
		state->completing = 0;
		return ch;
		
	case 9:
		if (state->completing == 0) {
			state->completing = 1;
			state->completion_index = 0;
		}
		else {
			state->completion_index = (state->completion_index + 1) % (count + 1);
			if (state->completion_index >= count) fprintf(stderr, "\x7");
		}
		break;
		
	case 27:
		state->completing = 0;
		moonline_refresh(state);
		return ch;
	}
	
	if (state->completing && state->completion_index < count) {
		moonline_refresh_with_completions(state, completions);
		return 0;
	}
	
	moonline_refresh(state);
	return 0;
}

static void moonline_show(struct moonline_state *state)
{
	char **completions;
	
	if (state->completing) {
		completions = moonfish_completions(state->data, state->line);
		moonline_refresh_with_completions(state, completions);
	}
	else {
		moonline_refresh(state);
	}
}

static void moonline_left(struct moonline_state *state)
{
	if (state->position == 0) return;
	state->position--;
	moonline_refresh(state);
}

static void moonline_right(struct moonline_state *state)
{
	if (state->position == (int) strlen(state->line)) return;
	state->position++;
	moonline_refresh(state);
}

static void moonline_home(struct moonline_state *state)
{
	if (state->position == 0) return;
	state->position = 0;
	moonline_refresh(state);
}

static void moonline_end(struct moonline_state *state)
{
	int length;
	length = strlen(state->line);
	if (state->position == length) return;
	state->position = length;
	moonline_refresh(state);
}

static void moonline_browse(struct moonline_state *state, int delta)
{
	int count;
	
	count = 0;
	while (state->history[count] != NULL) count++;
	
	state->history_index += delta;
	if (state->history_index < 0) {
		state->history_index = 0;
		return;
	}
	if (state->history_index > count) {
		state->history_index = count;
		return;
	}
	if (state->history_index == count) {
		state->line[0] = 0;
		state->position = 0;
		moonline_refresh(state);
		return;
	}
	
	state->line[0] = 0;
	strncat(state->line, state->history[state->history_index], sizeof state->line - 1);
	state->position = strlen(state->line);
	moonline_refresh(state);
}

static void moonline_delete(struct moonline_state *state)
{
	int length;
	length = strlen(state->line);
	if (state->position == length) return;
	memmove(state->line + state->position, state->line + state->position + 1, length - state->position);
	moonline_refresh(state);
}

static void moonline_backspace(struct moonline_state *state)
{
	if (state->position == 0) return;
	memmove(state->line + state->position - 1, state->line + state->position, strlen(state->line) - state->position + 1);
	state->position--;
	moonline_refresh(state);
}

static void moonline_delete_word(struct moonline_state *state)
{
	int n;
	
	n = 0;
	while (state->position - n > 0 && state->line[state->position - n - 1] == ' ') n++;
	while (state->position - n > 0 && state->line[state->position - n - 1] != ' ') n++;
	memmove(state->line + state->position - n, state->line + state->position, strlen(state->line) - state->position + 1);
	state->position -= n;
	moonline_refresh(state);
}

static void moonline_start(struct moonline_state *state)
{
	struct termios termios;
	struct winsize ws;
	
	state->completing = 0;
	state->position = 0;
	
	if (tcgetattr(0, &termios)) {
		perror("tcgetattr");
		exit(1);
	}
	state->termios = termios;
	
	termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	termios.c_oflag &= ~OPOST;
	termios.c_cflag |= CS8;
	termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	termios.c_cc[VMIN] = 1;
	termios.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSAFLUSH, &termios)) {
		perror("tcgetattr");
		exit(1);
	}
	
	state->width = 80;
	if (!ioctl(1, TIOCGWINSZ, &ws) && ws.ws_col != 0) {
		state->width = ws.ws_col;
	}
	
	state->history_index = 0;
	while (state->history[state->history_index] != NULL) {
		state->history_index++;
	}
	
	state->line[0] = 0;
}

static char *moonline_edit(struct moonline_state *state)
{
	int ch;
	int aux;
	int ch1, ch2, ch3;
	int length;
	
	for (;;) {
		
		ch = getchar();
		if (ch == EOF) return NULL;
		
		if (state->completing || ch == 9) {
			ch = moonline_complete(state, ch);
		}
		
		switch (ch) {
		default:
			moonline_insert(state, ch);
			break;
		case 13:
			return state->line;
		case 3:
			if (strlen(state->line) == 0) return NULL;
			state->line[0] = 0;
			state->position = 0;
			moonline_refresh(state);
			break;
		case 8:
		case 127:
			moonline_backspace(state);
			break;
		case 4:
			if (strlen(state->line) == 0) return NULL;
			moonline_delete(state);
			break;
		case 20:
			length = strlen(state->line);
			if (state->position > 0 && state->position < length) {
				aux = state->line[state->position - 1];
				state->line[state->position - 1] = state->line[state->position];
				state->line[state->position] = aux;
				if (state->position != length - 1) state->position++;
				moonline_refresh(state);
			}
			break;
		case 2:
			moonline_left(state);
			break;
		case 6:
			moonline_right(state);
			break;
		case 16:
			moonline_browse(state, -1);
			break;
		case 14:
			moonline_browse(state, 1);
			break;
		case 27:
			ch1 = getchar();
			if (ch1 == EOF) break;
			ch2 = getchar();
			if (ch2 == EOF) break;
			
			if (ch1 == '[') {
				
				if (ch2 >= '0' && ch2 <= '9') {
					ch3 = getchar();
					if (ch3 == EOF) break;
					if (ch3 == '~' && ch2 == '3') {
						moonline_delete(state);
					}
					break;
				}
				
				switch (ch2) {
				case 'A':
					moonline_browse(state, -1);
					break;
				case 'B':
					moonline_browse(state, 1);
					break;
				case 'C':
					moonline_right(state);
					break;
				case 'D':
					moonline_left(state);
					break;
				case 'H':
					moonline_home(state);
					break;
				case 'F':
					moonline_end(state);
					break;
				}
				
				break;
			}
			
			if (ch1 == 'O') {
				switch (ch2) {
				case 'H':
					moonline_home(state);
					break;
				case 'F':
					moonline_end(state);
					break;
				}
				break;
			}
			
			break;
		case 21:
			state->line[0] = 0;
			state->position = 0;
			moonline_refresh(state);
			break;
		case 11:
			state->line[state->position] = 0;
			moonline_refresh(state);
			break;
		case 1:
			moonline_home(state);
			break;
		case 5:
			moonline_end(state);
			break;
		case 12:
			printf("\x1B[H\x1B[2J");
			moonline_refresh(state);
			break;
		case 23:
			moonline_delete_word(state);
			break;
		}
	}
}

static void moonline_add_history(struct moonline_state *state, char *line0)
{
	char *line;
	int count;
	int max;
	
	count = 0;
	while (state->history[count] != NULL) count++;
	
	if (count != 0 && !strcmp(state->history[count - 1], line0)) return;
	
	line = strdup(line0);
	if (line == NULL) {
		perror("strdup");
		exit(1);
	}
	
	max = sizeof state->history / sizeof *state->history - 1;
	if (count == max) {
		free(state->history[0]);
		memmove(state->history, state->history + 1, sizeof *state->history * (max - 1));
		count--;
	}
	
	state->history[count] = line;
	state->history[count + 1] = NULL;
}

static void moonline_save(struct moonline_state *state, FILE *file)
{
	int i;
	for (i = 0 ; state->history[i] != NULL ; i++) {
		fprintf(file, "%s\n", state->history[i]);
	}
}

static void moonline_load(struct moonline_state *state, FILE *file)
{
	static char line[sizeof state->line];
	char *last;
	
	while (fgets(line, sizeof line, file) != NULL) {
		last = strchr(line, '\r');
		if (last == NULL) last = strchr(line, '\n');
		if (last != NULL) *last = 0;
		moonline_add_history(state, line);
	}
}

static char *moonline_line(struct moonline_state *state, char *prompt)
{
	char *line;
	
	state->prompt = prompt;
	moonline_start(state);
	moonline_refresh(state);
	line = moonline_edit(state);
	printf("\r");
	
	if (tcsetattr(0, TCSAFLUSH, &state->termios)) {
		perror("tcsetattr");
		exit(1);
	}
	
	return line;
}

/* - - - - - */

#include <sys/stat.h>
#include <strings.h>
#include <errno.h>
#include <pthread.h>

#include "../moonfish.h"
#include "tools.h"

struct moonfish_highlight {
	int offset, count;
	char *color;
};

enum {
	moonfish_type_none,
	moonfish_type_check,
	moonfish_type_spin,
	moonfish_type_combo,
	moonfish_type_button,
	moonfish_type_string
};

struct moonfish_option {
	char name[64];
	char initial[2048];
	char value[2048];
	unsigned char type;
	char vars[16][64];
	int min, max;
};

struct moonfish_state {
	char **commands;
	struct moonfish_option options[256];
	char fen[256];
	char moves[256][6];
	FILE *in, *out;
	char name[64], author[64];
	struct moonline_state line_state;
	pthread_mutex_t mutex;
	char *session;
	int san;
	_Atomic int searching;
	_Atomic int quit;
};

struct moonfish_uci_command {
	char *name;
	void (*fn)(struct moonfish_state *state, char *line);
};

static void moonfish_out(struct moonfish_state *state)
{
	moonfish_lock(state);
	printf("\r\x1B[38;5;244m<-\x1B[0m ");
}

static void moonfish_error(struct moonfish_state *state)
{
	moonfish_lock(state);
	printf("\r\x1B[38;5;203m<-\x1B[0m ");
}

static void moonfish_to_name(struct moonfish_chess *chess, struct moonfish_move *move, char *name, struct moonfish_state *state)
{
	if (state->san) moonfish_to_san(chess, move, name, 0);
	else moonfish_to_uci(chess, move, name);
}

static void moonfish_from_name(struct moonfish_chess *chess, struct moonfish_move *move, char *name)
{
	if (moonfish_from_uci(chess, move, name)) strcpy(name, "0000");
}

static void moonfish_rename(struct moonfish_chess *chess, struct moonfish_move *move, char *name, struct moonfish_state *state)
{
	moonfish_from_name(chess, move, name);
	if (strcmp(name, "0000")) moonfish_to_name(chess, move, name, state);
}

static void moonfish_uci_chess(struct moonfish_state *state, struct moonfish_chess *chess)
{
	static struct moonfish_move move;
	int i;
	
	moonfish_chess(chess);
	if (state->fen[0] != 0) moonfish_from_fen(chess, state->fen);
	for (i = 0 ; state->moves[i][0] != 0 ; i++) {
		moonfish_from_san(chess, &move, state->moves[i]);
		moonfish_play(chess, &move);
	}
}

static void moonfish_highlight_part(struct moonfish_highlight *highlight, char **line, int count, char *color)
{
	if (count <= highlight->offset) {
		highlight->offset -= count;
		*line += count;
		return;
	}
	
	*line += highlight->offset;
	count -= highlight->offset;
	highlight->offset = 0;
	
	if (count == 0) return;
	
	if (count > highlight->count) {
		if (highlight->count == 0) {
			*line += count;
			return;
		}
		count = highlight->count;
	}
	highlight->count -= count;
	
	if (color == NULL || !strcmp(color, highlight->color)) {
		printf("%.*s", count, *line);
	}
	else {
		printf("\x1B[%sm%.*s", color, count, *line);
		highlight->color = color;
	}
	
	*line += count;
}

static void moonfish_highlight(void *data, char *line, int offset, int count)
{
	static struct moonfish_highlight highlight;
	static struct moonfish_move move;
	static struct moonfish_chess chess;
	static char name[16];
	
	struct moonfish_state *state;
	int i, j;
	int n;
	int notation;
	
	state = data;
	
	moonfish_uci_chess(state, &chess);
	highlight.offset = offset;
	highlight.count = count;
	highlight.color = "0";
	
	moonfish_highlight_part(&highlight, &line, strspn(line, " "), NULL);
	
	n = strcspn(line, " ");
	for (i = 0 ; state->commands[i] != NULL ; i++) {
		if (!strncmp(state->commands[i], line, n) && state->commands[i][n] == 0) {
			break;
		}
		while (state->commands[i] != NULL) i++;
	}
	
	if (state->commands[i] == NULL) {
		moonfish_highlight_part(&highlight, &line, strlen(line), "38;5;203");
		return;
	}
	
	moonfish_highlight_part(&highlight, &line, n, "38;5;141");
	moonfish_highlight_part(&highlight, &line, strspn(line, " "), NULL);
	
	if (!strcmp(state->commands[i], "quote") || !strcmp(state->commands[i], "play")) {
		moonfish_highlight_part(&highlight, &line, strlen(line), "38;5;111");
		return;
	}
	
	i++;
	
	n = strcspn(line, " ");
	for (j = i ; state->commands[j] != NULL ; j++) {
		if (!strncmp(state->commands[j], line, n) && state->commands[j][n] == 0) {
			break;
		}
	}
	
	if (state->commands[j] == NULL) {
		if (state->commands[i] != NULL && !strcmp(state->commands[i], "name")) {
			i++;
		}
		else {
			if (strcmp(state->commands[i - 1], "bestmove")) {
				moonfish_highlight_part(&highlight, &line, strlen(line), "38;5;203");
				return;
			}
			if (state->san) {
				name[0] = 0;
				strncat(name, line, n);
				moonfish_rename(&chess, &move, name, state);
				highlight.count -= n;
				line += n;
				printf("\x1B[38;5;111m%s", name);
				highlight.color = "38;5;111";
			}
		}
	}
	
	for (;;) {
		
		notation = 0;
		
		if (state->commands[j] != NULL) {
			
			if (!strcmp(state->commands[j], "startpos")) {
				moonfish_highlight_part(&highlight, &line, n, "38;5;111");
				moonfish_highlight_part(&highlight, &line, strspn(line, " "), NULL);
				n = strcspn(line, " ");
				i = ++j + 2;
				if (n < 5 || strncmp(line, "moves", n)) {
					moonfish_highlight_part(&highlight, &line, n, "38;5;203");
					return;
				}
				moonfish_highlight_part(&highlight, &line, n, "38;5;213");
				moonfish_highlight_part(&highlight, &line, strlen(line), "38;5;111");
				return;
			}
			
			if (!strcmp(state->commands[j], "name") || !strcmp(state->commands[j], "value") || !strcmp(state->commands[j], "default") || !strcmp(state->commands[j], "fen") || !strcmp(state->commands[j], "moves")) {
				i = j + 1;
			}
			
			if (!strcmp(state->commands[j], "on") || !strcmp(state->commands[j], "off")) {
				moonfish_highlight_part(&highlight, &line, n, "38;5;111");
				moonfish_highlight_part(&highlight, &line, strlen(line), "38;5;203");
				return;
			}
			
			moonfish_highlight_part(&highlight, &line, n, "38;5;213");
			
			if (!strcmp(state->commands[j], "string")) {
				moonfish_highlight_part(&highlight, &line, strlen(line), "38;5;111");
				return;
			}
			
			if (state->san && (!strcmp(state->commands[j], "pv") || !strcmp(state->commands[j], "refutation") || !strcmp(state->commands[j], "currline"))) {
				if (!strcmp(state->commands[j], "currline")) notation = 2;
				else notation = 1;
			}
		}
		
		for (;;) {
			
			moonfish_highlight_part(&highlight, &line, strspn(line, " "), NULL);
			n = strcspn(line, " ");
			if (n == 0) return;
			
			for (j = i ; state->commands[j] != NULL ; j++) {
				if (!strcmp(state->commands[j], "value") && !strncmp(line, "=", n)) {
					break;
				}
				if (!strncmp(state->commands[j], line, n) && state->commands[j][n] == 0) {
					break;
				}
			}
			
			if (state->commands[j] != NULL) break;
			
			if (!notation) {
				moonfish_highlight_part(&highlight, &line, n, "38;5;111");
				continue;
			}
			
			if (notation == 2 && line[0] >= '0' && line[0] <= '9') {
				notation = 1;
				moonfish_highlight_part(&highlight, &line, n, "38;5;111");
				continue;
			}
			
			if (n >= (int) sizeof line) {
				notation = -1;
				continue;
			}
			
			if (notation == -1) {
				moonfish_highlight_part(&highlight, &line, n, "38;5;203");
				continue;
			}
			
			notation = 1;
			name[0] = 0;
			strncat(name, line, n);
			moonfish_rename(&chess, &move, name, state);
			if (strcmp(name, "0000")) moonfish_play(&chess, &move);
			highlight.count -= n;
			line += n;
			printf("\x1B[38;5;111m%s", name);
			highlight.color = "38;5;111";
		}
	}
}

static void moonfish_lock(void *data)
{
	struct moonfish_state *state;
	int error;
	
	state = data;
	error = pthread_mutex_lock(&state->mutex);
	if (error != 0) {
		fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(error));
		exit(1);
	}
}

static void moonfish_unlock(void *data)
{
	struct moonfish_state *state;
	int error;
	
	state = data;
	error = pthread_mutex_unlock(&state->mutex);
	if (error != 0) {
		fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(error));
		exit(1);
	}
}

static char *moonfish_word(char *line, char *word)
{
	char *result;
	int n;
	
	n = strlen(word);
	for (;;) {
		result = strstr(line, word);
		if (result == NULL) return line + strlen(line);
		if ((result == line || result[-1] == ' ') && (result[n] == 0 || result[n] == ' ')) {
			return result;
		}
		line = result + n;
	}
}

static char *moonfish_option_name(char *line)
{
	static char name[64];
	
	char *next, *next1;
	int max;
	
	if (line[0] != 0 && moonfish_word(line, "name") == line) {
		line += 4;
		line += strspn(line, " ");
	}
	
	max = sizeof name - 1;
	next = moonfish_word(line, "value");
	next1 = moonfish_word(line, "=");
	if (next > next1) next = next1;
	if (next == line) return "";
	if (*next != 0) next--;
	if (max > next - line) max = next - line;
	name[0] = 0;
	strncat(name, line, max);
	return name;
}

static char *moonfish_option_value(char *line)
{
	char *next, *next1;
	
	next = moonfish_word(line, "value");
	next1 = moonfish_word(line, "=");
	if (next > next1) next = next1;
	line = next;
	line += strcspn(line, " ");
	line += strspn(line, " ");
	if (line[0] == 0) return "<empty>";
	return line;
}

static int moonfish_option_index(struct moonfish_state *state, char *name)
{
	int i;
	
	i = 0;
	for (;;) {
		if (state->options[i].type == moonfish_type_none) return -1;
		if (!strcasecmp(name, state->options[i].name)) {
			return i;
		}
		i++;
	}
}

static void moonfish_setoption(struct moonfish_state *state, char *line)
{
	int i, j;
	char *name, *value;
	int n;
	
	name = moonfish_option_name(line);
	
	if (name[0] == 0) {
		moonfish_error(state);
		printf("variable name cannot be empty\n");
		moonfish_unlock(state);
		return;
	}
	
	if (moonfish_word(name, "name")[0] != 0) {
		moonfish_error(state);
		printf("variable name cannot contain token 'name'\n");
		moonfish_unlock(state);
		return;
	}
	
	i = moonfish_option_index(state, name);
	
	if (i < 0) {
		moonfish_error(state);
		printf("unknown variable '%s'\n", name);
		moonfish_unlock(state);
		return;
	}
	
	value = moonfish_option_value(line);
	
	if (state->options[i].type == moonfish_type_combo) {
		for (j = 0 ; state->options[i].vars[j][0] != 0 ; j++) {
			if (!strcmp(state->options[i].vars[j], value)) break;
		}
		if (state->options[i].vars[j][0] == 0) {
			moonfish_error(state);
			printf("value not allowed for this 'combo' option\n");
			moonfish_unlock(state);
			return;
		}
	}
	
	if (state->options[i].type == moonfish_type_spin) {
		if (moonfish_int(value, &n)) {
			moonfish_error(state);
			printf("value '%s' is not a number\n", value);
			moonfish_unlock(state);
			return;
		}
		sprintf(value, "%d", n);
		if (n < state->options[i].min || n > state->options[i].max) {
			moonfish_error(state);
			printf("value '%s' is out of bounds\n", value);
			moonfish_unlock(state);
			return;
		}
	}
	
	if (state->options[i].type == moonfish_type_check) {
		if (strcmp(value, "true") && strcmp(value, "false")) {
			moonfish_error(state);
			printf("value '%s' not allowed (must be either 'true' or 'false')\n", value);
			moonfish_unlock(state);
			return;
		}
	}
	
	if (state->options[i].type == moonfish_type_button) {
		if (strcmp(value, "<empty>")) {
			moonfish_error(state);
			printf("giving a value is not allowed for type 'button'\n");
			moonfish_unlock(state);
			return;
		}
	}
	
	state->options[i].value[0] = 0;
	strncat(state->options[i].value, value, sizeof state->options[i].value / sizeof *state->options[i].value - 1);
	
	if (state->options[i].type == moonfish_type_button) {
		fprintf(state->in, "setoption name %s\n", name);
	}
	if (!strcmp(value, "<empty>")) {
		fprintf(state->in, "setoption name %s value\n", name);
	}
	else {
		fprintf(state->in, "setoption name %s value %s\n", name, value);
	}
}

static void moonfish_show_option(struct moonfish_state *state, struct moonfish_option *option)
{
	int j;
	
	moonfish_out(state);
	printf("\x1B[38;5;141moption \x1B[38;5;213mname \x1B[38;5;111m%s \x1B[38;5;213mtype \x1B[38;5;111m", option->name);
	if (option->type == moonfish_type_check) printf("check");
	if (option->type == moonfish_type_spin) printf("spin");
	if (option->type == moonfish_type_combo) printf("combo");
	if (option->type == moonfish_type_button) printf("button");
	if (option->type == moonfish_type_string) printf("string");
	if (option->type != moonfish_type_button) {
		printf(" \x1B[38;5;213mdefault \x1B[38;5;111m%s", option->value);
	}
	if (option->type == moonfish_type_spin) {
		printf(" \x1B[38;5;213mmin \x1B[38;5;111m%d", option->min);
		printf(" \x1B[38;5;213mmax \x1B[38;5;111m%d", option->max);
	}
	if (option->type == moonfish_type_combo) {
		for (j = 0 ; option->vars[j][0] != 0 ; j++) {
			printf(" \x1B[38;5;213mvar \x1B[38;5;111m%s", option->vars[j]);
		}
	}
	printf("\x1B[0m\n");
	moonfish_unlock(state);
}

static void moonfish_getoption(struct moonfish_state *state, char *line)
{
	int i;
	
	if (line[0] != 0 && moonfish_word(line, "name") == line) {
		line += 4;
		line += strspn(line, " ");
	}
	
	if (line[0] == 0) {
		moonfish_error(state);
		printf("missing variable name\n");
		moonfish_unlock(state);
		return;
	}
	
	i = moonfish_option_index(state, line);
	
	if (i < 0) {
		moonfish_error(state);
		printf("unset variable '%s'\n", line);
		moonfish_unlock(state);
		return;
	}
	
	moonfish_show_option(state, state->options + i);
}

static int moonfish_no_parameters(struct moonfish_state *state, char *line)
{
	if (line[0] != 0) {
		moonfish_error(state);
		printf("this command does not accept parameters\n");
		moonfish_unlock(state);
		return 1;
	}
	
	return 0;
}

static void moonfish_show(struct moonfish_state *state, char *line)
{
	static struct moonfish_chess chess;
	static struct moonfish_move move;
	static char name[16];
	
	int i;
	
	if (moonfish_no_parameters(state, line)) return;
	
	moonfish_out(state);
	
	printf("\x1B[38;5;141mposition ");
	
	if (state->fen[0] == 0) printf("\x1B[38;5;213mstartpos");
	else printf("\x1B[38;5;213mfen \x1B[38;5;111m%s", state->fen);
	
	if (state->moves[0][0] == 0) {
		printf("\x1B[0m\n");
		moonfish_unlock(state);
		return;
	}
	
	printf(" \x1B[38;5;213mmoves\x1B[38;5;111m");
	
	moonfish_chess(&chess);
	if (state->fen[0] != 0) moonfish_from_fen(&chess, state->fen);
	
	for (i = 0 ; state->moves[i][0] != 0 ; i++) {
		moonfish_from_san(&chess, &move, state->moves[i]);
		moonfish_to_name(&chess, &move, name, state);
		printf(" %s", name);
		moonfish_play(&chess, &move);
	}
	
	printf("\x1B[0m\n");
	moonfish_unlock(state);
}

static void moonfish_uci_position(struct moonfish_state *state)
{
	int i;
	
	if (state->fen[0] == 0) fprintf(state->in, "position startpos");
	else fprintf(state->in, "position fen %s", state->fen);
	
	if (state->moves[0][0] == 0) {
		fprintf(state->in, "\n");
		return;
	}
	
	fprintf(state->in, " moves");
	for (i = 0 ; state->moves[i][0] != 0 ; i++) fprintf(state->in, " %s", state->moves[i]);
	fprintf(state->in, "\n");
}

static void moonfish_uci_play(struct moonfish_state *state, char *line)
{
	static struct moonfish_move move;
	static struct moonfish_chess chess;
	
	int i;
	int n;
	
	moonfish_uci_chess(state, &chess);
	
	if (line[0] == 0) {
		moonfish_error(state);
		printf("missing moves\n");
		moonfish_unlock(state);
		return;
	}
	
	i = 0;
	while (state->moves[i][0] != 0) i++;
	
	for (;;) {
		
		if (line[0] == 0) break;
		n = strcspn(line, " ");
		if (line[n] != 0) line[n++] = 0;
		
		if (!strcmp(line, "..")) {
			if (i > 0) i--;
			state->moves[i][0] = 0;
			line += n;
			continue;
		}
		
		if (!strcmp(line, "...")) {
			i = 0;
			state->moves[0][0] = 0;
			line += n;
			continue;
		}
		
		if (moonfish_from_san(&chess, &move, line)) {
			moonfish_error(state);
			printf("malformed move '%s'\n", line);
			moonfish_unlock(state);
			break;
		}
		
		moonfish_to_uci(&chess, &move, state->moves[i++]);
		moonfish_play(&chess, &move);
		line += n;
	}
	
	moonfish_uci_position(state);
}

static void moonfish_clear(struct moonfish_state *state, char *line)
{
	(void) state;
	if (moonfish_no_parameters(state, line)) return;
	printf("\x1B[H\x1B[2J");
}

static char *moonfish_next_word(char **line)
{
	char *word;
	
	word = *line;
	*line += strcspn(*line, " ");
	if ((*line)[0] != 0) {
		(*line)[0] = 0;
		(*line)++;
	}
	
	return word;
}

static int moonfish_uci(struct moonfish_state *state, struct moonfish_uci_command *commands, char *line)
{
	int i;
	char *command;
	
	command = moonfish_next_word(&line);
	
	for (i = 0 ; commands[i].name != NULL ; i++) {
		if (!strcmp(commands[i].name, command)) {
			(*commands[i].fn)(state, line);
			return 0;
		}
	}
	
	return 1;
}

static void moonfish_normalise(char *line)
{
	int i, j;
	
	i = 0;
	j = 0;
	
	for (;;) {
		if (i != 0 && line[j] != 0 && strchr("\r\n\t ", line[j]) != NULL) {
			line[i++] = ' ';
		}
		j += strspn(line + j, "\r\n\t ");
		line[i] = line[j];
		if (line[j] == 0) break;
		i++;
		j++;
	}
	if (line[i - 1] == ' ') line[i - 1] = 0;
}

static char **moonfish_completions(void *data, char *line)
{
	static char normalised[2048];
	static char completions[256][2048];
	static char *completions1[256];
	static char partial[2048];
	static char fen[2048];
	static struct moonfish_chess chess, other;
	static struct moonfish_move move, moves[32];
	
	int i, j, k;
	struct moonfish_state *state;
	int n;
	char *command, *arg;
	int x, y;
	char name[16], name2[16];
	int count;
	
	state = data;
	moonfish_uci_chess(state, &chess);
	
	i = 0;
	for (i = 0 ; i < (int) (sizeof completions1 / sizeof *completions1) ; i++) {
		completions1[i] = completions[i];
	}
	
	strncpy(normalised, line, sizeof normalised - 1);
	normalised[sizeof normalised - 1] = 0;
	moonfish_normalise(normalised);
	if (line[strlen(line) - 1] == ' ') strcat(normalised, " ");
	
	i = 0;
	if (strcmp(normalised, line)) strcpy(completions[i++], normalised);
	
	line = normalised;
	
	n = strlen(line);
	for (j = 0 ; state->commands[j] != NULL ; j++) {
		if (!strncasecmp(line, state->commands[j], n) && strcmp(line, state->commands[j])) {
			strcpy(completions[i++], state->commands[j]);
		}
		while (state->commands[j] != NULL) j++;
	}
	
	command = moonfish_next_word(&line);
	for (j = 0 ; state->commands[j] != NULL ; j++) {
		if (!strcmp(command, state->commands[j])) break;
		while (state->commands[j] != NULL) j++;
	}
	
	if (state->commands[j] == NULL) {
		completions1[i] = NULL;
		return completions1;
	}
	
	strcpy(partial, command);
	strcat(partial, " ");
	
	arg = "";
	
	if (!strcmp(command, "position")) {
		
		for (;;) {
			arg = moonfish_next_word(&line);
			if (!strcmp(arg, "moves")) {
				strcat(partial, "moves ");
				break;
			}
			if (line[0] == 0) {
				if (line[-1] != 0 || arg[0] == 0) break;
			}
			if (arg[0] != 0) {
				strcat(partial, arg);
				strcat(partial, " ");
			}
			if (!strcmp(arg, "startpos")) {
				moonfish_chess(&chess);
				while (state->commands[j + 2]) j++;
				continue;
			}
			if (!strcmp(arg, "fen")) {
				arg = moonfish_word(line, "moves");
				fen[0] = 0;
				strncat(fen, line, arg - line);
				if (fen[0] == 0 || moonfish_from_fen(&chess, fen)) moonfish_chess(&chess);
				n = strlen(partial);
				moonfish_to_fen(&chess, partial + n);
				if (strcmp(partial + n, fen) && arg == line) strcpy(completions1[i++], partial);
				strcat(partial, " ");
				while (state->commands[j + 2]) j++;
				line = arg;
			}
		}
	}
	
	if (!strcmp(command, "play") || !strcmp(arg, "moves")) {
		
		arg = "";
		for (;;) {
			arg = moonfish_next_word(&line);
			if (line[0] == 0) {
				if (line[-1] != 0 || arg[0] == 0) break;
			}
			if (moonfish_from_san(&chess, &move, arg)) {
				completions1[i] = NULL;
				return completions1;
			}
			if (arg[0] != 0) {
				moonfish_to_name(&chess, &move, partial + strlen(partial), state);
				moonfish_play(&chess, &move);
				strcat(partial, " ");
			}
		}
		
		n = strlen(arg);
		
		for (y = 0 ; y < 8 ; y++) {
			for (x = 0 ; x < 8 ; x++) {
				count = moonfish_moves(&chess, moves, (x + 1) + (y + 2) * 10);
				for (k = 0 ; k < count ; k++) {
					other = chess;
					moonfish_play(&other, moves + k);
					if (!moonfish_validate(&other)) continue;
					moonfish_to_name(&chess, moves + k, name, state);
					moonfish_to_uci(&chess, moves + k, name2);
					if (!strncmp(arg, name, n) || !strncmp(arg, name2, n)) {
						strcpy(completions[i], partial);
						strcat(completions[i], name);
						i++;
					}
				}
			}
		}
		
		completions1[i] = NULL;
		return completions1;
	}
	
	if (!strcmp(command, "set") || !strcmp(command, "get") || !strcmp(command, "setoption") || !strcmp(command, "getoption")) {
		
		if (command[0] == 'g') strcpy(partial, "get ");
		if (command[0] == 's') strcpy(partial, "set ");
		
		if (line[0] != 0 && moonfish_word(line, "name") == line) {
			moonfish_next_word(&line);
		}
		
		n = strlen(line);
		
		for (k = 0 ; state->options[k].name[0] != 0 ; k++) {
			if (!strncasecmp(line, state->options[k].name, n) && strcmp(line, state->options[k].name)) {
				strcpy(completions[i], partial);
				strcat(completions[i], state->options[k].name);
				if (command[0] == 's') strcat(completions[i], " =");
				i++;
			}
		}
		
		completions1[i] = NULL;
		return completions1;
	}
	
	if (arg[0] == 0) {
		for (;;) {
			arg = moonfish_next_word(&line);
			if (line[0] == 0) {
				if (line[-1] != 0 || arg[0] == 0) break;
			}
			if (arg[0] != 0) {
				strcat(partial, arg);
				strcat(partial, " ");
			}
		}
	}
	
	n = strlen(arg);
	for (k = j + 1 ; state->commands[k] != NULL ; k++) {
		if (!strncasecmp(arg, state->commands[k], n) && strcmp(arg, state->commands[k])) {
			strcpy(completions[i], partial);
			strcat(completions[i], state->commands[k]);
			i++;
		}
	}
	
	completions1[i] = NULL;
	return completions1;
}

static void moonfish_option(struct moonfish_state *state, char *line)
{
	char *name, *type, *value, *min, *max, *vars[63];
	char *arg;
	int i, j;
	char **which;
	int count;
	int var;
	int type1;
	int improper;
	int n;
	
	if (line[0] == 0) {
		fprintf(stderr, "missing option parameters\n");
		exit(1);
	}
	
	count = sizeof vars / sizeof *vars;
	for (j = 0 ; j < count ; j++) vars[j] = NULL;
	
	name = NULL;
	type = NULL;
	value = NULL;
	min = NULL;
	max = NULL;
	var = 0;
	
	for (;;) {
		if (line[0] == 0) break;
		arg = moonfish_next_word(&line);
		which = NULL;
		if (!strcmp(arg, "name")) which = &name;
		if (!strcmp(arg, "type")) which = &type;
		if (!strcmp(arg, "min")) which = &min;
		if (!strcmp(arg, "max")) which = &max;
		if (!strcmp(arg, "default")) which = &value;
		if (!strcmp(arg, "var")) {
			if (var >= count) continue;
			which = vars + var++;
		}
		if (which == NULL) {
			arg[-1] = ' ';
			continue;
		}
		if (*which != NULL) {
			fprintf(stderr, "repeated option parameter\n");
			exit(1);
		}
		*which = line;
	}
	
	if (name == NULL) {
		fprintf(stderr, "missing option name\n");
		exit(1);
	}
	
	i = 0;
	while (state->options[i].type != moonfish_type_none) i++;
	if (i >= (int) (sizeof state->options / sizeof *state->options - 1)) return;
	
	strcpy(state->options[i].name, name);
	
	if (type == NULL) {
		fprintf(stderr, "missing option type\n");
		exit(1);
	}
	
	type1 = -1;
	if (!strcmp(type, "check")) type1 = moonfish_type_check;
	if (!strcmp(type, "spin")) type1 = moonfish_type_spin;
	if (!strcmp(type, "combo")) type1 = moonfish_type_combo;
	if (!strcmp(type, "button")) type1 = moonfish_type_button;
	if (!strcmp(type, "string")) type1 = moonfish_type_string;
	if (type1 < 0) {
		fprintf(stderr, "unknown option type '%s'\n", type);
		exit(1);
	}
	
	state->options[i].type = type1;
	
	improper = 0;
	if (type1 == moonfish_type_check) {
		if (value == NULL) improper = 1;
		if (min != NULL) improper = 1;
		if (max != NULL) improper = 1;
		if (var != 0) improper = 1;
	}
	if (type1 == moonfish_type_spin) {
		if (value == NULL) improper = 1;
		if (min == NULL) improper = 1;
		if (max == NULL) improper = 1;
		if (var != 0) improper = 1;
	}
	if (type1 == moonfish_type_combo) {
		if (value == NULL) improper = 1;
		if (min != NULL) improper = 1;
		if (max != NULL) improper = 1;
		if (var == 0) improper = 1;
	}
	if (type1 == moonfish_type_button) {
		if (value != NULL) improper = 1;
		if (min != NULL) improper = 1;
		if (max != NULL) improper = 1;
		if (var != 0) improper = 1;
	}
	if (type1 == moonfish_type_string) {
		if (value == NULL) improper = 1;
		if (min != NULL) improper = 1;
		if (max != NULL) improper = 1;
		if (var != 0) improper = 1;
	}
	
	if (improper) {
		fprintf(stderr, "improper parameter combination for option type '%s'\n", type);
		exit(1);
	}
	
	if (value == NULL || value[0] == 0) value = "<empty>";
	strcpy(state->options[i].initial, value);
	strcpy(state->options[i].value, value);
	
	if (min != NULL && moonfish_int(min, &state->options[i].min)) {
		fprintf(stderr, "could not parse option min\n");
		exit(1);
	}
	
	if (max != NULL && moonfish_int(max, &state->options[i].max)) {
		fprintf(stderr, "could not parse option min\n");
		exit(1);
	}
	
	for (j = 0 ; j < var ; j++) {
		if (vars[j][0] == 0) {
			fprintf(stderr, "empty var\n");
			exit(1);
		}
		strcpy(state->options[i].vars[j], vars[j]);
	}
	state->options[i].vars[j][0] = 0;
	
	if (type1 == moonfish_type_combo) {
		for (j = 0 ; j < var ; j++) {
			if (!strcmp(vars[j], value)) break;
		}
		if (j == var) {
			fprintf(stderr, "default not in var\n");
			exit(1);
		}
	}
	
	if (type1 == moonfish_type_spin) {
		if (moonfish_int(value, &n)) {
			fprintf(stderr, "non-integer of type 'spin'\n");
			exit(1);
		}
		sprintf(state->options[i].value, "%d", n);
		if (n < state->options[i].min || n > state->options[i].max) {
			fprintf(stderr, "default out of bounds\n");
			exit(1);
		}
	}
	
	if (type1 == moonfish_type_check) {
		if (strcmp(value, "true") && strcmp(value, "false")) {
			fprintf(stderr, "improper 'check' default\n");
			exit(1);
		}
	}
}

static void moonfish_id(struct moonfish_state *state, char *line)
{
	char *which;
	
	which = moonfish_next_word(&line);
	if (!strcmp(which, "name")) {
		state->name[0] = 0;
		strncat(state->name, line, sizeof state->name - 1);
		return;
	}
	if (!strcmp(which, "author")) {
		state->author[0] = 0;
		strncat(state->author, line, sizeof state->author - 1);
		return;
	}
}

static void moonfish_initialise(struct moonfish_state *state)
{
	static struct moonfish_uci_command commands[] = {
		{"option", &moonfish_option},
		{"id", &moonfish_id},
		{NULL, NULL}
	};
	static char line[2048];
	
	fprintf(state->in, "uci\n");
	
	for (;;) {
		if (fgets(line, sizeof line, state->out) == NULL) {
			fprintf(stderr, "could not initialise bot\n");
			exit(1);
		}
		moonfish_normalise(line);
		moonfish_uci(state, commands, line);
		if (!strcmp(line, "uciok")) break;
	}
	
	fprintf(state->in, "ucinewgame\n");
	fprintf(state->in, "isready\n");
	moonfish_wait(state->out, "readyok");
}

static void moonfish_show_all(struct moonfish_state *state, char *line)
{
	int i;
	
	if (moonfish_no_parameters(state, line)) return;
	
	if (state->name[0] != 0) {
		moonfish_out(state);
		printf("\x1B[38;5;141mid \x1B[38;5;213mname \x1B[38;5;111m%s\n", state->name);
		moonfish_unlock(state);
	}
	if (state->author[0] != 0) {
		moonfish_out(state);
		printf("\x1B[38;5;141mid \x1B[38;5;213mauthor \x1B[38;5;111m%s\n", state->author);
		moonfish_unlock(state);
	}
	
	for (i = 0 ; state->options[i].type != moonfish_type_none ; i++) {
		moonfish_show_option(state, state->options + i);
	}
	
	moonfish_out(state);
	printf("\x1B[38;5;141muciok\x1B[0m\n");
	moonfish_unlock(state);
}

static void moonfish_position(struct moonfish_state *state, char *line)
{
	static struct moonfish_chess chess;
	
	char *arg;
	
	moonfish_uci_chess(state, &chess);
	
	if (line[0] == 0) {
		moonfish_error(state);
		printf("missing parameters\n");
		moonfish_unlock(state);
		return;
	}
	
	arg = moonfish_next_word(&line);
	if (!strcmp(arg, "fen")) {
		arg = moonfish_word(line, "moves");
		if (arg[0] != 0) arg[-1] = 0;
		if (moonfish_from_fen(&chess, line)) {
			moonfish_error(state);
			printf("malformed FEN\n");
			moonfish_unlock(state);
			return;
		}
		line = arg;
		moonfish_to_fen(&chess, state->fen);
		arg = moonfish_next_word(&line);
	}
	else {
		if (!strcmp(arg, "startpos")) {
			moonfish_chess(&chess);
			arg = moonfish_next_word(&line);
		}
	}
	
	if (strcmp(arg, "moves")) {
		moonfish_error(state);
		printf("unexpected '%s' token in position\n", arg);
		moonfish_unlock(state);
		moonfish_uci_position(state);
		return;
	}
	
	moonfish_uci_play(state, line);
}

static void moonfish_help(struct moonfish_state *state, char *line)
{
	static char *messages[][2] = {
		{"uci", "displays all options and their values"},
		{"debug on", "enables debugging information from the bot"},
		{"debug off", "disables debugging information"},
		{"isready", "prints 'readyok'"},
		{"setoption name <name> value <value>", "sets an option to a given value"},
		{"setoption <name> = <value>", "same as above"},
		{"set <name> = <value>", "same as above"},
		{"getoption name <name>", "gets the current value of an option"},
		{"getoption <name>", "same as above"},
		{"get <name>", "same as above"},
		{"ucinewgame", "tells the bot that a new game has started"},
		{"position (startpos | fen <FEN>) [moves <moves>...]", "sets the position"},
		{"position moves <moves>...", "plays given moves from the current position"},
		{"play <moves>...", "same as above"},
		{"show", "shows the current position"},
		{"go ...", "tells the bot to evaluate the current position"},
		{"stop", "tells the bot to stop evaluating"},
		{"quit", "(self explanatory)"},
		{"quote <cmd>", "sends a raw command to the bot"},
		{"clear", "clears the screen (hint: use Ctrl-L)"},
		{"help", "displays this message"},
		{"", ""},
	};
	
	int i;
	
	(void) state;
	
	if (moonfish_no_parameters(state, line)) return;
	
	for (i = 0 ; messages[i][0][0] != 0 ; i++) {
		moonfish_out(state);
		printf("\x1B[38;5;141m%s\x1B[0m - %s\n", messages[i][0], messages[i][1]);
		moonfish_unlock(state);
	}
}

static void moonfish_quote(struct moonfish_state *state, char *line)
{
	fprintf(state->in, "%s\n", line);
}

static void moonfish_isready(struct moonfish_state *state, char *line)
{
	if (moonfish_no_parameters(state, line)) return;
	moonfish_out(state);
	printf("\x1B[38;5;141mreadyok\x1B[0m\n");
	moonfish_unlock(state);
}

static FILE *moonfish_file(char *name0, char *flags)
{
	static char name[2048];
	
	char *arg;
	char *part;
	
	name[0] = 0;
	
	arg = getenv("XDG_STATE_HOME");
	if (arg != NULL && arg[0] == '/') {
		strncat(name, arg, sizeof name - 1);
	}
	else {
		arg = getenv("HOME");
		if (arg == NULL || arg[0] != '/') {
			fprintf(stderr, "missing HOME\n");
			exit(1);
		}
		strncat(name, arg, sizeof name - 1);
		strncat(name, "/.local/state", sizeof name - 1);
	}
	
	strncat(name, "/moonline/", sizeof name - 1);
	strncat(name, name0, sizeof name - 1);
	
	if (strlen(name) == sizeof name - 1) {
		fprintf(stderr, "name too long\n");
		exit(1);
	}
	
	part = name;
	for (;;) {
		part = strchr(part + 1, '/');
		if (part == NULL) break;
		*part = '\0';
		if (mkdir(name, 0777) != 0 && errno != EEXIST) {
			return NULL;
		}
		*part = '/';
	}
	
	return fopen(name, flags);
}

static void moonfish_quit(struct moonfish_state *state, char *line)
{
	static struct moonfish_chess chess;
	static struct moonfish_move move;
	static char name[16];
	
	FILE *file;
	int i;
	
	if (moonfish_no_parameters(state, line)) return;
	
	state->quit = 1;
	fprintf(state->in, "quit\n");
	fclose(state->in);
	
	file = moonfish_file("history", "w");
	if (file == NULL) {
		perror("moonfish_file");
		exit(1);
	}
	
	moonline_save(&state->line_state, file);
	fclose(file);
	
	if (state->session == NULL) exit(0);
	
	file = fopen(state->session, "w");
	if (file == NULL) {
		perror("fopen");
		exit(1);
	}
	
	if (state->fen[0] == 0) fprintf(file, "position startpos");
	else fprintf(file, "position fen %s", state->fen);
	
	if (state->moves[0][0] != 0) {
		
		fprintf(file, " moves");
		
		moonfish_chess(&chess);
		if (state->fen[0] != 0) moonfish_from_fen(&chess, state->fen);
		
		for (i = 0 ; state->moves[i][0] != 0 ; i++) {
			moonfish_from_san(&chess, &move, state->moves[i]);
			moonfish_to_uci(&chess, &move, name);
			moonfish_play(&chess, &move);
			fprintf(file, " %s", name);
		}
	}
	
	fprintf(file, "\n");
	
	for (i = 0 ; state->options[i].type != moonfish_type_none ; i++) {
		if (state->options[i].type == moonfish_type_button) continue;
		fprintf(file, "setoption name %s value %s\n", state->options[i].name, state->options[i].value);
	}
	
	fclose(file);
	
	exit(0);
}

static void moonfish_go(struct moonfish_state *state, char *line)
{
	static struct moonfish_chess chess;
	
	if (state->searching) {
		moonfish_error(state);
		printf("the bot is already searching\n");
		moonfish_unlock(state);
		return;
	}
	
	state->searching = 1;
	moonfish_uci_chess(state, &chess);
	
	if (moonfish_finished(&chess)) {
		moonfish_error(state);
		printf("the game is already finished\n");
		moonfish_unlock(state);
		return;
	}
	fprintf(state->in, "go %s\n", line);
}

static void moonfish_ucinewgame(struct moonfish_state *state, char *line)
{
	if (moonfish_no_parameters(state, line)) return;
	fprintf(state->in, "ucinewgame\n");
}

static void moonfish_debug(struct moonfish_state *state, char *line)
{
	if (strcmp(line, "on") && strcmp(line, "off")) {
		moonfish_error(state);
		printf("unexpected parameters '%s'\n", line);
		moonfish_unlock(state);
		return;
	}
	
	fprintf(state->in, "debug %s\n", line);
}

static void *moonfish_start(void *data)
{
	static char line[0x1000];
	static struct moonfish_state state0;
	static char *commands[] = {
		"info", "depth", "seldepth", "time", "nodes", "pv", "multipv", "score", "currmove", "currmovenumber", "hashfull", "nps", "tbhits", "sbhits", "cpuload", "string", "refutation", "currline", NULL,
		"bestmove", "ponder", NULL,
		NULL
	};
	
	struct moonfish_state *state;
	
	state = data;
	
	for (;;) {
		if (fgets(line, sizeof line, state->out) == NULL) {
			if (state->quit) break;
			fprintf(stderr, "bot crashed\n");
			exit(1);
		}
		moonfish_normalise(line);
		moonfish_out(state);
		state0 = *state;
		state0.commands = commands;
		moonfish_normalise(line);
		if (!strncmp(line, "bestmove ", 9)) state->searching = 0;
		moonfish_highlight(&state0, line, 0, sizeof line);
		printf("\x1B[0m\n");
		moonfish_unlock(state);
		moonline_show(&state->line_state);
	}
	
	return NULL;
}

static void moonfish_uci_stop(struct moonfish_state *state, char *line)
{
	if (moonfish_no_parameters(state, line)) return;
	
	if (!state->searching) {
		moonfish_error(state);
		printf("the bot is not searching\n");
		moonfish_unlock(state);
		return;
	}
	
	fprintf(state->in, "stop\n");
}

int main(int argc, char **argv)
{
	static struct moonfish_command cmd = {
		"wrap UCI bots for additional features on the terminal",
		"<cmd> <args>...",
		{
			{"S", "session", "<file-name>", NULL, "use the given file to keep session"},
			{"N", "notation", "<type>", "uci", "'uci' or 'san' (default: 'uci')"},
		},
		{
			{"lc0", "wrap Leela"},
			{"-S session.txt lc0", "keep or use a session file"},
		},
		{{NULL, NULL, NULL}},
		{"this assumes that the given bot properly comforms to UCI"},
	};
	
	static char *commands[] = {
		"help", NULL,
		"uci", NULL,
		"debug", "on", "off", NULL,
		"isready", NULL,
		"set", "name", "value", NULL,
		"get", "name", NULL,
		"setoption", "name", "value", NULL,
		"getoption", "name", NULL,
		"ucinewgame", NULL,
		"position", "startpos", "fen", "moves", NULL,
		"go", "infinite", "wtime", "btime", "winc", "binc", "movetime", "depth", "nodes", "mate", "movestogo", "searchmoves", "ponder", NULL,
		"stop", NULL,
		"quit", NULL,
		"quote", "", NULL,
		"play", "", NULL,
		"show", NULL,
		"clear", NULL,
		NULL
	};
	
	static struct moonfish_uci_command commands1[] = {
		{"get", &moonfish_getoption},
		{"set", &moonfish_setoption},
		{"getoption", &moonfish_getoption},
		{"setoption", &moonfish_setoption},
		{"quit", &moonfish_quit},
		{"show", &moonfish_show},
		{"play", &moonfish_uci_play},
		{"clear", &moonfish_clear},
		{"uci", &moonfish_show_all},
		{"position", &moonfish_position},
		{"help", &moonfish_help},
		{"quote", &moonfish_quote},
		{"isready", &moonfish_isready},
		{"go", &moonfish_go},
		{"ucinewgame", &moonfish_ucinewgame},
		{"debug", &moonfish_debug},
		{"stop", &moonfish_uci_stop},
	};
	
	static struct moonfish_state state = {0};
	static char line2[2048];
	
	char *line;
	char **command;
	FILE *file;
	int error;
	pthread_t thread;
	
	command = moonfish_args(&cmd, argc, argv);
	if (command - argv >= argc) moonfish_usage(&cmd, argv[0]);
	if (strcmp(cmd.args[1].value, "uci") && strcmp(cmd.args[1].value, "san")) {
		moonfish_usage(&cmd, argv[0]);
	}
	
	if (!isatty(0) || !isatty(1)) {
		execvp(command[0], command);
		perror("execvp");
		return 1;
	}
	
	moonfish_spawn(command, &state.in, &state.out, NULL);
	
	error = pthread_mutex_init(&state.mutex, NULL);
	if (error != 0) {
		fprintf(stderr, "pthread_mutex_init: %s\n", strerror(error));
		return 1;
	}
	
	state.commands = commands;
	state.line_state.data = &state;
	state.session = cmd.args[0].value;
	if (!strcmp(cmd.args[1].value, "san")) state.san = 1;
	
	moonfish_initialise(&state);
	
	error = pthread_create(&thread, NULL, &moonfish_start, &state);
	if (error != 0) {
		fprintf(stderr, "pthread_create: %s\n", strerror(error));
		return 1;
	}
	
	file = moonfish_file("history", "r");
	if (file != NULL) {
		moonline_load(&state.line_state, file);
		fclose(file);
	}
	
	file = NULL;
	if (state.session != NULL) {
		file = fopen(state.session, "r");
		if (file == NULL) {
			printf("warning: could not open session '%s' (a new session will be created)\n", state.session);
		}
	}
	
	if (file != NULL) {
		for (;;) {
			if (fgets(line2, sizeof line2, file) == NULL) {
				if (feof(file)) break;
				perror("fgets");
			}
			moonfish_normalise(line2);
			moonfish_uci(&state, commands1, line2);
		}
		fclose(file);
	}
	
	for (;;) {
		line = moonline_line(&state.line_state, "-> ");
		if (line == NULL) {
			printf("\n");
			break;
		}
		if (line[strspn(line, " ")] == 0) continue;
		moonline_add_history(&state.line_state, line);
		printf("-> ");
		moonfish_highlight(&state, line, 0, strlen(line));
		printf("\x1B[0m\n");
		moonfish_normalise(line);
		if (moonfish_uci(&state, commands1, line)) {
			moonfish_error(&state);
			printf("unknown command '%s'\n", line);
			moonfish_unlock(&state);
		}
	}
	
	moonfish_quit(&state, "");
	return 0;
}
