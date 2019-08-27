/*
 * phone.c
 * This is the 'phone' client program run by users.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "phone_ncurses.h"
#ifdef HAVE_GTK
extern int gtk_phone_init(int argc, char *argv[]);
extern int gtk_phone_run(char *);
#endif

// Print a usage message.
static void usage(char *name, FILE *f)
{
    fprintf(f, "\nusage: %s [OPTIONS] infile", name);

    fprintf(f, "\n\n");
    fprintf(f, " Options\n");
    fprintf(f, "  -? -h        display this help message\n");
    fprintf(f, "  -V           show version number.\n");
    fprintf(f, "  -s <char>    set switch hook character.\n");
#ifdef HAVE_GTK
    fprintf(f, "  -n           disable X-windows interface.\n");
#endif
    fprintf(f, "\n");
}

//
// You are here...
//
//
int main(int argc, char *argv[])
{
    char hook_char = '%'; // Default switch hook character
    char opt;
    int  enable_x = 1; // Not written yet.
    
    opterr = 0;
    optind = 0;
    while ((opt=getopt(argc,argv,"?Vhns:")) != EOF)
    {
	switch(opt)
	{
	case 'h': 
	    usage(argv[0], stdout);
	    exit(0);

	case '?': // Called if getopt doesn't recognise the option
	    usage(argv[0], stderr);
	    exit(0);

	case 'V':
	    printf("\nphone from dnprogs version %s\n\n", VERSION);
	    exit(0);

	case 's':
	    hook_char = optarg[0];
	    break;
	    
	case 'n':
	    enable_x = 0;
	    break;
	}
    }

    // argv[optind] is an initial command to pass to the UI so commands
    // like "phone answer" and "phone marsha::system" will work.
    
    // Check if X-Windows UI is possible
    if (enable_x)
    {
#ifdef HAVE_GTK
	if (gtk_phone_init(argc, argv))
	{
	    gtk_phone_run(argv[optind]);
	    exit(0);
	}
#endif
    }

    // Otherwise use terminal UI
    ncurses_init(hook_char);
    ncurses_run(argv[optind]);
    return 1;
}
