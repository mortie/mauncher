#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
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
		if (execv("mauncher", (char *[]) { "mauncher", NULL }) < 0) {
			perror("execv");
			return EXIT_FAILURE;
		}
	} else {
		close(fds[1]);
		int status;
		if (waitpid(child, &status, 0) < 0) {
			perror("wait");
			return EXIT_FAILURE;
		}

		if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS)
			return WEXITSTATUS(status);
		else if (WIFSIGNALED(status))
			return WSTOPSIG(status) + 128;

		char buf[512];
		size_t mem_size = 512;
		size_t mem_len = 0;
		char *mem = malloc(mem_size);
		while (1) {
			ssize_t len = read(fds[0], buf, sizeof(buf));
			if (len < 0) {
				perror("read");
				return EXIT_FAILURE;
			} else if (len == 0) {
				break;
			}

			if (mem_len + len >= mem_size) {
				mem_size *= 2;
				mem = realloc(mem, mem_size);
			}

			memcpy(mem + mem_len, buf, len);
			mem_len += len;
		}

		printf("Got output: %.*s\n", mem_len, mem);
	}

	return EXIT_SUCCESS;
}
