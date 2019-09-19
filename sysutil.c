#include "sysutil.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

char *string_concat(char **strs) {
	size_t len = 0;
	for (char **s = strs; *s != NULL; ++s) {
		len += strlen(*s);
	}

	char *str = malloc(len + 1);
	char *strptr = str;
	for (char **s = strs; *s != NULL; ++s) {
		strcpy(strptr, *s);
		strptr += strlen(*s);
	}

	*strptr = '\0';
	return str;
}

char **string_split(char *str, char c, size_t *len) {
	*len = 0;
	size_t size = 512;
	char **strs = malloc(size * sizeof(*strs));

	while (1) {
		char *end = strchr(str, c);
		if (end == NULL)
			break;

		if (*len >= size - 2) {
			size *= 2;
			strs = realloc(strs, size * sizeof(*strs));
		}

		strs[(*len)++] = str;
		*end = '\0';
		str = end + 1;
	}

	strs[*len] = NULL;

	return strs;
}

char *read_until(int fd, char c, size_t *len) {
	char buf[1024];
	size_t size = 1024;
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

		char *found = memchr(buf, c, l);
		if (found)
			l = found - buf;

		if (*len + l >= size - 2) {
			size *= 2;
			mem = realloc(mem, size);
		}

		memcpy(mem + *len, buf, l);
		*len += l;

		if (found)
			break;
	}

	mem[*len] = '\0';

	return mem;
}

char *read_all(int fd, size_t *len) {
	char buf[1024];
	size_t size = 1024;
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

		if (*len + l >= size - 2) {
			size *= 2;
			mem = realloc(mem, size);
		}

		memcpy(mem + *len, buf, l);
		*len += l;
	}

	mem[*len] = '\0';

	return mem;
}

char **bs_lookup(
		const char *prefix, char **strs, size_t len,
		int (*cmp)(const char *a, const char *b, size_t n)) {
	if (prefix[0] == '\0')
		return strs;
	if (len == 0)
		return NULL;

	ssize_t pfxlen = strlen(prefix);
	ssize_t start = 0;
	ssize_t end = len - 1;
	ssize_t index;
	int logcount = 0;
	while (1) {
		logcount += 1;
		index = start + (end - start) / 2;
		char *str = strs[index];

		int ret = cmp(str, prefix, pfxlen);
		if (ret == 0) {
			if (index > 0 && cmp(strs[index - 1], prefix, pfxlen) == 0)
				end = index;
			else
				break;
		} else if (ret < 0) {
			start = index + 1;
		} else {
			end = index - 1;
		}

		if (end < start || end == 0)
			return NULL;
	}

	return strs + index;
}

uint64_t read_uint64(uint8_t buf[8]) {
	return 0 |
		(uint64_t)buf[0] << 56 |
		(uint64_t)buf[1] << 48 |
		(uint64_t)buf[2] << 40 |
		(uint64_t)buf[3] << 32 |
		(uint64_t)buf[4] << 24 |
		(uint64_t)buf[5] << 16 |
		(uint64_t)buf[6] << 8 |
		(uint64_t)buf[7] << 0;
}

void write_uint64(uint8_t buf[8], uint64_t num) {
	buf[0] = num >> 56;
	buf[1] = num >> 48;
	buf[2] = num >> 40;
	buf[3] = num >> 32;
	buf[4] = num >> 24;
	buf[5] = num >> 16;
	buf[6] = num >> 8;
	buf[7] = num >> 0;
}

uint32_t read_uint32(uint8_t buf[4]) {
	return 0 |
		(uint32_t)buf[0] << 24 |
		(uint32_t)buf[1] << 16 |
		(uint32_t)buf[2] << 8 |
		(uint32_t)buf[3] << 0;
}

void write_uint32(uint8_t buf[4], uint32_t num) {
	buf[0] = num >> 24;
	buf[1] = num >> 16;
	buf[2] = num >> 8;
	buf[3] = num >> 0;
}

char *xdg_runtime_dir() {
	static char *path = NULL;
	if (path != NULL)
		return path;

	path = getenv("XDG_RUNTIME_DIR");
	if (path != NULL)
		return path;

	path = getenv("TMPDIR");
	if (path != NULL)
		return path;

	return path = "/tmp";
}

char *xdg_data_dirs() {
	static char *path = NULL;
	if (path != NULL)
		return path;

	path = getenv("XDG_DATA_DIRS");
	if (path != NULL)
		return path;

	return path = "/usr/local/share:/usr/share";
}

char *xdg_config_dirs() {
	static char *path = NULL;
	if (path != NULL)
		return path;

	path = getenv("XDG_CONFIG_DIRS");
	if (path != NULL)
		return path;

	return path = "/etc/xdg";
}

char *xdg_config_home() {
	static char *path = NULL;
	if (path != NULL)
		return path;

	path = getenv("XDG_CONFIG_HOME");
	if (path != NULL)
		return path;

	return path = string_concat((char *[]) { getenv("HOME"), "/.config", NULL });
}

char *xdg_cache_home() {
	static char *path = NULL;
	if (path != NULL)
		return path;

	path = getenv("XDG_CACHE_HOME");
	if (path != NULL)
		return path;

	return path = string_concat((char *[]) { getenv("HOME"), "/.cache", NULL });
}

char *xdg_data_home() {
	static char *path = NULL;
	if (path != NULL)
		return path;

	path = getenv("XDG_DATA_HOME");
	if (path != NULL)
		return path;

	return path = string_concat((char *[]) { getenv("HOME"), "/.local/share", NULL });
}
