/*
	$Id$
	$URL$

	Routines for ImageEntry_Func


	Copyright (C) 2003 Alex Waugh
	
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <swis.h>
#include <unixlib.h>
#include <unistd.h>
#include <sys/param.h>

#include "imageentry_func.h"

#include "rpc-calls.h"
#include "nfs-calls.h"
#include "mount-calls.h"
#include "portmapper-calls.h"
#include "pcnfsd-calls.h"


/* The format of info returned for OS_GBPB 10 */
struct dir_entry {
	unsigned int load;
	unsigned int exec;
	unsigned int len;
	unsigned int attr;
	unsigned int type;
};


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

/* Encode a username or password for pcnfsd */
static void encode(char *str)
{
	while (*str) {
		*str = *str ^ 0x5b;
		str++;
	}
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
			return gen_error(FUNCERRBASE + 0, "Unknown protocol '%s'", val);
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
	} else if (CHECK("PCNFSDPort")) {
		conn->pcnfsd_port = (int)strtol(val, NULL, 10);
	} else if (CHECK("Export")) {
		conn->export = val;
	} else if (CHECK("UID")) {
		conn->uid = (int)strtol(val, NULL, 10);
	} else if (CHECK("GID")) {
		conn->gid = (int)strtol(val, NULL, 10);
	} else if (CHECK("GIDs")) {
		char *end = val;

		while (*end && conn->numgids < MAX_GIDS) {
			conn->gids[conn->numgids++] = (int)strtol(val, &end, 10);
			val = end;
		}
	} else if (CHECK("Username")) {
		conn->username = val;
		encode(val);
	} else if (CHECK("Password")) {
		conn->password = val;
		encode(val);
	} else if (CHECK("Logging")) {
		conn->logging = (int)strtol(val, NULL, 10);
	} else if (CHECK("umask")) {
		conn->umask = 07777 & (int)strtol(val, NULL, 8); /* umask is specified in octal */
	} else if (CHECK("ShowHidden")) {
		conn->hidden = (int)strtol(val, NULL, 10);
	} else if (CHECK("Timeout")) {
		conn->timeout = (int)strtol(val, NULL, 10);
	} else if (CHECK("Retries")) {
		conn->retries = (int)strtol(val, NULL, 10);
	} else if (CHECK("DefaultFiletype")) {
		conn->defaultfiletype = 0xFFF & (int)strtol(val, NULL, 16);
	} else if (CHECK("AddExt")) {
		conn->xyzext = (int)strtol(val, NULL, 10);
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

	/* Make an estimate of the maximum size needed for the
	   auth structure */
	conn->authsize = sizeof(struct auth_unix) + strlen(conn->machinename) + MAX_GIDS * sizeof(unsigned int);
	conn->auth = malloc(conn->authsize);
	if (conn == NULL) return gen_error(NOMEM,NOMEMMESS);

	auth_unix.stamp = 0;
	auth_unix.machinename.data = conn->machinename;
	auth_unix.machinename.size = strlen(conn->machinename);
	auth_unix.uid = conn->uid;
	auth_unix.gid = conn->gid;
	auth_unix.gids.size = conn->numgids;
	auth_unix.gids.data = conn->gids;

	buf = conn->auth;
	bufend = conn->auth + conn->authsize;
	process_struct_auth_unix(0, auth_unix, 0);
	conn->authsize = buf - conn->auth;

buffer_overflow: /* Should never occur */
	return NULL;
}

static os_error *getport(int program, int version, unsigned int *progport, struct conn_info *conn)
{
	struct mapping map = {program, version, IPPROTO_UDP, 0};
	os_error *err;
	int port;

	err = PMAPPROC_GETPORT(&map, &port, conn);
	if (err) return err;
	if (port == 0) {
		return gen_error(FUNCERRBASE + 1, "Unable to map RPC program to port number (%d, %d)", program, version);
	}
	
	*progport = port;

	return NULL;
}

os_error *func_newimage(unsigned int fileswitchhandle, struct conn_info **myhandle)
{
	struct conn_info *conn;
	string dir;
	struct fhstatus rootfh;
	os_error *err;
	static char machinename[MAXHOSTNAMELEN] = "";

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
	conn->pcnfsd_port = 0;
	conn->server = "";
	conn->export = "";
	conn->timeout = 3;
	conn->retries = 2;
	conn->hidden = 1;
	conn->umask = 022;
	conn->username = NULL;
	conn->password = "";
	conn->uid = 0;
	conn->gid = 0;
	conn->numgids = 0;
	conn->logging = 0;
	conn->auth = NULL;
	conn->machinename = NULL;
	conn->defaultfiletype = 0xFFF;
	conn->xyzext = NEEDED;

	/* Read details from file */
	err = parse_file(fileswitchhandle, conn);
	if (err) {
		free_conn_info(conn);
		return err;
	}

	if (conn->logging) enablelog++;

	if (conn->machinename == NULL) {
		/* Get the hostname of the machine we are running on */
		conn->machinename = machinename;
		if (machinename[0] == '\0') {
			gethostname(machinename, MAXHOSTNAMELEN);
		}
	}

	/* Initialise socket etc. */
	err = rpc_init_connection(conn);
	if (err) {
		free_conn_info(conn);
		return err;
	}

	/* Get port numbers if not specified */
	if (conn->pcnfsd_port == 0 && conn->username) {
		err = getport(PCNFSD_RPC_PROGRAM, PCNFSD_RPC_VERSION, &(conn->pcnfsd_port), conn);
		if (err) {
			free_conn_info(conn);
			return err;
		}
	}

	if (conn->mount_port == 0) {
		err = getport(MOUNT_RPC_PROGRAM, MOUNT_RPC_VERSION, &(conn->mount_port), conn);
		if (err) {
			free_conn_info(conn);
			return err;
		}
	}

	if (conn->nfs_port == 0) {
		err = getport(NFS_RPC_PROGRAM, NFS_RPC_VERSION, &(conn->nfs_port), conn);
		if (err) {
			free_conn_info(conn);
			return err;
		}
	}

	/* Use pcnfsd to map username/password to uid/gid */
	if (conn->username) {
		struct auth_args args;
		struct auth_res res;
		int i;

		args.system.data = conn->machinename;
		args.system.size = strlen(conn->machinename);
		args.id.data = conn->username;
		args.id.size = strlen(conn->username);
		args.pw.data = conn->password;
		args.pw.size = strlen(conn->password);
		args.comment.size = 0;

		err = PCNFSD_AUTH(&args, &res, conn);
		if (err) {
			free_conn_info(conn);
			return err;
		}
		if (res.stat != AUTH_RES_OK) {
			free_conn_info(conn);
			return gen_error(FUNCERRBASE + 2, "PCNFSD authentication failed");
		}
		conn->uid = res.uid;
		conn->gid = res.gid;
		conn->umask = res.def_umask;
		conn->numgids = res.gids.size;
		for (i = 0; i < res.gids.size; i++) {
			conn->gids[i] = res.gids.data[i];
		}
	}

	/* Create the opaque auth structure */
	err = create_auth(conn);
	if (err) {
		free_conn_info(conn);
		return err;
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


/* Read directory entries and possibly info */
os_error *func_readdirinfo(int info, char *dirname, char *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos)
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
		err = filename_to_finfo(dirname, NULL, &finfo, NULL, NULL, NULL, conn);
		if (err) return err;
		if (finfo == NULL) return gen_nfsstatus_error(NFSERR_NOENT);

		memcpy(rddir.dir, finfo->file, FHSIZE);
	}

	rddir.count = MAXDATA;
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
			int len;

			if (info) {
				info_entry = (struct dir_entry *)bufferpos;
				bufferpos += sizeof(struct dir_entry);
			}

			/* Check there is room in the output buffer. */
			if (bufferpos > buffer + buflen) break;

			/* Copy leafname into output buffer, translating some
			   chars and stripping any ,xyz */
			len = filename_riscosify(direntry->name.data, direntry->name.size, bufferpos, buffer + buflen - bufferpos, &filetype, conn);
			if (len == 0) break; /* Buffer overflowed */

			bufferpos += (len + 3) & ~3;

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


/* Rename a file or directory */
os_error *func_rename(char *oldfilename, char *newfilename, struct conn_info *conn, int *renamefailed)
{
    struct diropok *dinfo;
    struct diropok *finfo;
    struct renameargs renameargs;
    enum nstat renameres;
    os_error *err;
    char *leafname;
    static char oldleafname[MAXNAMLEN];
    int filetype;

	err = filename_to_finfo(oldfilename, &dinfo, &finfo, &leafname, &filetype, NULL, conn);
	if (err) return err;
	if (finfo == NULL) return gen_nfsstatus_error(NFSERR_NOENT);

	memcpy(renameargs.from.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
	strcpy(oldleafname, leafname);
	renameargs.from.name.data = oldleafname;
	renameargs.from.name.size = strlen(oldleafname);

	if (strncmp(oldfilename, newfilename, leafname - oldfilename) == 0) {
		/* Both files are in the same directory */
		memcpy(renameargs.to.dir, renameargs.from.dir, FHSIZE);
		leafname = newfilename + (leafname - oldfilename);
	} else {
		/* Files are in different directories, so find the handle of the new dir */
		err = filename_to_finfo(newfilename, &dinfo, NULL, &leafname, NULL, NULL, conn);
		if (err) return err;

		memcpy(renameargs.to.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
	}

	/* Add ,xyz on if necessary to preserve filetype */
	renameargs.to.name.data = addfiletypeext(leafname, strlen(leafname), 0, filetype, &(renameargs.to.name.size), conn);

	err = NFSPROC_RENAME(&renameargs, &renameres, conn);
	if (err) return err;
	if (renameres != NFS_OK) return gen_nfsstatus_error(renameres);

	*renamefailed = 0;
	return NULL;
}

