/* Example librms program using the RAB/FAB functions.

   This program is in the public domain
*/

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "rms.h"

int main(int argc, char *argv[])
{
    /* Open the file. Keep the RMSHANDLE returned */
    RMSHANDLE h;

    h = rms_open("ford::index.dat", O_RDWR, NULL);

    if (h)
    {
        char b[10240];
	int got;
	struct RAB rab;

	/* Clear out the rab */
	memset(&rab, 0, sizeof(rab));

	/* Look for the record with my name in it. */
	rab.rab$b_rac = RAB$C_KEY;
	rab.rab$l_kbf = "CHRISSIE";
	
	got = rms_read(h, b, sizeof(b), &rab);
	if (got > 0)
	{
	    b[got] = '\0';
	    printf("Got %d bytes: %s\n", got, b);
	}
	else
	{
	    fprintf(stderr, "%s\n", rms_lasterror(h));
	}

        /* Update it */
	memcpy(b+8, "????", 4);
	if (rms_update(h, b, got, NULL) == -1)
	{
	    fprintf(stderr, "%s\n", rms_lasterror(h));
	}
	
	rms_close(h);
    }
    else
    {
	fprintf(stderr, "connect: %s\n", rms_openerror());
    }

    return 0;
}
