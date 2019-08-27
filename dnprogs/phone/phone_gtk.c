/*
 * phone_gtk.c
 *
 * This is the stuff that GLADE didn't build for me.
 */
#include <gtk/gtk.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "phone.h"
#include "backend.h"
#include "gtkphonesrc.h"

extern GtkWidget *MainWindow;

// Callback routines.
static void add_new_caller(int, int, char *, fd_callback);
static void show_error(int level, char *msg);
static void delete_caller(int);
static void quit(void);
static void write_text(char *name, char *msg);
static void hold_window(int held, char *name);
static int  get_fds(struct fd_list *fds);
static void answer_caller(int int_fd, int out_fd);
static void lost_server(void);
static void open_display_window(char *);
static void display_line(char *text);

void set_widget_state(int online);
void set_dialled_state(int dialled);

#define MAX_WINDOWS 6

// All the information about a user. userinfo[0] is the local user.
static struct user
{
    GtkText   *widget; /* Text widget for the user */
    GtkLabel  *label;  /* Label for the user */
    GtkBox    *box;    /* packing box for text and scrollbar */
    int         fd;
    int         out_fd;
    gint        fd_tag;
    char        name[64];
    int         held; // 0=not held, 1=held by me, 2=held by remote
    fd_callback fd_callback;
} userinfo[MAX_WINDOWS];
static int num_users=1;
static int num_connected_users=1;
static int local_sock;
static gint localsock_tag;
GtkWidget *Display_Widget = NULL;

void gtkphone_backend_init(void)
{
    struct  callback_routines cr;

    local_sock = init_backend();
    if (local_sock == -1)
    {
	fprintf(stderr, "The phone server is not running, please contact your system administrator\n");
	exit(1);
    }
    
    localsock_tag = gdk_input_add(local_sock,
				  GDK_INPUT_READ,
				  (GdkInputFunction)localsock_callback,
				  (gpointer)local_sock);
    

    cr.new_caller    = add_new_caller;
    cr.delete_caller = delete_caller;
    cr.show_error    = show_error;
    cr.write_text    = write_text;
    cr.get_fds       = get_fds;
    cr.answer        = answer_caller;
    cr.hold          = hold_window;
    cr.lost_server   = lost_server;
    cr.open_display_window = open_display_window;
    cr.display_line  = display_line;
    cr.quit          = quit;
    register_callbacks(&cr);

    userinfo[0].widget = GTK_TEXT(get_widget(MainWindow, "LocalText"));
    userinfo[0].label  = GTK_LABEL(get_widget(MainWindow, "LocalTitle"));
    strcpy(userinfo[0].name, get_local_name());
    gtk_label_set_text(userinfo[0].label, userinfo[0].name);

    set_widget_state(0); // Offline
    set_dialled_state(0);
}

void new_talk_window(int num, char *name)
{
    GtkWidget *label;
    GtkWidget *text;
    GtkWidget *box = get_widget(MainWindow, "talkbox");
    GtkWidget *hbox1;
    GtkWidget *vscrollbar;

    label = gtk_label_new (name);
    gtk_widget_show (GTK_WIDGET(label));
    gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET(label), FALSE, FALSE, 0);


    // A box to pack the scrollbar into
    hbox1 = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hbox1);
    gtk_box_pack_start (GTK_BOX (box), hbox1, TRUE, TRUE, 0);

    text = gtk_text_new (NULL, NULL);
    gtk_widget_show (GTK_WIDGET(text));
    gtk_box_pack_start (GTK_BOX (hbox1), GTK_WIDGET(text), TRUE, TRUE, 0);
    gtk_text_set_editable (GTK_TEXT (text), FALSE);

    vscrollbar = gtk_vscrollbar_new (GTK_TEXT(text)->vadj);
    gtk_box_pack_start(GTK_BOX(hbox1), vscrollbar, FALSE, FALSE, 0);
    gtk_widget_show (vscrollbar);
    
    userinfo[num].widget = GTK_TEXT(text);
    userinfo[num].label  = GTK_LABEL(label);
    userinfo[num].box    = GTK_BOX(hbox1);
}

/* got some text from GTK - write it into the widget and send to users */
void write_typed_text(char *text)
{
    int i;

    if (num_connected_users > 1)
    {
	// Send it to connected users.
	for (i=1; i<num_connected_users; i++)
	{
	    send_data(userinfo[i].out_fd, text, strlen(text));
	}
	// Add it to the text widget (done last because it changes CRs to LFs)
	write_text(userinfo[0].name, text);
    }
}

// Enable/disable menu/toolbar widgets accroding to our online state.
void set_widget_state(int online)
{
    if (online)
    {
	gtk_widget_set_sensitive(get_widget(MainWindow, "Hold"), 1);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Hold_button"), 1);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Facsimile_button"), 1);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Facsimile"), 1);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Hangup"), 1);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Hangup_button"), 1);
    }
    else
    {
	gtk_widget_set_sensitive(get_widget(MainWindow, "Hold"), 0);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Hold_button"), 0);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Facsimile_button"), 0);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Facsimile"), 0);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Hangup"), 0);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Hangup_button"), 0);
    }
}

// Enable the buttons for answering/rejecting a call
void set_dialled_state(int dialled)
{
    if (dialled)
    {
	gtk_widget_set_sensitive(get_widget(MainWindow, "Answer"), 1);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Answer_button"), 1);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Reject"), 1);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Reject_button"), 1);
    }
    else
    {
	gtk_widget_set_sensitive(get_widget(MainWindow, "Answer"), 0);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Answer_button"), 0);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Reject"), 0);
	gtk_widget_set_sensitive(get_widget(MainWindow, "Reject_button"), 0);
    }
}

/* ------------------------------------------------------------------------- */
/*                Routines that can be called from the back-end              */
/* ------------------------------------------------------------------------- */
static void add_new_caller(int in_fd, int out_fd, char *name, fd_callback fdc)
{
    userinfo[num_users].fd          = in_fd;
    userinfo[num_users].out_fd      = out_fd;
    userinfo[num_users].fd_callback = fdc;
    strcpy(userinfo[num_users].name, name);

    userinfo[num_users].fd_tag = gdk_input_add(in_fd,
					       GDK_INPUT_READ,
					       (GdkInputFunction)fdc,
					       (gpointer)in_fd);
    
    // Only create the window if the user has been answered.
    if (out_fd != -1) 
    {
	new_talk_window(num_users, name);
    }
    else
    {
	userinfo[num_users].widget = NULL;
	userinfo[num_users].label = NULL;
	set_dialled_state(1);
    }
    num_users++;
}


static void show_error(int level, char *msg)
{
    // Ring the bell if the first char was a 7 (BEL)
    if (msg[0] == 7)
    {
	msg++;
	gdk_beep();
    }

    // If level>0 then create a dial box for the message
    if (level)
    {
	GtkWidget *textlabel;
	GtkWidget *dialog;
	GtkWidget *box;
	
	dialog = create_MessageDialog();
	box = get_widget(dialog, "vbox2");

	textlabel = gtk_label_new (msg);
	gtk_widget_show (textlabel);
	gtk_box_pack_start (GTK_BOX (box), textlabel, FALSE, FALSE, 0);
	gtk_window_set_transient_for(GTK_WINDOW(dialog),
				     GTK_WINDOW(MainWindow));
	gtk_grab_add(dialog);
	gtk_widget_show (dialog);

    }
    else
    {
	GtkWidget *sb = get_widget(MainWindow, "statusbar");
	gtk_statusbar_pop(GTK_STATUSBAR(sb), 1);
	gtk_statusbar_push(GTK_STATUSBAR(sb), 1, msg);
    }
}

static void write_text(char *name, char *msg)
{
    int i,j;
    
    for (i=0; i<num_users; i++)
    {
	if (strcmp(name, userinfo[i].name) == 0)
	{
	    int x,y;
	   
	    for (j=0; j<strlen(msg); j++)
	    {
		if (msg[j] == '\r') msg[j] = '\n';
		if (isprint(msg[j]) || msg[j] == '\n')
		{
		    gtk_text_insert( userinfo[i].widget,
				     NULL,
				     NULL,
				     NULL,
				     &msg[j],
				     1 );
		}
		
		if (msg[j] == '\014') // Clear window
		{
		    gtk_text_backward_delete(userinfo[i].widget,
					     gtk_text_get_length(userinfo[i].widget));
		}
		
		if (msg[j] == 127) // Backspace
		{
		    gtk_text_backward_delete(userinfo[i].widget, 1);
		}
	    }
	}
    }
}

static void lost_server()
{
    gdk_input_remove(localsock_tag);
    
    show_error(1, "phoned server has died");
}


static void quit()
{
    int i;
/*
 * Close any DECnet connections
 */
    for (i=1; i<num_users; i++)
    {
	close_connection(userinfo[i].out_fd); // Sends hangup.
	close(userinfo[i].fd);
	close(userinfo[i].out_fd);
    }
    exit(0);
}

static void delete_caller(int fd)
{
    int i,j;
    int done = 0;

    for (i=1; i<num_users && !done; i++)
    {
	if (userinfo[i].fd == fd ||
	    userinfo[i].out_fd == fd)
	{
	    close(fd);
	    close(userinfo[i].out_fd);
	    if (userinfo[i].widget)
	    {
		gtk_object_destroy(GTK_OBJECT(userinfo[i].widget));
		gtk_object_destroy(GTK_OBJECT(userinfo[i].label));
		gtk_object_destroy(GTK_OBJECT(userinfo[i].box));
		gdk_input_remove(userinfo[i].fd_tag);
		userinfo[i].widget = NULL;
		num_connected_users--;
	    }
	    
	    // Remove it from the list
	    if (i != num_users)
	    {
		for (j=num_users-1; j>i; j--)
		{
		    userinfo[j-1] = userinfo[j];
		}
	    }
	    num_users--;
	    done = 1;
	    break;
	}
    }
    if (num_connected_users == 1)
	set_widget_state(0); // Just us ... Offline

// Is there still someone dialling us ?
    if (num_users != num_connected_users)
	set_dialled_state(1);
    else
	set_dialled_state(0);
}

// Update the window title
static void draw_window_decorations(int win)
{
    char title[255];

    strcpy(title, userinfo[win].name);
    if (userinfo[win].held)
    {
	if (userinfo[win].held == 2)
	    sprintf(title, "(YOU HAVE HELD) %s", userinfo[win].name);
	else
	    sprintf(title, "%s (HAS YOU HELD)", userinfo[win].name);
    }
    gtk_label_set_text(userinfo[win].label, title);

}

static void hold_window(int held, char *name)
{
    int i;
    int y,x;

    userinfo[0].held = held; // Also hold main window
    
    for (i=0; i<num_users; i++)
    {
	if (strcmp(name, userinfo[i].name) == 0)
	{
	    userinfo[i].held = held;
	    draw_window_decorations(i);
	}
    }
}

static int get_fds(struct fd_list *fds)
{
    int i;
    for (i=1; i<num_users; i++)
    {
	fds[i-1].out_fd       = userinfo[i].out_fd;
	fds[i-1].in_fd        = userinfo[i].fd;
	fds[i-1].remote_name  = userinfo[i].name;
    }
    return num_users-1;
}

static void answer_caller(int in_fd, int out_fd)
{
    int i;

    for (i=1; i<num_users; i++)
    {
	if (userinfo[i].fd == in_fd)
	{
	    userinfo[i].out_fd = out_fd;
	    new_talk_window(i, userinfo[i].name);
	    num_connected_users++;
	    set_widget_state(1); // Online
	    break;
	}
    }
    
    // Is there still someone dialling us ?
    if (num_users != num_connected_users)
	set_dialled_state(1);
    else
	set_dialled_state(0);
}

/* Two routines dealing with the full-screen display window
   for HELP and DIRECTORY
 */
static void open_display_window(char *title)
{
    if (Display_Widget) return;
    Display_Widget = create_DisplayDialog(title);
    gtk_widget_show (Display_Widget);
    gtk_window_set_transient_for(GTK_WINDOW(Display_Widget),
				 GTK_WINDOW(MainWindow));
	
}

static void display_line(char *text)
{
    GtkWidget *textlabel;
    GtkWidget *box = get_widget(Display_Widget, "vbox2");

    if (!Display_Widget) return;
    
    textlabel = gtk_label_new (text);
    gtk_label_set_justify(GTK_LABEL(textlabel), GTK_JUSTIFY_LEFT);
    gtk_widget_show (textlabel);
    gtk_box_pack_start (GTK_BOX (box), textlabel, FALSE, FALSE, 0);
    
}

