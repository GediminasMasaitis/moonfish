/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#ifndef MOONFISH_UGI
#define MOONFISH_UGI

void *moonfish_convert_in(void *data);
void *moonfish_convert_out(void *data);
void moonfish_convert_ugi(char *argv0, char **argv, char *convert_arg, char *pipe_arg);

#endif
