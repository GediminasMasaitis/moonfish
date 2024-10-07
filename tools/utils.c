/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <sys/un.h>

#include "tools.h"

static pid_t moonfish_spawner_pid;
static char *moonfish_spawner_argv0;
static int moonfish_spawner_fd;
static char *moonfish_spawner_dir_name;
static char *moonfish_spawner_socket_name;

static void *moonfish_read_pipe(void *data)
{
	char ch;
	int *fds;
	fds = data;
	while (read(fds[0], &ch, 1) > 0) { }
	if (moonfish_spawner_socket_name != NULL)
	{
		if (unlink(moonfish_spawner_socket_name) != 0)
			perror(moonfish_spawner_argv0);
	}
	if (moonfish_spawner_dir_name != NULL)
	{
		if (rmdir(moonfish_spawner_dir_name) != 0)
			perror(moonfish_spawner_argv0);
	}
	exit(0);
}

void moonfish_spawner(char *argv0)
{
	int fd;
	FILE *in, *out;
	pid_t pid;
	int i, count, length;
	char **argv;
	char *directory;
	struct sigaction action;
	pthread_t thread;
	int fds[2], fds2[2];
	struct sockaddr_un address = {0};
	char ch;
	ssize_t size;
	
	moonfish_spawner_argv0 = argv0;
	
	moonfish_spawner_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (moonfish_spawner_fd < 0)
	{
		perror(argv0);
		exit(1);
	}
	
	if (pipe(fds) != 0)
	{
		perror(argv0);
		exit(1);
	}
	
	if (pipe(fds2) != 0)
	{
		perror(argv0);
		exit(1);
	}
	
	moonfish_spawner_pid = fork();
	if (moonfish_spawner_pid < 0)
	{
		perror(argv0);
		exit(1);
	}
	
	if (moonfish_spawner_pid != 0)
	{
		close(fds2[1]);
		for (;;)
		{
			size = read(fds2[0], &ch, 1);
			if (size > 0) break;
			if (size < 0)
			{
				perror(argv0);
				exit(1);
			}
		}
		close(fds2[0]);
		return;
	}
	
	close(fds2[0]);
	close(fds[1]);
	
	pthread_create(&thread, NULL, &moonfish_read_pipe, fds);
	
	action.sa_flags = SA_NOCLDWAIT;
	action.sa_handler = SIG_DFL;
	if (sigemptyset(&action.sa_mask))
	{
		perror(argv0);
		exit(1);
	}
	if (sigaction(SIGCHLD, &action, NULL))
	{
		perror(argv0);
		exit(1);
	}
	
	address.sun_family = AF_UNIX;
	strcpy(address.sun_path, "/tmp/moon-XXXXXX");
	
	if (mkdtemp(address.sun_path) == NULL)
	{
		perror(argv0);
		exit(1);
	}
	
	moonfish_spawner_dir_name = strdup(address.sun_path);
	if (moonfish_spawner_dir_name == NULL)
	{
		perror(argv0);
		exit(1);
	}
	
	strcat(address.sun_path, "/socket");
	
	moonfish_spawner_socket_name = strdup(address.sun_path);
	if (moonfish_spawner_socket_name == NULL)
	{
		perror(argv0);
		exit(1);
	}
	
	if (bind(moonfish_spawner_fd, (void *) &address, sizeof address) != 0)
	{
		perror(argv0);
		exit(1);
	}
	
	if (listen(moonfish_spawner_fd, 8) < 0)
	{
		perror(argv0);
		exit(1);
	}
	
	for (;;)
	{
		size = write(fds2[1], &ch, 1);
		if (size > 0) break;
		if (size < 0)
		{
			perror(argv0);
			exit(1);
		}
	}
	close(fds2[1]);
	
	for (;;)
	{
		fd = accept(moonfish_spawner_fd, NULL, NULL);
		if (fd < 0)
		{
			perror(argv0);
			exit(1);
		}
		
		pid = fork();
		if (pid < 0)
		{
			perror(argv0);
			exit(1);
		}
		
		if (pid == 0) break;
		close(fd);
	}
	
	close(fds[0]);
	close(moonfish_spawner_fd);
	
	in = fdopen(fd, "r");
	if (in == NULL)
	{
		perror(argv0);
		exit(1);
	}
	
	out = fdopen(fd, "w");
	if (out == NULL)
	{
		perror(argv0);
		exit(1);
	}
	
	if (setvbuf(in, NULL, _IONBF, 0) != 0)
	{
		perror(argv0);
		exit(1);
	}
	
	if (setvbuf(out, NULL, _IONBF, 0) != 0)
	{
		perror(argv0);
		exit(1);
	}
	
	pid = getpid();
	if (fwrite(&pid, sizeof pid, 1, out) != 1)
	{
		perror(argv0);
		exit(1);
	}
	
	if (fread(&length, sizeof length, 1, in) != 1)
	{
		perror(argv0);
		exit(1);
	}
	
	if (length < 0)
	{
		fprintf(stderr, "%s: invalid length\n", argv0);
		exit(1);
	}
	
	if (length > 0)
	{
		directory = malloc(length + 1);
		if (directory == NULL)
		{
			perror(argv0);
			exit(1);
		}
		
		directory[length] = 0;
		
		if (fread(directory, length, 1, in) != 1)
		{
			perror(argv0);
			exit(1);
		}
		
		if (chdir(directory) != 0)
		{
			perror(argv0);
			exit(1);
		}
	}
	
	if (fread(&count, sizeof count, 1, in) != 1)
	{
		perror(argv0);
		exit(1);
	}
	
	if (count < 1)
	{
		fprintf(stderr, "%s: too few arguments\n", argv0);
		exit(1);
	}
	
	argv = malloc(sizeof *argv * (count + 1));
	if (argv == NULL)
	{
		perror(argv0);
		exit(1);
	}
	
	argv[count] = NULL;
	
	for (i = 0 ; i < count ; i++)
	{
		if (fread(&length, sizeof length, 1, in) != 1)
		{
			perror(argv0);
			exit(1);
		}
		
		argv[i] = malloc(length + 1);
		if (argv[i] == NULL)
		{
			perror(argv0);
			exit(1);
		}
		argv[i][length] = 0;
		
		if (fread(argv[i], length, 1, in) != 1)
		{
			perror(argv0);
			exit(1);
		}
	}
	
	if (dup2(fd, 0) < 0 || dup2(fd, 1) < 0)
	{
		perror(argv0);
		exit(1);
	}
	
	close(fd);
	
	fd = open("/dev/null", O_WRONLY);
	if (fd < 0)
	{
		perror(argv0);
		exit(1);
	}
	
	if (dup2(fd, 2) < 0)
	{
		perror(argv0);
		exit(1);
	}
	
	execvp(argv[0], argv);
	fprintf(stderr, "%s: %s: %s\n", argv0, argv[0], strerror(errno));
	exit(1);
}

void moonfish_spawn(char **argv, FILE **in, FILE **out, char *directory)
{
	pid_t pid;
	int fd;
	int i, count;
	struct sockaddr_un address;
	socklen_t length;
	
	pid = waitpid(moonfish_spawner_pid, NULL, WNOHANG);
	if (pid < 0)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	if (pid != 0)
	{
		fprintf(stderr, "%s: spawner exited\n", moonfish_spawner_argv0);
		exit(1);
	}
	
	length = sizeof address;
	if (getsockname(moonfish_spawner_fd, (void *) &address, &length) != 0)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	
	if (connect(fd, (void *) &address, length) != 0)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	
	*in = fdopen(dup(fd), "w");
	if (*in == NULL)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	
	*out = fdopen(dup(fd), "r");
	if (*out == NULL)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	
	close(fd);
	
	if (setvbuf(*in, NULL, _IONBF, 0) != 0)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	
	if (setvbuf(*out, NULL, _IONBF, 0) != 0)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	
	if (directory == NULL) directory = "";
	count = strlen(directory);
	if (fwrite(&count, sizeof count, 1, *in) != 1)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	
	if (count > 0 && fwrite(directory, count, 1, *in) != 1)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	
	count = 0;
	while (argv[count] != NULL)
		count++;
	
	if (fwrite(&count, sizeof count, 1, *in) != 1)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	
	for (i = 0 ; argv[i] != NULL ; i++)
	{
		count = strlen(argv[i]);
		if (fwrite(&count, sizeof count, 1, *in) != 1)
		{
			perror(moonfish_spawner_argv0);
			exit(1);
		}
		
		if (fwrite(argv[i], count, 1, *in) != 1)
		{
			perror(moonfish_spawner_argv0);
			exit(1);
		}
	}
	
	if (fread(&pid, sizeof pid, 1, *out) != 1)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	
	if (setvbuf(*in, NULL, _IOLBF, 0) != 0)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
	
	if (setvbuf(*out, NULL, _IOLBF, 0) != 0)
	{
		perror(moonfish_spawner_argv0);
		exit(1);
	}
}

char *moonfish_next(FILE *file)
{
	static char line[2048];
	
	if (fgets(line, sizeof line, file) == NULL)
		return NULL;
	return line;
}

char *moonfish_wait(FILE *file, char *name)
{
	char *line, *arg, *buffer;
	
	for (;;)
	{
		line = moonfish_next(file);
		if (line == NULL) exit(1);
		
		arg = strtok_r(line, "\r\n\t ", &buffer);
		if (arg == NULL) continue;
		if (!strcmp(line, name))
			return strtok_r(NULL, "\r\n\t ", &buffer);
	}
}

static void moonfish_usage_to(struct moonfish_arg *args, char *rest_format, char *argv0, FILE *out)
{
	int i;
	int col1, col2, n;
	
	if (argv0 == NULL) argv0 = "<program>";
	if (rest_format == NULL) rest_format = "<args>...";
	
	col1 = 0;
	col2 = 0;
	
	for (i = 0 ; args[i].letter != NULL || args[i].name != NULL ; i++)
	{
		n = 0;
		if (args[i].letter != NULL)
		{
			n += 1 + strlen(args[i].letter);
			if (args[i].format != NULL) n += strlen(args[i].format);
		}
		if (n > col1) col1 = n;
		
		n = 0;
		if (args[i].name != NULL)
		{
			n += 2 + strlen(args[i].name);
			if (args[i].format != NULL) n += 1 + strlen(args[i].format);
		}
		if (n > col2) col2 = n;
	}
	
	fprintf(out, "usage: %s <options>... [--] %s\n", argv0, rest_format);
	fprintf(out, "options:\n");
	
	for (i = 0 ; args[i].letter != NULL || args[i].name != NULL ; i++)
	{
		fprintf(out, "   ");
		
		n = 0;
		if (args[i].letter != NULL)
		{
			n += 1 + strlen(args[i].letter);
			fprintf(out, "-%s", args[i].letter);
			if (args[i].format != NULL)
			{
				n += strlen(args[i].format);
				fprintf(out, "\x1B[36m%s\x1B[0m", args[i].format);
			}
		}
		
		if (args[i].letter != NULL && args[i].name != NULL) fprintf(out, ", ");
		else fprintf(out, "  ");
		
		while (n < col1)
		{
			fprintf(out, " ");
			n++;
		}
		
		n = 0;
		if (args[i].name != NULL)
		{
			n += 2 + strlen(args[i].name);
			fprintf(out, "--%s", args[i].name);
			if (args[i].format != NULL)
			{
				n += 1 + strlen(args[i].format);
				fprintf(out, "=\x1B[36m%s\x1B[0m", args[i].format);
			}
		}
		
		if (args[i].description != NULL)
		{
			while (n < col2)
			{
				fprintf(out, " ");
				n++;
			}
			
			fprintf(out, "   %s", args[i].description);
		}
		
		fprintf(out, "\n");
	}
}

static int moonfish_letter_arg(struct moonfish_arg *args, char *arg, int *argc, char ***argv)
{
	int i;
	char *name;
	size_t length;
	
	for (i = 0 ; args[i].letter != NULL || args[i].name != NULL ; i++)
	{
		name = args[i].letter;
		if (name == NULL) continue;
		length = strlen(name);
		if (strncmp(arg, name, length)) continue;
		
		if (args[i].format == NULL)
		{
			arg += length;
			if (arg[0] == 0) return 0;
			continue;
		}
		else
		{
			args[i].value = arg + length;
			
			if (arg[length] == '=')
				args[i].value = arg + length + 1;
			
			if (arg[length] == 0)
			{
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
	
	for (i = 0 ; args[i].letter != NULL || args[i].name != NULL ; i++)
	{
		name = args[i].name;
		if (name == NULL) continue;
		length = strlen(name);
		if (strncmp(arg, name, length)) continue;
		
		if (args[i].format == NULL)
		{
			if (arg[length] == 0) return 0;
			if (arg[length] == '=') return 1;
		}
		else
		{
			if (arg[length] == 0)
			{
				if (*argc <= 0) return 1;
				(*argc)--;
				(*argv)++;
				args[i].value = (*argv)[0];
				return 0;
			}
			if (arg[length] == '=')
			{
				args[i].value = arg + length + 1;
				return 0;
			}
		}
	}
	
	return 1;
}

char **moonfish_args(struct moonfish_arg *args, char *rest_format, int argc, char **argv)
{
	char *arg, *argv0;
	
	if (argc <= 0) return argv;
	argv0 = argv[0];
	
	for (;;)
	{
		argc--;
		argv++;
		if (argc <= 0) return argv;
		
		arg = *argv;
		if (!strcmp(arg, "-")) return argv;
		if (arg[0] != '-') return argv;
		while (arg[0] == '-') arg++;
		if (arg[0] == 0) return argv + 1;
		
		if (!strcmp(arg, "help") || !strcmp(arg, "h") || !strcmp(arg, "H"))
		{
			moonfish_usage_to(args, rest_format, argv0, stdout);
			exit(0);
		}
		
		if (moonfish_letter_arg(args, arg, &argc, &argv) == 0) continue;
		if (moonfish_name_arg(args, arg, &argc, &argv) == 0) continue;
		
		moonfish_usage(args, rest_format, argv0);
	}
}

void moonfish_usage(struct moonfish_arg *args, char *rest_format, char *argv0)
{
	moonfish_usage_to(args, rest_format, argv0, stderr);
	exit(1);
}
