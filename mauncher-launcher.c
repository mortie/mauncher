#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sysutil.h"

static char *default_cmd[] = { "mauncher", "-i" };
static char **dmenu_cmd = default_cmd;
static int dmenu_cmd_len = sizeof(default_cmd) / sizeof(*default_cmd);

// not const because execvp takes pointers to non-const
static char *calcstring =
	"import sys, os, math\n"
	"from math import (ceil, floor, log, log10, pow, sqrt,\n"
	"    cos, sin, tan, acos, asin, atan, atan2, hypot, degrees, radians,\n"
	"    pi, e)\n"
	""
	"ans = 0\n"
	"if os.getenv('PY_ANS') != None:\n"
	"    try: ans = eval(os.getenv('PY_ANS'))\n"
	"    except: pass\n"
	""
	"def digit_to_char(digit):\n"
	"    if digit < 10:\n"
	"        return str(digit)\n"
	"    return chr(ord('a') + digit - 10)\n"
	""
	"def base(num, b=16):\n"
	"    if num < 0:\n"
	"        return '-' + base(-num, b)\n"
	"    (d, m) = divmod(num, b)\n"
	"    if d > 0:\n"
	"        return base(d, b) + digit_to_char(m)\n"
	"    return digit_to_char(m)\n"
	""
	"def solve(s):\n"
	"    try:\n"
	"        from sympy.parsing.sympy_parser import (\n"
	"            parse_expr, standard_transformations, implicit_multiplication)\n"
	"        from sympy import Eq, solve\n"
	"    except ImportError:\n"
	"        return 'Missing sympy module.'\n"
	""
	"    transformations = (\n"
	"        standard_transformations + (\n"
	"        implicit_multiplication,))\n"
	""
	"    parts = s.split('=')\n"
	"    part1 = parse_expr(parts[0], transformations=transformations)\n"
	"    part2 = parse_expr(parts[1], transformations=transformations)\n"
	""
	"    r = (solve(Eq(part1, part2)))\n"
	"    if len(r) == 1:\n"
	"        return r[0]\n"
	"    else:\n"
	"        return r\n"
	""
	"res = ''\n"
	"try:\n"
	"    res = eval(os.getenv('PY_EXPR'))\n"
	"except Exception as e:\n"
	"    res = 'Exception'\n"
	"    sys.stderr.write(str(e)+'\\n')\n"
	"\n"
	"print('{!r}'.format(res))\n";

char *desktops_data = NULL;
char **desktops = NULL;
size_t desktops_len = 0;
size_t desktops_size = 0;

static int str_compare(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}

static void read_desktop_file(char *fpath, char *entname) {
	FILE *f = fopen(fpath, "r");
	if (f == NULL) {
		perror(fpath);
		return;
	}

	char linebuf[1024];
	while (1) {
		if (fgets(linebuf, sizeof(linebuf), f) == NULL)
			break;

		if (strncmp(linebuf, "Name", 4) == 0 && (linebuf[4] == ' ' || linebuf[4] == '=')) {
			size_t start = 4;
			while (linebuf[start] == ' ') start += 1;
			start += 1;
			while (linebuf[start] == ' ') start += 1;

			size_t end = start;
			while (linebuf[end] != '\n' && linebuf[end] != '\0') end += 1;
			linebuf[end] = '\0';

			char *line = string_concat(
					(char *[]) { linebuf + start, ";", entname, NULL });

			if (desktops_size == 0) {
				desktops_size = 32;
				desktops = realloc(desktops, desktops_size * sizeof(*desktops));
			}

			if (desktops_len >= desktops_size - 1) {
				desktops_size *= 2;
				desktops = realloc(desktops, desktops_size * sizeof(*desktops));
			}

			desktops[desktops_len++] = line;

			break;
		}
	}

	fclose(f);
}

static void find_desktop_files_in_dir(char *path) {
	DIR *dir = opendir(path);
	if (dir == NULL) {
		if (errno != ENOENT)
			perror(path);
		return;
	}

	while (1) {
		errno = 0;
		struct dirent *ent = readdir(dir);
		if (ent == NULL && errno == 0) {
			break;
		} else if (ent == NULL) {
			perror(path);
			break;
		}

		if (ent->d_type != DT_REG && ent->d_type != DT_LNK)
			continue;

		char *fpath = string_concat(
				(char *[]) { path, "/", ent->d_name, NULL });
		read_desktop_file(fpath, ent->d_name);
		free(fpath);
	}

	closedir(dir);
}

static void find_desktop_files() {
	char *datadirs = string_concat(
			(char *[]) { xdg_data_dirs(), ":", xdg_data_home(), NULL });

	size_t start = 0;
	size_t i = 0;
	while (1) {
		char c = datadirs[i];
		if (c == ':' || c == '\0') {
			datadirs[i] = '\0';

			char *str = string_concat(
					(char *[]) { datadirs + start, "/applications", NULL });
			find_desktop_files_in_dir(str);
			free(str);

			start = i + 1;
			if (c == '\0')
				break;
		}

		i += 1;
	}

	free(datadirs);

	qsort(desktops, desktops_len, sizeof(*desktops), &str_compare);
}

static int exec_menu(char *prompt) {
	if (prompt == NULL) {
		char *argv[dmenu_cmd_len + 1];
		memcpy(argv, dmenu_cmd, dmenu_cmd_len * sizeof(*dmenu_cmd));
		argv[dmenu_cmd_len] = NULL;
		if (execvp(argv[0], argv) < 0) {
			perror(argv[0]);
			return -1;
		}
	} else {
		char *argv[dmenu_cmd_len + 3];
		memcpy(argv, dmenu_cmd, dmenu_cmd_len * sizeof(*dmenu_cmd));
		argv[dmenu_cmd_len] = "-p";
		argv[dmenu_cmd_len + 1] = prompt;
		argv[dmenu_cmd_len + 2] = NULL;
		if (execvp(argv[0], argv) < 0) {
			perror(argv[0]);
			return -1;
		}
	}

	return 0;
}

static int calculator(char *str, char *ans);

static int calculator_menu(char *answer) {
	int infds[2];
	int outfds[2];

	if (pipe(infds) < 0) {
		perror("pipe");
		return EXIT_FAILURE;
	}

	if (pipe(outfds) < 0) {
		close(infds[0]);
		close(outfds[0]);
		perror("pipe");
		return EXIT_FAILURE;
	}

	pid_t child = fork();
	if (child < 0) {
		perror("fork");
		close(infds[0]);
		close(outfds[0]);
		return EXIT_FAILURE;
	}

	if (child == 0) {
		close(infds[1]);
		close(outfds[0]);
		dup2(infds[0], STDIN_FILENO);
		dup2(outfds[1], STDOUT_FILENO);
		if (exec_menu(answer) < 0)
			exit(EXIT_FAILURE);
	} else {
		close(infds[0]);
		close(outfds[1]);

		write(infds[1], "$\n", 2);
		close(infds[1]);

		size_t len;
		char *expr = read_until(outfds[0], '\n', &len);
		close(outfds[0]);
		if (expr == NULL)
			return EXIT_FAILURE;

		int status;
		if (waitpid(child, &status, 0) < 0) {
			perror("wait");
			free(expr);
			return EXIT_FAILURE;
		}

		if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS) {
			free(expr);
			return WEXITSTATUS(status);
		} else if (WIFSIGNALED(status)) {
			free(expr);
			return WSTOPSIG(status) + 128;
		}

		if (strcmp(expr, "$\n") == 0) {
			free(expr);
			return EXIT_SUCCESS;
		} else {
			calculator(expr, answer);
			free(expr);
		}
	}
}

static int calculator(char *str, char *ans) {
	int fds[2];
	if (pipe(fds) < 0) {
		perror("pipe");
		return EXIT_FAILURE;
	}

	setenv("PY_EXPR", str, 1);
	if (ans != NULL)
		setenv("PY_ANS", ans, 1);

	pid_t child = fork();
	if (child < 0) {
		perror("fork");
		return EXIT_FAILURE;
	}

	if (child == 0) {
		close(fds[0]);
		dup2(fds[1], STDOUT_FILENO);
		if (execvp("python3", (char *const[]) { "python3", "-c", calcstring, NULL }) < 0) {
			perror("python3");
			exit(EXIT_FAILURE);
		}
	} else {
		close(fds[1]);
		size_t len;
		char *output = read_until(fds[0], '\n', &len);
		close(fds[0]);
		if (output == NULL)
			return EXIT_FAILURE;

		int status;
		if (waitpid(child, &status, 0) < 0) {
			perror("wait");
			free(output);
			return EXIT_FAILURE;
		}

		if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS) {
			free(output);
			return WEXITSTATUS(status);
		} else if (WIFSIGNALED(status)) {
			free(output);
			return WSTOPSIG(status) + 128;
		}

		int ret = calculator_menu(output);
		free(output);
		return ret;
	}
}

static int shell(char *str) {
	int ret = system(str);
	if (ret < 0) {
		perror("system");
		return EXIT_FAILURE;
	} else {
		return ret;
	}
}

static int launch(char *str) {
	char **key = bs_lookup(str, desktops, desktops_len, strncmp);
	if (key == NULL)
		return -1;

	char *val = strchr(*key, ';');
	if (val == NULL) {
		fprintf(stderr, "Desktop file entry doesn't contain a value: %s\n", *key);
		return EXIT_FAILURE;
	}
	val += 1;

	if (execvp("gtk-launch", (char *[]) { "gtk-launch", val, NULL }) < 0) {
		perror("gtk-launch");
		return EXIT_FAILURE;
	}
}

int main(int argc, char **argv) {
	for (int i = 1; i < argc; ++i) {
		char *arg = argv[i];
		if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
			printf("Usage: %s [--dmenu=\"mauncher\"]\n");
			printf("\n");
			printf("Options:\n");
			printf("    --help|-h:            Show this help text.\n");
			printf("    --dmenu|-d <argv...>: Use a different dmenu command.\n");
			printf("    --calculator|-c:      Go straight to the calculator instead of launcher.\n");
			return EXIT_SUCCESS;
		} else if (strcmp(arg, "--dmenu") == 0 || strcmp(arg, "-d") == 0) {
			dmenu_cmd = argv + i + 1;
			dmenu_cmd_len = argc - i - 1;
			break;
		} else if (strcmp(arg, "--calculator") == 0 || strcmp(arg, "-c") == 0) {
			return calculator_menu("=");
		}
	}

	find_desktop_files();
	for (size_t i = 0; i < desktops_len; ++i) {
		char *x = strchr(desktops[i], ';');
		*x = '\0';
		printf("%s\n", desktops[i]);
	}
	exit(0);

	int infds[2];
	if (pipe(infds) < 0) {
		perror("pipe");
		return EXIT_FAILURE;
	}

	int outfds[2];
	if (pipe(outfds) < 0) {
		perror("pipe");
		return EXIT_FAILURE;
	}

	pid_t child = fork();
	if (child < 0) {
		perror("fork");
		return EXIT_FAILURE;
	}

	find_desktop_files();

	if (child == 0) {
		close(infds[1]);
		close(outfds[0]);
		dup2(infds[0], STDIN_FILENO);
		dup2(outfds[1], STDOUT_FILENO);
		if (exec_menu(NULL))
			exit(EXIT_FAILURE);
	} else {
		close(infds[0]);
		close(outfds[1]);

		for (size_t i = 0; i < desktops_len; ++i) {
			char *chr = strchr(desktops[i], ';');
			if (chr == NULL)
				break;

			if (write(infds[1], desktops[i], chr - desktops[i]) < 0) {
				perror("write");
				break;
			}

			if (write(infds[1], "\n", 1) < 0) {
				perror("write");
				break;
			}
		}
		close(infds[1]);

		size_t len;
		char *output = read_until(outfds[0], '\n', &len);
		close(outfds[0]);
		if (output == NULL)
			return EXIT_FAILURE;

		int status;
		if (waitpid(child, &status, 0) < 0) {
			perror("wait");
			free(output);
			return EXIT_FAILURE;
		}

		if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS) {
			free(output);
			return WEXITSTATUS(status);
		} else if (WIFSIGNALED(status)) {
			free(output);
			return WSTOPSIG(status) + 128;
		}

		int ret = launch(output);
		if (ret < 0) {
			if (output[0] == '$')
				return shell(output + 1);
			else
				return calculator(output, NULL);
		} else {
			return ret;
		}

		free(output);
		return ret;
	}

	return EXIT_SUCCESS;
}
