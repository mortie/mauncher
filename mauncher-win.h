#ifndef MAUNCHER_WIN
#define MAUNCHER_WIN

#include <gtk/gtk.h>

struct mauncher_opts {
	gchar *prompt;
	gboolean insensitive;
} mauncher_opts;

void mauncher_win_run(
		GtkApplication *app, char *input, struct mauncher_opts opts,
		void (*callback)(const char *output, int status, void *data), void *data);

#endif
