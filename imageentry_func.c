/*
	$Id$

	Routines for ImageEntry_Func
*/


#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <swis.h>
#include <unixlib.h>

#include "imageentry_func.h"

#include "rpc-calls.h"
#include "nfs-calls.h"
#include "mount-calls.h"
#include "portmapper-calls.h"

#define NOMEM 1
#define NOMEMMESS "Out of memory"

/*FIXME*/
#define IMAGEERRBASE 20


#define NFSSTATBASE 1
/*FIXME*/

#define MAX_PAYLOAD 7000
/*FIXME*/

/*FIXME - verify stack usage is < 1024 bytes */

/* Generate a RISC OS error block based on the NFS status code */
static os_error *gen_nfsstatus_error(enum nstat stat)
{
	char *str;

	switch (stat) {
		case NFSERR_PERM: str = "Not owner"; break;
		case NFSERR_NOENT: str = "No such file or directory"; break;
		case NFSERR_IO: str = "IO error"; break;
		case NFSERR_NXIO: str = "No such device or address"; break;
		case NFSERR_ACCES: str = "Permission denied"; break;
		case NFSERR_EXIST: str = "File already exists"; break;
		case NFSERR_NODEV: str = "No such device"; break;
		case NFSERR_NOTDIR: str = "Not a directory"; break;
		case NFSERR_ISDIR: str = "Is a directory"; break;
		case NFSERR_FBIG: str = "File too large"; break;
		case NFSERR_NOSPC: str = "No space left on device"; break;
		case NFSERR_ROFS: str = "Read only filesystem"; break;
		case NFSERR_NAMETOOLONG: str = "File name too long"; break;
		case NFSERR_NOTEMPTY: str = "Directory not empty"; break;
		case NFSERR_DQUOT: str = "Disc quota exceeded"; break;
		case NFSERR_STALE: str = "Stale NFS filehandle"; break;
		default: str = "Unknown error"; break;
	}
	return gen_error(NFSSTATBASE, "NFS call failed (%s)", str);
}

static void free_conn_info(struct conn_info *conn)
{
	if (conn == NULL) return;
	if (conn->config) free(conn->config);
	if (conn->auth) free(conn->auth);
	free(conn);
}

os_error *func_closeimage(struct conn_info *conn)
{
	string dir;
	os_error *err;

	dir.size = strlen(conn->export);
	dir.data = conn->export;
	err = MNTPROC_UMNT(&dir, conn);
	if (err && enablelog) log_error(err);

	/* Close socket etc. */
	err = rpc_close_connection(conn);
	if (err && enablelog) log_error(err);

	if (conn->logging) enablelog--;

	free_conn_info(conn);
	/* Don't return an error as neither of the above is essential,
	   and giving an error will prevent killing the module */
	return NULL;
}

#define CHECK(str) (strncasecmp(line,str,sizeof(str))==0)


static os_error *parse_line(char *line, struct conn_info *conn)
{
	char *val;

	/* Find the end of the field */
	val = line;
	while (*val && *val != ':') val++;
	if (*val) *val++ = '\0';
	/* Find the start of the value.
	   Trailing spaces have already been removed by parse_file */
	while (isspace(*val)) val++;

	if (CHECK("#")) {
		/* A comment */
	} else if (CHECK("Protocol")) {
		if (strcasecmp(val, "NFS2") != 0) {
			return gen_error(IMAGEERRBASE + 0, "Unknown protocol '%s'", val);
		}
	} else if (CHECK("Server")) {
		conn->server = val;
	} else if (CHECK("MachineName")) {
		conn->machinename = val;
	} else if (CHECK("PortMapperPort")) {
		conn->portmapper_port = (int)strtol(val, NULL, 10);
	} else if (CHECK("MountPort")) {
		conn->mount_port = (int)strtol(val, NULL, 10);
	} else if (CHECK("NFSPort")) {
		conn->nfs_port = (int)strtol(val, NULL, 10);
	} else if (CHECK("Export")) {
		conn->export = val;
	} else if (CHECK("UID")) {
		conn->uid = (int)strtol(val, NULL, 10);
	} else if (CHECK("GID")) {
		conn->gid = (int)strtol(val, NULL, 10);
	} else if (CHECK("GIDs")) {
		conn->gids = val;
	} else if (CHECK("Username")) {
		conn->username = val;
	} else if (CHECK("Password")) {
		conn->password = val;
	} else if (CHECK("Logging")) {
		conn->logging = (int)strtol(val, NULL, 10);
	} else if (CHECK("umask")) {
		conn->umask = (int)strtol(val, NULL, 8); /* umask is specified in octal */
	} else if (CHECK("ShowHidden")) {
		conn->hidden = (int)strtol(val, NULL, 10);
	} else if (CHECK("Timeout")) {
		conn->timeout = (int)strtol(val, NULL, 10);
	} else if (CHECK("Retries")) {
		conn->retries = (int)strtol(val, NULL, 10);
	}
	/* Ignore unrecognised lines */
	return NULL;
}

/* Read a config file and fill in the conn structure */
static os_error *parse_file(unsigned int fileswitchhandle, struct conn_info *conn)
{
	char *ch;
	char *end;
	unsigned int size;
	unsigned int remain;
	os_error *err;

	/* Get filesize */
	err = _swix(OS_Args, _INR(0,1) | _OUT(2), 2, fileswitchhandle, &size);
	if (err) return err;
	
	conn->config = malloc(size + 1); /* Allow space for a terminating 0 */
	if (conn->config == NULL) return gen_error(NOMEM,NOMEMMESS);

	/* Load entire file into memory */
	err = _swix(OS_GBPB, _INR(0,4) | _OUT(3), 3, fileswitchhandle, conn->config, size, 0, &remain);
	if (err) return err;
	size -= remain;

	/* Split the file into lines */
	ch = conn->config;
	end = ch + size;
	do {
		char *line = ch;
		char *endspc;

		/* Find end of line */
		while (ch < end && *ch != '\n') ch++;
		*ch++ = '\0';
		/* Strip trailing spaces */
		endspc = ch - 2;
		while (endspc > line && isspace(*endspc)) endspc--;
		err = parse_line(line, conn);
	} while (ch < end && err == NULL);

	return err;
}

static os_error *create_auth(struct conn_info *conn)
{
	struct auth_unix auth_unix;
	unsigned int gids[16];

	/* Make an estimate of the maximum size needed for the
	   auth structure */
	conn->authsize = sizeof(struct auth_unix) + strlen(conn->machinename) + 16 * 4;
	conn->auth = malloc(conn->authsize);
	if (conn == NULL) return gen_error(NOMEM,NOMEMMESS);

	auth_unix.stamp = 0;
	auth_unix.machinename.data = conn->machinename;
	auth_unix.machinename.size = strlen(conn->machinename);
	auth_unix.uid = conn->uid;
	auth_unix.gid = conn->gid;
	auth_unix.gids.size = 0;
	auth_unix.gids.data = gids;
	if (conn->gids) {
		char *str = conn->gids;
		char *end = str;

		while (*end && auth_unix.gids.size < 16) {
			gids[auth_unix.gids.size++] = (int)strtol(str, &end, 10);
			str = end;
		}
	}
	buf = conn->auth;
	bufend = conn->auth + conn->authsize;
	process_struct_auth_unix(0, auth_unix, 0);
	conn->authsize = buf - conn->auth;

	return NULL;
buffer_overflow:
	return gen_error(1,"foobared");
}

os_error *func_newimage(unsigned int fileswitchhandle, struct conn_info **myhandle)
{
	struct conn_info *conn;
	string dir;
	struct fhstatus rootfh;
	os_error *err;

	/* Allocate the conn structure which hold all information about the mount.
	   The image filing system internal handle is a pointer to this struct */
	conn = malloc(sizeof(struct conn_info));
	if (conn == NULL) return gen_error(NOMEM,NOMEMMESS);
	*myhandle = conn;

	/* Set defaults */
	conn->config = NULL;
	conn->portmapper_port = PMAP_PORT;
	conn->mount_port = 0;
	conn->nfs_port = 0;
	conn->server = "";
	conn->export = "";
	conn->timeout = 5;
	conn->retries = 2;
	conn->hidden = 1;
	conn->umask = 022;
	conn->username = NULL;
	conn->password = NULL;
	conn->uid = 0;
	conn->gid = 0;
	conn->gids = NULL;
	conn->logging = 0;
	conn->auth = NULL;
	conn->machinename = NULL;

	/* Read details from file */
	err = parse_file(fileswitchhandle, conn);
	if (err) {
		free_conn_info(conn);
		return err;
	}

	if (conn->logging) enablelog++;

	if (conn->password) {
		/* use pcnfsd to map to uid/gid */
	}

	if (conn->machinename == NULL) {
		conn->machinename = ""; /* FIXME: get hostname */
	}

	/* Create the opaque auth structure */
	err = create_auth(conn);
	if (err) {
		free_conn_info(conn);
		return err;
	}

	/* Initialise socket etc. */
	err = rpc_init_connection(conn);
	if (err) {
		free_conn_info(conn);
		return err;
	}

	/* Get port numbers if not specified */
	if (conn->mount_port == 0) {
		struct mapping map = {MOUNT_RPC_PROGRAM,MOUNT_RPC_VERSION,IPPROTO_UDP,0};
		int port;

		err = PMAPPROC_GETPORT(&map,&port,conn);
		if (err) {
			free_conn_info(conn);
			return err;
		}
		conn->mount_port = port;
	}

	if (conn->nfs_port == 0) {
		struct mapping map = {NFS_RPC_PROGRAM,NFS_RPC_VERSION,IPPROTO_UDP,0};
		int port;

		err = PMAPPROC_GETPORT(&map,&port,conn);
		if (err) {
			free_conn_info(conn);
			return err;
		}
		conn->nfs_port = port;
	}

	/* Get a filehandle for the root directory */
	dir.size = strlen(conn->export);
	dir.data = conn->export;
	err = MNTPROC_MNT(&dir, &rootfh, conn);
	if (err) {
		free_conn_info(conn);
		return err;
	}
	if (rootfh.status != 0) {
		free_conn_info(conn);
		/* status is an errno value. Probably the same as an NFS status value. */
		return gen_nfsstatus_error((enum nstat)rootfh.status);
	}
	memcpy(conn->rootfh, rootfh.u.directory, FHSIZE);

	return NULL;
}


struct dir_entry {
	unsigned int load;
	unsigned int exec;
	unsigned int len;
	unsigned int attr;
	unsigned int type;
};

#define return_error(msg) return gen_error(1,msg)

#define OBJ_NONE 0
#define OBJ_FILE 1
#define OBJ_DIR  2

static char *filename_unixify(char *name, int len)
{
	static char namebuffer[MAXNAMLEN + 1];
	int i;

	if (len > MAXNAMLEN) len = MAXNAMLEN; /*?*/

	for (i = 0; i < len; i++) {
		switch (name[i]) {
			case '/':
				namebuffer[i] = '.';
				break;
			case 160: /*hard space*/
				namebuffer[i] = ' ';
				break;
			/*case '&': etc */
			default:
				namebuffer[i] = name[i];
		}
	}
	namebuffer[len] = '\0';
	return namebuffer;
}

#define MimeMap_Translate 0x50B00
#define MMM_TYPE_RISCOS 0
#define MMM_TYPE_RISCOS_STRING 1
#define MMM_TYPE_MIME 2
#define MMM_TYPE_DOT_EXTN 3

static int filename_riscosify(char *name, int len, char **buffer)
{
	int i;
	int filetype = -1;
	char *lastdot = NULL;

	for (i = 0; i < len; i++) {
		switch (name[i]) {
			case '.':
				(*buffer)[i] = '/';
				lastdot = name + i;
				break;
			case ' ':
				(*buffer)[i] = 160;
				break;
			/*case '&': etc */
			case ',':
				if (len - i == 4 && isxdigit(name[i+1]) && isxdigit(name[i+2]) && isxdigit(name[i+3])) {
					char tmp[4];
					tmp[0] = name[i+1];
					tmp[1] = name[i+2];
					tmp[2] = name[i+3];
					tmp[3] = '\0';
					filetype = (int)strtol(tmp, NULL, 16);
					len -= 4;
				} else {
					(*buffer)[i] = name[i];
				}
				break;
			default:
				(*buffer)[i] = name[i];
		}
	}
	(*buffer)[len] = '\0';
	(*buffer) += (len + 4) & ~3;
    if (filetype == -1) {
    	if (lastdot) {
			os_error *err;
		
			/* No explicit ,xyz found, so try MimeMap to get a filetype */
			err = _swix(MimeMap_Translate,_INR(0,2) | _OUT(3), MMM_TYPE_DOT_EXTN, lastdot + 1, MMM_TYPE_RISCOS, &filetype);
			if (err) filetype = 0xFFF;
		} else {
			/* No ,xyz and no extension, so default to text */
			filetype = 0xFFF;
		}
	}
	return filetype;
}

/* Find leafname,xyz given leafname by enumerating the entire directory
   until a matching file is found. If there are two matching files, the
   first found is used. This function would benefit in many cases by some
   cacheing of leafnames and extensions. The returned leafname is a pointer
   to static data and should be copied before any subsequent calls. */
static os_error *lookup_leafname(char *dhandle, struct opaque *leafname, struct opaque **found, struct conn_info *conn)
{
	static struct readdirargs rddir;
	static struct readdirres rdres;
	struct entry *direntry;
	os_error *err;

	memcpy(rddir.dir, dhandle, FHSIZE);
	rddir.count = MAX_PAYLOAD;
	rddir.cookie = 0;

	do {
		err = NFSPROC_READDIR(&rddir, &rdres, conn);
		if (err) return err;
		if (rdres.status != NFS_OK) return gen_nfsstatus_error(rdres.status);

		direntry = rdres.u.readdirok.entries;
		while (direntry) {
			rddir.cookie = direntry->cookie;

			if (direntry->name.size == leafname->size + sizeof(",xyz") - 1) {
				if (strncmp(direntry->name.data, leafname->data, leafname->size) == 0) {
					if (direntry->name.data[leafname->size] == ','
					     && isxdigit(direntry->name.data[leafname->size + 1])
					     && isxdigit(direntry->name.data[leafname->size + 2])
					     && isxdigit(direntry->name.data[leafname->size + 3])) {
						*found = &(direntry->name);
						return NULL;
					}
				}
			}
			direntry = direntry->next;
		}
	} while (!rdres.u.readdirok.eof);

	*found = NULL;
	return NULL;
}

/* Convert a full filename/dirname into a leafname, and find the nfs handle
   for the file/dir and for the directory containing it.
   Returns a pointer to static data.
   This function would really benefit from some cacheing.
   In theory it should deal with wildcarded names, but that is just too much
   hassle. And, really, that should be the job of fileswitch. */
static os_error *filename_to_finfo(char *filename, struct diropok **dinfo, struct diropok **finfo, char **leafname, struct conn_info *conn)
{
	char *start = filename;
	char *end;
	char *dirhandle = conn->rootfh;
	struct diropargs current;
	static struct diropres next;
	os_error *err;

	if (dinfo) *dinfo = NULL; /* A NULL directory indicates the root directory */
	if (finfo) *finfo = NULL; /* A NULL file indicates it wasn't found */

	do {
		end = start;
		/* Find end of dirname segment */
		while (*end && *end != '.') end++;

		current.name.data = filename_unixify(start, end - start);
		current.name.size = end - start;

		if (leafname) *leafname = current.name.data;

		memcpy(current.dir, dirhandle, FHSIZE);
		err = NFSPROC_LOOKUP(&current, &next, conn);
		if (err) return err;

		/* It is not an error if the file isn't found, but the
		   containing directory must exist */
		if (*end == '\0' && next.status == NFSERR_NOENT) break;
		if (next.status != NFS_OK) return gen_nfsstatus_error(next.status);

		if (*end != '\0') {
			if (next.u.diropok.attributes.type != NFDIR) {
				/* Every segment except the leafname must be a directory */
				return gen_nfsstatus_error(NFSERR_NOTDIR);
			}
			if (dinfo) *dinfo = &(next.u.diropok);
		}

		dirhandle = next.u.diropok.file;
		start = end + 1;
	} while (*end != '\0');

	/* By this point, the directory has been found, the file may or
	   may not have been found (either because it doesn't exist, or
	   because we need to add an ,xyz extension) */

	/* There's no need to continue if the caller doesn't care about the file */
	if (finfo == NULL) return NULL;

	if (next.status == NFSERR_NOENT) {
		if (1/*conn->xyz_ext != NEVER*/) { /*FIXME */
			struct opaque *file;
	
			err = lookup_leafname(dirhandle, &(current.name), &file, conn);
			if (err) return err;
	
			if (file) {
				/* A matching file,xyz was found */
				current.name.data = file->data;
				current.name.size = file->size;
	
				/* Lookup the handle with the new leafname */
				err = NFSPROC_LOOKUP(&current, &next, conn);
				if (err) return err;
				if (next.status != NFS_OK) return gen_nfsstatus_error(next.status);
	
				*finfo = &(next.u.diropok);
			}
		}
	} else {
		/* The file was found without needing to add ,xyz */
		*finfo = &(next.u.diropok);
	}
	return NULL;
}

/* Convert a unix timestamp into a RISC OS load and execution address */
static void timeval_to_loadexec(struct ntimeval *unixtime, int filetype, unsigned int *load, unsigned int *exec)
{
	uint64_t csecs;

	csecs = unixtime->seconds;
	csecs *= 100;
	csecs += ((int64_t)unixtime->useconds / 10000);
	csecs += 0x336e996a00; /* Difference between 1900 and 1970 */
	*load = (unsigned int)((csecs >> 32) & 0xFF);
	*load |= (0xFFF00000 | ((filetype & 0xFFF) << 8));
	*exec = (unsigned int)(csecs & 0xFFFFFFFF);
}

/* Convert a RISC OS load and execution address into a unix timestamp */
static void loadexec_to_timeval(unsigned int load, unsigned int exec, struct ntimeval *unixtime)
{
	uint64_t csecs, secs;

	csecs = exec;
	csecs |= ((uint64_t)load & 0xFF) << 32;
	csecs -= 0x336e996a00; /* Difference between 1900 and 1970 */
    secs = csecs / (uint64_t)100;
    unixtime->seconds = (unsigned int)(secs & 0xFFFFFFFF);
    unixtime->useconds = (unsigned int)((csecs % 100) * 10000);
}

/* Convert a unix mode to RISC OS attributes */
static unsigned int mode_to_attr(unsigned int mode)
{
	unsigned int attr;
	attr  = (mode & 0x100) >> 8; /* Owner read */
	attr |= (mode & 0x080) >> 6; /* Owner write */
	attr |= (mode & 0x004) << 2; /* Others read */
	attr |= (mode & 0x002) << 4; /* Others write */
	return attr;
}

/* Convert a unix mode to RISC OS file information word */
static unsigned int mode_to_fileinfo(unsigned int mode)
{
	unsigned int attr;
	attr  = (mode & 0x100) << 22; /* Owner read */
	attr |= (mode & 0x080) << 24; /* Owner write */
	/* FIXME: This is wrong, as we may not be the owner */
	return attr;
}

/* Read directory entries and possibly info */
os_error *func_readdirinfo(int info, char *dirname, void *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos)
{
	char *bufferpos;
	struct readdirargs rddir;
	struct readdirres rdres;
	struct entry *direntry;
	os_error *err;

	bufferpos = buffer;
	*objsread = 0;

	if (dirname[0] == '\0') {
		/* An empty dirname specifies the root directory */
		memcpy(rddir.dir, conn->rootfh, FHSIZE);
	} else {
		struct diropok *finfo;

		/* Find the handle of the directory */
		err = filename_to_finfo(dirname, NULL, &finfo, NULL, conn);
		if (err) return err;
		if (finfo == NULL) return gen_nfsstatus_error(NFSERR_NOENT);

		memcpy(rddir.dir, finfo->file, FHSIZE);
	}

	rddir.count = MAX_PAYLOAD;
	rddir.cookie = start;
	err = NFSPROC_READDIR(&rddir, &rdres, conn);
	if (err) return err;
	if (rdres.status != NFS_OK) return gen_nfsstatus_error(rdres.status);

	/* We may need to do a lookup on each filename, but this would
	   corrupt the rx buffer which holds our list, so swap buffers */
	swap_rxbuffers();

	direntry = rdres.u.readdirok.entries;
	while (direntry && *objsread < numobjs) {
		if (direntry->name.size == 0) {
			/* Ignore null names */
		} else if (!conn->hidden && direntry->name.data[0] == '.') {
			/* Ignore hidden files if so configured */
		} else if (direntry->name.size == 1 && direntry->name.data[0] == '.') {
			/* current dir */
		} else if (direntry->name.size == 2 && direntry->name.data[0] == '.' && direntry->name.data[1] == '.') {
			/* parent dir */
		} else {
			struct dir_entry *info_entry = NULL;
			int filetype;

			if (info) {
				info_entry = (struct dir_entry *)bufferpos;
				bufferpos += sizeof(struct dir_entry);
			}

			/* Check there is room in the output buffer.
			   This check may be slightly pessimistic. */
			if (bufferpos + direntry->name.size + 4 > (char *)buffer + buflen) break;

			/* Copy leafname into output buffer, translating some
			   chars and stripping any ,xyz */
			filetype = filename_riscosify(direntry->name.data, direntry->name.size, &bufferpos);

			if (info) {
				struct diropargs lookupargs;
				struct diropres lookupres;

				/* Lookup file attributes.
				   READDIRPLUS in NFS3 would eliminate this call. */
				memcpy(lookupargs.dir, rddir.dir, FHSIZE);
				lookupargs.name.data = direntry->name.data;
				lookupargs.name.size = direntry->name.size;

				err = NFSPROC_LOOKUP(&lookupargs, &lookupres, conn);
				if (err) return err;
				if (lookupres.status != NFS_OK) return gen_nfsstatus_error(lookupres.status);

				timeval_to_loadexec(&(lookupres.u.diropok.attributes.mtime), filetype, &(info_entry->load), &(info_entry->exec));
				info_entry->len = lookupres.u.diropok.attributes.size;
				info_entry->attr = mode_to_attr(lookupres.u.diropok.attributes.mode);
				info_entry->type = lookupres.u.diropok.attributes.type == NFDIR ? OBJ_DIR : OBJ_FILE;
			}

			(*objsread)++;
		}
		start = direntry->cookie;
		direntry = direntry->next;
	}

	if (direntry == NULL && rdres.u.readdirok.eof) {
		*continuepos = -1;
		/* If there are entries still in the buffer that won't fit in the
		   output buffer then we should cache them to save rerequesting
		   them on the next iteration */
	} else {
		*continuepos = start;
		/* At this point we could speculativly request the next set of
		   entries to reduce the latency on the next request */
	}
	return NULL;
}

/* Open a file. The handle returned is a pointer to a struct holding the
   NFS handle and any other info needed */
os_error *open_file(char *filename, int access, struct conn_info *conn, int *file_info_word, int *internal_handle, int *extent)
{
	struct file_handle *handle;
    struct diropok *dinfo;
    struct diropok *finfo;
    struct createargs createargs;
    struct diropres createres;
    os_error *err;
    char *leafname;

	err = filename_to_finfo(filename, &dinfo, &finfo, &leafname, conn);
	if (err) return err;

	if (finfo == NULL) {
		if (access == 1) {
			/* Create and open for update */
			memcpy(createargs.where.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
			createargs.where.name.data = leafname;
			createargs.where.name.size = strlen(leafname);
			createargs.attributes.mode = 0x00008000 | (0666 & ~conn->umask); /* FIXME: umask needs testing */
			createargs.attributes.uid = -1;
			createargs.attributes.gid = -1;
			createargs.attributes.size = 0;
			createargs.attributes.atime.seconds = -1;
			createargs.attributes.atime.useconds = -1;
			createargs.attributes.mtime.seconds = -1;
			createargs.attributes.mtime.useconds = -1;

			err = NFSPROC_CREATE(&createargs, &createres, conn);
			if (err) return err;
			if (createres.status != NFS_OK) return gen_nfsstatus_error(createres.status);

			finfo = &(createres.u.diropok);
		} else {
			/* File not found */
			*internal_handle = 0;
			return NULL;
		}
	}

	handle = malloc(sizeof(struct file_handle));
	if (handle == NULL) return gen_error(NOMEM, NOMEMMESS);

	handle->conn = conn;
	memcpy(handle->fhandle, finfo->file, FHSIZE);

	*internal_handle = (int)handle;
	*file_info_word = mode_to_fileinfo(finfo->attributes.mode);
	*extent = finfo->attributes.size;
	return NULL;
}

os_error *close_file(struct file_handle *handle, unsigned int load, unsigned int exec)
{
#if 0 /* loadexec_to_timeval seems to be broken, and this operation is likely to fail if we aren't the file owner */
	struct sattrargs sattrargs;
	struct attrstat sattrres;
	os_error *err;

	/* Set a new timestamp.
	   Ignore the filetype, as there is no API for applications to change a
	   filetype from a file handle so it shouldn't have changed */
	memcpy(sattrargs.file, handle->fhandle, FHSIZE);
	sattrargs.attributes.mode = -1;
	sattrargs.attributes.uid = -1;
	sattrargs.attributes.gid = -1;
	sattrargs.attributes.size = -1;
	sattrargs.attributes.atime.seconds = -1;
	sattrargs.attributes.atime.useconds = -1;
	loadexec_to_timeval(load, exec, &(sattrargs.attributes.mtime));

	err = NFSPROC_SETATTR(&sattrargs, &sattrres, handle->conn);
#endif
	free(handle); /* Free memory before returning even if there is an error */
#if 0
	if (err) return err;
	if (sattrres.status != NFS_OK) gen_nfsstatus_error(sattrres.status);
#endif
	return NULL;
}


os_error *get_bytes(struct file_handle *handle, char *buffer, unsigned len, unsigned offset)
{
	os_error *err;
	struct readargs args;
	struct readres res;

	memcpy(args.file, handle->fhandle, FHSIZE);
	do {
		args.offset = offset;
		args.count = len;
		if (args.count > MAX_PAYLOAD) args.count = MAX_PAYLOAD;
		offset += args.count;
		err = NFSPROC_READ(&args, &res, handle->conn);
		if (err) return err;
		if (res.status != NFS_OK) return gen_nfsstatus_error(res.status);
		if (res.u.data.size > args.count) return_error("unexpectedly large amount of data read");
		memcpy(buffer, res.u.data.data, res.u.data.size);
		buffer += args.count;
		len -= args.count;
	} while (len > 0 && res.u.data.size == args.count);
	return NULL;
}

os_error *put_bytes(struct file_handle *handle, char *buffer, unsigned len, unsigned offset)
{
	os_error *err;
	struct writeargs args;
	struct attrstat res;

	memcpy(args.file, handle->fhandle, FHSIZE);
	do {
		args.offset = offset;
		args.data.size = len;
		if (args.data.size > MAX_PAYLOAD) args.data.size = MAX_PAYLOAD;
		offset += args.data.size;
		len -= args.data.size;
		args.data.data = buffer;
		buffer += args.data.size;
		err = NFSPROC_WRITE(&args, &res, handle->conn);
		if (err) return err;
		if (res.status != NFS_OK) return gen_nfsstatus_error(res.status);
	} while (len > 0);
	return NULL;
}

os_error *file_readcatinfo(char *filename, struct conn_info *conn, int *objtype, int *load, int *exec, int *len, int *attr)
{
    struct diropok *finfo;
    os_error *err;

	err = filename_to_finfo(filename, NULL, &finfo, NULL, conn);
	if (err) return err;
	if (finfo == NULL) {
		*objtype = 0;
		return NULL;
	}

	*objtype = finfo->attributes.type == NFDIR ? OBJ_DIR : OBJ_FILE; /* other types? */
	*load = (int)0xFFFFFF00;
	*exec = 0;
	*len = finfo->attributes.size;
	*attr = 3;/*((finfo->attributes.mode & 0x400) >> 10) | ((finfo->attributes.mode & 0x200) >> 8);*/
	return NULL;
}

os_error *file_writecatinfo(char *filename, int load, int exec, int attr, struct conn_info *conn)
{
	/**/
	return NULL;
}

static os_error *createfile(char *filename, int dir, int load, int exec, char *buffer, char *buffer_end, struct conn_info *conn, char **leafname, char **fhandle)
{
    struct diropok *dinfo;
    os_error *err;
    struct createargs createargs;
    struct diropres createres;

	err = filename_to_finfo(filename, &dinfo, NULL, leafname, conn);
	if (err) return err;

	memcpy(createargs.where.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
	createargs.where.name.data = *leafname;
	createargs.where.name.size = strlen(*leafname);
	createargs.attributes.mode = 0x00008180; /**/
	createargs.attributes.uid = -1;
	createargs.attributes.gid = -1;
	createargs.attributes.size = buffer_end - buffer;
	createargs.attributes.atime.seconds = -1;
	createargs.attributes.atime.useconds = -1;
	createargs.attributes.mtime.seconds = -1;
	createargs.attributes.mtime.useconds = -1;
	if (dir) {
		err = NFSPROC_MKDIR(&createargs, &createres, conn);
	} else {
		err = NFSPROC_CREATE(&createargs, &createres, conn);
	}
	if (err) return err;
	if (createres.status != NFS_OK && createres.status != NFSERR_EXIST) return gen_nfsstatus_error(createres.status);
	if (fhandle) *fhandle = createres.u.diropok.file;
	return NULL;
}

os_error *file_createfile(char *filename, int load, int exec, char *buffer, char *buffer_end, struct conn_info *conn)
{
	char *leafname;
	return createfile(filename, 0, load, exec, buffer, buffer_end, conn, &leafname, NULL);
	/* Should createfile return an error if the file already exists? */
}

os_error *file_createdir(char *filename, int load, int exec, struct conn_info *conn)
{
	char *leafname;
	return createfile(filename, 1, load, exec, 0, 0, conn, &leafname, NULL);
}

os_error *file_savefile(char *filename, int load, int exec, char *buffer, char *buffer_end, struct conn_info *conn, char **leafname)
{
    os_error *err;
    char *fhandle;
	struct writeargs writeargs;
	struct attrstat writeres;

	if (buffer_end - buffer > 7000/*tx_buffersize*/) return_error("file too big");
	err = createfile(filename, 0, load, exec, buffer, buffer_end, conn, leafname, &fhandle);
	memcpy(writeargs.file, fhandle, FHSIZE);
	writeargs.offset = 0;
	writeargs.data.size = buffer_end - buffer;
	writeargs.data.data = buffer;
	err = NFSPROC_WRITE(&writeargs, &writeres, conn);
	
	return NULL;
}

os_error *file_delete(char *filename, struct conn_info *conn, int *objtype, int *load, int *exec, int *len, int *attr)
{
    struct diropok *dinfo;
    struct diropok *finfo;
    os_error *err;
    char *leafname;

	err = filename_to_finfo(filename, &dinfo, &finfo, &leafname, conn);
	if (err) return err;

	if (finfo == NULL) {
		*objtype = 0;
	} else {
	    struct diropargs removeargs;
	    enum nstat removeres;

		memcpy(removeargs.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
		removeargs.name.data = leafname;
		removeargs.name.size = strlen(leafname);
		if (finfo->attributes.type == NFDIR) {
			err = NFSPROC_RMDIR(&removeargs, &removeres, conn);
		} else {
			err = NFSPROC_REMOVE(&removeargs, &removeres, conn);
		}
		if (err) return err;
		if (removeres != NFS_OK) return gen_nfsstatus_error(removeres);
		*objtype = finfo->attributes.type == NFDIR ? 2 : 1;
		*load = (int)0xFFFFFF00;
		*exec = 0;
		*len = finfo->attributes.size;
		*attr = 3;/*((finfo->attributes.mode & 0x400) >> 10) | ((finfo->attributes.mode & 0x200) >> 8);*/
	}
	return NULL;
}

os_error *func_rename(char *oldfilename, char *newfilename, struct conn_info *conn, int *renamefailed)
{
    struct diropok *dinfo;
    struct diropok *finfo;
    struct renameargs renameargs;
    enum nstat renameres;
    os_error *err;
    char *leafname;

	err = filename_to_finfo(oldfilename, &dinfo, &finfo, &leafname, conn);
	if (err) return err;
	if (finfo == NULL) return_error("file not found");

	memcpy(renameargs.from.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
	renameargs.from.name.data = leafname;
	renameargs.from.name.size = strlen(leafname);

	if (strncmp(oldfilename, newfilename, leafname - oldfilename) == 0) {
		/* Both files are in the same directory */
		memcpy(renameargs.to.dir, renameargs.from.dir, FHSIZE);
		renameargs.to.name.data = newfilename + (leafname - oldfilename);
		renameargs.to.name.size = strlen(renameargs.to.name.data);
	} else {
		err = filename_to_finfo(newfilename, &dinfo, NULL, &leafname, conn);
		if (err) return err;
		memcpy(renameargs.to.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
		renameargs.to.name.data = leafname;
		renameargs.to.name.size = strlen(leafname);
	}

	err = NFSPROC_RENAME(&renameargs, &renameres, conn);
	if (err) return err;
	if (renameres != NFS_OK) return gen_nfsstatus_error(renameres);
	*renamefailed = 0;
	return NULL;
}
/*
os_error *args_writeextent(struct file_handle *handle, unsigned extent)
{
	os_error *err;
	struct sattrargs args;
	struct attrstat res;

	memcpy(args.file, handle->fhandle, FHSIZE);
	args.mode = -1;
	args.uid = -1;
	args.gid = -1;
	args.atime.seconds = -1;
	args.atime.useconds = -1;
	args.mtime.seconds = -1;
	args.mtime.useconds = -1;
	args.size = extent;
	err = NFSPROC_SETATTR(&args, &res, handle->conn);
	if (err) return err;
	if (res.status != NFS_OK) return gen_nfsstatus_error(res.status); */
	/* FIXME: should zero pad if new extent > old extent */  /*
	return NULL;
}

os_error *args_readdatestamp(struct file_handle *handle, int *load, int *exec)
{
	*load = 0;
	*exec = 0; *//* We should cache the filetype (and other attrs?) when the file is opened */
/*	return NULL;
} */
