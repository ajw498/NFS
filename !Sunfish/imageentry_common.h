/*
	$Id$
	$URL$

	Common bits for all entry points
*/

#ifndef IMAGEENTRY_COMMON_H
#define IMAGEENTRY_COMMON_H


#include <kernel.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "nfs-structs.h"

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

#define OBJ_NONE 0
#define OBJ_FILE 1
#define OBJ_DIR  2


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

/* Directory not empty error number must match what filecore uses */ 
#define ERRDIRNOTEMPTY 67764


#define MAX_GIDS 16

/* All infomation associated with an open file */
struct file_handle {
	struct conn_info *conn;
	char fhandle[FHSIZE];
	unsigned int extent;
	unsigned int load;
	unsigned int exec;
	enum ftype type;
};


/* All infomation associated with a connection */
struct conn_info {
	char *server;
	unsigned int portmapper_port;
	unsigned int mount_port;
	unsigned int nfs_port;
	unsigned int pcnfsd_port;
	char *export;
	char rootfh[FHSIZE];
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
	char lastdir[MAXNAMLEN];
	int lastcookie;
	int laststart;
	char lastdirhandle[FHSIZE];
};

extern int enablelog;

typedef _kernel_oserror os_error;

/* Generate an os_error block */
os_error *gen_error(int num, char *msg, ...);


#define LOGNAME "Sunfish"
#define LOGENTRY 40
#define LOGEXIT  60
#define LOGERROR 20
#define LOGDATASUMMARY 70
#define LOGDATA  130


void syslogf(char *logname, int level, char *fmt, ...);

void log_error(os_error *err);


/* Generate a RISC OS error block based on the NFS status code */
os_error *gen_nfsstatus_error(enum nstat stat);

/* Convert unix leafname into RISC OS format */
int filename_riscosify(char *name, int namelen, char *buffer, int buflen, int *filetype, struct conn_info *conn);

/* Convert a leafname into an nfs handle, following symlinks as necessary */
os_error *leafname_to_finfo(char *leafname, unsigned int *len, int simple, char *dirhandle, struct diropok **finfo, enum nstat *status, struct conn_info *conn);

/* Convert a full filename/dirname into an nfs handle */
os_error *filename_to_finfo(char *filename, struct diropok **dinfo, struct diropok **finfo, char **leafname, int *filetype, int *extfound, struct conn_info *conn);

/* Convert a unix timestamp into a RISC OS load and execution address */
void timeval_to_loadexec(struct ntimeval *unixtime, int filetype, unsigned int *load, unsigned int *exec);

/* Convert a RISC OS load and execution address into a unix timestamp */
void loadexec_to_timeval(unsigned int load, unsigned int exec, struct ntimeval *unixtime);

/* Convert a unix mode to RISC OS attributes */
unsigned int mode_to_attr(unsigned int mode);

/* Convert RISC OS attributes to a unix mode */
unsigned int attr_to_mode(unsigned int attr, unsigned int oldmode, struct conn_info *conn);

/* Add ,xyz onto the leafname if necessary */
char *addfiletypeext(char *leafname, unsigned int len, int extfound, int newfiletype, unsigned int *newlen, struct conn_info *conn);

/* Drop to user mode to trigger any pending callbacks */
void trigger_callback(void);

#endif

