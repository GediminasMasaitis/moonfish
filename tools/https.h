/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <tls.h>

int moonfish_read(struct tls *tls, void *data, size_t length);
int moonfish_write(struct tls *tls, void *data, size_t length);
int moonfish_write_text(struct tls *tls, char *text);
char *moonfish_read_line(struct tls *tls);
void moonfish_request(struct tls *tls, char *host, char *request, char *token, char *type, int length);
int moonfish_response(struct tls *tls);
int moonfish_basic_request(char *host, char *port, char *request, char *token, char *type, char *body);
struct tls *moonfish_connect(char *host, char *port);
void moonfish_close(struct tls *tls);
