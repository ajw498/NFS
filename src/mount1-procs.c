/*
	$Id$

	Mount v1 procedures


	Copyright (C) 2006 Alex Waugh
	
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

#include <string.h>

#include "moonfish.h"
#include "serverconn.h"
#include "exports.h"
#include "mount1-procs.h"
#include "utils.h"


enum accept_stat MNTPROC_NULL(struct server_conn *conn)
{
	(void)conn;
	return SUCCESS;
}


enum accept_stat MNTPROC_MNT(mountargs *args, struct mountres *res, struct server_conn *conn)
{
	struct export *export;

	export = conn->exports;

	while (export) {
		if (strncmp(export->exportname, args->dirpath.data, args->dirpath.size) == 0) {
			int i;
			unsigned int fhsize = FHSIZE;
			char *fh = res->u.mountinfo.fhandle.data;

			if ((conn->host & export->mask) != export->host) {
				res->status = NFSERR_ACCES;
				return SUCCESS;
			}
			conn->export = export;
			res->status = path_to_fh("", &fh, &fhsize, conn);

			if (res->status == 0) {
				for (i = 0; i < MAXHOSTS; i++) {
					if (export->hosts[i] == 0) {
						export->hosts[i] = conn->host;
						break;
					}
				}
			}
			return SUCCESS;
		}
		export = export->next;
	}
	res->status = NFSERR_NOENT;
	return SUCCESS;
}

enum accept_stat MNTPROC_DUMP(struct mountlist2 *res, struct server_conn *conn)
{
	struct mountlist *mounts = NULL;
	struct export *export = conn->exports;

	while (export) {
		int i;

		for (i = 0; i < MAXHOSTS; i++) {
			if (export->hosts[i] != 0) {
				struct mountlist *mount;

				mount = palloc(sizeof(struct mountlist), conn->pool);
				if (mount == NULL) break;
				mount->hostname.data = host_to_str(export->hosts[i], conn->pool);
				if (mount->hostname.data == NULL) break;
				mount->hostname.size = strlen(mount->hostname.data);
				mount->directory.data = export->basedir;
				mount->directory.size = export->basedirlen;
				mount->next = mounts;
				mounts = mount;
			}
		}
		export = export->next;
	}

	res->list = mounts;
	return SUCCESS;
}

enum accept_stat MNTPROC_UMNT(struct mountargs *args, struct server_conn *conn)
{
	int i;
	struct export *export = conn->exports;

	while (export) {
		if (strncmp(export->exportname, args->dirpath.data, args->dirpath.size) == 0) {
			for (i = 0; i < MAXHOSTS; i++) {
				if (export->hosts[i] == conn->host) {
					export->hosts[i] = 0;
					break;
				}
			}
		}
		export = export->next;
	}

	return SUCCESS;
}

enum accept_stat MNTPROC_UMNTALL(struct server_conn *conn)
{
	int i;
	struct export *export = conn->exports;

	while (export) {
		for (i = 0; i < MAXHOSTS; i++) {
			if (export->hosts[i] == conn->host) {
				export->hosts[i] = 0;
				break;
			}
		}
		export = export->next;
	}

	return SUCCESS;
}

enum accept_stat MNTPROC_EXPORT(struct exportlist2 *res, struct server_conn *conn)
{
	struct export *export = conn->exports;

	res->list = NULL;
	while (export) {
		struct exportlist *list;

		list = palloc(sizeof(struct exportlist), conn->pool);
		if (list == NULL) return SUCCESS;

		list->filesys.data = export->exportname;
		list->filesys.size = strlen(export->exportname);
		list->groups = NULL;
		list->next = res->list;
		res->list = list;
		export = export->next;
	}
	return SUCCESS;
}

