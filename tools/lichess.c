/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

#include <cjson/cJSON.h>

#include "../moonfish.h"
#include "tools.h"
#include "https.h"

struct moonfish_game
{
	char *argv0;
	char *host;
	char *port;
	char *token;
	char id[512];
	char **argv;
	char *username;
	char fen[512];
};

static void moonfish_json_error(char *argv0)
{
	fprintf(stderr, "%s: malformed JSON\n", argv0);
	exit(1);
}

static pthread_mutex_t moonfish_mutex = PTHREAD_MUTEX_INITIALIZER;

static void moonfish_handle_game_events(struct tls *tls, struct moonfish_game *game, FILE *in, FILE *out)
{
	char request[2048];
	char *line;
	cJSON *root, *type, *state, *white_player, *id, *moves, *fen;
	cJSON *wtime, *btime, *winc, *binc;
	const char *end;
	int white;
	int move_count, count;
	int i;
	char *name, name0[6];
	int variant;
	struct moonfish_chess chess;
	struct moonfish_move move;
	
	pthread_mutex_lock(&moonfish_mutex);
	
	fprintf(in, "uci\n");
	moonfish_wait(out, "uciok");
	
	fprintf(in, "isready\n");
	moonfish_wait(out, "readyok");
	
	fprintf(in, "ucinewgame\n");
	
	fprintf(in, "isready\n");
	moonfish_wait(out, "readyok");
	
	root = NULL;
	white = -1;
	move_count = -1;
	variant = 0;
	
	for (;;)
	{
		pthread_mutex_unlock(&moonfish_mutex);
		
		if (root != NULL)
		{
			cJSON_Delete(root);
			root = NULL;
		}
		
		line = moonfish_read_line(game->argv0, tls);
		if (line == NULL) break;
		
		pthread_mutex_lock(&moonfish_mutex);
		
		if (line[0] == 0)
		{
			free(line);
			continue;
		}
		
		end = NULL;
		root = cJSON_ParseWithOpts(line, &end, 1);
		if (end != line + strlen(line)) moonfish_json_error(game->argv0);
		free(line);
		
		if (!cJSON_IsObject(root)) moonfish_json_error(game->argv0);
		
		type = cJSON_GetObjectItem(root, "type");
		if (!cJSON_IsString(type)) moonfish_json_error(game->argv0);
		
		if (!strcmp(type->valuestring, "gameState"))
		{
			state = root;
		}
		else if (!strcmp(type->valuestring, "gameFull"))
		{
			state = cJSON_GetObjectItem(root, "state");
			if (!cJSON_IsObject(state)) moonfish_json_error(game->argv0);
			
			white = 0;
			
			white_player = cJSON_GetObjectItem(root, "white");
			if (!cJSON_IsObject(white_player)) moonfish_json_error(game->argv0);
			
			id = cJSON_GetObjectItem(white_player, "id");
			if (id != NULL && !cJSON_IsNull(id))
			{
				if (!cJSON_IsString(id)) moonfish_json_error(game->argv0);
				if (!strcmp(id->valuestring, game->username)) white = 1;
			}
			
			fen = cJSON_GetObjectItem(root, "initialFen");
			if (!cJSON_IsString(fen)) moonfish_json_error(game->argv0);
			
			if (strcmp(fen->valuestring, "startpos"))
			{
				strcpy(game->fen, "fen ");
				strcat(game->fen, fen->valuestring);
				variant = 1;
			}
			else
			{
				strcpy(game->fen, "startpos");
			}
		}
		else
		{
			continue;
		}
		
		if (white == -1)
		{
			fprintf(stderr, "%s: 'gameState' event received prior to 'gameFull' event\n", game->argv0);
			exit(1);
		}
		
		moves = cJSON_GetObjectItem(state, "moves");
		if (!cJSON_IsString(moves)) moonfish_json_error(game->argv0);
		
		count = 0;
		if (moves->valuestring[0] != 0)
		{
			count = 1;
			for (i = 0 ; moves->valuestring[i] != 0 ; i++)
				if (moves->valuestring[i] == ' ')
					count++;
		}
		
		if (count <= move_count) continue;
		
		move_count = count;
		
		if (count % 2 == white) continue;
		
		wtime = cJSON_GetObjectItem(state, "wtime");
		btime = cJSON_GetObjectItem(state, "btime");
		winc = cJSON_GetObjectItem(state, "winc");
		binc = cJSON_GetObjectItem(state, "binc");
		
		if (!cJSON_IsNumber(wtime)) moonfish_json_error(game->argv0);
		if (!cJSON_IsNumber(btime)) moonfish_json_error(game->argv0);
		if (!cJSON_IsNumber(winc)) moonfish_json_error(game->argv0);
		if (!cJSON_IsNumber(binc)) moonfish_json_error(game->argv0);
		
		fprintf(in, "isready\n");
		moonfish_wait(out, "readyok");
		
		fprintf(in, "position %s", game->fen);
		if (count > 0)
		{
			fprintf(in, " moves ");
			if (!variant)
			{
				fprintf(in, "%s", moves->valuestring);
			}
			else
			{
				moonfish_from_fen(&chess, game->fen + 4);
				name = strtok(moves->valuestring, " ");
				for (;;)
				{
					moonfish_from_uci(&chess, &move, name);
					moonfish_to_uci(name0, &move);
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
		
		if (count > 1)
		{
			fprintf(in, "go wtime %d btime %d", wtime->valueint, btime->valueint);
			if (winc->valueint > 0) fprintf(in, " winc %d", winc->valueint);
			if (binc->valueint > 0) fprintf(in, " binc %d", binc->valueint);
			fprintf(in, "\n");
		}
		else
		{
			fprintf(in, "go movetime 6000\n");
		}
		
		name = moonfish_wait(out, "bestmove");
		if (name == NULL)
		{
			fprintf(stderr, "%s: could not find 'bestmove' command\n", game->argv0);
			exit(1);
		}
		
		if (variant)
		{
			moonfish_from_uci(&chess, &move, name);
			if (move.piece % 16 == moonfish_king)
			{
				if (!strcmp(name, "e1c1")) name = "e1a1";
				else if (!strcmp(name, "e1g1")) name = "e1h1";
				else if (!strcmp(name, "e8c8")) name = "e8a8";
				else if (!strcmp(name, "e8g8")) name = "e8h8";
			}
		}
		
		snprintf(request, sizeof request, "POST /api/bot/game/%s/move/%s", game->id, name);
		if (moonfish_basic_request(game->argv0, game->host, game->port, game->token, request, NULL, NULL))
		{
			fprintf(stderr, "%s: could not make move '%s' in game '%s'\n", game->argv0, name, game->id);
			snprintf(request, sizeof request, "POST /api/bot/game/%s/resign", game->id);
			if (moonfish_basic_request(game->argv0, game->host, game->port, game->token, request, NULL, NULL))
				fprintf(stderr, "%s: could not resign game '%s'\n", game->argv0, game->id);
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
	
	moonfish_spawn(game->argv0, game->argv, &in, &out, NULL);
	tls = moonfish_connect(game->argv0, game->host, game->port);
	
	snprintf(request, sizeof request, "GET /api/bot/game/stream/%s", game->id);
	
	moonfish_request(game->argv0, tls, game->host, request, game->token, NULL, 0);
	
	if (moonfish_response(game->argv0, tls))
	{
		fprintf(stderr, "%s: could not request game event stream\n", game->argv0);
		exit(1);
	}
	
	moonfish_handle_game_events(tls, game, in, out);
	
	moonfish_close(game->argv0, tls);
	free(game);
	return NULL;
}

static void moonfish_handle_events(struct tls *tls, char *argv0, char *host, char *port, char *token, char **argv, char *username)
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
	
	for (;;)
	{
		if (root != NULL)
		{
			cJSON_Delete(root);
			root = NULL;
		}
		
		line = moonfish_read_line(argv0, tls);
		if (line == NULL)
		{
			fprintf(stderr, "%s: connection with Lichess closed\n", argv0);
			exit(1);
		}
		
		if (line[0] == 0)
		{
			free(line);
			continue;
		}
		
		end = NULL;
		root = cJSON_ParseWithOpts(line, &end, 1);
		if (end != line + strlen(line)) moonfish_json_error(argv0);
		free(line);
		
		if (!cJSON_IsObject(root)) moonfish_json_error(argv0);
		
		type = cJSON_GetObjectItem(root, "type");
		if (!cJSON_IsString(type)) moonfish_json_error(argv0);
		
		if (!strcmp(type->valuestring, "gameStart"))
		{
			challenge = cJSON_GetObjectItem(root, "game");
			if (!cJSON_IsObject(challenge)) moonfish_json_error(argv0);
			
			id = cJSON_GetObjectItem(challenge, "id");
			if (!cJSON_IsString(id)) moonfish_json_error(argv0);
			
			game = malloc(sizeof *game);
			if (game == NULL)
			{
				fprintf(stderr, "%s: could not allocate game\n", argv0);
				exit(1);
			}
			
			if (strlen(id->valuestring) > sizeof game->id - 1)
			{
				fprintf(stderr, "%s: game ID '%s' too long\n", argv0, id->valuestring);
				exit(1);
			}
			
			game->argv0 = argv0;
			game->host = host;
			game->port = port;
			game->token = token;
			strcpy(game->id, id->valuestring);
			game->argv = argv;
			game->username = username;
			
			error = pthread_create(&thread, NULL, &moonfish_handle_game, game);
			if (error != 0)
			{
				fprintf(stderr, "%s: %s\n", argv0, strerror(error));
				exit(1);
			}
			
			continue;
		}
		
		if (strcmp(type->valuestring, "challenge")) continue;
		
		challenge = cJSON_GetObjectItem(root, "challenge");
		if (!cJSON_IsObject(challenge)) moonfish_json_error(argv0);
		
		id = cJSON_GetObjectItem(challenge, "id");
		if (!cJSON_IsString(id)) moonfish_json_error(argv0);
		
		variant = cJSON_GetObjectItem(challenge, "variant");
		if (!cJSON_IsObject(variant)) moonfish_json_error(argv0);
		
		variant = cJSON_GetObjectItem(variant, "key");
		if (!cJSON_IsString(variant)) moonfish_json_error(argv0);
		
		if (!strcmp(variant->valuestring, "fromPosition"))
		{
			fen = cJSON_GetObjectItem(challenge, "initialFen");
			if (!cJSON_IsString(fen)) moonfish_json_error(argv0);
			
			invalid = moonfish_from_fen(&chess, fen->valuestring);
			
			if (!invalid)
			if (chess.castle.white_oo || chess.castle.white_ooo)
			if (chess.board[25] != moonfish_white_king)
				invalid = 1;
			
			if (!invalid)
			if (chess.castle.black_oo || chess.castle.black_ooo)
			if (chess.board[95] != moonfish_black_king)
				invalid = 1;
			
			if (!invalid && chess.castle.white_ooo && chess.board[21] != moonfish_white_rook) invalid = 1;
			if (!invalid && chess.castle.white_oo && chess.board[28] != moonfish_white_rook) invalid = 1;
			if (!invalid && chess.castle.black_ooo && chess.board[91] != moonfish_black_rook) invalid = 1;
			if (!invalid && chess.castle.black_oo && chess.board[98] != moonfish_black_rook) invalid = 1;
			
			if (invalid)
			{
				snprintf(request, sizeof request, "POST /api/challenge/%s/decline", id->valuestring);
				if (moonfish_basic_request(argv0, host, port, token, request, NULL, "reason=standard"))
					fprintf(stderr, "%s: could not decline challenge '%s' (chess 960 FEN)\n", argv0, id->valuestring);
				continue;
			}
		}
		else if (strcmp(variant->valuestring, "standard"))
		{
			snprintf(request, sizeof request, "POST /api/challenge/%s/decline", id->valuestring);
			if (moonfish_basic_request(argv0, host, port, token, request, NULL, "reason=standard"))
				fprintf(stderr, "%s: could not decline challenge '%s' (variant)\n", argv0, id->valuestring);
			continue;
		}
		
		speed = cJSON_GetObjectItem(challenge, "speed");
		if (!cJSON_IsString(speed)) moonfish_json_error(argv0);
		
		if (!strcmp(speed->valuestring, "correspondence"))
		{
			snprintf(request, sizeof request, "POST /api/challenge/%s/decline", id->valuestring);
			if (moonfish_basic_request(argv0, host, port, token, request, NULL, "reason=tooSlow"))
				fprintf(stderr, "%s: could not decline challenge '%s' (too slow)\n", argv0, id->valuestring);
			continue;
		}
		
		snprintf(request, sizeof request, "POST /api/challenge/%s/accept", id->valuestring);
		if (moonfish_basic_request(argv0, host, port, token, request, NULL, NULL))
			fprintf(stderr, "%s: could not accept challenge '%s'\n", argv0, id->valuestring);
	}
}

static char *moonfish_username(char *argv0, char *host, char *port, char *token)
{
	char *line;
	char *username;
	const char *end;
	cJSON *root, *id;
	struct tls *tls;
	
	tls = moonfish_connect(argv0, host, port);
	
	moonfish_request(argv0, tls, host, "GET /api/account", token, NULL, 0);
	
	if (moonfish_response(argv0, tls))
	{
		fprintf(stderr, "%s: could not request the Lichess username\n", argv0);
		exit(1);
	}
	
	line = moonfish_read_line(argv0, tls);
	
	end = NULL;
	root = cJSON_ParseWithOpts(line, &end, 1);
	if (end != line + strlen(line)) moonfish_json_error(argv0);
	free(line);
	
	if (!cJSON_IsObject(root)) moonfish_json_error(argv0);
	
	id = cJSON_GetObjectItem(root, "id");
	if (!cJSON_IsString(id)) moonfish_json_error(argv0);
	
	username = strdup(id->valuestring);
	if (username == NULL)
	{
		perror(argv0);
		exit(0);
	}
	
	moonfish_close(argv0, tls);
	
	return username;
}

int main(int argc, char **argv)
{
	static char *format = "<cmd> <args>...";
	static struct moonfish_arg args[] =
	{
		{"N", "host", "<name>", "lichess.org", "Lichess' host name (default: 'lichess.org')"},
		{"P", "port", "<port>", "443", "Lichess' port (default: '443')"},
		{NULL, NULL, NULL, NULL, NULL},
	};
	
	char *token;
	int i;
	char **command;
	int command_count;
	struct tls *tls;
	char *username;
	
	command = moonfish_args(args, format, argc, argv);
	command_count = argc - (command - argv);
	if (command_count < 1) moonfish_usage(args, format, argv[0]);
	
	token = getenv("lichess_token");
	if (token == NULL || token[0] == 0)
	{
		fprintf(stderr, "%s: Lichess token not provided\n", argv[0]);
		return 1;
	}
	
	for (i = 0 ; token[i] != 0 ; i++)
	{
		if (token[i] <= 0x20 || token[i] >= 0x7F)
		{
			fprintf(stderr, "%s: invalid token provided for Lichess\n", argv[0]);
			return 1;
		}
	}
	
	if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) return 1;
	
	tls = moonfish_connect(argv[0], args[0].value, args[1].value);
	moonfish_request(argv[0], tls, args[0].value, "GET /api/stream/event", token, NULL, 0);
	if (moonfish_response(argv[0], tls))
	{
		fprintf(stderr, "%s: could not request event stream\n", argv[0]);
		return 1;
	}
	
	username = moonfish_username(argv[0], args[0].value, args[1].value, token);
	moonfish_handle_events(tls, argv[0], args[0].value, args[1].value, token, command, username);
	
	fprintf(stderr, "%s: Unreachable\n", argv[0]);
	return 1;
}
