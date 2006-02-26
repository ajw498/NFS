/*
	$Id$

	Common bits for all entry points
*/

#ifndef UTILS_H
#define UTILS_H


#include <kernel.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/errno.h>

#include "pools.h"

#define IMAGEENTRY_FUNC_RENAME 8
#define IMAGEENTRY_FUNC_READDIR 14
#define IMAGEENTRY_FUNC_READDIRINFO 15
#define IMAGEENTRY_FUNC_NEWIMAGE 21
#define IMAGEENTRY_FUNC_CLOSEIMAGE 22
#define IMAGEENTRY_FUNC_READDEFECT 25
#define IMAGEENTRY_FUNC_ADDDEFECT 26
#define IMAGEENTRY_FUNC_READBOOT 27
#define IMAGEENTRY_FUNC_WRITEBOOT 28
#define IMAGEENTRY_FUNC_READUSEDSPACE 29
#define IMAGEENTRY_FUNC_READFREESPACE 30
#define IMAGEENTRY_FUNC_NAME 31
#define IMAGEENTRY_FUNC_STAMP 32
#define IMAGEENTRY_FUNC_GETUSAGE 33

#define IMAGEENTRY_FILE_SAVEFILE 0
#define IMAGEENTRY_FILE_WRITECATINFO 1
#define IMAGEENTRY_FILE_READCATINFO 5
#define IMAGEENTRY_FILE_DELETE 6
#define IMAGEENTRY_FILE_CREATEFILE 7
#define IMAGEENTRY_FILE_CREATEDIR 8
#define IMAGEENTRY_FILE_READBLKSIZE 10

#define IMAGEENTRY_ARGS_WRITEEXTENT 3
#define IMAGEENTRY_ARGS_READSIZE 4
#define IMAGEENTRY_ARGS_FLUSH 6
#define IMAGEENTRY_ARGS_ENSURESIZE 7
#define IMAGEENTRY_ARGS_ZEROPAD 8
#define IMAGEENTRY_ARGS_READDATESTAMP 9


/* Value to specify to leave an attribute unchanged */
#define NOVALUE (-1)

#define OBJ_NONE  0
#define OBJ_FILE  1
#define OBJ_DIR   2
#define OBJ_IMAGE 3

/* Types for adding ,xyz extensions */
#ifndef NEVER
#define NEVER  0
#define NEEDED 1
#define ALWAYS 2
#endif

/* Pretend to fileswitch that we have a block/sector size */
#define FAKE_BLOCKSIZE 1024


/* Error numbers */
#define ERRBASE 0x0081b400

#define NOMEM (ERRBASE + 0)
#define NOMEMMESS "Unable to allocate memory"

#define UNSUPP (ERRBASE + 1)
#define UNSUPPMESS "Unsupported entry point called"

/* 3 errors */
#define BYTESERRBASE (ERRBASE + 2)

/* 4 errors */
#define FUNCERRBASE (ERRBASE + 8)

/* 1 error */
#define NFSSTATBASE (ERRBASE + 16)

/* 14 errors */
#define RPCERRBASE (ERRBASE + 24)
#define RPCBUFOVERFLOW (RPCERRBASE + 0)
#define RPCBUFOVERFLOWMESS "RPC buffer overflow"

/* 1 error */
#define MODULEERRBASE (ERRBASE + 48)

/* 1 error */
#define OPENCLOSEERRBASE (ERRBASE + 50)


#define NOATTRS (ERRBASE + 52)
#define NOATTRSMESS "Object attributes not supplied by server"

#define STARTUPERR (ERRBASE + 53)

/* Directory not empty error number must match what filecore uses */
#define ERRDIRNOTEMPTY 67764

#define filesize(size) (size > 0xFFFFFFFF ? 0xFFFFFFFF : (unsigned int)size);

#define MAX_GIDS 16
#define MAXFHSIZE 64
#ifndef NFS3_COOKIEVERFSIZE
#define NFS3_COOKIEVERFSIZE 8
#endif

/* The worstcase size of a header for read/write calls.
   If this value is not big enough, the data part will not be of
   an optimum size, but nothing bad will happen */
#define MAX_HDRLEN 416

/* The size to use for tx and rx buffers */
#define BUFFERSIZE (MAX_HDRLEN + MAX_DATABUFFER)

/* The maximum pathname size that we support */
#define MAX_PATHNAME 1024

/* The filetype returned for directories */
#define DIR_FILETYPE 0xFFD

/* Size of the fifo to use for pipelining. Must be at least 2, to allow
   READDIR to double buffer. Greater than 2 has negligible increase in
   performance. */
#define FIFOSIZE 2

struct commonfh {
	char handle[MAXFHSIZE];
	int size;
};

/* All infomation associated with an open file */
struct file_handle {
	struct conn_info *conn;
	struct commonfh fhandle;
	unsigned int extent;
	unsigned int load;
	unsigned int exec;
	int dir;
};

#ifndef NSTAT
#define NSTAT

enum nstat {
   NFS_OK             = 0,
   NFSERR_PERM        = 1,
   NFSERR_NOENT       = 2,
   NFSERR_IO          = 5,
   NFSERR_NXIO        = 6,
   NFSERR_ACCES       = 13,
   NFSERR_EXIST       = 17,
   NFSERR_XDEV        = 18,
   NFSERR_NODEV       = 19,
   NFSERR_NOTDIR      = 20,
   NFSERR_ISDIR       = 21,
   NFSERR_INVAL       = 22,
   NFSERR_FBIG        = 27,
   NFSERR_NOSPC       = 28,
   NFSERR_ROFS        = 30,
   NFSERR_MLINK       = 31,
   NFSERR_NAMETOOLONG = 63,
   NFSERR_NOTEMPTY    = 66,
   NFSERR_DQUOT       = 69,
   NFSERR_STALE       = 70,
   NFSERR_REMOTE      = 71,
   NFSERR_BADHANDLE   = 10001,
   NFSERR_NOT_SYNC    = 10002,
   NFSERR_BAD_COOKIE  = 10003,
   NFSERR_NOTSUPP     = 10004,
   NFSERR_TOOSMALL    = 10005,
   NFSERR_SERVERFAULT = 10006,
   NFSERR_BADTYPE     = 10007,
   NFSERR_JUKEBOX     = 10008
};
#endif

#ifndef NTIMEVAL
#define NTIMEVAL

struct ntimeval {
   unsigned int seconds;
   unsigned int nseconds;
};

#endif

/* All infomation associated with a connection */
struct conn_info {
	char *server;
	unsigned int portmapper_port;
	unsigned int mount_port;
	unsigned int nfs_port;
	unsigned int pcnfsd_port;
	char *export;
	struct commonfh rootfh;
	char *config;
	struct sockaddr_in sockaddr;
	int sock;
	long timeout;
	int retries;
	int hidden;
	unsigned int umask;
	unsigned int unumask;
	unsigned int numgids;
	unsigned int uid;
	unsigned int gid;
	unsigned int gids[MAX_GIDS];
	char *username;
	char *password;
	int logging;
	char *auth;
	int authsize;
	char *machinename;
	int defaultfiletype;
	int xyzext;
	int localportmin;
	int localportmax;
	int maxdatabuffer;
	int followsymlinks;
	int pipelining;
	int casesensitive;
	char lastdir[MAX_PATHNAME];
	uint64_t lastcookie;
	char lastcookieverf[NFS3_COOKIEVERFSIZE];
	int laststart;
	struct commonfh lastdirhandle;
	int tcp;
	int nfs3;
	struct pool *pool;
};

extern int enablelog;

typedef _kernel_oserror os_error;

/* Generate an os_error block */
os_error *gen_error(int num, char *msg, ...);

extern void (*error_func)(void);



void syslogf(char *logname, int level, char *fmt, ...);

void log_error(os_error *err);


/* Convert unix leafname into RISC OS format */
int filename_riscosify(char *name, int namelen, char *buffer, int buflen, int *filetype, int defaultfiletype, int xyzext);

/* Convert a RISC OS leafname into unix format */
char *filename_unixify(char *name, unsigned int len, unsigned int *newlen, struct pool *pool);

/* Use MimeMap to lookup a filetype from an extension */
int lookup_mimetype(char *ext, int defaultfiletype);

/* Convert a unix mode to RISC OS attributes */
unsigned int mode_to_attr(unsigned int mode);

/* Convert RISC OS attributes to a unix mode */
unsigned int attr_to_mode(unsigned int attr, unsigned int oldmode, struct conn_info *conn);

/* Add ,xyz onto the leafname if necessary */
char *addfiletypeext(char *leafname, unsigned int len, int extfound, int newfiletype, unsigned int *newlen, int defaultfiletype, int xyzext, struct pool *pool);

/* Drop to user mode to trigger any pending callbacks */
void trigger_callback(void);

/* Convert an IP address to a string */
char *host_to_str(unsigned int host, struct pool *pool);

/* Convert a RISC OS error number to an NFS error */
enum nstat oserr_to_nfserr(int errnum);

/* Convert a RISC OS load and execution address into a unix timestamp */
void loadexec_to_timeval(unsigned int load, unsigned int exec, struct ntimeval *unixtime, int mult);
#ifdef NFS3
#define loadexec_to_timeval(load, exec, unixtime) (loadexec_to_timeval)(load, exec, unixtime, 1000)
#else
#define loadexec_to_timeval(load, exec, unixtime) (loadexec_to_timeval)(load, exec, unixtime, 1)
#endif

/* Convert a unix timestamp into a RISC OS load and execution address */
void timeval_to_loadexec(struct ntimeval *unixtime, int filetype, unsigned int *load, unsigned int *exec, int mult);
#ifdef NFS3
#define timeval_to_loadexec(unixtime, filetype, load, exec) (timeval_to_loadexec)(unixtime, filetype, load, exec, 1000)
#else
#define timeval_to_loadexec(unixtime, filetype, load, exec) (timeval_to_loadexec)(unixtime, filetype, load, exec, 1)
#endif


/* A version of gethostbyname that will timeout.
   Also handles IP addresses without needing a reverse lookup */
os_error *gethostbyname_timeout(char *host, unsigned long timeout, struct hostent **hp);


#ifdef NFS3
#define NFSPROC(proc, args) NFSPROC3_##proc args
#else
#define NFSPROC(proc, args) NFSPROC_##proc args
#endif

#ifdef NFS3
#define MNTPROC(proc, args) MOUNTPROC3_##proc args
#else
#define MNTPROC(proc, args) MNTPROC_##proc args
#endif

#ifdef NFS3
#define ENTRYFUNC(func) nfs3_entry_##func
#else
#define ENTRYFUNC(func) nfs2_entry_##func
#endif

#define ENTRYFUNCDECL(ret, func, args) ret nfs3_entry_##func args; ret nfs2_entry_##func args

#define CALLENTRY(name, conn, args) (conn->nfs3 ? nfs3_entry_##name args : nfs2_entry_##name args)

#endif
