#ifndef SYSUTIL_H
#define SYSUTIL_H

#include <stddef.h>
#include <stdint.h>

char *string_concat(char **strs);
char **string_split(char *str, char c, size_t *len);
char **bs_lookup(const char *prefix, char **strs, size_t len, int (*cmp)(const char *a, const char *b, size_t n));
char *read_all(int fd, size_t *len);
char *read_until(int fd, char c, size_t *len);

uint64_t read_uint64(uint8_t buf[8]);
void write_uint64(uint8_t buf[8], uint64_t num);
uint32_t read_uint32(uint8_t buf[4]);
void write_uint32(uint8_t buf[4], uint32_t num);

char *xdg_runtime_dir();
char *xdg_data_dirs();
char *xdg_config_dirs();
char *xdg_config_home();
char *xdg_cache_home();
char *xdg_data_home();

#endif
