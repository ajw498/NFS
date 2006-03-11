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

typedef enum nstat {
           NFS_OK                 = 0,    /* everything is okay      */
           NFSERR_PERM            = 1,    /* caller not privileged   */
           NFSERR_NOENT           = 2,    /* no such file/directory  */
           NFSERR_IO              = 5,    /* hard I/O error          */
           NFSERR_NXIO            = 6,    /* no such device          */
           NFSERR_ACCESS          = 13,   /* access denied           */
           NFSERR_EXIST           = 17,   /* file already exists     */
           NFSERR_XDEV            = 18,   /* different filesystems   */
           /* Unused/reserved       19 */
           NFSERR_NOTDIR          = 20,   /* should be a directory   */
           NFSERR_ISDIR           = 21,   /* should not be directory */
           NFSERR_INVAL           = 22,   /* invalid argument        */
           NFSERR_FBIG            = 27,   /* file exceeds server max */
           NFSERR_NOSPC           = 28,   /* no space on filesystem  */
           NFSERR_ROFS            = 30,   /* read-only filesystem    */
           NFSERR_MLINK           = 31,   /* too many hard links     */
           NFSERR_NAMETOOLONG     = 63,   /* name exceeds server max */
           NFSERR_NOTEMPTY        = 66,   /* directory not empty     */
           NFSERR_DQUOT           = 69,   /* hard quota limit reached*/
           NFSERR_STALE           = 70,   /* file no longer exists   */
           NFSERR_BADHANDLE       = 10001,/* Illegal filehandle      */
           NFSERR_NOT_SYNC        = 10002,/* NFS3 only */
           NFSERR_BAD_COOKIE      = 10003,/* READDIR cookie is stale */
           NFSERR_NOTSUPP         = 10004,/* operation not supported */
           NFSERR_TOOSMALL        = 10005,/* response limit exceeded */
           NFSERR_SERVERFAULT     = 10006,/* undefined server error  */
           NFSERR_BADTYPE         = 10007,/* type invalid for CREATE */
           NFSERR_DELAY           = 10008,/* file "busy" - retry     */
           NFSERR_SAME            = 10009,/* nverify says attrs same */
           NFSERR_DENIED          = 10010,/* lock unavailable        */
           NFSERR_EXPIRED         = 10011,/* lock lease expired      */
           NFSERR_LOCKED          = 10012,/* I/O failed due to lock  */
           NFSERR_GRACE           = 10013,/* in grace period         */
           NFSERR_FHEXPIRED       = 10014,/* filehandle expired      */
           NFSERR_SHARE_DENIED    = 10015,/* share reserve denied    */
           NFSERR_WRONGSEC        = 10016,/* wrong security flavor   */
           NFSERR_CLID_INUSE      = 10017,/* clientid in use         */
           NFSERR_RESOURCE        = 10018,/* resource exhaustion     */
           NFSERR_MOVED           = 10019,/* filesystem relocated    */
           NFSERR_NOFILEHANDLE    = 10020,/* current FH is not set   */
           NFSERR_MINOR_VERS_MISMATCH = 10021,/* minor vers not supp */
           NFSERR_STALE_CLIENTID  = 10022,/* server has rebooted     */
           NFSERR_STALE_STATEID   = 10023,/* server has rebooted     */
           NFSERR_OLD_STATEID     = 10024,/* state is out of sync    */
           NFSERR_BAD_STATEID     = 10025,/* incorrect stateid       */
           NFSERR_BAD_SEQID       = 10026,/* request is out of seq.  */
           NFSERR_NOT_SAME        = 10027,/* verify - attrs not same */
           NFSERR_LOCK_RANGE      = 10028,/* lock range not supported*/
           NFSERR_SYMLINK         = 10029,/* should be file/directory*/
           NFSERR_RESTOREFH       = 10030,/* no saved filehandle     */
           NFSERR_LEASE_MOVED     = 10031,/* some filesystem moved   */
           NFSERR_ATTRNOTSUPP     = 10032,/* recommended attr not sup*/
           NFSERR_NO_GRACE        = 10033,/* reclaim outside of grace*/
           NFSERR_RECLAIM_BAD     = 10034,/* reclaim error at server */
           NFSERR_RECLAIM_CONFLICT = 10035,/* conflict on reclaim    */
           NFSERR_BADXDR          = 10036,/* XDR decode failed       */
           NFSERR_LOCKS_HELD      = 10037,/* file locks held at CLOSE*/
           NFSERR_OPENMODE        = 10038,/* conflict in OPEN and I/O*/
           NFSERR_BADOWNER        = 10039,/* owner translation bad   */
           NFSERR_BADCHAR         = 10040,/* utf-8 char not supported*/
           NFSERR_BADNAME         = 10041,/* name not supported      */
           NFSERR_BAD_RANGE       = 10042,/* lock range not supported*/
           NFSERR_LOCK_NOTSUPP    = 10043,/* no atomic up/downgrade  */
           NFSERR_OP_ILLEGAL      = 10044,/* undefined operation     */
           NFSERR_DEADLOCK        = 10045,/* file locking deadlock   */
           NFSERR_FILE_OPEN       = 10046,/* open file blocks op.    */
           NFSERR_ADMIN_REVOKED   = 10047,/* lockowner state revoked */
           NFSERR_CB_PATH_DOWN    = 10048 /* callback path down      */
} nstat;
#endif

#ifndef NTIMEVAL
#define NTIMEVAL

typedef struct ntimeval {
   unsigned int seconds;
   unsigned int nseconds;
} ntimeval;

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
#ifdef NFS2
#error foo
#define timeval_to_loadexec(unixtime, filetype, load, exec) (timeval_to_loadexec)(unixtime, filetype, load, exec, 1)
#else
#define timeval_to_loadexec(unixtime, filetype, load, exec) (timeval_to_loadexec)(unixtime, filetype, load, exec, 1000)
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

