#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static char *default_cmd[] = { "mauncher", "-i" };
static char **dmenu_cmd = default_cmd;
static int dmenu_cmd_len = sizeof(default_cmd) / sizeof(*default_cmd);

// not const because, execvp takes pointers to non-const
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

char *readall(int fd, size_t *len) {
		char buf[512];
		size_t size = 512;
		*len = 0;
		char *mem = malloc(size);

		while (1) {
			ssize_t l = read(fd, buf, sizeof(buf));
			if (l < 0) {
				perror("read");
				free(mem);
				return NULL;
			} else if (l == 0) {
				break;
			}

			if (*len + l >= size - 1) {
				size *= 2;
				mem = realloc(mem, size);
			}

			memcpy(mem + *len, buf, l);
			*len += l;
		}

		mem[*len] = '\0';

		return mem;
}

int exec_menu(char *prompt) {
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

int calculator(char *str, char *ans);

int calculator_menu(char *answer) {
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
		char *expr = readall(outfds[0], &len);
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

int calculator(char *str, char *ans) {
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
		char *output = readall(fds[0], &len);
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

int launch(char *str) {
	int ret = system(str);
	if (ret < 0) {
		perror("system");
		return EXIT_FAILURE;
	} else {
		return ret;
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
			break;
		} else if (strcmp(arg, "--calculator") == 0 || strcmp(arg, "-c") == 0) {
			return calculator_menu(NULL);
		}
	}

	FILE *paths = popen("dmenu_path", "r");
	if (paths == NULL) {
		perror("dmenu_path");
		return EXIT_FAILURE;
	}

	int fds[2];
	if (pipe(fds) < 0) {
		perror("pipe");
		pclose(paths);
		return EXIT_FAILURE;
	}

	pid_t child = fork();
	if (child < 0) {
		perror("fork");
		pclose(paths);
		return EXIT_FAILURE;
	}

	if (child == 0) {
		close(fds[0]);
		dup2(fileno(paths), STDIN_FILENO);
		dup2(fds[1], STDOUT_FILENO);
		if (exec_menu(NULL))
			exit(EXIT_FAILURE);
	} else {
		close(fds[1]);
		size_t len;
		char *output = readall(fds[0], &len);
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

		int ret;
		if (output[0] == '=' || output[0] == '@')
			ret = calculator(output + 1, NULL);
		else
			ret = launch(output);

		free(output);
		return ret;
	}

	return EXIT_SUCCESS;
}
