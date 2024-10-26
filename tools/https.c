/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
	
	for (;;) {
		free(line);
		line = moonfish_read_line(tls);
		if (*line == 0) {
			free(line);
			break;
		}
	}
	
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
	if (tls_close(tls)) {
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
