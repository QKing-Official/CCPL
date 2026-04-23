
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "icon.h"

// The global UI

GtkWidget *left_view;     // build/debug
GtkWidget *right_view;    // runtime output
GtkWidget *file_label;
GtkWidget *icon_image;

GdkPixbuf *global_icon = NULL;

char selected_file[1024] = "";
char output_binary[1024] = "output";

// Append the text

static void append(GtkWidget *view, const char *msg) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, msg, -1);
}

// Stream output and run command to actually run the program

static void run_cmd(GtkWidget *view, const char *cmd) {

    append(view, "\n> ");
    append(view, cmd);
    append(view, "\n");

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        append(view, "Failed to run command\n");
        return;
    }

    char line[512];

    while (fgets(line, sizeof(line), fp)) {
        append(view, line);

        while (gtk_events_pending())
            gtk_main_iteration();
    }

    int status = pclose(fp);

    if (status != 0) {
        append(view, "\n[ERROR] command failed\n");
    }
}

// The filepicker to select you CCPL program

static void on_choose_file(GtkWidget *btn, gpointer data) {

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Select CCPL File",
        GTK_WINDOW(data),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL
    );

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);

        strcpy(selected_file,
               gtk_file_chooser_get_filename(chooser));

        gtk_label_set_text(GTK_LABEL(file_label),
                           selected_file);
    }

    gtk_widget_destroy(dialog);
}

// The build windows (on the left side).
// This also includes the execution of the compiler with build flags

static void on_build(GtkWidget *btn, gpointer data) {

    if (strlen(selected_file) == 0) {
        append(left_view, "No file selected\n");
        return;
    }

    char cmd[4096];

    snprintf(cmd, sizeof(cmd),
        "bash -c 'cd .. && ./ccpl-bin \"%s\" -o %s -a'", // -a to auto fetch dependecies and -o to give it the name of the source file
        selected_file,
        output_binary
    );

    append(left_view, "\n===== BUILD START =====\n");
    run_cmd(left_view, cmd);
    append(left_view, "\n===== BUILD END =====\n");
}

// The run section (on the rigtt side)
// This also includes the execution of the compiler with the run flags

static void on_run(GtkWidget *btn, gpointer data) {

    char cmd[2048];

    snprintf(cmd, sizeof(cmd),
    // Execute the actual program using the shell
        "bash -c 'cd .. && ./output 2>/dev/null'"
    );

    append(right_view, "\n===== RUN START =====\n");

    FILE *fp = popen(cmd, "r");

    if (!fp) {
        append(right_view, "Failed to run program\n");
        return;
    }

    char line[512];

    while (fgets(line, sizeof(line), fp)) {

        /* FILTER OUT noisy shell messages */
        if (strstr(line, "fire-and-forget")) continue;
        if (strstr(line, "shell")) continue;

        append(right_view, line);

        while (gtk_events_pending())
            gtk_main_iteration();
    }

    pclose(fp);

    append(right_view, "\n===== RUN END =====\n");
}
// The main loop and function
// Called on launch of program (ccpl-bin)

int main(int argc, char *argv[]) {

    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "CCPL GUI");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 650);

    // Icon

    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_write(loader, Icon_png, Icon_png_len, NULL);
    gdk_pixbuf_loader_close(loader, NULL);

    global_icon = gdk_pixbuf_loader_get_pixbuf(loader);

    if (global_icon) {

        gtk_window_set_icon(GTK_WINDOW(window), global_icon);

        GdkPixbuf *scaled =
            gdk_pixbuf_scale_simple(global_icon, 64, 64,
                                    GDK_INTERP_BILINEAR);

        icon_image = gtk_image_new_from_pixbuf(scaled);
    }

    // The main layout of the ui
    // I added a the icon for all users who cant see it in the navbar

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    if (icon_image)
        gtk_box_pack_start(GTK_BOX(vbox), icon_image, FALSE, FALSE, 5);

    file_label = gtk_label_new("No file selected");
    gtk_box_pack_start(GTK_BOX(vbox), file_label, FALSE, FALSE, 5);

    // Button
    // For building running and selecting your source file

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    GtkWidget *btn_file  = gtk_button_new_with_label("Open File");
    GtkWidget *btn_build = gtk_button_new_with_label("Build");
    GtkWidget *btn_run   = gtk_button_new_with_label("Run");

    gtk_box_pack_start(GTK_BOX(hbox), btn_file, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), btn_build, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), btn_run, FALSE, FALSE, 5);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    // The splìt view to ensure that the user sees what the build does, and what the program outputs

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

    GtkWidget *scroll_left  = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *scroll_right = gtk_scrolled_window_new(NULL, NULL);

    left_view  = gtk_text_view_new();
    right_view = gtk_text_view_new();

    gtk_text_view_set_editable(GTK_TEXT_VIEW(left_view), FALSE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(right_view), FALSE);

    gtk_container_add(GTK_CONTAINER(scroll_left), left_view);
    gtk_container_add(GTK_CONTAINER(scroll_right), right_view);

    gtk_paned_add1(GTK_PANED(paned), scroll_left);
    gtk_paned_add2(GTK_PANED(paned), scroll_right);

    // The split size
    // We want an almost even split
    // Not exact to piss people off
    // Lol
    gtk_paned_set_position(GTK_PANED(paned), 550);

    gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 5);

    // The signals for ui
    // Not used in actual backend

    g_signal_connect(btn_file, "clicked",
        G_CALLBACK(on_choose_file), window);

    g_signal_connect(btn_build, "clicked",
        G_CALLBACK(on_build), NULL);

    g_signal_connect(btn_run, "clicked",
        G_CALLBACK(on_run), NULL);

    g_signal_connect(window, "destroy",
        G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
// THE END!