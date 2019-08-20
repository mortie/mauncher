#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <stdio.h>
#include <stdlib.h>

struct context {
	GtkApplication *app;
	GtkWidget *container;
	char *data;
	size_t *strs;
	size_t strs_len;
	ssize_t cursor;
	ssize_t view;

	struct opts {
		gchar *prompt;
	} opts;
};

static int strs_compare(const void *aptr, const void *bptr, void *data) {
	struct context *ctx = (struct context *)data;
	int *a = (int *)aptr;
	int *b = (int *)bptr;
	return strcmp(ctx->data + *a, ctx->data + *b);
}

static ssize_t lookup(const char *prefix, struct context *ctx) {
	if (prefix[0] == '\0')
		return 0;
	if (ctx->strs_len == 0)
		return -1;

	size_t pfxlen = strlen(prefix);
	size_t start = 0;
	size_t end = ctx->strs_len - 1;
	size_t index;
	int logcount = 0;
	while (1) {
		logcount += 1;
		index = start + (end - start) / 2;
		char *str = ctx->data + ctx->strs[index];

		int ret = strncmp(str, prefix, pfxlen);
		if (ret == 0) {
			if (index > 0 && strncmp(ctx->data + ctx->strs[index - 1], prefix, pfxlen) == 0) {
				end = index;
			} else {
				break;
			}
		} else if (ret < 0) {
			start = index;
		} else {
			end = index;
		}

		if (end <= start + 1)
			return -1;
	}

	return (ssize_t)index;
}

static int read_input(struct context *ctx, FILE *f) {
	size_t data_size = 1024;
	size_t strs_size = 32;
	size_t data_idx = 0;
	size_t str_start = 0;

	ctx->data = malloc(data_size);
	ctx->strs = malloc(strs_size * sizeof(*ctx->strs));

	char buf[1024];
	while (1) {
		ssize_t n = fread(buf, 1, sizeof(buf), f);
		if (n < 0) {
			perror("fread()");
			free(ctx->data);
			free(ctx->strs);
			return -1;
		}

		if (n == 0)
			break;

		for (ssize_t i = 0; i < n; ++i) {
			char c = buf[i];
			if (c == '\n') {
				ctx->data[data_idx++] = '\0';
				ctx->strs[ctx->strs_len++] = str_start;
				str_start = data_idx;

				if (ctx->strs_len >= strs_size) {
					strs_size *= 2;
					ctx->strs = realloc(ctx->strs, strs_size * sizeof(*ctx->strs));
				}
			} else {
				ctx->data[data_idx++] = c;
			}

			if (data_idx >= data_size) {
				data_size *= 2;
				ctx->data = realloc(ctx->data, data_size);
			}
		}
	}

	qsort_r(ctx->strs, ctx->strs_len, sizeof(*ctx->strs), strs_compare, ctx);

	return 0;
}

void draw_list(struct context *ctx) {
	// Clear out existing children
	GList *iter, *children = gtk_container_get_children(GTK_CONTAINER(ctx->container));
	for(iter = children; iter != NULL; iter = g_list_next(iter))
		gtk_widget_destroy(GTK_WIDGET(iter->data));
	g_list_free(children);

	// Draw new children
	if (ctx->cursor >= 0) {
		for (ssize_t i = ctx->view; i < ctx->view + 30; ++i) {
			if (i >= (ssize_t)ctx->strs_len)
				break;

			GtkWidget *label;
			if (i == ctx->cursor) {
				char *str = g_markup_printf_escaped(
						"<span foreground=\"red\">%s</span>", ctx->data + ctx->strs[i]);
				label = gtk_label_new("");
				gtk_label_set_markup(GTK_LABEL(label), str);
			} else {
				label = gtk_label_new(ctx->data + ctx->strs[i]);
			}
			gtk_container_add(GTK_CONTAINER(ctx->container), label);
		}
	}

	gtk_widget_show_all(ctx->container);
}

static gboolean on_enter(GtkEntry *entry, void *data) {
	struct context *ctx = (struct context *)data;

	if (ctx->cursor >= 0 && ctx->cursor < (ssize_t)ctx->strs_len)
		puts(ctx->data + ctx->strs[ctx->cursor]);
	else
		puts(gtk_entry_get_text(entry));
	g_application_quit(G_APPLICATION(ctx->app));
	return FALSE;
}

static gboolean on_keyboard(GtkWidget *widget, GdkEventKey *event, void *data) {
	struct context *ctx = (struct context *)data;

	if (event->keyval == GDK_KEY_Escape) {
		g_application_quit(G_APPLICATION(ctx->app));
	} else if (event->keyval == GDK_KEY_Left && ctx->cursor >= 0) {
		if (ctx->cursor > 0)
			ctx->cursor -= 1;
		if (ctx->cursor < ctx->view)
			ctx->view = ctx->cursor;
		draw_list(ctx);
	} else if (event->keyval == GDK_KEY_Right && ctx->cursor >= 0) {
		if (ctx->cursor < (ssize_t)ctx->strs_len - 1)
			ctx->cursor += 1;
		draw_list(ctx);
	}
	return FALSE;
}

static gboolean on_change(GtkEditable *editable, void *data) {
	struct context *ctx = (struct context *)data;

	ctx->cursor = lookup(gtk_entry_get_text(GTK_ENTRY(editable)), ctx);
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

	gtk_window_set_title(GTK_WINDOW(win), "Mauncher");
	gtk_window_set_decorated(GTK_WINDOW(win), FALSE);

	GdkRectangle geometry;
	GdkMonitor *mon = get_monitor(gdk_display_get_default());
	gdk_monitor_get_geometry(mon, &geometry);
	gtk_window_set_default_size(GTK_WINDOW(win), geometry.width, 24);

	gtk_layer_init_for_window(GTK_WINDOW(win));
	gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_TOP);
	gtk_layer_set_monitor(GTK_WINDOW(win), mon);
	gtk_layer_set_keyboard_interactivity(GTK_WINDOW(win), TRUE);

	gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

	/*
	 * Populate window
	 */

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_add(GTK_CONTAINER(win), box);

	if (ctx->opts.prompt) {
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

	draw_list(ctx);
	gtk_widget_show_all(win);
	gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
	struct context ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.cursor = 0;
	ctx.view = 0;

	GOptionEntry optents[] = {
		{
			"prompt", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &ctx.opts.prompt,
			"The prompt to be displayed left of the input field", NULL,
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
	return status;
}
