/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2025 zamfofex */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef moonfish_plan9

#include <libsec.h>

#define TLS_WANT_POLLIN (-2)
#define TLS_WANT_POLLOUT (-2)

struct tls {
	int fd;
};

static struct tls *tls_client(void)
{
	struct tls *tls;
	tls = malloc(sizeof *tls);
	return tls;
}

static int tls_connect(struct tls *tls, char *host, char *port)
{
	TLSconn conn = {0};
	int fd;
	
	fd = dial(netmkaddr(host, "tcp", port), NULL, NULL, NULL);
	if (fd < 0) return -1;
	
	tls->fd = tlsClient(fd, &conn);
	if (tls->fd < 0) return -1;
	free(conn.cert);
	free(conn.sessionID);
	return 0;
}

static int tls_close(struct tls *tls)
{
	return close(tls->fd);
}

static void tls_free(struct tls *tls)
{
	free(tls);
}

static ssize_t tls_read(struct tls *tls, void *buffer, size_t count)
{
	ssize_t n;
	n = read(tls->fd, buffer, count);
	if (n < 0) return 0;
	return n;
}

static ssize_t tls_write(struct tls *tls, void *buffer, size_t count)
{
	return write(tls->fd, buffer, count);
}

static char *tls_error(struct tls *tls)
{
	(void) tls;
	return "TLS error";
}

#endif

#include "https.h"

int moonfish_read(struct tls *tls, void *data0, size_t length)
{
	char *data;
	ssize_t result;
	
	data = data0;
	
	while (length > 0) {
		
		result = tls_read(tls, data, length);
		if (result == 0) return 1;
		if (result == TLS_WANT_POLLIN || result == TLS_WANT_POLLOUT) continue;
		if (result == -1) {
			fprintf(stderr, "%s\n", tls_error(tls));
			exit(1);
		}
		
		data += result;
		length -= result;
	}
	
	return 0;
}

int moonfish_write(struct tls *tls, void *data0, size_t length)
{
	char *data;
	ssize_t result;
	
	data = data0;
	
	while (length > 0) {
		
		result = tls_write(tls, data, length);
		if (result == 0) return 1;
		if (result == TLS_WANT_POLLIN || result == TLS_WANT_POLLOUT) continue;
		if (result == -1) {
			fprintf(stderr, "%s\n", tls_error(tls));
			exit(1);
		}
		
		data += result;
		length -= result;
	}
	
	return 0;
}

int moonfish_write_text(struct tls *tls, char *text)
{
	return moonfish_write(tls, text, strlen(text));
}

char *moonfish_read_line(struct tls *tls)
{
	char *line;
	size_t length;
	
	line = NULL;
	length = 0;
	
	for (;;) {
		
		line = realloc(line, length + 1);
		if (line == NULL) {
			perror("realloc");
			exit(1);
		}
		
		if (moonfish_read(tls, line + length, 1)) {
			if (length == 0) {
				free(line);
				return NULL;
			}
			line[length] = 0;
			return line;
		}
		if (line[length] == '\n') {
			line[length] = 0;
			return line;
		}
		if (line[length] != '\r') length++;
	}
}

void moonfish_request(struct tls *tls, char *host, char *request, char *token, char *type, int length)
{
	char length_string[64];
	
	moonfish_write_text(tls, request);
	moonfish_write_text(tls, " HTTP/1.0\r\n");
	
	if (token != NULL) {
		moonfish_write_text(tls, "Authorization: Bearer ");
		moonfish_write_text(tls, token);
		moonfish_write_text(tls, "\r\n");
	}
	
	moonfish_write_text(tls, "Connection: close\r\n");
	
	moonfish_write_text(tls, "Host: ");
	moonfish_write_text(tls, host);
	moonfish_write_text(tls, "\r\n");
	
	if (type != NULL) {
		
		moonfish_write_text(tls, "Content-Type: ");
		moonfish_write_text(tls, type);
		moonfish_write_text(tls, "\r\n");
		
		sprintf(length_string, "%d", length);
		
		moonfish_write_text(tls, "Content-Length: ");
		moonfish_write_text(tls, length_string);
		moonfish_write_text(tls, "\r\n");
	}
	
	moonfish_write_text(tls, "User-Agent: moonfish/0\r\n\r\n");
}

int moonfish_response(struct tls *tls)
{
	static char success0[] = "HTTP/1.0 2";
	static char success1[] = "HTTP/1.1 2";
	
	char *line;
	
	line = moonfish_read_line(tls);
	
	if (strncmp(line, success0, sizeof success0 - 1) && strncmp(line, success1, sizeof success1 - 1)) {
		return 1;
	}
	
	while (*line != 0) {
		free(line);
		line = moonfish_read_line(tls);
	}
	
	free(line);
	return 0;
}

struct tls *moonfish_connect(char *host, char *port)
{
	struct tls *tls;
	
	tls = tls_client();
	if (tls == NULL) {
		fprintf(stderr, "could not create libtls client\n");
		exit(1);
	}
	
	if (tls_connect(tls, host, port)) {
		fprintf(stderr, "%s\n", tls_error(tls));
		exit(1);
	}
	
	return tls;
}

void moonfish_close(struct tls *tls)
{
	int result;
	
	do result = tls_close(tls);
	while (result == TLS_WANT_POLLIN || result == TLS_WANT_POLLOUT);
	
	if (result) {
		fprintf(stderr, "%s\n", tls_error(tls));
		exit(1);
	}
	
	tls_free(tls);
}

int moonfish_basic_request(char *host, char *port, char *request, char *token, char *type, char *body)
{
	int status;
	struct tls *tls;
	
	tls = moonfish_connect(host, port);
	
	if (type == NULL && body != NULL) type = "application/x-www-form-urlencoded";
	
	moonfish_request(tls, host, token, request, type, body == NULL ? 0 : strlen(body));
	
	if (body != NULL) moonfish_write_text(tls, body);
	
	status = moonfish_response(tls);
	moonfish_close(tls);
	return status;
}
