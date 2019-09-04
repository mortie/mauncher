#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <stdio.h>
#include <stdlib.h>

#include "sysutil.h"

struct context {
	GtkApplication *app;
	GtkWidget *container;
	char *data;
	char **strs;
	size_t strs_len;
	char **cursor;
	char **view;
	int status;

	struct opts {
		gchar *prompt;
		gboolean insensitive;
	} opts;
};

int str_compare(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}

int str_case_compare(const void *a, const void *b) {
	return strcasecmp(*(const char **)a, *(const char **)b);
}

static int read_input(struct context *ctx, FILE *f) {
	size_t len;
	char *str = ctx->data = read_all(fileno(f), &len);
	ctx->strs = string_split(ctx->data, '\n', &ctx->strs_len);

	if (ctx->opts.insensitive)
		qsort(ctx->strs, ctx->strs_len, sizeof(*ctx->strs), &str_case_compare);
	else
		qsort(ctx->strs, ctx->strs_len, sizeof(*ctx->strs), &str_compare);

	return 0;
}

void draw_list(struct context *ctx) {
	// Clear out existing children
	GList *iter, *children = gtk_container_get_children(GTK_CONTAINER(ctx->container));
	for(iter = children; iter != NULL; iter = g_list_next(iter))
		gtk_widget_destroy(GTK_WIDGET(iter->data));
	g_list_free(children);

	// Draw new children
	if (ctx->cursor != NULL) {
		for (char **i = ctx->view; i < ctx->view + 30 && *i != NULL; ++i) {
			GtkWidget *label;
			if (i == ctx->cursor) {
				char *str = g_markup_printf_escaped(
						"<span foreground=\"red\">%s</span>", *i);
				label = gtk_label_new("");
				gtk_label_set_markup(GTK_LABEL(label), str);
			} else {
				label = gtk_label_new(*i);
			}
			gtk_container_add(GTK_CONTAINER(ctx->container), label);
		}
	}

	gtk_widget_show_all(ctx->container);
}

static gboolean on_enter(GtkEntry *entry, void *data) {
	struct context *ctx = (struct context *)data;

	if (ctx->cursor != NULL)
		puts(*ctx->cursor);
	else
		puts(gtk_entry_get_text(entry));

	g_application_quit(G_APPLICATION(ctx->app));
	return FALSE;
}

static gboolean on_keyboard(GtkWidget *widget, GdkEventKey *event, void *data) {
	struct context *ctx = (struct context *)data;

	if (event->keyval == GDK_KEY_Escape) {
		ctx->status = EXIT_FAILURE;
		g_application_quit(G_APPLICATION(ctx->app));
	} else if (event->keyval == GDK_KEY_Left && ctx->cursor > ctx->view) {
		if (ctx->cursor > ctx->strs)
			ctx->cursor -= 1;
		draw_list(ctx);
	} else if (event->keyval == GDK_KEY_Right && ctx->cursor >= ctx->strs) {
		if (ctx->cursor[1] != NULL)
			ctx->cursor += 1;
		draw_list(ctx);
	}
	return FALSE;
}

static gboolean on_change(GtkEditable *editable, void *data) {
	struct context *ctx = (struct context *)data;

	int (*cmp)(const char *a, const char *b, size_t n) =
		ctx->opts.insensitive ? &strncasecmp : &strncmp;
	ctx->cursor = bs_lookup(
			gtk_entry_get_text(GTK_ENTRY(editable)), ctx->strs, ctx->strs_len, cmp);
	ctx->view = ctx->cursor;
	draw_list(ctx);
	return FALSE;
}

static GdkMonitor *get_monitor(GdkDisplay *disp) {
	int nmons = gdk_display_get_n_monitors(disp);
	GdkMonitor *mon = NULL;

	for (int i = 0; i < nmons; ++i) {
		GdkMonitor *m = gdk_display_get_monitor(disp, i);
		if (mon == NULL)
			mon = m;

		if (gdk_monitor_is_primary(m)) {
			mon = m;
			break;
		}
	}

	return mon;
}

static void activate(GtkApplication *app, void *data) {
	struct context *ctx = (struct context *)data;

	GtkWidget *win = gtk_application_window_new(ctx->app);

	if (read_input(ctx, stdin) < 0)
		g_application_quit(G_APPLICATION(ctx->app));
	ctx->cursor = ctx->strs;
	ctx->view = ctx->strs;

	gtk_window_set_title(GTK_WINDOW(win), "Mauncher");
	gtk_window_set_decorated(GTK_WINDOW(win), FALSE);

	GdkRectangle geometry;
	GdkMonitor *mon = get_monitor(gdk_display_get_default());
	gdk_monitor_get_geometry(mon, &geometry);
	gtk_window_set_default_size(GTK_WINDOW(win), geometry.width, 24);

	gtk_layer_init_for_window(GTK_WINDOW(win));
	gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
	gtk_layer_set_monitor(GTK_WINDOW(win), mon);
	gtk_layer_set_keyboard_interactivity(GTK_WINDOW(win), TRUE);

	gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

	/*
	 * Populate window
	 */

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_container_add(GTK_CONTAINER(win), box);

	if (ctx->opts.prompt) {
		gtk_widget_set_margin_start(box, 8);

		// Only show one line
		char *c = ctx->opts.prompt;
		while (*c) {
			if (*c == '\n') {
				*c = '\0';
				break;
			}
			c += 1;
		}

		GtkWidget *prompt = gtk_label_new(ctx->opts.prompt);
		gtk_container_add(GTK_CONTAINER(box), prompt);
	}

	GtkWidget *input = gtk_entry_new();
	gtk_entry_set_placeholder_text(GTK_ENTRY(input), "Search...");
	g_signal_connect(input, "activate", G_CALLBACK(on_enter), ctx);
	g_signal_connect(input, "key-press-event", G_CALLBACK(on_keyboard), ctx);
	g_signal_connect(input, "changed", G_CALLBACK(on_change), ctx);
	gtk_widget_grab_focus(input);
	gtk_container_add(GTK_CONTAINER(box), input);

	ctx->container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add(GTK_CONTAINER(box), ctx->container);

	gtk_widget_show_all(win);
	gtk_window_present(GTK_WINDOW(win));

	draw_list(ctx);
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

	free(ctx.data);
	free(ctx.strs);
	if (status)
		return status;
	else
		return ctx.status;
}
