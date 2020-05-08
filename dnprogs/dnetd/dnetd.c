/******************************************************************************
    (c) 1999 Christine Caulfield               christine.caulfield@googlemail.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 ******************************************************************************
*/
////
// dnetd.cc
// DECnet super-server
////

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>

#ifndef SDF_WILD
#warning SDF_WILD not defined. This program may not work with your kernel
#define SDF_WILD 1
#endif

#define MAX_ARGS 256

#define NODE_ADDRESS    "/proc/sys/net/decnet/node_address"

void sigchild(int s);
void sigterm(int s);
int  open_server_socket(void);

extern void task_server(int, int, int);

// Global variables.
static int verbosity = 0;
static volatile int do_shutdown = 0;
static char binary_dir[PATH_MAX];

void usage(char *prog, FILE *f)
{
    fprintf(f,"\n%s options:\n", prog);
    fprintf(f," -v        Verbose messages\n");
    fprintf(f," -h        Show this help text\n");
    fprintf(f," -d        Debug - don't do initial fork\n");
    fprintf(f," -s        Don't run scripts in users' directories\n");
    fprintf(f," -l<type>  Logging type(s:syslog, e:stderr, m:mono)\n");
    fprintf(f," -p<dir>   Path to find daemon programs\n");
    fprintf(f," -V        Show version number\n\n");
}

// Run a pre-defined daemon
void exec_daemon(int sockfd, char *daemon_name)
{
    char *argv[MAX_ARGS];
    int   argc = 0;
    char *argp;
    int   err;
    int   i;
    char  name[PATH_MAX];

    // Split the daemon command into a command and its args
    argp = strtok(daemon_name, " ");
    while (argp && argc < MAX_ARGS)
    {
        argv[argc++] = argp;
        argp = strtok(NULL, " ");
    }
    argv[argc] = NULL;

    // Point stderr to /dev/null just in case
    err = open("/dev/null", O_RDWR);


    // Make STDIN & STDOUT the DECnet socket.
    dup2(sockfd, 0);
    dup2(sockfd, 1);
    if (err != 2) dup2(err, 2);
    fcntl(0, F_SETFD, 0); // Don't close on exec
    fcntl(1, F_SETFD, 0);
    fcntl(2, F_SETFD, 0);

    // Close all the others. This next line is, of course, bollocks
    for (i=3; i<256; i++) close(i);

    // Look for the daemon in $(prefix) if the name is not absolute
    if (daemon_name[0] != '/')
    {
        if (strlen(binary_dir)+strlen(daemon_name)+1 > PATH_MAX)
        {
            DNETLOG((LOG_ERR, "Can't exec daemon %s. Name too long ", daemon_name));
            return;
        }
        strcpy(name, binary_dir);
        strcat(name, "/");
        strcat(name, daemon_name);
    }
    else
    {
        if (strlen(daemon_name) > PATH_MAX)
        {
            DNETLOG((LOG_ERR, "Can't exec daemon %s. Name too long ", daemon_name));
            return;
        }
        strcpy(name, daemon_name);
    }

    // Run it...
    execvp(name, argv);
    DNETLOG((LOG_ERR, "exec of daemon %s failed: %m", name));
}

// Code for MIRROR object
void mirror(int insock)
{
    char ibuf[4097];
    char condata[] = {0x00, 0x20}; // Actually 4096 as a LE word
    int readnum;

    dnet_accept(insock, 0, condata, 2);

    while ( (readnum=read(insock,ibuf,sizeof(ibuf))) > 0)
    {
        ibuf[0]=0x01;
        if (write(insock,ibuf,readnum) < 0)
        {
            DNETLOG((LOG_WARNING, "mirror, write failed: %m\n"));
            close(insock);
            break;
        }
    }
    close(insock);
}


// Start here...
int main(int argc, char *argv[])
{
    pid_t              pid;
    char               opt;
    int                fd;
    struct sockaddr_dn sockaddr;
    struct stat        st;
    int                debug=0;
    int                status;
    int                secure=0;
    int                len = sizeof(sockaddr);
    char               log_char = 'l'; // Default to syslog(3)
    char               condata[] = {0x00, 0x20}; // Actually 4096 as a LE word
    FILE               *file;

    /*
     * Make sure that the DECnet module is loaded and has a valid node
     * address.
     */
    if ((file = fopen(NODE_ADDRESS, "r")) != NULL)
    {
        int area, addr;

        if ((fscanf(file, "%d.%d\n", &area, &addr) != 2) ||
            (addr > 1023) || (addr <= 0) ||
            (area > 63) || (area <= 0))
        {
            fprintf(stderr, "Invalid DECnet node address\n");
            return -1;
        }
        fclose(file);
    }
    else
    {
        fprintf(stderr, "%s missing, is DECnet module loaded?\n",
                NODE_ADDRESS);
        return -1;
    }

    // make default binaries directory name
    strcpy(binary_dir, BINARY_PREFIX);
    strcat(binary_dir, "/sbin");

    // Deal with command-line arguments. Do these before the check for root
    // so we can check the version number and get help without being root.
    opterr = 0;
    optind = 0;
    while ((opt=getopt(argc,argv,"?vVhp:sdl:")) != EOF)
    {
        switch(opt)
        {
        case 'h':
            usage(argv[0], stdout);
            exit(0);

        case '?':
            usage(argv[0], stderr);
            exit(0);

        case 'v':
            verbosity++;
            break;

        case 'd':
            debug++;
            break;

        case 's':
            secure++;
            break;

        case 'V':
            printf("\ntaskd from dnprogs version %s\n\n", VERSION);
            exit(1);
            break;

        case 'p':
            if (stat(optarg, &st) < 0)
            {
                fprintf(stderr, "%s does not exist\n", optarg);
                exit(-1);
            }
            if (!S_ISDIR(st.st_mode))
            {
                fprintf(stderr, "%s is not a directory\n", optarg);
                exit(-1);
            }
            strcpy(binary_dir, optarg);
            break;

        case 'l':
            if (optarg[0] != 's' &&
                optarg[0] != 'm' &&
                optarg[0] != 'e')
            {
                usage(argv[0], stderr);
                exit(2);
            }
            log_char = optarg[0];
            break;
        }
    }

    // Initialise logging
    init_daemon_logging("dnetd", log_char);

    // Needed for dnetd on Eduardo's kernel to
    // be able to do MIRROR
    dnet_set_optdata(condata, sizeof(condata));

    // set handling of hinum objects (needed for use with NIS)
    dnet_setobjhinum_handling(DNOBJHINUM_ZERO, 1);

    fd = dnet_daemon(0, NULL, verbosity, debug?0:1);
    if (fd > -1)
    {
        char *daemon_name = dnet_daemon_name();
        struct   sockaddr_dn  sockaddr;
        int      er;
        unsigned int namlen = sizeof(sockaddr);

        // Should we execute an external daemon ?
        // The external daemon should dnet_accept() the socket
        if (daemon_name && strcmp(daemon_name, "internal"))
        {
            if (verbosity >1)
                DNETLOG((LOG_INFO, "Starting daemon '%s'\n", daemon_name));
            exec_daemon(fd, daemon_name);
            return 0;
        }

        // Dispatch the object internally
        // If it's a named object then run a task script
        getsockname(fd, (struct sockaddr *)&sockaddr, &namlen);
        if (sockaddr.sdn_objnamel)
        {
            task_server(fd, verbosity, secure);
            return 0;
        }

        // Choose a numbered object
        if ( sockaddr.sdn_objnum == getobjectbyname("MIRROR") ) {
            if (verbosity >1)
                DNETLOG((LOG_INFO, "Doing mirror\n"));
            mirror(fd);
        } else {
            DNETLOG((LOG_ERR, "Don't know how to handle object %d\n",
                    sockaddr.sdn_objnum));
            dnet_reject(fd, DNSTAT_OBJECT, NULL, 0);
        }
    }
    return 0;
}

