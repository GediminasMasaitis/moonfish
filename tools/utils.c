/* moonfish's license: 0BSD */
/* copyright 2025 zamfofex */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>

#ifdef moonfish_plan9

#define execvp exec
#define dup2 dup
#define fork() rfork(RFFDG | RFREND | RFPROC | RFNOWAIT | RFENVG)
#define O_WRONLY OWRITE

/* note: the following assumes '_SC_OPEN_MAX' only */
#define sysconf(which) 2048

static char *strtok_r(char *src, char *delim, char **ptr)
{
	char *r;
	size_t n;
	if (src != NULL) *ptr = src;
	*ptr += strspn(*ptr, delim);
	if (**ptr == 0) return NULL;
	r = *ptr;
	n = strcspn(r, delim);
	r[n] = 0;
	*ptr += n + 1;
	return r;
}

#endif

#include "../moonfish.h"
#include "tools.h"

static void moonfish_fork(char **argv, int *in_fd, int *out_fd, char *directory)
{
	int p1[2], p2[2], fd;
	int pid;
	long int count, i;
	
	if (pipe(p1) < 0 || pipe(p2) < 0) {
		perror("pipe");
		exit(1);
	}
	
	pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(1);
	}
	
	if (pid != 0) {
		*in_fd = p1[1];
		*out_fd = p2[0];
		close(p1[0]);
		close(p2[1]);
		return;
	}
	
	if (directory != NULL && chdir(directory)) {
		perror("chdir");
		exit(1);
	}
	
	fd = open("/dev/null", O_WRONLY);
	if (fd < 0) {
		perror("open");
		exit(1);
	}
	
	if (dup2(p1[0], 0) != 0 || dup2(p2[1], 1) != 1 || dup2(fd, 2) != 2) {
		perror("dup2");
		exit(1);
	}
	
	count = sysconf(_SC_OPEN_MAX);
	for (i = 3 ; i < count ; i++) close(i);
	
	execvp(argv[0], argv);
	perror("execvp");
	exit(1);
}

void moonfish_spawn(char **argv, FILE **in, FILE **out, char *directory)
{
	int in_fd, out_fd;
	
	moonfish_fork(argv, &in_fd, &out_fd, directory);
	
	*in = fdopen(in_fd, "w");
	if (*in == NULL) {
		perror("fdopen");
		exit(1);
	}
	
	*out = fdopen(out_fd, "r");
	if (*out == NULL) {
		perror("fdopen");
		exit(1);
	}
	
	if (setvbuf(*in, NULL, _IONBF, 0)) {
		perror("setvbuf");
		exit(1);
	}
	
	if (setvbuf(*out, NULL, _IONBF, 0)) {
		perror("setvbuf");
		exit(1);
	}
}

char *moonfish_next(FILE *file)
{
	static char line[2048];
	if (fgets(line, sizeof line, file) == NULL) return NULL;
	return line;
}

char *moonfish_wait(FILE *file, char *name)
{
	char *line, *arg, *buffer;
	
	for (;;) {
		
		line = moonfish_next(file);
		if (line == NULL) {
			fprintf(stderr, "file closed unexpectedly\n");
			exit(1);
		}
		
		arg = strtok_r(line, "\r\n\t ", &buffer);
		if (arg == NULL) continue;
		if (!strcmp(line, name)) return strtok_r(NULL, "\r\n\t ", &buffer);
	}
}

int moonfish_int(char *arg, int *result)
{
	char *end;
	long int long_result;
	
	errno = 0;
	long_result = strtol(arg, &end, 10);
	if (errno || *end != 0) return 1;
	if (long_result < INT_MIN) return 1;
	if (long_result > INT_MAX) return 1;
	*result = long_result;
	return 0;
}

void moonfish_usage(struct moonfish_command *cmd, char *argv0)
{
	int i;
	int col1, col2, n;
	
	if (argv0 == NULL) argv0 = "<program>";
	
	if (cmd->args[0].letter == NULL && cmd->args[0].name == NULL) fprintf(stderr, "usage: %s", argv0);
	else fprintf(stderr, "usage: %s <opts>...", argv0);
	if (cmd->rest != NULL) fprintf(stderr, " [--] %s", cmd->rest);
	fprintf(stderr, "\n");
	
	fprintf(stderr, "purpose: %s\n", cmd->description);
	
	if (cmd->examples[0].description != NULL) {
		
		col1 = 0;
		
		for (i = 0 ; cmd->examples[i].description != NULL ; i++) {
			n = strlen(cmd->examples[i].args);
			if (n > col1) col1 = n;
		}
		
		fprintf(stderr, "examples:\n");
		for (i = 0 ; cmd->examples[i].description != NULL ; i++) {
			
			n = strlen(cmd->examples[i].args);
			
			fprintf(stderr, "   ");
			fprintf(stderr, "\x1B[36m%s", argv0);
			if (cmd->examples[i].args != NULL) {
				fprintf(stderr, " %s", cmd->examples[i].args);
			}
			
			fprintf(stderr, "\x1B[0m");
			
			while (n < col1) {
				fprintf(stderr, " ");
				n++;
			}
			
			fprintf(stderr, "   %s\n", cmd->examples[i].description);
		}
	}
	
	col1 = 0;
	col2 = 0;
	
	for (i = 0 ; cmd->args[i].letter != NULL || cmd->args[i].name != NULL ; i++) {
		
		n = 0;
		if (cmd->args[i].letter != NULL) {
			n += 1 + strlen(cmd->args[i].letter);
			if (cmd->args[i].format != NULL) n += strlen(cmd->args[i].format);
		}
		if (n > col1) col1 = n;
		
		n = 0;
		if (cmd->args[i].name != NULL) {
			n += 2 + strlen(cmd->args[i].name);
			if (cmd->args[i].format != NULL) n += 1 + strlen(cmd->args[i].format);
		}
		if (n > col2) col2 = n;
	}
	
	if (cmd->args[0].letter == NULL && cmd->args[0].name == NULL) return;
	
	fprintf(stderr, "options:\n");
	
	for (i = 0 ; cmd->args[i].letter != NULL || cmd->args[i].name != NULL ; i++) {
		
		fprintf(stderr, "   ");
		
		n = 0;
		if (cmd->args[i].letter != NULL) {
			n += 1 + strlen(cmd->args[i].letter);
			fprintf(stderr, "-%s", cmd->args[i].letter);
			if (cmd->args[i].format != NULL) {
				n += strlen(cmd->args[i].format);
				fprintf(stderr, "\x1B[36m%s\x1B[0m", cmd->args[i].format);
			}
		}
		
		if (cmd->args[i].letter != NULL && cmd->args[i].name != NULL) fprintf(stderr, ", ");
		else fprintf(stderr, "  ");
		
		while (n < col1) {
			fprintf(stderr, " ");
			n++;
		}
		
		n = 0;
		if (cmd->args[i].name != NULL) {
			n += 2 + strlen(cmd->args[i].name);
			fprintf(stderr, "--%s", cmd->args[i].name);
			if (cmd->args[i].format != NULL) {
				n += 1 + strlen(cmd->args[i].format);
				fprintf(stderr, "=\x1B[36m%s\x1B[0m", cmd->args[i].format);
			}
		}
		
		if (cmd->args[i].description != NULL) {
			while (n < col2) {
				fprintf(stderr, " ");
				n++;
			}
			fprintf(stderr, "   %s", cmd->args[i].description);
		}
		
		fprintf(stderr, "\n");
	}
	
	if (cmd->env[0].name != NULL) {
		
		col1 = 0;
		
		for (i = 0 ; cmd->env[i].name != NULL ; i++) {
			n = strlen(cmd->env[i].name) + strlen(cmd->env[i].format);
			if (n > col1) col1 = n;
		}
		
		fprintf(stderr, "environment:\n");
		for (i = 0 ; cmd->env[i].name != NULL ; i++) {
			
			n = strlen(cmd->env[i].name) + strlen(cmd->env[i].format);
			
			fprintf(stderr, "   ");
			fprintf(stderr, "%s=\x1B[36m%s\x1B[0m", cmd->env[i].name, cmd->env[i].format);
			
			while (n < col1) {
				fprintf(stderr, " ");
				n++;
			}
			
			fprintf(stderr, "   %s\n", cmd->env[i].description);
		}
	}
	
	if (cmd->notes[0] != NULL) {
		fprintf(stderr, "notes:\n");
		for (i = 0 ; cmd->notes[i] != NULL ; i++) fprintf(stderr, "   %s\n", cmd->notes[i]);
	}
	
	fprintf(stderr, "version:\n");
	fprintf(stderr, "   %s is part of \x1B[36mmoonfish " moonfish_version "\x1B[0m\n", argv0);
	fprintf(stderr, "   report bugs to \x1B[36mzamfofex@twdb.moe\x1B[0m\n");
	fprintf(stderr, "   source code at \x1B[36mhttps://git.sr.ht/~zamfofex/moonfish\x1B[0m\n");
	
	exit(1);
}

static int moonfish_letter_arg(struct moonfish_arg *args, char *arg, int *argc, char ***argv)
{
	int i;
	char *name;
	size_t length;
	
	for (i = 0 ; args[i].letter != NULL || args[i].name != NULL ; i++) {
		
		name = args[i].letter;
		if (name == NULL) continue;
		length = strlen(name);
		if (strncmp(arg, name, length)) continue;
		
		if (args[i].format == NULL) {
			arg += length;
			if (arg[0] == 0) {
				args[i].value = "";
				return 0;
			}
			continue;
		}
		else {
			
			args[i].value = arg + length;
			
			if (arg[length] == '=') args[i].value = arg + length + 1;
			
			if (arg[length] == 0) {
				if (*argc <= 0) return 1;
				(*argc)--;
				(*argv)++;
				args[i].value = (*argv)[0];
			}
			
			return 0;
		}
	}
	
	return 1;
}

static int moonfish_name_arg(struct moonfish_arg *args, char *arg, int *argc, char ***argv)
{
	int i;
	char *name;
	size_t length;
	
	for (i = 0 ; args[i].letter != NULL || args[i].name != NULL ; i++) {
		
		name = args[i].name;
		if (name == NULL) continue;
		length = strlen(name);
		if (strncmp(arg, name, length)) continue;
		
		if (args[i].format == NULL) {
			if (arg[length] == 0) {
				args[i].value = "";
				return 0;
			}
			return 1;
		}
		else {
			if (arg[length] == 0) {
				if (*argc <= 0) return 1;
				(*argc)--;
				(*argv)++;
				args[i].value = (*argv)[0];
				return 0;
			}
			if (arg[length] == '=') {
				args[i].value = arg + length + 1;
				return 0;
			}
		}
	}
	
	return 1;
}

char **moonfish_args(struct moonfish_command *cmd, int argc, char **argv)
{
	char *arg, *argv0;
	
	if (argc <= 0) return argv;
	argv0 = argv[0];
	
	for (;;) {
		
		argc--;
		argv++;
		if (argc <= 0) return argv;
		
		arg = *argv;
		
		if (!strcmp(arg, "-")) return argv;
		if (!strcmp(arg, "--")) {
			if (cmd->rest == NULL) moonfish_usage(cmd, argv0);
			return argv + 1;
		}
		if (arg[0] != '-') return argv;
		arg++;
		
		if (arg[0] == '-') {
			arg++;
			if (moonfish_name_arg(cmd->args, arg, &argc, &argv)) moonfish_usage(cmd, argv0);
			continue;
		}
		
		if (moonfish_letter_arg(cmd->args, arg, &argc, &argv)) moonfish_usage(cmd, argv0);
	}
}
