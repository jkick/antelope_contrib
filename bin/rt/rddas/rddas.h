/***********************************************************************
 *
 *
 *  Include file of constants, macros, function declarations, etc.
 *
 ***********************************************************************/
#ifndef rddas_h_included
#define rddas_h_included
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ser_sync.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <stdio.h>      
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stropts.h>
#include <netdb.h>
#include "pkt.h"

#define DSK_BLK_SIZE    1024	/* PASSCAL disk block size  */
#define IBUF_SIZE       4096

 /* Input Ports  */

#define  IN_CHR		0x434852  /* 'CHR'  */
#define  DASRATE	57600

struct Prts  {
    int ifp;			/* input port file pointer  */
    int orb;			/* orb file pointer  */
    int brate;			/* baud rate on input port  */
    int reset;			/* do we want reset inport at the beginning */
    int uncompress;		/* do we want uncompress data before ORB  */
    char ip_name[132];		/* input port name  */
    char orbname[132];		/* orbserver name  */
};

typedef struct Compnt {
    int lval;
    int streamid;
    char name[64];
} Compnt;

int Log;
 
#ifdef __STDC__
#define PL_(x) x
#else
#define PL_(x) ( )
#endif /* __STDC__ */
 
extern void usage PL_(( void ));
extern int main PL_(( int argc, char *argv[] ));
extern int open_IN_ports PL_(( struct Prts *ports ));
extern int open_serial PL_(( struct Prts *ports ));
extern void *read_in_ports PL_(( struct Prts *ports ));
extern int read_serial PL_(( struct Prts *inport, unsigned char **buffer ));
 
#undef PL_

#endif
