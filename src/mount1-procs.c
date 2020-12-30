/*
	Mount v1 procedures


	Copyright (C) 2006 Alex Waugh

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
		if ((export->exportnum > 0) && (strncmp(export->exportname, args->dirpath.data, args->dirpath.size) == 0)) {
			int i;
			unsigned int fhsize = FHSIZE;
			char *fh = res->u.mountinfo.fhandle.data;

			if ((conn->host & export->mask) != export->host) {
				res->status = NFSERR_ACCESS;
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
			if ((export->exportnum > 0) && (export->hosts[i] != 0)) {
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
		if ((export->exportnum > 0) && (strncmp(export->exportname, args->dirpath.data, args->dirpath.size) == 0)) {
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
			if ((export->exportnum > 0) && (export->hosts[i] == conn->host)) {
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
		if (export->exportnum > 0) {
			struct exportlist *list;

			list = palloc(sizeof(struct exportlist), conn->pool);
			if (list == NULL) return SUCCESS;

			list->filesys.data = export->exportname;
			list->filesys.size = strlen(export->exportname);
			list->groups = NULL;
			list->next = res->list;
			res->list = list;
		}
		export = export->next;
	}
	return SUCCESS;
}

