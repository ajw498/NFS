/*
	$Id$

	Common bits for all entry points
*/

#ifndef UTILS_H
#define UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kernel.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/errno.h>
#include <iconv.h>

#include "pools.h"

#define IMAGEENTRY_FUNC_RENAME 8
#define IMAGEENTRY_FUNC_READDIR 14
#define IMAGEENTRY_FUNC_READDIRINFO 15
#define FSENTRY_FUNC_SHUTDOWN 16
#define FSENTRY_FUNC_READDIRINFO 19
#define IMAGEENTRY_FUNC_NEWIMAGE 21
#define IMAGEENTRY_FUNC_CLOSEIMAGE 22
#define FSENTRY_FUNC_CANONICALISE 23
#define FSENTRY_FUNC_RESOLVEWILDCARD 24
#define IMAGEENTRY_FUNC_READDEFECT 25
#define IMAGEENTRY_FUNC_ADDDEFECT 26
#define IMAGEENTRY_FUNC_READBOOT 27
#define IMAGEENTRY_FUNC_WRITEBOOT 28
#define IMAGEENTRY_FUNC_READUSEDSPACE 29
#define IMAGEENTRY_FUNC_READFREESPACE 30
#define IMAGEENTRY_FUNC_NAME 31
#define IMAGEENTRY_FUNC_STAMP 32
#define IMAGEENTRY_FUNC_GETUSAGE 33
#define IMAGEENTRY_FUNC_READFREESPACE64 35

#define IMAGEENTRY_FILE_SAVEFILE 0
#define IMAGEENTRY_FILE_WRITECATINFO 1
#define FSENTRY_FILE_WRITELOAD 2
#define FSENTRY_FILE_WRITEEXEC 3
#define FSENTRY_FILE_WRITEATTR 4
#define IMAGEENTRY_FILE_READCATINFO 5
#define IMAGEENTRY_FILE_DELETE 6
#define IMAGEENTRY_FILE_CREATEFILE 7
#define IMAGEENTRY_FILE_CREATEDIR 8
#define IMAGEENTRY_FILE_READBLKSIZE 10
#define FSENTRY_FILE_LOADFILE 255

#define IMAGEENTRY_ARGS_WRITEEXTENT 3
#define IMAGEENTRY_ARGS_READSIZE 4
#define IMAGEENTRY_ARGS_FLUSH 6
#define IMAGEENTRY_ARGS_ENSURESIZE 7
#define IMAGEENTRY_ARGS_ZEROPAD 8
#define IMAGEENTRY_ARGS_READDATESTAMP 9
#define FSENTRY_ARGS_NEWIMAGESTAMP 10


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

/* 2 errors */
#define OPENCLOSEERRBASE (ERRBASE + 50)


#define NOATTRS (ERRBASE + 52)
#define NOATTRSMESS "Object attributes not supplied by server"

#define STARTUPERR (ERRBASE + 53)

#define ICONVERR (ERRBASE + 54)

#define NODISCERR (ERRBASE + 55)
#define INVALFILENAMEERR (ERRBASE + 56)

/* Directory not empty error number must match what filecore uses */
#define ERRDIRNOTEMPTY 67764

#define filesize(size) (size > 0x7FFFFFFF ? 0x7FFFFFFF : (unsigned int)size);

#define MAX_GIDS 16
#define MAXFHSIZE 64
#ifndef NFS3_COOKIEVERFSIZE
#define NFS3_COOKIEVERFSIZE 8
#endif

/* The worstcase size of a header for read/write calls.
   If this value is not big enough, the data part will not be of
   an optimum size, but nothing bad will happen */
#define MAX_HDRLEN 416

/* The maximum size of our data buffers for Sunfish */
#define SFMAXDATABUFFER 32*1024

/* The maximum size of our data buffers for Moonfish */
#define MFMAXDATABUFFER 16*1024

/* The size to use for tx and rx buffers */
#define SFBUFFERSIZE (MAX_HDRLEN + SFMAXDATABUFFER)

/* The size to use for tx and rx buffers */
#define MFBUFFERSIZE (MAX_HDRLEN + MFMAXDATABUFFER)

/* The maximum pathname size that we support */
#define MAX_PATHNAME 1024

/* The filetype returned for directories */
#define DIR_FILETYPE 0xFFD

/* The filetype returned for files with the executable bit set */
#define UNIXEX_FILETYPE 0xFE6

/* The filetype used for Mac resource forks */
#define MACFORKS_FILETYPE 0xFFD

/* Size of the fifo to use for pipelining. Must be at least 2, to allow
   READDIR to double buffer. Greater than 2 has negligible increase in
   performance. */
#define FIFOSIZE 2

#ifndef NSTAT
#define NSTAT
typedef int nstat;
#endif


typedef _kernel_oserror os_error;

/* Generate an os_error block */
os_error *gen_error(int num, char *msg, ...);

extern void (*error_func)(void);



void syslogf(char *logname, int level, char *fmt, ...);

void log_error(os_error *err);


/* Convert unix leafname into RISC OS format */
int filename_riscosify(const char *name, int namelen, char *buffer, int buflen, int *filetype, int defaultfiletype, int xyzext, int macforks);

/* Convert a RISC OS leafname into unix format */
char *filename_unixify(const char *name, unsigned int len, unsigned int *newlen, struct pool *pool);

/* Get the local alphabet encoding */
char *encoding_getlocal(void);

/* Use MimeMap to lookup a filetype from an extension */
int ext_to_filetype(char *ext, int defaultfiletype);

/* Use MimeMap to lookup a mimetype from a filetype */
char *filetype_to_mimetype(int filetype, struct pool *pool);

/* Convert a unix mode to RISC OS attributes */
unsigned int mode_to_attr(unsigned int mode);

/* Convert RISC OS attributes to a unix mode */
unsigned attr_to_mode(unsigned attr, unsigned oldmode, unsigned umask, unsigned unumask);

/* Add ,xyz onto the leafname if necessary */
char *addfiletypeext(char *leafname, unsigned int len, int extfound, int newfiletype, unsigned int *newlen, int defaultfiletype, int xyzext, int unixexfiletype, int macforks, struct pool *pool);

/* Drop to user mode to trigger any pending callbacks */
void trigger_callback(void);

/* Convert an IP address to a string */
char *host_to_str(unsigned int host, struct pool *pool);

/* Convert a RISC OS error number to an NFS error */
nstat oserr_to_nfserr(int errnum);

/* Convert NFSv4 specific errors to their nearest NFSv2/NFSv3 equivalent */
nstat nfserr_removenfs4(nstat errnum);

/* Convert a RISC OS load and execution address into a unix timestamp */
void loadexec_to_timeval(unsigned load, unsigned exec, unsigned *seconds, unsigned *nseconds, int nfs2);

/* Convert a unix timestamp into a RISC OS load and execution address */
void timeval_to_loadexec(unsigned seconds, unsigned nseconds, int filetype, unsigned *load, unsigned *exec, int nfs2);


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


#ifndef NFS_OK
#define NFS_OK 0
#endif

#ifndef NFSERR_PERM
#define NFSERR_PERM 1
#endif

#ifndef NFSERR_NOENT
#define NFSERR_NOENT 2
#endif

#ifndef NFSERR_IO
#define NFSERR_IO 5
#endif

#ifndef NFSERR_NXIO
#define NFSERR_NXIO 6
#endif

#ifndef NFSERR_ACCESS
#define NFSERR_ACCESS 13
#endif

#ifndef NFSERR_EXIST
#define NFSERR_EXIST 17
#endif

#ifndef NFSERR_XDEV
#define NFSERR_XDEV 18
#endif

#ifndef NFSERR_NOTDIR
#define NFSERR_NOTDIR 20
#endif

#ifndef NFSERR_ISDIR
#define NFSERR_ISDIR 21
#endif

#ifndef NFSERR_INVAL
#define NFSERR_INVAL 22
#endif

#ifndef NFSERR_FBIG
#define NFSERR_FBIG 27
#endif

#ifndef NFSERR_NOSPC
#define NFSERR_NOSPC 28
#endif

#ifndef NFSERR_ROFS
#define NFSERR_ROFS 30
#endif

#ifndef NFSERR_MLINK
#define NFSERR_MLINK 31
#endif

#ifndef NFSERR_NAMETOOLONG
#define NFSERR_NAMETOOLONG 63
#endif

#ifndef NFSERR_NOTEMPTY
#define NFSERR_NOTEMPTY 66
#endif

#ifndef NFSERR_DQUOT
#define NFSERR_DQUOT 69
#endif

#ifndef NFSERR_STALE
#define NFSERR_STALE 70
#endif

#ifndef NFSERR_BADHANDLE
#define NFSERR_BADHANDLE 10001
#endif

#ifndef NFSERR_BAD_COOKIE
#define NFSERR_BAD_COOKIE 10003
#endif

#ifndef NFSERR_NOTSUPP
#define NFSERR_NOTSUPP 10004
#endif

#ifndef NFSERR_TOOSMALL
#define NFSERR_TOOSMALL 10005
#endif

#ifndef NFSERR_SERVERFAULT
#define NFSERR_SERVERFAULT 10006
#endif

#ifndef NFSERR_BADTYPE
#define NFSERR_BADTYPE 10007
#endif

#ifndef NFSERR_DELAY
#define NFSERR_DELAY 10008
#endif

#ifndef NFSERR_SAME
#define NFSERR_SAME 10009
#endif

#ifndef NFSERR_DENIED
#define NFSERR_DENIED 10010
#endif

#ifndef NFSERR_EXPIRED
#define NFSERR_EXPIRED 10011
#endif

#ifndef NFSERR_LOCKED
#define NFSERR_LOCKED 10012
#endif

#ifndef NFSERR_GRACE
#define NFSERR_GRACE 10013
#endif

#ifndef NFSERR_FHEXPIRED
#define NFSERR_FHEXPIRED 10014
#endif

#ifndef NFSERR_SHARE_DENIED
#define NFSERR_SHARE_DENIED 10015
#endif

#ifndef NFSERR_WRONGSEC
#define NFSERR_WRONGSEC 10016
#endif

#ifndef NFSERR_CLID_INUSE
#define NFSERR_CLID_INUSE 10017
#endif

#ifndef NFSERR_RESOURCE
#define NFSERR_RESOURCE 10018
#endif

#ifndef NFSERR_MOVED
#define NFSERR_MOVED 10019
#endif

#ifndef NFSERR_NOFILEHANDLE
#define NFSERR_NOFILEHANDLE 10020
#endif

#ifndef NFSERR_MINOR_VERS_MISMATCH
#define NFSERR_MINOR_VERS_MISMATCH 10021
#endif

#ifndef NFSERR_STALE_CLIENTID
#define NFSERR_STALE_CLIENTID 10022
#endif

#ifndef NFSERR_STALE_STATEID
#define NFSERR_STALE_STATEID 10023
#endif

#ifndef NFSERR_OLD_STATEID
#define NFSERR_OLD_STATEID 10024
#endif

#ifndef NFSERR_BAD_STATEID
#define NFSERR_BAD_STATEID 10025
#endif

#ifndef NFSERR_BAD_SEQID
#define NFSERR_BAD_SEQID 10026
#endif

#ifndef NFSERR_NOT_SAME
#define NFSERR_NOT_SAME 10027
#endif

#ifndef NFSERR_LOCK_RANGE
#define NFSERR_LOCK_RANGE 10028
#endif

#ifndef NFSERR_SYMLINK
#define NFSERR_SYMLINK 10029
#endif

#ifndef NFSERR_RESTOREFH
#define NFSERR_RESTOREFH 10030
#endif

#ifndef NFSERR_LEASE_MOVED
#define NFSERR_LEASE_MOVED 10031
#endif

#ifndef NFSERR_ATTRNOTSUPP
#define NFSERR_ATTRNOTSUPP 10032
#endif

#ifndef NFSERR_NO_GRACE
#define NFSERR_NO_GRACE 10033
#endif

#ifndef NFSERR_RECLAIM_BAD
#define NFSERR_RECLAIM_BAD 10034
#endif

#ifndef NFSERR_RECLAIM_CONFLICT
#define NFSERR_RECLAIM_CONFLICT 10035
#endif

#ifndef NFSERR_BADXDR
#define NFSERR_BADXDR 10036
#endif

#ifndef NFSERR_LOCKS_HELD
#define NFSERR_LOCKS_HELD 10037
#endif

#ifndef NFSERR_OPENMODE
#define NFSERR_OPENMODE 10038
#endif

#ifndef NFSERR_BADOWNER
#define NFSERR_BADOWNER 10039
#endif

#ifndef NFSERR_BADCHAR
#define NFSERR_BADCHAR 10040
#endif

#ifndef NFSERR_BADNAME
#define NFSERR_BADNAME 10041
#endif

#ifndef NFSERR_BAD_RANGE
#define NFSERR_BAD_RANGE 10042
#endif

#ifndef NFSERR_LOCK_NOTSUPP
#define NFSERR_LOCK_NOTSUPP 10043
#endif

#ifndef NFSERR_OP_ILLEGAL
#define NFSERR_OP_ILLEGAL 10044
#endif

#ifndef NFSERR_DEADLOCK
#define NFSERR_DEADLOCK 10045
#endif

#ifndef NFSERR_FILE_OPEN
#define NFSERR_FILE_OPEN 10046
#endif

#ifndef NFSERR_ADMIN_REVOKED
#define NFSERR_ADMIN_REVOKED 10047
#endif

#ifndef NFSERR_CB_PATH_DOWN
#define NFSERR_CB_PATH_DOWN 10048
#endif

#ifdef __cplusplus
}
#endif

#endif

