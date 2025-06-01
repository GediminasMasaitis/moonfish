/* moonfish's license: 0BSD */
/* copyright 2025 zamfofex */

#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#ifndef moonfish_plan9

#include <signal.h>

#include <cjson/cJSON.h>

#else

#include <json.h>

#define valuestring s
#define valueint n

typedef JSON cJSON;

static void cJSON_Delete(cJSON *json)
{
	jsonfree(json);
}

static cJSON *cJSON_GetObjectItem(cJSON *json, char *name)
{
	return jsonbyname(json, name);
}

static int cJSON_IsNull(cJSON *json)
{
	return json->t == JSONNull;
}

static int cJSON_IsNumber(cJSON *json)
{
	return json->t == JSONNumber;
}

static int cJSON_IsObject(cJSON *json)
{
	return json->t == JSONObject;
}

static int cJSON_IsString(cJSON *json)
{
	return json->t == JSONString;
}

static cJSON *cJSON_ParseWithOpts(char *string, const char **end, int check_null)
{
	(void) check_null;
	*end = string + strlen(string);
	return jsonparse(string);
}

#endif

#include "../moonfish.h"
#include "tools.h"
#include "https.h"

struct moonfish_game {
	char *host;
	char *port;
	char *token;
	char id[512];
	char **argv;
	char **options;
	char *username;
	char fen[512];
	int ponder;
};

static void moonfish_json_error(void)
{
	fprintf(stderr, "malformed JSON\n");
	exit(1);
}

static pthread_mutex_t moonfish_mutex;

static void moonfish_handle_game_events(struct tls *tls, struct moonfish_game *game, FILE *in, FILE *out)
{
	static char request[2048];
	static char name0[6];
	
	char *line;
	cJSON *root, *type, *state, *white_player, *id, *moves, *fen;
	cJSON *wtime, *btime, *winc, *binc;
	const char *end;
	int white;
	int move_count, count;
	int i;
	char *name;
	int variant;
	struct moonfish_chess chess;
	struct moonfish_move move;
	char *value, **options;
	
	pthread_mutex_lock(&moonfish_mutex);
	
	fprintf(in, "uci\n");
	moonfish_wait(out, "uciok");
	
	options = game->options;
	for (;;) {
		value = strchr(*options, '=');
		if (value == NULL) break;
		fprintf(in, "setoption name %.*s value %s\n", (int) (value - *options), *options, value + 1);
		options++;
	}
	
	fprintf(in, "ucinewgame\n");
	fprintf(in, "isready\n");
	moonfish_wait(out, "readyok");
	
	root = NULL;
	white = -1;
	move_count = -1;
	variant = 0;
	
	for (;;) {
		
		pthread_mutex_unlock(&moonfish_mutex);
		
		if (root != NULL) {
			cJSON_Delete(root);
			root = NULL;
		}
		
		line = moonfish_read_line(tls);
		if (line == NULL) break;
		
		pthread_mutex_lock(&moonfish_mutex);
		
		if (line[0] == 0) {
			free(line);
			continue;
		}
		
		end = NULL;
		root = cJSON_ParseWithOpts(line, &end, 1);
		if (end != line + strlen(line)) moonfish_json_error();
		free(line);
		
		if (!cJSON_IsObject(root)) moonfish_json_error();
		
		type = cJSON_GetObjectItem(root, "type");
		if (!cJSON_IsString(type)) moonfish_json_error();
		
		state = NULL;
		
		if (!strcmp(type->valuestring, "gameState")) state = root;
		
		if (!strcmp(type->valuestring, "gameFull")) {
			
			state = cJSON_GetObjectItem(root, "state");
			if (!cJSON_IsObject(state)) moonfish_json_error();
			
			white = 0;
			
			white_player = cJSON_GetObjectItem(root, "white");
			if (!cJSON_IsObject(white_player)) moonfish_json_error();
			
			id = cJSON_GetObjectItem(white_player, "id");
			if (id != NULL && !cJSON_IsNull(id)) {
				if (!cJSON_IsString(id)) moonfish_json_error();
				if (!strcmp(id->valuestring, game->username)) white = 1;
			}
			
			fen = cJSON_GetObjectItem(root, "initialFen");
			if (!cJSON_IsString(fen)) moonfish_json_error();
			
			if (strcmp(fen->valuestring, "startpos")) {
				strcpy(game->fen, "fen ");
				strcat(game->fen, fen->valuestring);
				variant = 1;
			}
			else {
				strcpy(game->fen, "startpos");
			}
		}
		
		if (state == NULL) continue;
		
		if (white == -1) {
			fprintf(stderr, "received 'gameState' event prior to 'gameFull' event\n");
			exit(1);
		}
		
		moves = cJSON_GetObjectItem(state, "moves");
		if (!cJSON_IsString(moves)) moonfish_json_error();
		
		count = 0;
		if (moves->valuestring[0] != 0) {
			count = 1;
			for (i = 0 ; moves->valuestring[i] != 0 ; i++) {
				if (moves->valuestring[i] == ' ') count++;
			}
		}
		
		if (count <= move_count) continue;
		move_count = count;
		if (count % 2 == white && !game->ponder) continue;
		
		wtime = cJSON_GetObjectItem(state, "wtime");
		btime = cJSON_GetObjectItem(state, "btime");
		winc = cJSON_GetObjectItem(state, "winc");
		binc = cJSON_GetObjectItem(state, "binc");
		
		if (!cJSON_IsNumber(wtime)) moonfish_json_error();
		if (!cJSON_IsNumber(btime)) moonfish_json_error();
		if (!cJSON_IsNumber(winc)) moonfish_json_error();
		if (!cJSON_IsNumber(binc)) moonfish_json_error();
		
		if (game->ponder) fprintf(in, "stop\n");
		fprintf(in, "isready\n");
		moonfish_wait(out, "readyok");
		
		fprintf(in, "position %s", game->fen);
		
		if (count > 0) {
			
			fprintf(in, " moves ");
			if (!variant) {
				fprintf(in, "%s", moves->valuestring);
			}
			else {
				
				moonfish_from_fen(&chess, game->fen + 4);
				name = strtok(moves->valuestring, " ");
				for (;;) {
					
					if (moonfish_from_uci(&chess, &move, name)) {
						fprintf(stderr, "malformed move '%s' from the bot\n", name);
						exit(1);
					}
					
					moonfish_to_uci(&chess, &move, name0);
					fprintf(in, "%s", name0);
					name = strtok(NULL, " ");
					if (name == NULL) break;
					fprintf(in, " ");
				}
			}
		}
		
		fprintf(in, "\n");
		
		fprintf(in, "isready\n");
		moonfish_wait(out, "readyok");
		
		if (count % 2 == white) {
			fprintf(in, "go infinite\n");
			continue;
		}
		
		if (count > 1) {
			fprintf(in, "go wtime %d btime %d", wtime->valueint, btime->valueint);
			if (winc->valueint > 0) fprintf(in, " winc %d", winc->valueint);
			if (binc->valueint > 0) fprintf(in, " binc %d", binc->valueint);
			fprintf(in, "\n");
		}
		else {
			fprintf(in, "go movetime 6000\n");
		}
		
		name = moonfish_wait(out, "bestmove");
		if (name == NULL) {
			fprintf(stderr, "could not find 'bestmove' command\n");
			exit(1);
		}
		
		if (variant) {
			
			if (chess.board[25] == moonfish_white_king) {
				if (!strcmp(name, "e1c1")) name = "e1a1";
				if (!strcmp(name, "e1g1")) name = "e1h1";
			}
			if (chess.board[25] % 16 == moonfish_black_king) {
				if (!strcmp(name, "e8c8")) name = "e8a8";
				if (!strcmp(name, "e8g8")) name = "e8h8";
			}
			
			if (moonfish_from_uci(&chess, &move, name)) {
				fprintf(stderr, "malformed move '%s' from the bot\n", name);
				exit(1);
			}
		}
		
		snprintf(request, sizeof request, "POST /api/bot/game/%s/move/%s", game->id, name);
		if (moonfish_basic_request(game->host, game->port, game->token, request, NULL, NULL)) {
			fprintf(stderr, "could not make move '%s' in game '%s'\n", name, game->id);
			snprintf(request, sizeof request, "POST /api/bot/game/%s/resign", game->id);
			if (moonfish_basic_request(game->host, game->port, game->token, request, NULL, NULL)) {
				fprintf(stderr, "could not resign game '%s'\n", game->id);
			}
			break;
		}
	}
	
	fprintf(in, "isready\n");
	moonfish_wait(out, "readyok");
	fprintf(in, "quit\n");
	fclose(in);
	fclose(out);
	
	pthread_mutex_unlock(&moonfish_mutex);
}

static void *moonfish_handle_game(void *data)
{
	char request[2048];
	struct moonfish_game *game;
	FILE *in, *out;
	struct tls *tls;
	
	game = data;
	
	moonfish_spawn(game->argv, &in, &out, NULL);
	tls = moonfish_connect(game->host, game->port);
	
	snprintf(request, sizeof request, "GET /api/bot/game/stream/%s", game->id);
	
	moonfish_request(tls, game->host, request, game->token, NULL, 0);
	
	if (moonfish_response(tls)) {
		fprintf(stderr, "could not request game event stream\n");
		exit(1);
	}
	
	moonfish_handle_game_events(tls, game, in, out);
	
	moonfish_close(tls);
	free(game);
	return NULL;
}

static void moonfish_handle_events(struct tls *tls, char *host, char *port, char *token, char **options, char **argv, char *username, int ponder)
{
	char request[2048];
	char *line;
	cJSON *root, *type, *challenge, *id, *variant, *speed, *fen;
	const char *end;
	struct moonfish_game *game;
	pthread_t thread;
	struct moonfish_chess chess;
	int invalid;
	int error;
	
	root = NULL;
	
	for (;;) {
		
		if (root != NULL) {
			cJSON_Delete(root);
			root = NULL;
		}
		
		line = moonfish_read_line(tls);
		if (line == NULL) {
			fprintf(stderr, "connection with Lichess closed\n");
			exit(1);
		}
		
		if (line[0] == 0) {
			free(line);
			continue;
		}
		
		end = NULL;
		root = cJSON_ParseWithOpts(line, &end, 1);
		if (end != line + strlen(line)) moonfish_json_error();
		free(line);
		
		if (!cJSON_IsObject(root)) moonfish_json_error();
		
		type = cJSON_GetObjectItem(root, "type");
		if (!cJSON_IsString(type)) moonfish_json_error();
		
		if (!strcmp(type->valuestring, "gameStart")) {
			
			challenge = cJSON_GetObjectItem(root, "game");
			if (!cJSON_IsObject(challenge)) moonfish_json_error();
			
			id = cJSON_GetObjectItem(challenge, "id");
			if (!cJSON_IsString(id)) moonfish_json_error();
			
			game = malloc(sizeof *game);
			if (game == NULL) {
				perror("malloc");
				exit(1);
			}
			
			if (strlen(id->valuestring) > sizeof game->id - 1) {
				fprintf(stderr, "game ID '%s' too long\n", id->valuestring);
				exit(1);
			}
			
			game->host = host;
			game->port = port;
			game->token = token;
			strcpy(game->id, id->valuestring);
			game->argv = argv;
			game->options = options;
			game->username = username;
			game->ponder = ponder;
			
			error = pthread_create(&thread, NULL, &moonfish_handle_game, game);
			if (error) {
				fprintf(stderr, "pthread_create: %s\n", strerror(error));
				exit(1);
			}
			
			continue;
		}
		
		if (strcmp(type->valuestring, "challenge")) continue;
		
		challenge = cJSON_GetObjectItem(root, "challenge");
		if (!cJSON_IsObject(challenge)) moonfish_json_error();
		
		id = cJSON_GetObjectItem(challenge, "id");
		if (!cJSON_IsString(id)) moonfish_json_error();
		
		variant = cJSON_GetObjectItem(challenge, "variant");
		if (!cJSON_IsObject(variant)) moonfish_json_error();
		
		variant = cJSON_GetObjectItem(variant, "key");
		if (!cJSON_IsString(variant)) moonfish_json_error();
		
		if (!strcmp(variant->valuestring, "fromPosition")) {
			
			fen = cJSON_GetObjectItem(challenge, "initialFen");
			if (!cJSON_IsString(fen)) moonfish_json_error();
			
			invalid = moonfish_from_fen(&chess, fen->valuestring);
			
			if (!invalid && (chess.oo[0] || chess.ooo[0]) && chess.board[25] != moonfish_white_king) invalid = 1;
			if (!invalid && (chess.oo[1] || chess.ooo[1]) && chess.board[95] != moonfish_black_king) invalid = 1;
			if (!invalid && chess.oo[0] && chess.board[28] != moonfish_white_rook) invalid = 1;
			if (!invalid && chess.oo[1] && chess.board[98] != moonfish_black_rook) invalid = 1;
			if (!invalid && chess.ooo[0] && chess.board[21] != moonfish_white_rook) invalid = 1;
			if (!invalid && chess.ooo[1] && chess.board[91] != moonfish_black_rook) invalid = 1;
			
			if (invalid) {
				snprintf(request, sizeof request, "POST /api/challenge/%s/decline", id->valuestring);
				if (moonfish_basic_request(host, port, token, request, NULL, "reason=standard")) {
					fprintf(stderr, "could not decline challenge '%s' (chess 960 FEN)\n", id->valuestring);
				}
				continue;
			}
		}
		
		if (strcmp(variant->valuestring, "standard")) {
			snprintf(request, sizeof request, "POST /api/challenge/%s/decline", id->valuestring);
			if (moonfish_basic_request(host, port, token, request, NULL, "reason=standard")) {
				fprintf(stderr, "could not decline challenge '%s' (variant)\n", id->valuestring);
			}
			continue;
		}
		
		speed = cJSON_GetObjectItem(challenge, "speed");
		if (!cJSON_IsString(speed)) moonfish_json_error();
		
		if (!strcmp(speed->valuestring, "correspondence")) {
			snprintf(request, sizeof request, "POST /api/challenge/%s/decline", id->valuestring);
			if (moonfish_basic_request(host, port, token, request, NULL, "reason=tooSlow")) {
				fprintf(stderr, "could not decline challenge '%s' (too slow)\n", id->valuestring);
			}
			continue;
		}
		
		snprintf(request, sizeof request, "POST /api/challenge/%s/accept", id->valuestring);
		if (moonfish_basic_request(host, port, token, request, NULL, NULL)) {
			fprintf(stderr, "could not accept challenge '%s'\n", id->valuestring);
		}
	}
}

static char *moonfish_username(char *host, char *port, char *token)
{
	char *line;
	char *username;
	const char *end;
	cJSON *root, *id;
	struct tls *tls;
	
	tls = moonfish_connect(host, port);
	
	moonfish_request(tls, host, "GET /api/account", token, NULL, 0);
	
	if (moonfish_response(tls)) {
		fprintf(stderr, "could not request the Lichess account information\n");
		exit(1);
	}
	
	line = moonfish_read_line(tls);
	
	end = NULL;
	root = cJSON_ParseWithOpts(line, &end, 1);
	if (end != line + strlen(line)) moonfish_json_error();
	free(line);
	
	if (!cJSON_IsObject(root)) moonfish_json_error();
	
	id = cJSON_GetObjectItem(root, "id");
	if (!cJSON_IsString(id)) moonfish_json_error();
	
	username = strdup(id->valuestring);
	if (username == NULL) {
		perror("strdup");
		exit(1);
	}
	
	moonfish_close(tls);
	
	return username;
}

int main(int argc, char **argv)
{
	static struct moonfish_command cmd = {
		"integrate a UCI bot with Lichess",
		"<UCI-opts>... [--] <cmd> <args>...",
		{
			{"N", "host", "<name>", "lichess.org", "Lichess' host name (default: 'lichess.org')"},
			{"P", "port", "<port>", "443", "Lichess' port (default: '443')"},
			{"X", "ponder", NULL, NULL, "think during the opponent's turn"},
			{NULL, NULL, NULL, NULL, NULL},
		},
		{
			{"moonfish", "use moonfish as a Lichess bot"},
			{"-X lc0", "use Leela and enable pondering"},
			{"-N example.org lc0", "connect to another Lichess instance"},
		},
		{
			{"lichess_token", "<token>", "personal access token for Lichess (required)"},
		},
		{
			"this will connect through HTTPS (using HTTP without TLS is not supported)",
			"pondering uses UCI 'go infinite' rather than 'go ponder'",
			"this only accepts incoming challenges without matchmaking",
		},
	};
	
	char *token;
	int i;
	char **command, **options;
	int command_count;
	struct tls *tls;
	char *username;
	char *value;
#ifndef moonfish_plan9
	struct sigaction action;
#endif
	
	command = moonfish_args(&cmd, argc, argv);
	command_count = argc - (command - argv);
	if (command_count < 1) moonfish_usage(&cmd, argv[0]);
	options = command;
	
	for (;;) {
		
		value = strchr(*command, '=');
		if (value == NULL) break;
		
		if (strchr(*command, '\n') != NULL || strchr(*command, '\r') != NULL) moonfish_usage(&cmd, argv[0]);
		
		command_count--;
		command++;
		
		if (command_count <= 0) moonfish_usage(&cmd, argv[0]);
	}
	
	if (!strcmp(*command, "--")) {
		command_count--;
		command++;
		if (command_count <= 0) moonfish_usage(&cmd, argv[0]);
	}
	
	token = getenv("lichess_token");
	if (token == NULL || token[0] == 0) {
		fprintf(stderr, "Lichess token not provided\n");
		return 1;
	}
	
	for (i = 0 ; token[i] != 0 ; i++) {
		if (token[i] <= 0x20 || token[i] >= 0x7F) {
			fprintf(stderr, "invalid token provided for Lichess\n");
			return 1;
		}
	}
	
	if (pthread_mutex_init(&moonfish_mutex, NULL)) {
		fprintf(stderr, "could not initialise mutex\n");
		return 1;
	}
	
#ifndef moonfish_plan9
	
	action.sa_handler = SIG_DFL;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_RESTART | SA_NOCLDWAIT;
	
	if (sigaction(SIGCHLD, &action, NULL) != 0) {
		perror("sigaction");
		return 1;
	}
	
#endif
	
	tls = moonfish_connect(cmd.args[0].value, cmd.args[1].value);
	moonfish_request(tls, cmd.args[0].value, "GET /api/stream/event", token, NULL, 0);
	if (moonfish_response(tls)) {
		fprintf(stderr, "could not request event stream\n");
		return 1;
	}
	
	username = moonfish_username(cmd.args[0].value, cmd.args[1].value, token);
	moonfish_handle_events(tls, cmd.args[0].value, cmd.args[1].value, token, options, command, username, cmd.args[2].value != NULL ? 1 : 0);
	return 1;
}
