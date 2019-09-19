#ifndef MAUNCHER_IPC_H
#define MAUNCHER_IPC_H

#include "mauncher-win.h"

struct daemon_message {
	char *payload;
	struct mauncher_win_opts opts;
};

struct daemon_reply {
	char *reply;
	int status;
};

int daemon_message_read(int fd, struct daemon_message *msg);
int daemon_reply_read(int fd, struct daemon_reply *reply);
int daemon_message_write(int fd, struct daemon_message *msg);
int daemon_reply_write(int fd, struct daemon_reply *reply);

#endif
