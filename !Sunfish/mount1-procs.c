/*
	$Id$
	$URL$

	Mount v3 procedures


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

#include <stdio.h>

#include "mount1-procs.h"

static struct mountlist *mounts = NULL;

enum accept_stat MNTPROC_NULL(struct server_conn *conn)
{
	(void)conn;
	return SUCCESS;
}

static char *host_to_str(unsigned int host, struct pool *pool)
{
	char *str;

	str = palloc(16, pool);
	if (str == NULL) return NULL;

	snprintf(str, 16, "%d.%d.%d.%d",
	         (host & 0xFF000000) >> 24,
	         (host & 0x00FF0000) >> 16,
	         (host & 0x0000FF00) >> 8,
	         (host & 0x000000FF) >> 0);
	return str;
}

enum accept_stat MNTPROC_MNT(string *args, struct mountres *res, struct server_conn *conn)
{
	struct export *export;

	printf("MNTPROC_MNT\n");

	export = conn->exports;

	while (export) {
		if (strncmp(export->exportname, args->data, args->size) == 0) {
/*			struct mountlist *mount;*/

			/* FIXME Host checking */
			res->status = 0;
			memset(res->u.directory.data, 0, FHSIZE);
			res->u.directory.data[0] = export->exportnum;
			res->u.directory.data[1] = export->basedirhash;
/*			UE(mount = palloc(sizeof(struct mountlist), conn->gpool));
			UE(mount->hostname->data = host_to_str(conn->host, conn->gpool));
			mount->hostname->size = strlen(mount->hostname->data);
			mount->dirpath->data = export->basedir;
			mount->dirpath->size = export->basedirlen;
			mount->next = mounts;
			mounts = mount;*/
			return SUCCESS;
		}
		export = export->next;
	}
	res->status = 2; /*NFSERR_NOENT*/
	return SUCCESS;
}

enum accept_stat MNTPROC_DUMP(struct mountlist2 *res, struct server_conn *conn)
{
	(void)conn;

	res->list = mounts;
	return SUCCESS;
}

enum accept_stat MNTPROC_UMNT(string *args, struct server_conn *conn)
{
	printf("MNTPROC_UMNT\n");
	/*FIXME - remove from mount list (and free the memory, not just release to the pool)*/
	return SUCCESS;
}

enum accept_stat MNTPROC_UMNTALL(struct server_conn *conn)
{
	printf("MNTPROC_UMNTALL\n");
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

