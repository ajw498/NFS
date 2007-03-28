/*
	$Id$

	Main module entry points.
	This then calls the imageentry_* functions as appropriate.


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

#include <kernel.h>
#include <stdio.h>
#include <stdarg.h>
#include <swis.h>
#include <unixlib.h>

#include "sunfishdefs.h"
#include "sunfish.h"

#include "imageentry_func.h"
#include "imageentry_file.h"
#include "imageentry_bytes.h"
#include "imageentry_openclose.h"
#include "imageentry_args.h"

#include "rpc.h"


extern int module_base_address;

typedef int veneer;

struct imagefs_info_block {
	unsigned int information_word;
	unsigned int filetype;
	veneer imageentry_open;
	veneer imageentry_getbytes;
	veneer imageentry_putbytes;
	veneer imageentry_args;
	veneer imageentry_close;
	veneer imageentry_file;
	veneer imageentry_func;
};

/* A count of the number of connections that have logging enabled.
   We log all connections if any of them have logging enabled. */
int enablelog = 0;

#define ENTRY(name, filename, regs) do { \
	if (enablelog) logentry(name, filename, regs); \
} while (0)

#define EXIT(regs, err) do { \
	if (enablelog) logexit(regs, err); \
} while (0)

#define CONNENTRY(conn) do { \
conn->reference++; \
} while (0)

#define CONNEXIT(conn) do { \
if (conn->reference == 1) { \
 pclear(conn->pool); \
 rpc_free_request_entry(conn); \
} \
conn->reference--; \
} while (0)

void log_error(os_error *err)
{
	syslogf(LOGNAME, LOGERROR, "Error %x, %s", err->errnum, err->errmess);
}

static void logexit(_kernel_swi_regs *r, os_error *err)
{
	if (err) {
		log_error(err);
	} else {
		syslogf(LOGNAME, LOGEXIT, "Exit                %.8x %.8x %.8x %.8x %.8x %.8x %.8x",r->r[0],r->r[1],r->r[2],r->r[3],r->r[4],r->r[5],r->r[6]);
	}
}

static void logentry(char *entry, char *filename, _kernel_swi_regs *r)
{
	syslogf(LOGNAME, LOGENTRY, "ImageEntry_%s %.8x %.8x %.8x %.8x %.8x %.8x %.8x %s", entry, r->r[0], r->r[1], r->r[2], r->r[3], r->r[4], r->r[5], r->r[6], filename);
}

static os_error *declare_fs(void *private_word)
{
	struct imagefs_info_block info_block;

	info_block.information_word = 0;
	info_block.filetype = SUNFISH_FILETYPE;
	info_block.imageentry_open =     ((int)imageentry_open) - module_base_address;
	info_block.imageentry_getbytes = ((int)imageentry_getbytes) - module_base_address;
	info_block.imageentry_putbytes = ((int)imageentry_putbytes) - module_base_address;
	info_block.imageentry_args =     ((int)imageentry_args) - module_base_address;
	info_block.imageentry_close =    ((int)imageentry_close) - module_base_address;
	info_block.imageentry_file =     ((int)imageentry_file) - module_base_address;
	info_block.imageentry_func =     ((int)imageentry_func) - module_base_address;

	return _swix(OS_FSControl, _INR(0,3), 35, module_base_address, ((int)(&info_block)) - module_base_address, private_word);
}

/* Must handle service redeclare */
void service_call(int service_number, _kernel_swi_regs *r, void *private_word)
{
	/* The veneer will have already filtered out uninteresting calls */
	(void)service_number;
	(void)r;

	declare_fs(private_word); /* Ignore errors */
}

_kernel_oserror *initialise(const char *cmd_tail, int podule_base, void *private_word)
{
	(void)cmd_tail;
	(void)podule_base;

	return declare_fs(private_word);
}

_kernel_oserror *finalise(int fatal, int podule_base, void *private_word)
{
	os_error *err = NULL;
	static int finalised = 0;

	(void)fatal;
	(void)podule_base;
	(void)private_word;

	if (enablelog) syslogf(LOGNAME, LOGENTRY, "Module finalisation");

	if (!finalised) {
		err = _swix(OS_FSControl, _INR(0,1), 36, SUNFISH_FILETYPE);
	}
	finalised = 1;
	/* OS_FSControl 36 can return an error which we want to notify the caller
	   of, but that prevents the module from dying, and the next time
	   finalisation is called the FSControl call will fail as the imagefs
	   is no longer registered, so the module can never be killed.
	   So we make sure that we only deregister once. */

	rpc_free_all_buffers();

	return err;
}

_kernel_oserror *imageentry_open_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;
	struct conn_info *conn = (struct conn_info *)(r->r[6]);
	unsigned info_word;

	(void)pw;

	ENTRY("Open    ", (char *)(r->r[1]), r);

	CONNENTRY(conn);

	info_word = 0;
	err = CALLENTRY(open_file, conn, ((char *)r->r[1], r->r[0], conn, &info_word, (struct file_handle **)&(r->r[1]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]), (unsigned int *)&(r->r[4])));
	r->r[0] = info_word;

	CONNEXIT(conn);

	EXIT(r, err);

	return err;
}

_kernel_oserror *imageentry_getbytes_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;
	struct file_handle *handle = (struct file_handle *)(r->r[1]);

	(void)pw;

	ENTRY("GetBytes", "", r);

	CONNENTRY(handle->conn);

	err = CALLENTRY(get_bytes, handle->conn, (handle, (char *)(r->r[2]), r->r[3], r->r[4]));

	CONNEXIT(handle->conn);

	EXIT(r, err);

	return err;
}

_kernel_oserror *imageentry_putbytes_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err = NULL;
	struct file_handle *handle = (struct file_handle *)(r->r[1]);

	(void)pw;

	ENTRY("PutBytes", "", r);

	CONNENTRY(handle->conn);

	err = CALLENTRY(put_bytes, handle->conn, (handle, (char *)(r->r[2]), r->r[3], r->r[4]));

	CONNEXIT(handle->conn);

	EXIT(r, err);

	return err;
}

_kernel_oserror *imageentry_args_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;
	struct file_handle *handle = (struct file_handle *)(r->r[1]);

	(void)pw;

	ENTRY("Args    ", "", r);

	switch (r->r[0]) {
		case IMAGEENTRY_ARGS_WRITEEXTENT:
			CONNENTRY(handle->conn);
			err = CALLENTRY(args_writeextent, handle->conn, (handle, (unsigned int)(r->r[2])));
			CONNEXIT(handle->conn);
			break;
		case IMAGEENTRY_ARGS_READSIZE:
			CONNENTRY(handle->conn);
			err = CALLENTRY(args_readallocatedsize, handle->conn, (handle, (unsigned int *)&(r->r[2])));
			CONNEXIT(handle->conn);
			break;
		case IMAGEENTRY_ARGS_FLUSH:
			CONNENTRY(handle->conn);
			err = CALLENTRY(args_readdatestamp, handle->conn,  (handle, (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3])));
			CONNEXIT(handle->conn);
			break;
		case IMAGEENTRY_ARGS_ENSURESIZE:
			CONNENTRY(handle->conn);
			err = CALLENTRY(args_ensuresize, handle->conn, (handle, (unsigned int)(r->r[2]), (unsigned int *)&(r->r[2])));
			CONNEXIT(handle->conn);
			break;
		case IMAGEENTRY_ARGS_ZEROPAD:
			CONNENTRY(handle->conn);
			err = CALLENTRY(args_zeropad, handle->conn, (handle, (unsigned int)(r->r[2]), (unsigned int)(r->r[3])));
			CONNEXIT(handle->conn);
			break;
		case IMAGEENTRY_ARGS_READDATESTAMP:
			CONNENTRY(handle->conn);
			err = CALLENTRY(args_readdatestamp, handle->conn, (handle, (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3])));
			CONNEXIT(handle->conn);
			break;
		case FSENTRY_ARGS_NEWIMAGESTAMP:
			err = NULL;
			break;
		default:
			err = gen_error(UNSUPP, UNSUPPMESS);
	}


	EXIT(r, err);

	return err;
}

_kernel_oserror *imageentry_close_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;
	struct file_handle *handle = (struct file_handle *)(r->r[1]);
	struct conn_info *conn = handle->conn;
	(void)pw;

	ENTRY("Close   ", "", r);

	CONNENTRY(conn);

	err = CALLENTRY(close_file, conn, (handle, r->r[2], r->r[3]));

	/* Handle has been free'd by this point */
	CONNEXIT(conn);

	EXIT(r, err);

	return err;
}

static os_error *file_handler(char *filename, struct conn_info *conn, _kernel_swi_regs *r)
{
	os_error *err;
	unsigned dummy;

	switch (r->r[0]) {
		case IMAGEENTRY_FILE_SAVEFILE:
			CONNENTRY(conn);
			err = CALLENTRY(file_savefile, conn, (filename, r->r[2], r->r[3], (char *)(r->r[4]), (char *)(r->r[5]),  conn, (char **)(&(r->r[6]))));
			CONNEXIT(conn);
			break;
		case FSENTRY_FILE_WRITELOAD:
			CONNENTRY(conn);
			err = CALLENTRY(file_readcatinfo, conn, (filename, conn, &(r->r[0]), &dummy, (unsigned int *)&(r->r[3]), &(r->r[4]), &(r->r[5])));
			if (err == NULL) {
				err = CALLENTRY(file_writecatinfo, conn, (filename, r->r[2], r->r[3], r->r[5], conn));
			}
			CONNEXIT(conn);
			break;
		case FSENTRY_FILE_WRITEEXEC:
			CONNENTRY(conn);
			err = CALLENTRY(file_readcatinfo, conn, (filename, conn, &(r->r[0]), (unsigned int *)&(r->r[2]), &dummy, &(r->r[4]), &(r->r[5])));
			if (err == NULL) {
				err = CALLENTRY(file_writecatinfo, conn, (filename, r->r[2], r->r[3], r->r[5], conn));
			}
			CONNEXIT(conn);
			break;
		case FSENTRY_FILE_WRITEATTR:
			CONNENTRY(conn);
			err = CALLENTRY(file_readcatinfo, conn, (filename, conn, &(r->r[0]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]), &(r->r[4]), (int *)&dummy));
			if (err == NULL) {
				err = CALLENTRY(file_writecatinfo, conn, (filename, r->r[2], r->r[3], r->r[5], conn));
			}
			CONNEXIT(conn);
			break;
		case IMAGEENTRY_FILE_WRITECATINFO:
			CONNENTRY(conn);
			err = CALLENTRY(file_writecatinfo, conn, (filename, r->r[2], r->r[3], r->r[5], conn));
			CONNEXIT(conn);
			break;
		case IMAGEENTRY_FILE_READCATINFO:
			CONNENTRY(conn);
			err = CALLENTRY(file_readcatinfo, conn, (filename, conn, &(r->r[0]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]), &(r->r[4]), &(r->r[5])));
			CONNEXIT(conn);
			break;
		case IMAGEENTRY_FILE_DELETE:
			CONNENTRY(conn);
			err = CALLENTRY(file_delete, conn, (filename, conn, &(r->r[0]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]), &(r->r[4]), &(r->r[5])));
			CONNEXIT(conn);
			break;
		case IMAGEENTRY_FILE_CREATEFILE:
			CONNENTRY(conn);
			err = CALLENTRY(file_createfile, conn, (filename, r->r[2], r->r[3], (char *)(r->r[4]), (char *)(r->r[5]), (char **)&(r->r[6]), conn));
			CONNEXIT(conn);
			break;
		case IMAGEENTRY_FILE_CREATEDIR:
			CONNENTRY(conn);
			err = CALLENTRY(file_createdir, conn, (filename, r->r[2], r->r[3], conn));
			CONNEXIT(conn);
			break;
		case IMAGEENTRY_FILE_READBLKSIZE:
			r->r[2] = FAKE_BLOCKSIZE;
			err = NULL;
			break;
		default:
			err = gen_error(UNSUPP, UNSUPPMESS);
	}

	return err;
}

_kernel_oserror *imageentry_file_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;
	struct conn_info *conn = (struct conn_info *)(r->r[6]);
	char *filename = (char *)(r->r[1]);

	(void)pw;

	ENTRY("File    ", filename, r);

	err = file_handler(filename, conn, r);

	EXIT(r, err);

	return err;
}

static os_error *func_handler(char *filename, struct conn_info *conn, _kernel_swi_regs *r)
{
	os_error *err = NULL;
	size_t len;
	char *to;

	switch (r->r[0]) {
		case IMAGEENTRY_FUNC_RENAME:
			CONNENTRY(conn);
			if (filename != (char *)(r->r[1])) {
				/* FS, so check specialfield and discnames match */
				if (r->r[6]) {
					if (r->r[7] == 0) return NULL;
					if (strcasecmp((char *)(r->r[6]), (char *)(r->r[7])) != 0) return NULL;
				} else {
					if (r->r[7] != 0) return NULL;
				}
				char *todisc = (char *)(r->r[2]);
				to = strchr(todisc, '$');
				if (to == NULL) return NULL;
				if (strncasecmp((char *)(r->r[1]), todisc, to - todisc) != 0) return NULL;
				to++;
				if (*to == '.') to++;
			} else {
				to = (char *)(r->r[2]);
			}
			err = CALLENTRY(func_rename, conn, (filename, to, conn, &(r->r[1])));
			CONNEXIT(conn);
			break;
		case IMAGEENTRY_FUNC_READDIR:
			CONNENTRY(conn);
			err = CALLENTRY(func_readdirinfo, conn, (0, filename, (void *)r->r[2], r->r[3], r->r[4], r->r[5], conn, &(r->r[3]), &(r->r[4])));
			CONNEXIT(conn);
			break;
		case IMAGEENTRY_FUNC_READDIRINFO:
			CONNENTRY(conn);
			err = CALLENTRY(func_readdirinfo, conn, (1, filename, (void *)r->r[2], r->r[3], r->r[4], r->r[5], conn, &(r->r[3]), &(r->r[4])));
			CONNEXIT(conn);
			break;
		case FSENTRY_FUNC_SHUTDOWN:
			break;
		case FSENTRY_FUNC_READDIRINFO:
			CONNENTRY(conn);
			err = CALLENTRY(func_readdirinfo, conn, (2, filename, (void *)r->r[2], r->r[3], r->r[4], r->r[5], conn, &(r->r[3]), &(r->r[4])));
			CONNEXIT(conn);
			break;
		case IMAGEENTRY_FUNC_NEWIMAGE:
			err = func_newimage(r->r[1], NULL, (struct conn_info **)&(r->r[1]));
			break;
		case IMAGEENTRY_FUNC_CLOSEIMAGE:
			conn = (struct conn_info *)(r->r[1]);
			err = CALLENTRY(func_closeimage, conn, (conn));
			break;
		case FSENTRY_FUNC_CANONICALISE:
			/* Only canonical form is accepted */
			if (r->r[1]) {
				len = strlen((char *)r->r[1]) + 1;
				if (r->r[3]) {
					memcpy((char *)(r->r[3]), (char *)(r->r[1]), len < r->r[5] ? len : r->r[5]);
					r->r[1] = r->r[3];
					r->r[3] = len - r->r[5];
					if (r->r[3] < 0) r->r[3] = 0;
				} else {
					r->r[3] = len;
				}
			} else {
				r->r[3] = 0;
			}
			if (r->r[2]) {
				len = strlen((char *)r->r[2]) + 1;
				if (r->r[4]) {
					memcpy((char *)(r->r[4]), (char *)(r->r[2]), len < r->r[6] ? len : r->r[6]);
					r->r[2] = r->r[4];
					r->r[4] = len - r->r[6];
					if (r->r[4] < 0) r->r[4] = 0;
				} else {
					r->r[4] = len;
				}
			} else {
				r->r[4] = 0;
			}
			break;
		case FSENTRY_FUNC_RESOLVEWILDCARD:
			r->r[4] = -1;
			break;
		case IMAGEENTRY_FUNC_READBOOT:
			/* This entry point is meaningless, but we must
			   support it as *ex et al call it. */
			r->r[2] = 0;
			break;

		/* The following entrypoints are meaningless for NFS.
		   We could fake them, but I think it is better to give an error */
		case IMAGEENTRY_FUNC_READDEFECT:
		case IMAGEENTRY_FUNC_ADDDEFECT:
		case IMAGEENTRY_FUNC_WRITEBOOT:
		case IMAGEENTRY_FUNC_READUSEDSPACE:
		case IMAGEENTRY_FUNC_READFREESPACE:
		case IMAGEENTRY_FUNC_NAME:
		case IMAGEENTRY_FUNC_STAMP:
		case IMAGEENTRY_FUNC_GETUSAGE:
		default:
			err = gen_error(UNSUPP, UNSUPPMESS);
	}
	return err;
}

_kernel_oserror *imageentry_func_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err = NULL;
	struct conn_info *conn = (struct conn_info *)(r->r[6]);
	char *filename = (char *)(r->r[1]);

	(void)pw;

	ENTRY("Func    ", r->r[0] == IMAGEENTRY_FUNC_CLOSEIMAGE ? "" : (char *)(r->r[1]), r);

	err = func_handler(filename, conn, r);

	EXIT(r, err);

	return err;
}
