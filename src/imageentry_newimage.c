/*
	$Id$

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
#include "utils.h"

#include "sunfishdefs.h"

#include "sunfish.h"

#include "rpc-calls.h"
#include "portmapper-calls.h"
#include "pcnfsd-calls.h"
#include "mount1-calls.h"


void free_conn_info(struct conn_info *conn)
{
	if (conn == NULL) return;
	if (conn->config) free(conn->config);
	if (conn->auth) free(conn->auth);
	if (conn->pool) pfree(conn->pool);
	free(conn);
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
		if (strcasecmp(val, "NFS2") == 0) {
			conn->nfs3 = 0;
		} else if (strcasecmp(val, "NFS3") == 0) {
			conn->nfs3 = 1;
		} else {
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
		if (conn->maxdatabuffer > MAX_DATABUFFER) conn->maxdatabuffer = MAX_DATABUFFER;
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

	obuf = conn->auth;
	obufend = conn->auth + conn->authsize;
	process_auth_unix(OUTPUT, &auth_unix, conn->pool);
	conn->authsize = obuf - conn->auth;
	return NULL;
}

static os_error *getport(int program, int version, unsigned int *progport, struct conn_info *conn)
{
	struct mapping map = {program, version, IPPROTO_UDP, 0};
	os_error *err;
	unsigned port;

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
	conn->nfs3 = 0;
	if ((conn->pool = pinit(NULL)) == NULL) {
		free_conn_info(conn);
		return gen_error(NOMEM,NOMEMMESS);
	}

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

	err = CALLENTRY(func_newimage_mount, conn, (conn));
	if (err) {
		rpc_close_connection(conn);
		free_conn_info(conn);
		return err;
	}

	return NULL;
}

