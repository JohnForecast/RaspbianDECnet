/* dnmount.c */
/******************************************************************************
    (c) 1995-1998 E.M. Serrat             emserrat@geocities.com

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
#include <errno.h>
#include <ctype.h>
#include <mntent.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include <asm/types.h>
#ifndef BLOCK_SIZE
#include <linux/fs.h>
#endif
#include <linux/dap_fs.h>

/* DECnet phase IV limits */
#define MAX_NODE      6
#define MAX_USER     12
#define MAX_PASSWORD 12
#define MAX_ACCOUNT  12
#ifndef FALSE
#define TRUE 1
#define FALSE 0
#endif

struct	sockaddr_dn			sockaddr;
struct	accessdata_dn			accessdata;
struct	nodeent				*dp;
struct	dapfs_mount_data		data;
char  					node[MAX_NODE+1],dirname[250],
					vms_directory[250],mount_point[80],
					*lasterror;
/*-----------------------------------------------------------------------*/
void
usage(void)
{
	printf("\nusage: dnmount [OPTIONS] vms_directory mount_point\n");
	printf("OPTIONS:\n");
	printf("-u uid		User  ID for mounted filesystem\n");
	printf("-g gid		Group ID for mounted filesystem\n");
	printf("-h -?		Display this help\n\n");
}
/*-----------------------------------------------------------------------*/
/* Christine's Parser for VMS file specifications
 * It was taken from dndir.c
 *
*/
/*-------------------------------------------------------------------------*/
/* Parse the filename into its component parts */
static int parse(char *fname)
{
    enum  {NODE, USER, PASSWORD, ACCOUNT, NAME, FINISHED} state;
    int   n0=0;
    int   n1=0;
    char  sep, term;
    int   i;
    char *local_user;

    state = NODE; /* Node is mandatory */

    while (state != FINISHED)
    {
	switch (state)
	{
	case NODE:
            if (fname[n0] != ':' && fname[n0] != '\"' &&
                fname[n0] != '\'' && fname[n0] != '/')
            {
                if (n1 >= MAX_NODE ||
                    fname[n0] == ' ' || fname[n0] == '\n')
                {
                    lasterror = (char *)"File name parse error";
                    return FALSE;
                }
                node[n1++] = fname[n0++];
            }
            else
            {
                node[n1] = '\0';
                n1 = 0;
                switch (fname[n0])
                {
                case '\"':
                case '\'':
                    sep = ' ';
                    term = fname[n0++];
                    state = USER;
                    break;

                case '/':
                    sep = '/';
                    term = 0;
                    n0++;
                    state = USER;
                    break;

                default:
                    n0 += 2;
                    state = NAME;
                    break;
                }
            }
            break;

	case USER:
            if (fname[n0] != sep && fname[n0] != (term ? term : ':'))
            {
                if (n1 >= MAX_USER)
                {
                    lasterror = (char *)"File name parse error";
                    return FALSE;
                }
                accessdata.acc_user[n1++] = fname[n0++];
            }
            else
            {
                accessdata.acc_user[n1] = '\0';
                n1 = 0;
                if (fname[n0] == sep)
                {
                    state = PASSWORD;
                    n0++;
                }
                else
                {
                    state = NAME;
                    n0 += term ? 3 : 2;
                }
            }
            break;

	case PASSWORD:
            if (fname[n0] != sep && fname[n0] != (term ? term : ':'))
            {
                if (n1 >= MAX_PASSWORD)
                {
                    lasterror = (char *)"File name parse error";
                    return FALSE;
                }
                accessdata.acc_pass[n1++] = fname[n0++];
            }
            else
            {
                accessdata.acc_pass[n1] = '\0';
                n1 = 0;
                if (fname[n0] == sep)
                {
                    state = ACCOUNT;
                    n0++;
                }
                else
                {
                    state = NAME;
                    n0 += term ? 3 : 2;
                }
            }
            break;

	case ACCOUNT:
            if (fname[n0] != sep && fname[n0] != (term ? term : ':'))
            {
                if (n1 >= MAX_ACCOUNT)
                {
                    lasterror = (char *)"File name parse error";
                    return FALSE;
                }
                accessdata.acc_acc[n1++] = fname[n0++];
            }
            else
            {
                accessdata.acc_acc[n1] = '\0';
                n1 = 0;
                state = NAME;
                n0 += term ? 3 : 2;
            }
            break;

	case NAME:
	    strcpy(dirname, fname+n0);
	    state = FINISHED;
	    break;

	case FINISHED: // To keep the compiler happy
	    break;
	} /* switch */
    }

/* tail end validation */
    dp = getnodebyname(node);
    if (!dp)
    {
	lasterror = "Unknown or invalid node name ";
	return FALSE;
    }

    /* Default to a full wildcard if no filename is given. This will cope with
       the situation where no directory is provided (node::) and where a
       directory (node::[christine]) is provided. Logical names must end with
       a colon for this to happen or we will not be able to distinguish it
       from a filename

	EMS: Changed wildcard to *.*;0 so we only get the last version of
             the files. This is useful for the dapfs.
    lastchar = dirname[strlen(dirname)-1];
    if (lastchar == ']' || lastchar == ':' || dirname[0] == '\0')
	strcat(dirname, "*.*;0");

    */

    // Try very hard to get the local username
    local_user = cuserid(NULL);
    if (!local_user || local_user == (char *)0xffffffff)
      local_user = getenv("LOGNAME");
    if (!local_user) local_user = getenv("USER");
    if (local_user)
    {

	strcpy((char *)accessdata.acc_acc, local_user);
	accessdata.acc_accl = strlen((char *)accessdata.acc_acc);
        for (i=0; i<accessdata.acc_accl; i++)
            accessdata.acc_acc[i] = toupper(accessdata.acc_acc[i]);
    }
    else
        accessdata.acc_acc[0] = '\0';


    /* Complete the accessdata structure */
    accessdata.acc_userl = strlen(accessdata.acc_user);
    accessdata.acc_passl = strlen(accessdata.acc_pass);
    accessdata.acc_accl  = strlen(accessdata.acc_acc);
    return TRUE;
}

/*-----------------------------------------------------------------------*/
static int	dap_setup_link(void)
{
    int	sockfd;
    if ((sockfd=socket(AF_DECnet,SOCK_STREAM,DNPROTO_NSP)) == -1)
    {
	perror("socket");
	exit(-1);
    }


    if (setsockopt(sockfd,DNPROTO_NSP,SO_CONACCESS,&accessdata,
		   sizeof(accessdata)) < 0)
    {
	perror("setsockopt");
	exit(-1);
    }


    if (connect(sockfd, (struct sockaddr *)&sockaddr,
		sizeof(sockaddr)) < 0)
    {
	perror("socket");
	exit(-1);
    }

    return sockfd;
}
/*-------------------------------------------------------------------------*/
static void	dap_exchg_config(int sockfd)
{
    int		er;
    unsigned char	buf[100];
    unsigned char	confmsg[17] = {
	0x01,	/* Message type 1 = Conf */
	0x00,	/* Flag Field		 */
	0x2E,0x04, /* BufSiz 		 */
	0x07,	/* OS type = VMS	 */
	0x03,   /* RMS-32		 */
	7,0,0,0,0, /* Version 'stuff'     */
	0xA2,	/* Sequential Org Support*/
	0x80,
	0xB0,   /* Message Blocking. OK  */
	0xFE,   /* Supp Extended Attrib  */
	0xA8,   /* Supp. Seq Access/Del  */
	0x7C	/* WildCrds/Ren/Seg Msgs */
    };

    if ((er=write(sockfd,confmsg,sizeof(confmsg))) < 0)
    {
	perror("Send DAP config msg");
	exit(-1);
    }
    if ((er=read(sockfd,buf,sizeof(buf))) < 0)
    {
	perror("Recv DAP config msg");
	exit(-1);
    }
    /* Assume that Peer Supports the minimal characteristis we
       setup here
    */
    if (buf[0] == 0x09)
    {
	printf("Local DAP configuration rejected by remote node\n");
	exit(-1);
    }
}
/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/
void
dn_setmountentry(void)
{
	struct	mntent	mount_entry;
	char	vms_mount_point[250];
	FILE	*mtab;
	int	MFD;

	strcpy(vms_mount_point,node);
	strcat(vms_mount_point,"::");
	strcat(vms_mount_point,dirname);

	mount_entry.mnt_fsname = vms_mount_point;
	mount_entry.mnt_dir    = mount_point;
	mount_entry.mnt_type   = "dapfs";
	mount_entry.mnt_opts   = "rw";
	mount_entry.mnt_freq   = 0;
	mount_entry.mnt_passno = 0;



	if ( (MFD = open (MOUNTED "~", O_RDWR | O_CREAT | O_EXCL, 0600)) < 0)
	{
		perror("opening /etc/mtab file");
		exit(-1);
	}
	close(MFD);
	if ( (mtab = setmntent(MOUNTED, "a+")) == NULL)
	{
		perror("setmntent");
		exit(-1);
	}
	if ( addmntent(mtab,&mount_entry) == 1)
	{
		perror("addmntent");
		exit(-1);
	}
	if ( fchmod(fileno(mtab),0644) < 0)
	{
		perror ("fchmod");
		exit(-1);
	}
	endmntent(mtab);

	if ( unlink(MOUNTED "~") < 0 )
	{
		perror("unlink");
		exit(-1);
	}

}
/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
	struct	stat	st;
	struct	passwd	*pwd;
	struct	group	*grp;
	char		opt;

	memset(&vms_directory,0,sizeof(vms_directory));
	memset(&mount_point,0,sizeof(mount_point));
	data.uid=getuid();
	data.gid=getgid();
	opterr=0;
	optind=0;
	while ( (opt=getopt(argc,argv,"?hu:g:")) != EOF)
	{
	  switch (opt)
	  {
		case '?':
		case 'h':
			  usage();
			  exit(0);
		case 'u':
			  if (isdigit(optarg[0]))
				data.uid=atoi(optarg);
			  else
			  {
				if ( (pwd=getpwnam(optarg)) == NULL)
				{
					printf("Invalid uid %s\n",optarg);
					exit (-1);
				}
				data.uid=pwd->pw_uid;
			  }
			  break;
		case 'g':
			  if (isdigit(optarg[0]))
				data.gid=atoi(optarg);
			  else
			  {
				if ( (grp=getgrnam(optarg)) == NULL)
				{
					printf("Invalid gid %s\n",optarg);
					exit (-1);
				}
				data.gid=grp->gr_gid;
			  }
			  break;
		default:  usage();
			  exit(0);
	  }
	}
	if ( (argc - optind)  < 2)
	{
		usage();
		exit(0);
	}
	memcpy(vms_directory,argv[optind],strlen((char *)argv[optind]));
	optind +=1;
	memcpy(mount_point,argv[optind],strlen((char *)argv[optind]));

	/* Parse the filename into its parts */
	if (!parse(vms_directory))
	{
		fprintf(stderr, "%s\n", lasterror);
		exit(2);
	}
	if (stat(mount_point,&st) < 0)
	{
		fprintf(stderr, "Invalid mount point %s : %s\n", mount_point,
				strerror(errno));
		exit(2);
	}
	if (!S_ISDIR(st.st_mode))
	{
		fprintf(stderr, "Mount point %s is not a directory\n",
				mount_point);
		exit(2);
	}
	if ( (getuid() != 0) && ((getuid() != st.st_uid) ||
	     ((st.st_mode & S_IRWXU) != S_IRWXU)))
	{
		fprintf(stderr, "Must be root to mount\n");
		exit(2);
	}
	strcpy(data.mounted_dir,dirname);
    	sockaddr.sdn_family = AF_DECnet;
    	sockaddr.sdn_flags	= 0x00;
    	sockaddr.sdn_objnum	= 0x11;			/* FAL		*/
    	sockaddr.sdn_objnamel	= 0x00;

    	memcpy(sockaddr.sdn_add.a_addr, dp->n_addr,2);
	memcpy(&data.sockaddr,&sockaddr,sizeof(sockaddr));
	memcpy(&data.accessdata,&accessdata,sizeof(accessdata));

	data.sockfd=dap_setup_link();
	dap_exchg_config(data.sockfd);
	if (mount(vms_directory,mount_point,
                  "dapfs",MS_MGC_VAL,(char *)&data) < 0)
	{
		fprintf(stderr, "Error mounting %s \n",vms_directory);
		exit(-1);
	}
	dn_setmountentry();
	return 0;
}
/*-------------------------------------------------------------------------*/
