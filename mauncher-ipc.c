#include "mauncher-ipc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sysutil.h"

#define checklen(buf, len, l) do { \
	len -= l; \
	if (len < 0) { \
		fprintf(stderr, "Received malformed message.\n"); \
		free(buf); \
		return -1; \
	} \
} while (0)

int daemon_message_read(int fd, struct daemon_message *msg) {
	uint8_t total_len_buf[8];
	ssize_t n = read(fd, total_len_buf, sizeof(total_len_buf));
	if (n < 0) {
		perror("read");
		return -1;
	} else if (n < sizeof(total_len_buf)) {
		fprintf(stderr, "Short read: %zu/%zu\n", n, sizeof(total_len_buf));
		return -1;
	}

	ssize_t len = (ssize_t)read_uint64(total_len_buf);

	uint8_t *buf = malloc(len);
	uint8_t *b = buf;
	if (buf == NULL) {
		perror("malloc");
		return -1;
	}

	n = read(fd, buf, len);
	if (n < 0) {
		perror("read");
		free(buf);
		return FALSE;
	} else if (n < len) {
		fprintf(stderr, "Short read: %zu/%zu\n", n, len);
		free(buf);
		return -1;
	}

	// uint64 payload length
	checklen(buf, len, 8);
	uint64_t payload_len = read_uint64(b);
	b += 8;

	// payload
	checklen(buf, len, payload_len);
	char *payload = (char *)b;
	b += payload_len;

	// uint32 prompt length
	checklen(buf, len, 4);
	uint32_t prompt_len = read_uint32(b);
	b += 4;

	// prompt
	checklen(buf, len, prompt_len);
	char *prompt = (char *)b;
	b += prompt_len;

	// flags
	checklen(buf, len, 1);
	uint8_t flags = b[0];
	b += 1;

	msg->payload = strndup(payload, payload_len);
	msg->opts.prompt = strndup(prompt, prompt_len);
	msg->opts.insensitive = flags & (1 << 0);
	free(buf);

	return 0;
}

int daemon_reply_read(int fd, struct daemon_reply *reply) {
	uint8_t total_len_buf[8];
	ssize_t n = read(fd, total_len_buf, sizeof(total_len_buf));
	if (n < 0) {
		perror("read");
		return -1;
	} else if (n == 0) {
		fprintf(stderr, "Short read: %zu/%zu\n", n, sizeof(total_len_buf));
		return -1;
	}

	ssize_t len = (ssize_t)read_uint64(total_len_buf);

	uint8_t *buf = malloc(len);
	uint8_t *b = buf;
	if (buf == NULL) {
		perror("malloc");
		return -1;
	}

	n = read(fd, buf, len);
	if (n < 0) {
		perror("read");
		free(buf);
		return FALSE;
	} else if (n < len) {
		fprintf(stderr, "Short read: %zu/%zu\n", n, len);
		free(buf);
		return -1;
	}

	// uint64 reply length
	checklen(buf, len, 8);
	uint64_t reply_len = read_uint64(b);
	b += 8;

	// reply
	checklen(buf, len, reply_len);
	char *reply_str = (char *)b;
	b += reply_len;

	// status
	int status = (int)read_uint32(b);

	reply->reply = strndup(reply_str, reply_len);
	reply->status = status;

	free(buf);
	return 0;
}

#undef checklen

int daemon_message_write(int fd, struct daemon_message *msg) {
	uint64_t payload_len = msg->payload == NULL ? 0 : strlen(msg->payload);
	uint32_t prompt_len = msg->opts.prompt == NULL ? 0 : (uint32_t)strlen(msg->opts.prompt);
	uint64_t len = 8 + payload_len + 4 + prompt_len + 1;

	uint8_t *buf = malloc(len);
	uint8_t *b = buf;
	if (buf == NULL) {
		perror("malloc");
		return -1;
	}

	uint8_t len_buf[8];
	write_uint64(len_buf, len);
	if (write(fd, len_buf, 8) < 0) {
		perror("write");
		free(buf);
		return -1;
	}

	write_uint64(b, payload_len);
	b += 8;

	memcpy(b, msg->payload, payload_len);
	b += payload_len;

	write_uint32(b, prompt_len);
	b += 4;

	memcpy(b, msg->opts.prompt, prompt_len);
	b += prompt_len;

	b[0] = !!msg->opts.insensitive << 0;

	if (write(fd, buf, len) < 0) {
		perror("write");
		free(buf);
		return -1;
	}

	free(buf);
	return 0;
}

int daemon_reply_write(int fd, struct daemon_reply *reply) {
	uint64_t reply_len = reply->reply == NULL ? 0 : strlen(reply->reply);
	uint64_t len = 8 + reply_len + 4;

	uint8_t *buf = malloc(len);
	uint8_t *b = buf;
	if (buf == NULL) {
		perror("malloc");
		return -1;
	}

	uint8_t len_buf[8];
	write_uint64(len_buf, len);
	if (write(fd, len_buf, 8) < 0) {
		perror("write");
		free(buf);
		return -1;
	}

	write_uint64(b, reply_len);
	b += 8;

	memcpy(b, reply->reply, reply_len);
	b += reply_len;

	write_uint32(b, (uint32_t)reply->status);
	b += 4;

	if (write(fd, buf, len) < 0) {
		perror("write");
		free(buf);
		return -1;
	}

	free(buf);
	return 0;
}
