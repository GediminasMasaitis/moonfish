/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023 zamfofex */

#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include <bearssl.h>
#include <cjson/cJSON.h>

#include "tools.h"

#define moonfish_write_text(io_ctx, text) br_sslio_write_all(io_ctx, text, strlen(text))
#define moonfish_write_string(io_ctx, text) br_sslio_write_all(io_ctx, text, sizeof text - 1)

struct moonfish_game
{
	char *argv0;
	char *name;
	char *port;
	char *token;
	char id[512];
	char **argv;
	char *username;
};

static int moonfish_tcp(char *argv0, char *name, char *port)
{
	int fd;
	int error;
	struct addrinfo hints = {0};
	struct addrinfo *infos, *info;
	
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	
	error = getaddrinfo(name, port, &hints, &infos);
	if (error)
	{
		fprintf(stderr, "%s: %s\n", argv0, gai_strerror(error));
		exit(1);
	}
	
	fd = -1;
	
	for (info = infos ; info != NULL ; info = info->ai_next)
	{
		fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
		if (fd == -1) continue;
		if (connect(fd, info->ai_addr, info->ai_addrlen) != -1) break;
		close(fd);
	}
	
	freeaddrinfo(infos);
	
	if (info == NULL)
	{
		fprintf(stderr, "%s: could not connect to [%s]:%s\n", argv0, name, port);
		exit(1);
	}
	
	return fd;
}

static int moonfish_read(void *data, unsigned char *buffer, size_t length)
{
	int fd, *fds;
	ssize_t read_length;
	
	fds = data;
	fd = *fds;
	
	for (;;)
	{
		read_length = read(fd, buffer, length);
		if (read_length <= 0)
		{
			if (read_length < 0 && errno == EINTR) continue;
			return -1;
		}
		return read_length;
	}
}

static int moonfish_write(void *data, const unsigned char *buffer, size_t length)
{
	int fd, *fds;
	ssize_t write_length;
	
	fds = data;
	fd = *fds;
	
	for (;;)
	{
		write_length = write(fd, buffer, length);
		if (write_length <= 0) {
			if (write_length < 0 && errno == EINTR) continue;
			return -1;
		}
		return write_length;
	}
}

static void moonfish_pem_data(void *data, const void *buffer, size_t length)
{
	br_x509_decoder_context *x509_ctx;
	
	if (length == 0) return;
	
	x509_ctx = data;
	br_x509_decoder_push(x509_ctx, buffer, length);
}

static void moonfish_x509_data(void *data, const void *buffer, size_t length)
{
	br_x500_name *dn;
	
	if (length == 0) return;
	
	dn = data;
	dn->len += length;
	dn->data = realloc(dn->data, dn->len);
	
	if (dn->data == NULL)
	{
		perror(NULL);
		exit(1);
	}
	
	memcpy(dn->data + dn->len - length, buffer, length);
}

static void moonfish_copy_key(br_x509_pkey *to, br_x509_pkey *from)
{
	to->key_type = from->key_type;
	
	if (from->key_type == BR_KEYTYPE_RSA)
	{
		to->key.rsa.nlen = from->key.rsa.nlen;
		to->key.rsa.n = malloc(to->key.rsa.nlen);
		if (to->key.rsa.n == NULL)
		{
			perror(NULL);
			exit(1);
		}
		memcpy(to->key.rsa.n, from->key.rsa.n, to->key.rsa.nlen);
		
		to->key.rsa.elen = from->key.rsa.elen;
		to->key.rsa.e = malloc(to->key.rsa.elen);
		if (to->key.rsa.e == NULL)
		{
			perror(NULL);
			exit(1);
		}
		memcpy(to->key.rsa.e, from->key.rsa.e, to->key.rsa.elen);
	}
	
	if (from->key_type == BR_KEYTYPE_EC)
	{
		to->key.ec.curve = from->key.ec.curve;
		to->key.ec.qlen = from->key.ec.qlen;
		to->key.ec.q = malloc(to->key.ec.qlen);
		if (to->key.ec.q == NULL)
		{
			perror(NULL);
			exit(1);
		}
		memcpy(to->key.ec.q, from->key.ec.q, to->key.ec.qlen);
	}
}

static void moonfish_load_pem(char *argv0, FILE *file, br_x509_trust_anchor **tas, size_t *count)
{
	size_t read_length, n;
	unsigned char buffer0[1024], *buffer;
	br_pem_decoder_context pem_ctx;
	br_x509_decoder_context x509_ctx;
	br_x509_trust_anchor *trust_anchors, *trust_anchor;
	size_t trust_anchor_count;
	br_x509_pkey *pkey;
	
	br_pem_decoder_init(&pem_ctx);
	
	read_length = 0;
	trust_anchors = NULL;
	trust_anchor_count = 0;
	
	for (;;)
	{
		if (read_length == 0)
		{
			if (feof(file)) break;
			read_length = fread(buffer0, 1, sizeof buffer0, file);
			if (ferror(file))
			{
				fprintf(stderr, "%s: could not read certificat file\n", argv0);
				exit(1);
			}
			buffer = buffer0;
		}
		
		n = br_pem_decoder_push(&pem_ctx, buffer, read_length);
		buffer += n;
		read_length -= n;
		
		switch (br_pem_decoder_event(&pem_ctx))
		{
		case 0:
			break;
		default:
			fprintf(stderr, "%s: PEM decoding failed\n", argv0);
			exit(1);
		case BR_PEM_BEGIN_OBJ:
			trust_anchors = realloc(trust_anchors, sizeof *trust_anchors * (trust_anchor_count + 1));
			if (trust_anchors == NULL)
			{
				perror(argv0);
				exit(1);
			}
			trust_anchor = trust_anchors + trust_anchor_count;
			trust_anchor->dn.len = 0;
			trust_anchor->dn.data = NULL;
			br_x509_decoder_init(&x509_ctx, &moonfish_x509_data, &trust_anchor->dn);
			br_pem_decoder_setdest(&pem_ctx, &moonfish_pem_data, &x509_ctx);
			break;
		case BR_PEM_END_OBJ:
			trust_anchor = trust_anchors + trust_anchor_count;
			trust_anchor_count++;
			pkey = br_x509_decoder_get_pkey(&x509_ctx);
			if (pkey == NULL)
			{
				fprintf(stderr, "%s: X509 decoding failed: %d\n", argv0, br_x509_decoder_last_error(&x509_ctx));
				exit(-1);
			}
			moonfish_copy_key(&trust_anchor->pkey, pkey);
			trust_anchor->flags = 0;
			if (br_x509_decoder_isCA(&x509_ctx)) trust_anchor->flags |= BR_X509_TA_CA;
			break;
		}
	}
	
	*tas = trust_anchors;
	*count = trust_anchor_count;
}

static void moonfish_request(
	br_ssl_client_context *ctx,
	br_sslio_context *io_ctx,
	br_x509_minimal_context *min_ctx,
	void *buffer,
	size_t size,
	char *argv0,
	char *name,
	char *port,
	char *token,
	char *request,
	char *type,
	int length,
	int *fd
)
{
	char length_string[64];
	
	static FILE *certs;
	static br_x509_trust_anchor *tas = NULL;
	static size_t count;
	
	if (tas == NULL)
	{
		certs = fopen("/etc/ssl/certs/ca-certificates.crt", "rb");
		if (certs == NULL)
		{
			perror(argv0);
			exit(1);
		}
		
		moonfish_load_pem(argv0, certs, &tas, &count);
		fclose(certs);
	}
	
	br_ssl_client_init_full(ctx, min_ctx, tas, count);
	br_ssl_engine_set_buffer(&ctx->eng, buffer, size, 1);
	br_ssl_client_reset(ctx, name, 0);
	
	*fd = moonfish_tcp(argv0, name, port);
	br_sslio_init(io_ctx, &ctx->eng, &moonfish_read, fd, &moonfish_write, fd);
	
	moonfish_write_text(io_ctx, request);
	moonfish_write_string(io_ctx, " HTTP/1.0\r\n");
	
	moonfish_write_string(io_ctx, "Authorization: Bearer ");
	moonfish_write_text(io_ctx, token);
	moonfish_write_string(io_ctx, "\r\n");
	
	moonfish_write_string(io_ctx, "Connection: close\r\n");
	
	moonfish_write_string(io_ctx, "Host: ");
	moonfish_write_text(io_ctx, name);
	moonfish_write_string(io_ctx, "\r\n");
	
	if (type[0])
	{
		moonfish_write_string(io_ctx, "Content-Type: ");
		moonfish_write_text(io_ctx, type);
		moonfish_write_string(io_ctx, "\r\n");
		
		sprintf(length_string, "%d", length);
		
		moonfish_write_string(io_ctx, "Content-Length: ");
		moonfish_write_text(io_ctx, length_string);
		moonfish_write_string(io_ctx, "\r\n");
	}
	
	moonfish_write_string(io_ctx, "User-Agent: moonfish/0\r\n\r\n");
}

static int moonfish_response(br_ssl_engine_context *ctx, br_sslio_context *io_ctx, char *argv0)
{
	static char success[] = "HTTP/1.0 2";
	static char success1[] = "HTTP/1.1 2";
	
	char line[sizeof success];
	char prev, cur;
	int error;
	
	if (br_sslio_read_all(io_ctx, line, sizeof line - 1))
	{
		fprintf(stderr, "%s: malformed HTTP status\n", argv0);
		exit(1);
	}
	
	if (strncmp(line, success, sizeof line - 1))
	if (strncmp(line, success1, sizeof line - 1))
		return 1;
	
	prev = 0;
	for (;;)
	{
		if (br_sslio_read_all(io_ctx, &cur, 1))
		{
			fprintf(stderr, "%s: malformed HTTP response\n", argv0);
			exit(1);
		}
		if (prev == '\n' && cur == '\r') break;
		if (prev == '\r' && cur == '\r') break;
		if (prev == '\n' && cur == '\n') return 0;
		prev = cur;
	}
	
	if (br_sslio_read_all(io_ctx, &cur, 1) || cur != '\n')
	{
		fprintf(stderr, "%s: malformed HTTP header separator\n", argv0);
		exit(1);
	}
	
	error = br_ssl_engine_last_error(ctx);
	if (error)
	{
		fprintf(stderr, "%s: BearSSL error: %d\n", argv0, error);
		exit(1);
	}
	
	return 0;
}

static int moonfish_basic_request(char *argv0, char *name, char *port, char *token, char *request, char *type, char *body)
{
	br_ssl_client_context ctx;
	br_sslio_context io_ctx;
	br_x509_minimal_context min_ctx;
	void *buffer;
	int fd;
	int status;
	
	buffer = malloc(BR_SSL_BUFSIZE_BIDI);
	
	if (type[0] == 0 && body[0] != 0) type = "application/x-www-form-urlencoded";
	
	moonfish_request(
		&ctx, &io_ctx, &min_ctx,
		buffer, BR_SSL_BUFSIZE_BIDI,
		argv0,
		name, port,
		token, request, type,
		strlen(body),
		&fd
	);
	
	moonfish_write_text(&io_ctx, body);
	br_sslio_flush(&io_ctx);
	
	status = moonfish_response(&ctx.eng, &io_ctx, argv0);
	
	close(fd);
	free(buffer);
	
	return status;
}

static int moonfish_tls_line(br_ssl_engine_context *ctx, br_sslio_context *io_ctx, char *argv0, char *line, int length)
{
	int error;
	
	if (length-- == 0) return 0;
	
	for (;;)
	{
		if (!length--)
		{
			fprintf(stderr, "%s: line too long\n", argv0);
			exit(1);
		}
		if (br_sslio_read_all(io_ctx, line, 1))
		{
			error = br_ssl_engine_last_error(ctx);
			if (error)
			{
				fprintf(stderr, "%s: BearSSL error: %d\n", argv0, error);
				exit(1);
			}
			*line = 0;
			return 1;
		}
		if (*line == '\n') break;
		line++;
	}
	
	*line = 0;
	
	return 0;
}

static void moonfish_json_error(char *argv0)
{
	fprintf(stderr, "%s: malformed JSON\n", argv0);
	exit(1);
}

static void moonfish_handle_game_events(br_ssl_engine_context *ctx, br_sslio_context *io_ctx, struct moonfish_game *game, FILE *in, FILE *out)
{
	char line[4096];
	cJSON *root, *type, *state, *white_player, *id, *moves;
	cJSON *wtime, *btime, *winc, *binc;
	const char *end;
	int white;
	int move_count, count;
	int i;
	char *move;
	int done;
	
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
	done = 0;
	
	while (!done)
	{
		if (root != NULL)
		{
			cJSON_Delete(root);
			root = NULL;
		}
		
		done = moonfish_tls_line(ctx, io_ctx, game->argv0, line, sizeof line);
		if (line[0] == 0) continue;
		
		end = NULL;
		root = cJSON_ParseWithOpts(line, &end, 1);
		if (end != line + strlen(line)) moonfish_json_error(game->argv0);
		
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
		
		fprintf(in, "position startpos");
		if (count > 0) fprintf(in, " moves %s", moves->valuestring);
		fprintf(in, "\n");
		
		fprintf(in, "isready\n");
		moonfish_wait(out, "readyok");
		
		fprintf(in, "go wtime %d btime %d", wtime->valueint, btime->valueint);
		if (winc->valueint > 0) fprintf(in, " winc %d", winc->valueint);
		if (binc->valueint > 0) fprintf(in, " binc %d", binc->valueint);
		fprintf(in, "\n");
		
		move = moonfish_wait(out, "bestmove");
		if (move == NULL)
		{
			fprintf(stderr, "%s: could not find 'bestmove' command\n", game->argv0);
			exit(1);
		}
		
		snprintf(line, sizeof line, "POST /api/bot/game/%s/move/%s", game->id, move);
		if (moonfish_basic_request(game->argv0, game->name, game->port, game->token, line, "", ""))
		{
			fprintf(stderr, "%s: could not make move '%s' in game '%s'\n", game->argv0, move, game->id);
			snprintf(line, sizeof line, "POST /api/bot/game/%s/resign", game->id);
			if (moonfish_basic_request(game->argv0, game->name, game->port, game->token, line, "", ""))
				fprintf(stderr, "%s: could not resign game '%s'\n", game->argv0, game->id);
			break;
		}
	}
	
	fprintf(in, "isready\n");
	moonfish_wait(out, "readyok");
	fprintf(in, "quit\n");
	fclose(in);
	fclose(out);
}

static void *moonfish_handle_game(void *data)
{
	char request[4096];
	struct moonfish_game *game;
	int in_fd, out_fd;
	FILE *in, *out;
	br_ssl_client_context ctx;
	br_sslio_context io_ctx;
	br_x509_minimal_context min_ctx;
	void *buffer;
	int fd;
	
	game = data;
	
	if (moonfish_spawn(game->argv0, game->argv, &in_fd, &out_fd))
	{
		perror(game->argv0);
		exit(1);
	}
	
	in = fdopen(in_fd, "w");
	if (in == NULL)
	{
		perror(game->argv0);
		exit(1);
	}
	
	out = fdopen(out_fd, "r");
	if (out == NULL)
	{
		perror(game->argv0);
		exit(1);
	}
	
	errno = 0;
	if (setvbuf(in, NULL, _IOLBF, 0))
	{
		if (errno) perror(game->argv0);
		exit(1);
	}
	
	errno = 0;
	if (setvbuf(out, NULL, _IOLBF, 0))
	{
		if (errno) perror(game->argv0);
		exit(1);
	}
	
	buffer = malloc(BR_SSL_BUFSIZE_BIDI);
	
	snprintf(request, sizeof request, "GET /api/bot/game/stream/%s", game->id);
	
	moonfish_request(
		&ctx, &io_ctx, &min_ctx,
		buffer, BR_SSL_BUFSIZE_BIDI,
		game->argv0,
		game->name, game->port,
		game->token, request, "",
		0, &fd
	);
	
	br_sslio_flush(&io_ctx);
	
	if (moonfish_response(&ctx.eng, &io_ctx, game->argv0))
	{
		fprintf(stderr, "%s: could not request game event stream\n", game->argv0);
		exit(1);
	}
	
	moonfish_handle_game_events(&ctx.eng, &io_ctx, game, in, out);
	
	free(game);
	free(buffer);
	close(fd);
	
	return NULL;
}

static void moonfish_handle_events(
	br_ssl_engine_context *ctx,
	br_sslio_context *io_ctx,
	char *argv0,
	char *name,
	char *port,
	char *token,
	char **argv,
	char *username
)
{
	static char line[8192];
	cJSON *root, *type, *challenge, *id, *variant, *speed;
	const char *end;
	struct moonfish_game *game;
	pthread_t thread;
	
	game = malloc(sizeof *game);
	if (game == NULL)
	{
		fprintf(stderr, "%s: could not allocate game\n", argv0);
		exit(1);
	}
	
	root = NULL;
	
	for (;;)
	{
		if (root != NULL)
		{
			cJSON_Delete(root);
			root = NULL;
		}
		
		if (moonfish_tls_line(ctx, io_ctx, argv0, line, sizeof line))
		{
			fprintf(stderr, "%s: connection with Lichess closed\n", argv0);
			exit(1);
		}
		
		if (line[0] == 0) continue;
		
		end = NULL;
		root = cJSON_ParseWithOpts(line, &end, 1);
		if (end != line + strlen(line)) moonfish_json_error(argv0);
		
		if (!cJSON_IsObject(root)) moonfish_json_error(argv0);
		
		type = cJSON_GetObjectItem(root, "type");
		if (!cJSON_IsString(type)) moonfish_json_error(argv0);
		
		if (!strcmp(type->valuestring, "gameStart"))
		{
			challenge = cJSON_GetObjectItem(root, "game");
			if (!cJSON_IsObject(challenge)) moonfish_json_error(argv0);
			
			id = cJSON_GetObjectItem(challenge, "id");
			if (!cJSON_IsString(id)) moonfish_json_error(argv0);
			
			if (strlen(id->valuestring) > sizeof game->id - 1)
			{
				fprintf(stderr, "%s: game ID '%s' too long\n", argv0, id->valuestring);
				exit(1);
			}
			
			game->argv0 = argv0;
			game->name = name;
			game->port = port;
			game->token = token;
			strcpy(game->id, id->valuestring);
			game->argv = argv;
			game->username = username;
			
			pthread_create(&thread, NULL, &moonfish_handle_game, game);
			
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
		
		if (strcmp(variant->valuestring, "standard"))
		{
			snprintf(line, sizeof line, "POST /api/challenge/%s/decline", id->valuestring);
			if (moonfish_basic_request(argv0, name, port, token, line, "", "reason=standard"))
				fprintf(stderr, "%s: could not decline challenge '%s' (standard)\n", argv0, id->valuestring);
			continue;
		}
		
		speed = cJSON_GetObjectItem(challenge, "speed");
		if (!cJSON_IsString(speed)) moonfish_json_error(argv0);
		
		if (!strcmp(speed->valuestring, "correspondence"))
		{
			snprintf(line, sizeof line, "POST /api/challenge/%s/decline", id->valuestring);
			if (moonfish_basic_request(argv0, name, port, token, line, "", "reason=tooSlow"))
				fprintf(stderr, "%s: could not decline challenge '%s' (too slow)\n", argv0, id->valuestring);
			continue;
		}
		
		snprintf(line, sizeof line, "POST /api/challenge/%s/accept", id->valuestring);
		if (moonfish_basic_request(argv0, name, port, token, line, "", ""))
			fprintf(stderr, "%s: could not accept challenge '%s'\n", argv0, id->valuestring);
	}
}

static char *moonfish_username(char *argv0, char *name, char *port, char *token)
{
	static char line[8192];
	static char username[512];
	
	br_ssl_client_context ctx;
	br_sslio_context io_ctx;
	br_x509_minimal_context min_ctx;
	void *buffer;
	int fd;
	const char *end;
	cJSON *root, *id;
	
	buffer = malloc(BR_SSL_BUFSIZE_BIDI);
	
	moonfish_request(
		&ctx, &io_ctx, &min_ctx,
		buffer, BR_SSL_BUFSIZE_BIDI,
		argv0,
		name, port,
		token, "GET /api/account",
		"", 0, &fd
	);
	
	br_sslio_flush(&io_ctx);
	
	if (moonfish_response(&ctx.eng, &io_ctx, argv0))
	{
		fprintf(stderr, "%s: could not request the Lichess username\n", argv0);
		exit(1);
	}
	
	moonfish_tls_line(&ctx.eng, &io_ctx, argv0, line, sizeof line);
	
	end = NULL;
	root = cJSON_ParseWithOpts(line, &end, 1);
	if (end != line + strlen(line)) moonfish_json_error(argv0);
	
	if (!cJSON_IsObject(root)) moonfish_json_error(argv0);
	
	id = cJSON_GetObjectItem(root, "id");
	if (!cJSON_IsString(id)) moonfish_json_error(argv0);
	
	strcpy(username, id->valuestring);
	
	br_sslio_close(&io_ctx);
	
	free(buffer);
	close(fd);
	
	return username;
}

int main(int argc, char **argv)
{
	static unsigned char buffer[BR_SSL_BUFSIZE_BIDI];
	
	char *name, *port;
	char *token;
	br_ssl_client_context ctx;
	br_sslio_context io_ctx;
	br_x509_minimal_context min_ctx;
	int fd;
	int i;
	int error;
	char **command;
	
	if (argc < 1) return 1;
	
	if (argc < 3)
	{
		if (argc > 0)
		{
			fprintf(stderr, "usage: %s [<host-name>] [<host-port>] [--] <command>\n", argv[0]);
			fprintf(stderr, "note: '--' is only optional when both '<host-name>' and '<host-port>' are specified\n");
		}
		return 1;
	}
	
	name = "lichess.org";
	port = "443";
	
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
	
	if (!strcmp(argv[1], "--"))
	{
		command = argv + 2;
	}
	else
	{
		name = argv[1];
		if (!strcmp(argv[2], "--"))
		{
			command = argv + 3;
		}
		else
		{
			name = argv[1];
			if (!strcmp(argv[3], "--"))
				command = argv + 4;
			else
				command = argv + 3;
		}
	}
	
	if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) return 1;
	
	moonfish_request(&ctx, &io_ctx, &min_ctx, buffer, sizeof buffer, argv[0], name, port, token, "GET /api/stream/event", "", 0, &fd);
	br_sslio_flush(&io_ctx);
	if (moonfish_response(&ctx.eng, &io_ctx, argv[0]))
	{
		fprintf(stderr, "%s: could not request event stream\n", argv[0]);
		return 1;
	}
	
	moonfish_handle_events(&ctx.eng, &io_ctx, argv[0], name, port, token, command, moonfish_username(argv[0], name, port, token));
	
	br_ssl_engine_close(&ctx.eng);
	close(fd);
	
	if (br_ssl_engine_current_state(&ctx.eng) != BR_SSL_CLOSED)
	{
		fprintf(stderr,"%s: TLS connection closed improperly\n", argv[0]);
		return 1;
	}
	
	error = br_ssl_engine_last_error(&ctx.eng);
	if (error)
	{
		fprintf(stderr, "%s: BearSSL error: %d\n", argv[0], error);
		return 1;
	}
	
	return 0;
}
