#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include "mauncher-win.h"
#include "mauncher-ipc.h"
#include "sysutil.h"

struct daemon_conn;

struct daemon_ctx {
	GtkApplication *app;
	int sockfd;
	struct sockaddr_un addr;
	GIOChannel *channels[16];
};

struct daemon_conn {
	struct daemon_ctx *ctx;
	GIOChannel **channel;
	GSource *source;
};

struct daemon_invocation {
	struct daemon_conn *conn;
	struct daemon_message msg;
};

//static void daemon_free_channel(struct daemon_ctx *ctx, GIOChannel *channel) {
static void daemon_free_conn(struct daemon_conn *conn) {
	GError *err = NULL;
	g_io_channel_shutdown(*conn->channel, TRUE, &err);
	if (err != NULL) {
		fprintf(stderr, "Closing IO channel failed: %s\n", err->message);
		g_error_free(err);
	}

	g_io_channel_unref(*conn->channel);
	g_source_destroy(conn->source);
	g_source_unref(conn->source);

	*conn->channel = NULL;
}

static void win_callback(const char *output, int status, void *data) {
	struct daemon_invocation *invocation = (struct daemon_invocation *)data;

	struct daemon_reply reply = {
		.reply = (char *)output,
		.status = status,
	};

	daemon_reply_write(g_io_channel_unix_get_fd(*invocation->conn->channel), &reply);
	daemon_free_conn(invocation->conn);
	free(invocation->msg.payload);
	free(invocation->msg.opts.prompt);
	free(invocation->conn);
	free(invocation);
}

static gboolean on_data(GIOChannel *source, GIOCondition condition, void *data) {
	struct daemon_conn *conn = (struct daemon_conn *)data;

	if (condition & G_IO_IN) {
		struct daemon_invocation *invocation = malloc(sizeof(struct daemon_invocation));
		invocation->conn = conn;

		int fd = g_io_channel_unix_get_fd(source);

		if (daemon_message_read(fd, &invocation->msg) < 0) {
			daemon_free_conn(conn);
			return FALSE;
		}

		mauncher_win_run(
				conn->ctx->app, invocation->msg.payload, invocation->msg.opts,
				&win_callback, invocation);
	}

	if (condition & G_IO_HUP) {
		daemon_free_conn(conn);
		return FALSE;
	}

	return TRUE;
}

static gboolean on_connect(GIOChannel *source, GIOCondition condition, gpointer data) {
	struct daemon_ctx *ctx = (struct daemon_ctx *)data;

	socklen_t socklen = sizeof(ctx->addr);
	int fd = accept(ctx->sockfd, &ctx->addr, &socklen);
	if (fd < 0) {
		perror("accept");
		return TRUE;
	}

	GIOChannel **channel = NULL;
	for (size_t i = 0; i < sizeof(ctx->channels) / sizeof(*ctx->channels); ++i) {
		if (ctx->channels[i] == NULL) {
			channel = &ctx->channels[i];
			break;
		}
	}

	if (channel == NULL) {
		printf("Client attempted connection, but we're full!\n");
		close(fd);
		return TRUE;
	}

	*channel = g_io_channel_unix_new(fd);
	GSource *gsource = g_io_create_watch(*channel, G_IO_IN | G_IO_HUP);

	struct daemon_conn *conn = malloc(sizeof(*conn));
	conn->channel = channel;
	conn->source = gsource;
	conn->ctx = ctx;

	g_source_set_callback(gsource, (GSourceFunc)&on_data, conn, NULL);
	g_source_attach(conn->source, g_main_context_default());

	return TRUE;
}

static void activate(GtkApplication *app, gpointer data) {
	struct daemon_ctx *ctx = (struct daemon_ctx *)data;

	ctx->app = app;
	g_io_add_watch(g_io_channel_unix_new(ctx->sockfd), G_IO_IN, &on_connect, ctx);

	gtk_main();
}

static int daemon_main(int closefd) {
	chdir(g_get_home_dir());

	struct daemon_ctx ctx = { 0 };

	ctx.addr.sun_family = AF_UNIX;
	char *sockpath = string_concat((char *[]) { xdg_runtime_dir(), "/mauncher-daemon.sock", NULL });
	if (strlen(sockpath) >= sizeof(ctx.addr.sun_path)) {
		fprintf(stderr, "Unix socket path too long: %s\n", sockpath);
		free(sockpath);
		return EXIT_FAILURE;
	}

	strcpy(ctx.addr.sun_path, sockpath);
	free(sockpath);

	ctx.sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ctx.sockfd < 0) {
		perror(ctx.addr.sun_path);
		return EXIT_FAILURE;
	}

	if (bind(ctx.sockfd, (struct sockaddr *)&ctx.addr, sizeof(ctx.addr)) < 0) {
		if (errno == EADDRINUSE) {
			fprintf(stderr, "%s exists, removing it.\n", ctx.addr.sun_path);
			if (unlink(ctx.addr.sun_path) < 0) {
				perror(ctx.addr.sun_path);
				close(ctx.sockfd);
				return EXIT_FAILURE;
			}

			if (bind(ctx.sockfd, (struct sockaddr *)&ctx.addr, sizeof(ctx.addr)) < 0) {
				perror(ctx.addr.sun_path);
				close(ctx.sockfd);
			}
		} else {
			perror(ctx.addr.sun_path);
			close(ctx.sockfd);
			return EXIT_FAILURE;
		}
	}

	if (listen(ctx.sockfd, 2) < 0) {
		perror(ctx.addr.sun_path);
		close(ctx.sockfd);
		return EXIT_FAILURE;
	}

	if (closefd >= 0)
		close(closefd);

	GtkApplication *app = gtk_application_new("coffee.mort.mauncher", G_APPLICATION_NON_UNIQUE);
	g_signal_connect(app, "activate", G_CALLBACK(activate), &ctx);
	int status = g_application_run(G_APPLICATION(app), 0, NULL);
	g_object_unref(app);
	return status;
}

static int daemon_fork() {
	int fds[2];
	if (pipe(fds) < 0) {
		perror("pipe");
		return -1;
	}

	printf("forking\n");
	pid_t child = fork();
	if (child < 0) {
		perror("fork");
		close(fds[0]);
		close(fds[1]);
		return -1;
	}

	if (child == 0) {
		close(fds[0]);
		daemon(1, 0);
		exit(daemon_main(fds[1]));
	} else {
		close(fds[1]);
		char buf[1];

		// Just wait for the child to close the pipe
		if (read(fds[0], buf, 1) < 0) {
			perror("read");
			return -1;
		}
	}

	return 0;
}

struct opts {
	struct mauncher_win_opts winopts;
	gboolean daemon;
};

int main(int argc, char **argv) {
	struct opts opts = { 0 };

	GOptionEntry optents[] = {
		{
			"prompt", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &opts.winopts.prompt,
			"The prompt to be displayed left of the input field", NULL,
		}, {
			"insensitive", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &opts.winopts.insensitive,
			"Match case-insensitive", NULL,
		}, {
			"daemon", '\0', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &opts.daemon,
			"Spawn the mauncher daemon. This will be done automatically,\n"
			"                        "
			"but doing it explicitly on startup will make the first invocation faster.", NULL,
		},
		{ 0 },
	};

	GOptionContext *optctx = g_option_context_new(NULL);
	g_option_context_add_main_entries(optctx, optents, NULL);
	GError *err = NULL;
	g_option_context_parse(optctx, &argc, &argv, &err);
	g_option_context_free(optctx);
	if (err != NULL) {
		fprintf(stderr, "%s\n", err->message);
		g_error_free(err);
		return EXIT_FAILURE;
	}

	if (opts.daemon)
		return daemon_main(-1);

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	char *sockpath = string_concat((char *[]) { xdg_runtime_dir(), "/mauncher-daemon.sock", NULL });
	if (strlen(sockpath) >= sizeof(addr.sun_path)) {
		fprintf(stderr, "Unix socket path too long: %s\n", sockpath);
		free(sockpath);
		return EXIT_FAILURE;
	}

	strcpy(addr.sun_path, sockpath);
	free(sockpath);

	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror(addr.sun_path);
		printf("socket\n");
		return EXIT_FAILURE;
	}

	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		if (errno == ENOENT || errno == ECONNREFUSED) {
			if (daemon_fork() < 0) {
				close(sockfd);
				return EXIT_FAILURE;
			}

			if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
				perror(addr.sun_path);
				close(sockfd);
				return EXIT_FAILURE;
			}
		} else {
			perror(addr.sun_path);
			close(sockfd);
			return EXIT_FAILURE;
		}
	}

	struct daemon_message msg = { 0 };
	size_t payload_len;
	msg.payload = read_all(STDIN_FILENO, &payload_len);
	memcpy(&msg.opts, &opts.winopts, sizeof(msg.opts));

	if (daemon_message_write(sockfd, &msg) < 0) {
		free(msg.payload);
		close(sockfd);
		return EXIT_FAILURE;
	}

	free(msg.payload);

	struct daemon_reply reply;
	if (daemon_reply_read(sockfd, &reply) < 0) {
		close(sockfd);
		return EXIT_FAILURE;
	}

	close(sockfd);

	if (reply.status == EXIT_SUCCESS)
		puts(reply.reply);
	free(reply.reply);

	return reply.status;
}
