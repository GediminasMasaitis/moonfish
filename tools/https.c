/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "https.h"

int moonfish_read(char *argv0, struct tls *tls, void *data0, size_t length)
{
	char *data;
	ssize_t result;
	
	data = data0;
	
	while (length > 0) {
		
		result = tls_read(tls, data, length);
		if (result == 0) return 1;
		if (result == TLS_WANT_POLLIN || result == TLS_WANT_POLLOUT) continue;
		if (result == -1) {
			fprintf(stderr, "%s: %s\n", argv0, tls_error(tls));
			exit(1);
		}
		
		data += result;
		length -= result;
	}
	
	return 0;
}

int moonfish_write(char *argv0, struct tls *tls, void *data0, size_t length)
{
	char *data;
	ssize_t result;
	
	data = data0;
	
	while (length > 0) {
		
		result = tls_write(tls, data, length);
		if (result == 0) return 1;
		if (result == TLS_WANT_POLLIN || result == TLS_WANT_POLLOUT) continue;
		if (result == -1) {
			fprintf(stderr, "%s: %s\n", argv0, tls_error(tls));
			exit(1);
		}
		
		data += result;
		length -= result;
	}
	
	return 0;
}

int moonfish_write_text(char *argv0, struct tls *tls, char *text)
{
	return moonfish_write(argv0, tls, text, strlen(text));
}

char *moonfish_read_line(char *argv0, struct tls *tls)
{
	char *line;
	size_t length;
	
	line = NULL;
	length = 0;
	
	for (;;) {
		
		line = realloc(line, length + 1);
		if (line == NULL) {
			perror(argv0);
			exit(1);
		}
		
		if (moonfish_read(argv0, tls, line + length, 1)) {
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

void moonfish_request(char *argv0, struct tls *tls, char *host, char *request, char *token, char *type, int length)
{
	char length_string[64];
	
	moonfish_write_text(argv0, tls, request);
	moonfish_write_text(argv0, tls, " HTTP/1.0\r\n");
	
	if (token != NULL) {
		moonfish_write_text(argv0, tls, "Authorization: Bearer ");
		moonfish_write_text(argv0, tls, token);
		moonfish_write_text(argv0, tls, "\r\n");
	}
	
	moonfish_write_text(argv0, tls, "Connection: close\r\n");
	
	moonfish_write_text(argv0, tls, "Host: ");
	moonfish_write_text(argv0, tls, host);
	moonfish_write_text(argv0, tls, "\r\n");
	
	if (type != NULL) {
		
		moonfish_write_text(argv0, tls, "Content-Type: ");
		moonfish_write_text(argv0, tls, type);
		moonfish_write_text(argv0, tls, "\r\n");
		
		sprintf(length_string, "%d", length);
		
		moonfish_write_text(argv0, tls, "Content-Length: ");
		moonfish_write_text(argv0, tls, length_string);
		moonfish_write_text(argv0, tls, "\r\n");
	}
	
	moonfish_write_text(argv0, tls, "User-Agent: moonfish/0\r\n\r\n");
}

int moonfish_response(char *argv0, struct tls *tls)
{
	static char success0[] = "HTTP/1.0 2";
	static char success1[] = "HTTP/1.1 2";
	
	char *line;
	
	line = moonfish_read_line(argv0, tls);
	
	if (strncmp(line, success0, sizeof success0 - 1) && strncmp(line, success1, sizeof success1 - 1)) {
		return 1;
	}
	for (;;) {
		free(line);
		line = moonfish_read_line(argv0, tls);
		if (*line == 0) {
			free(line);
			break;
		}
	}
	
	return 0;
}

struct tls *moonfish_connect(char *argv0, char *host, char *port)
{
	struct tls *tls;
	
	tls = tls_client();
	if (tls == NULL) {
		fprintf(stderr, "%s: Could not create libtls client\n", argv0);
		exit(1);
	}
	
	if (tls_connect(tls, host, port)) {
		fprintf(stderr, "%s: %s\n", argv0, tls_error(tls));
		exit(1);
	}
	
	return tls;
}

void moonfish_close(char *argv0, struct tls *tls)
{
	if (tls_close(tls)) {
		fprintf(stderr, "%s: %s\n", argv0, tls_error(tls));
		exit(1);
	}
	
	tls_free(tls);
}

int moonfish_basic_request(char *argv0, char *host, char *port, char *request, char *token, char *type, char *body)
{
	int status;
	struct tls *tls;
	
	tls = moonfish_connect(argv0, host, port);
	
	if (type == NULL && body != NULL) type = "application/x-www-form-urlencoded";
	
	moonfish_request(argv0, tls, host, token, request, type, body == NULL ? 0 : strlen(body));
	
	if (body != NULL) moonfish_write_text(argv0, tls, body);
	
	status = moonfish_response(argv0, tls);
	moonfish_close(argv0, tls);
	return status;
}
