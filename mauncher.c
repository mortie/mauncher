#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>

#include "mauncher-win.h"
#include "sysutil.h"

struct context {
	GtkApplication *app;
	struct mauncher_opts opts;
	char *input;
	int status;
};

static void callback(const char *output, int status, void *data) {
	struct context *ctx = (struct context *)data;

	if (output != NULL)
		puts(output);

	free(ctx->input);
	ctx->status = status;
	g_application_quit(G_APPLICATION(ctx->app));
}

static void activate(GtkApplication *app, void *data) {
	struct context *ctx = (struct context *)data;

	size_t len;
	ctx->input = read_all(fileno(stdin), &len);
	mauncher_win_run(app, ctx->input, ctx->opts, callback, (void *)ctx);
}

int main(int argc, char **argv) {
	struct context ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.status = EXIT_SUCCESS;

	GOptionEntry optents[] = {
		{
			"prompt", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &ctx.opts.prompt,
			"The prompt to be displayed left of the input field", NULL,
		}, {
			"insensitive", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &ctx.opts.insensitive,
			"Match case-insensitive", NULL,
		},
		{ 0 },
	};

	ctx.app = gtk_application_new("coffee.mort.mauncher", G_APPLICATION_NON_UNIQUE);
	g_application_add_main_option_entries(G_APPLICATION(ctx.app), optents);

	g_signal_connect(ctx.app, "activate", G_CALLBACK(activate), &ctx);
	int status = g_application_run(G_APPLICATION(ctx.app), argc, argv);
	g_object_unref(ctx.app);

	if (status)
		return status;
	else
		return ctx.status;
}
