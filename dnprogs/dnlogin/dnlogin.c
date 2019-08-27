/******************************************************************************
    (c) 2002-2008      Christine Caulfield          christine.caulfield@googlemail.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <ctype.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include "dn_endian.h"
#include "dnlogin.h"

/* The global state */
int termfd = -1;
int exit_char = 035; /* Default to ^] */
int finished = 0;    /* terminate mainloop */

int debug = 0;
int char_timeout = 0;
int timeout_valid = 0;
int read_pending = 0;

static int mainloop(void)
{
        while (!finished)
        {
                char inbuf[1024];
                struct timeval tv;
                int res;
                int sockfd = found_getsockfd();
                fd_set in_set;

                tv.tv_usec = 0;
                tv.tv_sec = char_timeout;

                FD_ZERO(&in_set);
                FD_SET(termfd, &in_set);
                FD_SET(sockfd, &in_set);

                if ( (res=select(FD_SETSIZE, &in_set, NULL, NULL, timeout_valid?&tv:NULL)) < 0)
                {
                        break;
                }

                if (res == 0 && timeout_valid && tv.tv_sec == 0 && tv.tv_usec == 0)
                {
                        if (read_pending)
                        {
                                tty_timeout();
                                timeout_valid = 0;
                        }
                }

                /* Read from keyboard */
                if (FD_ISSET(termfd, &in_set))
                {
                        int len;

                        if ( (len=read(termfd, inbuf, sizeof(inbuf))) <= 0)
                        {
                                perror("read tty");
                                break;
                        }
                        tty_process_terminal(inbuf, len);
                }

                if (FD_ISSET(sockfd, &in_set))
                        if (found_read() == -1)
                                break;
        }
        write(termfd, "\n", 1);
        return 0;
}

static void set_exit_char(char *string)
{
        int newchar = 0;

        if (string[0] == '^')
        {
                newchar = toupper(string[1]) - '@';
        }
        else
                newchar = strtol(string, NULL, 0);      // Just a number

        // Make sure it's resaonable
        if (newchar > 0 && newchar < 256)
                exit_char = newchar;

}

static void usage(char *prog, FILE * f)
{
        fprintf(f, "\nUsage: %s [OPTIONS] node\n\n", prog);

        fprintf(f, "\nOptions:\n");
        fprintf(f, "  -? -h        display this help message\n");
        fprintf(f, "  -V           show version number\n");
        fprintf(f, "  -e <char>    set exit char\n");
        fprintf(f, "  -T <secs>    connect timeout (default 60 seconds)\n");
        fprintf(f, "  -d <mask>    debug information\n");

        fprintf(f, "\n");
}


int main(int argc, char *argv[])
{
        int opt;
        int connect_timeout = 60;

        // Deal with command-line arguments.
        opterr = 0;
        optind = 0;
        while ((opt = getopt(argc, argv, "?Vhd:te:T:")) != EOF)
        {
                switch (opt)
                {
                case 'h':
                        usage(argv[0], stdout);
                        exit(0);

                case '?':
                        usage(argv[0], stderr);
                        exit(0);

                case 'V':
                        printf("\ndnlogin from dnprogs version %s\n\n", VERSION);
                        exit(1);
                        break;

                case 'e':
                        set_exit_char(optarg);
                        break;

                case 'T':
                        connect_timeout = atoi(optarg);
                        break;

                case 'd':
                        debug = atoi(optarg);
                        break;
                }
        }

        if (optind >= argc)
        {
                usage(argv[0], stderr);
                exit(2);
        }

        send_input = cterm_send_input;
        send_oob = cterm_send_oob;
        rahead_change = cterm_rahead_change;
        if (found_setup_link(argv[optind], DNOBJECT_CTERM, cterm_process_network, connect_timeout) == 0)
        {
                if (tty_setup("/dev/fd/0", 1) == -1)
                        if (tty_setup("/proc/self/fd/0", 1)) {
                                perror("cannot connect to stdin\n");
                        }
                mainloop();
                tty_setup(NULL, 0);
        }
        else
        {
                return 2;
        }

        return 0;
}
