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

#include "moduledefs.h"

#include "sunfish.h"

#include "rpc-calls.h"
#ifdef NFS3
#include "nfs3-calls.h"
#include "mount3-calls.h"
#else
#include "nfs2-calls.h"
#include "mount1-calls.h"
#endif
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
	err = MNTPROC(UMNT, (&dir, conn));
	if (err && err != ERR_WOULDBLOCK && enablelog) log_error(err);

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
	} else if (CHECK("Transport")) {
		conn->tcp = strcasecmp(val, "tcp") == 0;
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
	} else if (CHECK("unumask")) {
		conn->unumask = 07777 & (int)strtol(val, NULL, 8); /* unumask is specified in octal */
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
	} else if (CHECK("LocalPort")) {
		char *end;

		conn->localportmin = (int)strtol(val, &end, 10);
		conn->localportmax = (int)strtol(end, NULL, 10);
		if (conn->localportmax == 0) conn->localportmax = conn->localportmin;
	} else if (CHECK("Pipelining")) {
		conn->pipelining = (int)strtol(val, NULL, 10);
	} else if (CHECK("MaxDataBuffer")) {
		conn->maxdatabuffer = (int)strtol(val, NULL, 10);
/* FIXME		if (conn->maxdatabuffer > MAXDATA) conn->maxdatabuffer = MAXDATA;*/
	} else if (CHECK("FollowSymlinks")) {
		conn->followsymlinks = (int)strtol(val, NULL, 10);
	} else if (CHECK("CaseSensitive")) {
		conn->casesensitive = (int)strtol(val, NULL, 10);
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
		if (isspace(*endspc)) *endspc = '\0';
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

	if (conn->tcp) map.prot = IPPROTO_TCP;

	err = PMAPPROC_GETPORT(&map, &port, conn);
	if (err) return err;
	if (port == 0) {
		char *daemon = program == PCNFSD_RPC_PROGRAM ? "pcnfsd" : (program == MOUNT_RPC_PROGRAM ? "mountd" : "nfsd");
		return gen_error(FUNCERRBASE + 1, "Unable to map RPC program to port number (%d, %d) - Check that %s is running on the server", program, version, daemon);
	}
	
	*progport = port;

	return NULL;
}

os_error *func_newimage(unsigned int fileswitchhandle, struct conn_info **myhandle)
{
	struct conn_info *conn;
	string dir;
	struct mountres mountres;
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
	conn->tcp = 0;
	conn->server = "";
	conn->export = "";
	conn->timeout = 3;
	conn->retries = 2;
	conn->hidden = 1;
	conn->umask = 022;
	conn->unumask = 0;
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
	conn->localportmin = LOCALPORTMIN_DEFAULT;
	conn->localportmax = LOCALPORTMAX_DEFAULT;
	conn->maxdatabuffer = 0;
	conn->followsymlinks = 5;
	conn->pipelining = 0;
	conn->casesensitive = 0;
	conn->laststart = 0;

	/* Read details from file */
	err = parse_file(fileswitchhandle, conn);
	if (err) {
		free_conn_info(conn);
		return err;
	}

	if (conn->logging) {
		enablelog++;
		syslogf(LOGNAME, LOGENTRY, "Logging enabled for new connection %s %s %s", Module_Title, Module_VersionString, Module_Date);
	}

	if (conn->maxdatabuffer == 0) {
		conn->maxdatabuffer =  conn->tcp ? MAXDATABUFFER_TCP_DEFAULT : MAXDATABUFFER_UDP_DEFAULT;
	}

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
		rpc_close_connection(conn);
		free_conn_info(conn);
		return err;
	}

	/* Get port numbers if not specified */
	if (conn->pcnfsd_port == 0 && conn->username) {
		err = getport(PCNFSD_RPC_PROGRAM, PCNFSD_RPC_VERSION, &(conn->pcnfsd_port), conn);
		if (err) {
			rpc_close_connection(conn);
			free_conn_info(conn);
			return err;
		}
	}

	if (conn->mount_port == 0) {
		err = getport(MOUNT_RPC_PROGRAM, MOUNT_RPC_VERSION, &(conn->mount_port), conn);
		if (err) {
			rpc_close_connection(conn);
			free_conn_info(conn);
			return err;
		}
	}

	if (conn->nfs_port == 0) {
		err = getport(NFS_RPC_PROGRAM, NFS_RPC_VERSION, &(conn->nfs_port), conn);
		if (err) {
			rpc_close_connection(conn);
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
			rpc_close_connection(conn);
			free_conn_info(conn);
			return err;
		}
		if (res.stat != AUTH_RES_OK) {
			rpc_close_connection(conn);
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
		rpc_close_connection(conn);
		free_conn_info(conn);
		return err;
	}

	/* Get a filehandle for the root directory */
	dir.size = strlen(conn->export);
	dir.data = conn->export;
	err = MNTPROC(MNT, (&dir, &mountres, conn));
	if (err) {
		rpc_close_connection(conn);
		free_conn_info(conn);
		return err;
	}
	if (mountres.status != 0) {
		rpc_close_connection(conn);
		free_conn_info(conn);
		/* status is an errno value. Probably the same as an NFS status value. */
		return gen_nfsstatus_error((enum nstat)mountres.status);
	}
	{
		static char tmp[64]; /*FIXME*/
		memcpy(tmp, mountres.u.directory.data.data, mountres.u.directory.data.size);
		conn->rootfh.data.size = mountres.u.directory.data.size;
		conn->rootfh.data.data = tmp;
	}

	return NULL;
}


/* Read directory entries and possibly info */
os_error *func_readdirinfo(int info, char *dirname, char *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos)
{
	char *bufferpos;
	struct readdirargs rddir;
	struct readdirres rdres;
	struct entry *direntry = NULL;
	os_error *err;
	int cookie = 0;
	int dirpos = 0;

	bufferpos = buffer;
	*objsread = 0;
#ifdef NFS3
	memset(rddir.cookieverf, 0, NFS3_COOKIEVERFSIZE);
#endif

	if (start != 0 && start == conn->laststart && strcmp(dirname, conn->lastdir) == 0) {
		/* Used cached values */
		rddir.dir = conn->lastdirhandle;
		cookie = conn->lastcookie;
#ifdef NFS3
		memcpy(rddir.cookieverf, conn->lastcookieverf, NFS3_COOKIEVERFSIZE);
#endif
		dirpos = start;
	} else if (dirname[0] == '\0') {
		/* An empty dirname specifies the root directory */
		rddir.dir = conn->rootfh;
	} else {
		struct objinfo *finfo;
		static char handle[FHSIZE];

		/* Find the handle of the directory */
		err = filename_to_finfo(dirname, 1, NULL, &finfo, NULL, NULL, NULL, conn);
		if (err) return err;
		if (finfo == NULL) return gen_nfsstatus_error(NFSERR_NOENT);

		memcpy(handle, finfo->objhandle.data.data, finfo->objhandle.data.size);
		rddir.dir.data.data = handle;
		rddir.dir.data.size = finfo->objhandle.data.size;
	}

	while (dirpos <= start) {
		rddir.count = conn->maxdatabuffer;
		rddir.cookie = cookie;
		err = NFSPROC(READDIR, (&rddir, &rdres, conn));
		if (err) return err;
		if (rdres.status != NFS_OK) return gen_nfsstatus_error(rdres.status);
	
		/* We may need to do a lookup on each filename, but this would
		   corrupt the rx buffer which holds our list, so swap buffers */
		swap_rxbuffers();
	
#ifdef NFS3
		memcpy(rddir.cookieverf, rdres.u.readdirok.cookieverf, NFS3_COOKIEVERFSIZE);
#endif
		direntry = rdres.u.readdirok.entries;
		while (direntry && *objsread < numobjs) {
			if (direntry->name.size == 0) {
				/* Ignore null names */
			} else if (direntry->name.size == 1 && direntry->name.data[0] == '.') {
				/* current dir */
			} else if (direntry->name.size == 2 && direntry->name.data[0] == '.' && direntry->name.data[1] == '.') {
				/* parent dir */
			} else if (direntry->name.data[0] == '.' && (conn->hidden == 0 || (conn->hidden == 2 && dirname[0] == '\0'))) {
				/* Ignore hidden files if so configured */
			} else {
				if (dirpos >= start) {
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
		
					bufferpos += len;
		
					if (info) {
						struct objinfo *lookupres;
						enum nstat status;
		
						bufferpos = (char *)(((int)bufferpos + 3) & ~3);
		
						/* Lookup file attributes.
						   READDIRPLUS in NFS3 would eliminate this call. */
						err = leafname_to_finfo(direntry->name.data, &(direntry->name.size), 1, 1, &(rddir.dir), &lookupres, &status, conn);
						if (err) return err;
						if (status == NFS_OK && lookupres->attributes.type != NFLNK) {
							timeval_to_loadexec(&(lookupres->attributes.mtime), filetype, &(info_entry->load), &(info_entry->exec));
							info_entry->len = lookupres->attributes.size;
							info_entry->attr = mode_to_attr(lookupres->attributes.mode);
							info_entry->type = lookupres->attributes.type == NFDIR ? OBJ_DIR : OBJ_FILE;
						} else {
							/* An error occured, either the file no longer exists, or
							   it is a symlink that doesn't point to a valid object. */
							info_entry->load = 0xDEADDEAD;
							info_entry->exec = 0xDEADDEAD;
							info_entry->len = 0;
							info_entry->attr = 0;
							info_entry->type = OBJ_FILE;
						}
					}
	
					(*objsread)++;
				}
				dirpos++;
			}
			cookie = direntry->cookie;
			direntry = direntry->next;
		}
		if (rdres.u.readdirok.eof) break;
	}

	if (direntry == NULL && rdres.u.readdirok.eof) {
		*continuepos = -1;
		conn->laststart = 0;
		/* If there are entries still in the buffer that won't fit in the
		   output buffer then we should cache them to save rerequesting
		   them on the next iteration */
	} else {
		int len;

		/* Ideally, we would just return the cookie to the user, but
		   RISC OS seems to treat any value with the top bit set
		   as if it were -1
		   Also Filer_Action seems to make assumptions that it
		   really shouldn't, and treats the value being the position
		   within the directory */
		*continuepos = dirpos;
		conn->lastcookie = cookie;
		conn->lastdirhandle = rddir.dir;  /*FIXME - deep copy */
#ifdef NFS3
		memcpy(conn->lastcookieverf, rdres.u.readdirok.cookieverf, NFS3_COOKIEVERFSIZE);
#endif
		len = strlen(dirname);
		if (len < MAXNAMLEN) {
			memcpy(conn->lastdir, dirname, len + 1);
			conn->laststart = dirpos;
		} else {
			/* No room to cache dirname, so don't bother */
			conn->laststart = 0;
		}
		/* At this point we could speculatively request the next set of
		   entries to reduce the latency on the next request */
	}

	return NULL;
}


/* Rename a file or directory */
os_error *func_rename(char *oldfilename, char *newfilename, struct conn_info *conn, int *renamefailed)
{
	struct objinfo *dinfo;
	struct objinfo *finfo;
	struct renameargs renameargs;
	struct renameres renameres;
	os_error *err;
	char *leafname;
	static char oldleafname[MAXNAMLEN];
	int filetype;
	int dirnamelen;
	unsigned int leafnamelen;

	err = filename_to_finfo(oldfilename, 1, &dinfo, &finfo, &leafname, &filetype, NULL, conn);
	if (err) return err;
	if (finfo == NULL) return gen_nfsstatus_error(NFSERR_NOENT);

	renameargs.from.dir = dinfo ? dinfo->objhandle : conn->rootfh;
	strcpy(oldleafname, leafname);
	renameargs.from.name.data = oldleafname;
	renameargs.from.name.size = strlen(oldleafname);

	dirnamelen = strlen(oldfilename) - renameargs.from.name.size;

	if (strncmp(oldfilename, newfilename, dirnamelen) == 0 &&
	    newfilename[dirnamelen] &&
	    strchr(newfilename + dirnamelen + 1, '.') == NULL) {
		/* Both files are in the same directory */
		renameargs.to.dir = renameargs.from.dir;
		leafname = newfilename + dirnamelen;
		leafname = filename_unixify(leafname, strlen(leafname), &leafnamelen);
	} else {
		/* Files are in different directories, so find the handle of the new dir */
		err = filename_to_finfo(newfilename, 1, &dinfo, NULL, &leafname, NULL, NULL, conn);
		if (err) return err;

		leafnamelen = strlen(leafname);
		renameargs.to.dir = dinfo ? dinfo->objhandle : conn->rootfh;
	}

	/* Add ,xyz on if necessary to preserve filetype */
	renameargs.to.name.data = addfiletypeext(leafname, leafnamelen, 0, filetype, &(renameargs.to.name.size), conn);

	err = NFSPROC(RENAME, (&renameargs, &renameres, conn));
	if (err) return err;
	if (renameres.status != NFS_OK) return gen_nfsstatus_error(renameres.status);

	*renamefailed = 0;
	return NULL;
}

/* This entry point is meaningless, but we must support it as *ex et al call it. */
os_error *func_readboot(int *option)
{
	*option = 0;

	return NULL;
}
