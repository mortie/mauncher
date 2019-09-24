#include "mauncher-win.h"

#include <stdlib.h>
#include <string.h>
#include <gdk/gdkwayland.h>
#include <gtk-layer-shell/gtk-layer-shell.h>

#include "sysutil.h"

struct win {
	GtkApplication *app;
	char *input;
	void *data;
	void (*callback)(const char *output, int status, void *data);

	GtkWidget *win;
	GtkWidget *container;
	int status;
	char **strs;
	size_t strs_len;
	char **cursor;
	char **view;

	gint entry_padding;
	gint entry_min_width;

	struct mauncher_win_opts opts;
};

static int str_compare(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}

static int str_case_compare(const void *a, const void *b) {
	return strcasecmp(*(const char **)a, *(const char **)b);
}

static void draw_list(struct win *win) {
	// Clear out existing children
	GList *iter, *children = gtk_container_get_children(GTK_CONTAINER(win->container));
	for(iter = children; iter != NULL; iter = g_list_next(iter))
		gtk_widget_destroy(GTK_WIDGET(iter->data));
	g_list_free(children);

	// Draw new children
	if (win->cursor != NULL) {
		for (char **i = win->view; i < win->view + 30 && *i != NULL; ++i) {
			GtkWidget *label;
			if (i == win->cursor) {
				char *str = g_markup_printf_escaped(
						"<span foreground=\"red\">%s</span>", *i);
				label = gtk_label_new("");
				gtk_label_set_markup(GTK_LABEL(label), str);
			} else {
				label = gtk_label_new(*i);
			}
			gtk_container_add(GTK_CONTAINER(win->container), label);
		}
	}

	gtk_widget_show_all(win->container);
}

static void cleanup(struct win *win) {
	free(win->strs);
	gtk_widget_destroy(win->win);
	free(win);
}

static gboolean on_enter(GtkEntry *entry, void *data) {
	struct win *win = (struct win *)data;

	if (win->cursor != NULL)
		win->callback(*win->cursor, win->status, win->data);
	else
		win->callback(gtk_entry_get_text(entry), win->status, win->data);

	cleanup(win);

	return FALSE;
}

static gboolean on_keyboard(GtkWidget *widget, GdkEventKey *event, void *data) {
	struct win *win = (struct win *)data;

	if (event->keyval == GDK_KEY_Escape) {
		win->status = EXIT_FAILURE;
		win->callback(NULL, win->status, win->data);
		cleanup(win);
	} else if (event->keyval == GDK_KEY_Left && win->cursor > win->view) {
		if (win->cursor > win->strs)
			win->cursor -= 1;
		draw_list(win);
	} else if (event->keyval == GDK_KEY_Right && win->cursor >= win->strs) {
		if (win->cursor[1] != NULL)
			win->cursor += 1;
		draw_list(win);
	}
	return FALSE;
}

static gboolean on_change(GtkEditable *editable, void *data) {
	struct win *win = (struct win *)data;

	const gchar *text = gtk_entry_get_text(GTK_ENTRY(editable));

	PangoLayout *layout = gtk_widget_create_pango_layout(GTK_WIDGET(editable), text);
	PangoRectangle rect;
	pango_layout_get_extents(layout, NULL, &rect);
	pango_extents_to_pixels(NULL, &rect);
	g_object_unref(layout);
	if (rect.width + win->entry_padding > win->entry_min_width)
		gtk_widget_set_size_request(GTK_WIDGET(editable), rect.width + win->entry_padding, -1);
	else
		gtk_widget_set_size_request(GTK_WIDGET(editable), win->entry_min_width, -1);

	int (*cmp)(const char *a, const char *b, size_t n) =
		win->opts.insensitive ? &strncasecmp : &strncmp;
	win->cursor = bs_lookup(text, win->strs, win->strs_len, cmp);
	win->view = win->cursor;
	draw_list(win);
	return FALSE;
}

void mauncher_win_run(
		GtkApplication *app, char *input, struct mauncher_win_opts opts,
		void (*callback)(const char *output, int status, void *data), void *data) {

	struct win *win = malloc(sizeof(*win));
	win->app = app;
	win->input = input;
	win->data = data;
	win->callback = callback;
	win->status = EXIT_SUCCESS;
	win->opts = opts;

	win->strs = string_split(win->input, '\n', &win->strs_len);
	if (win->opts.insensitive)
		qsort(win->strs, win->strs_len, sizeof(*win->strs), &str_case_compare);
	else
		qsort(win->strs, win->strs_len, sizeof(*win->strs), &str_compare);

	win->cursor = win->strs;
	win->view = win->strs;

	win->win = gtk_application_window_new(win->app);

	gtk_window_set_title(GTK_WINDOW(win->win), "Mauncher");
	gtk_window_set_decorated(GTK_WINDOW(win->win), FALSE);

	gtk_layer_init_for_window(GTK_WINDOW(win->win));
	gtk_layer_set_layer(GTK_WINDOW(win->win), GTK_LAYER_SHELL_LAYER_OVERLAY);
	gtk_layer_set_keyboard_interactivity(GTK_WINDOW(win->win), TRUE);

	gtk_layer_set_anchor(GTK_WINDOW(win->win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(win->win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(win->win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

	/*
	 * Populate window
	 */

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_container_add(GTK_CONTAINER(win->win), box);

	if (win->opts.prompt && win->opts.prompt[0] != '\0') {
		gtk_widget_set_margin_start(box, 8);

		// Only show one line
		char *c = win->opts.prompt;
		while (*c) {
			if (*c == '\n') {
				*c = '\0';
				break;
			}
			c += 1;
		}

		GtkWidget *prompt = gtk_label_new(win->opts.prompt);
		gtk_container_add(GTK_CONTAINER(box), prompt);
	}

	GtkWidget *in = gtk_entry_new();
	gtk_entry_set_placeholder_text(GTK_ENTRY(in), "Search...");
	g_signal_connect(in, "activate", G_CALLBACK(on_enter), win);
	g_signal_connect(in, "key-press-event", G_CALLBACK(on_keyboard), win);
	g_signal_connect(in, "changed", G_CALLBACK(on_change), win);
	gtk_widget_grab_focus(in);
	gtk_container_add(GTK_CONTAINER(box), in);

	win->container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add(GTK_CONTAINER(box), win->container);

	gtk_widget_show_all(win->win);
	gtk_window_present(GTK_WINDOW(win->win));

	win->entry_padding = 24;
	gtk_widget_get_preferred_width(in, NULL, &win->entry_min_width);

	draw_list(win);
}
