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

/* Flag to detect and generate an error on reentrancy */
static int entered = 0;

#define ENTRY(name, filename, regs) do { \
	if (enablelog) logentry(name, filename, regs); \
	if (entered) { \
		err = gen_error(MODULEERRBASE + 0,"Sunfish does not support accessing files from callbacks"); \
		if (enablelog) logexit(regs, err); \
		return err; \
	} \
	entered++; \
} while (0)

#define EXIT(regs, err) do { \
	if (enablelog) logexit(regs, err); \
	entered--; \
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
	os_error *err;

	(void)cmd_tail;
	(void)podule_base;

	err = declare_fs(private_word);

	error_func = rpc_resetfifo;

	rpc_init_header();

	return err;
}

_kernel_oserror *finalise(int fatal, int podule_base, void *private_word)
{
	os_error *err = NULL;
	static int finalised = 0;

	(void)fatal;
	(void)podule_base;
	(void)private_word;

	if (enablelog) syslogf(LOGNAME, LOGENTRY, "Module finalisation");

	if (!finalised) err = _swix(OS_FSControl, _INR(0,1), 36, SUNFISH_FILETYPE);
	finalised = 1;
	/* OS_FSControl 36 can return an error which we want to notify the caller
	   of, but that prevents the module from dying, and the next time
	   finalisation is called the FSControl call will fail as the imagefs
	   is no longer registered, so the module can never be killed.
	   So we make sure that we only deregister once. */

	return err;
}

_kernel_oserror *imageentry_open_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;
	struct conn_info *conn = (struct conn_info *)(r->r[6]);

	(void)pw;

	ENTRY("Open    ", (char *)(r->r[1]), r);

	err = CALLENTRY(open_file, conn, ((char *)r->r[1], r->r[0], conn, (unsigned int *)&(r->r[0]), (struct file_handle **)&(r->r[1]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]), (unsigned int *)&(r->r[4])));

	pclear(conn->pool);

	EXIT(r, err);

	return err;
}

_kernel_oserror *imageentry_getbytes_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;
	struct file_handle *handle = (struct file_handle *)(r->r[1]);

	(void)pw;

	ENTRY("GetBytes", "", r);

	err = CALLENTRY(get_bytes, handle->conn, (handle, (char *)(r->r[2]), r->r[3], r->r[4]));

	pclear(handle->conn->pool);

	EXIT(r, err);

	return err;
}

_kernel_oserror *imageentry_putbytes_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err = NULL;
	struct file_handle *handle = (struct file_handle *)(r->r[1]);

	(void)pw;

	ENTRY("PutBytes", "", r);

	err = CALLENTRY(put_bytes, handle->conn, (handle, (char *)(r->r[2]), r->r[3], r->r[4]));

	pclear(handle->conn->pool);

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
			err = CALLENTRY(args_writeextent, handle->conn, (handle, (unsigned int)(r->r[2])));
			pclear(handle->conn->pool);
			break;
		case IMAGEENTRY_ARGS_READSIZE:
			err = CALLENTRY(args_readallocatedsize, handle->conn, (handle, (unsigned int *)&(r->r[2])));
			pclear(handle->conn->pool);
			break;
		case IMAGEENTRY_ARGS_FLUSH:
			err = CALLENTRY(args_readdatestamp, handle->conn,  (handle, (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3])));
			pclear(handle->conn->pool);
			break;
		case IMAGEENTRY_ARGS_ENSURESIZE:
			err = CALLENTRY(args_ensuresize, handle->conn, (handle, (unsigned int)(r->r[2]), (unsigned int *)&(r->r[2])));
			pclear(handle->conn->pool);
			break;
		case IMAGEENTRY_ARGS_ZEROPAD:
			err = CALLENTRY(args_zeropad, handle->conn, (handle, (unsigned int)(r->r[2]), (unsigned int)(r->r[3])));
			pclear(handle->conn->pool);
			break;
		case IMAGEENTRY_ARGS_READDATESTAMP:
			err = CALLENTRY(args_readdatestamp, handle->conn, (handle, (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3])));
			pclear(handle->conn->pool);
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

	err = CALLENTRY(close_file, conn, (handle, r->r[2], r->r[3]));

	/* Handle has been free'd by this point */
	pclear(conn->pool);

	EXIT(r, err);

	return err;
}

_kernel_oserror *imageentry_file_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;
	struct conn_info *conn = (struct conn_info *)(r->r[6]);

	(void)pw;

	ENTRY("File    ", (char *)(r->r[1]), r);

	switch (r->r[0]) {
		case IMAGEENTRY_FILE_SAVEFILE:
			err = CALLENTRY(file_savefile, conn, ((char *)(r->r[1]), r->r[2], r->r[3], (char *)(r->r[4]), (char *)(r->r[5]),  conn, (char **)(&(r->r[6]))));
			pclear(conn->pool);
			break;
		case IMAGEENTRY_FILE_WRITECATINFO:
			err = CALLENTRY(file_writecatinfo, conn, ((char *)(r->r[1]), r->r[2], r->r[3], r->r[5], conn));
			pclear(conn->pool);
			break;
		case IMAGEENTRY_FILE_READCATINFO:
			err = CALLENTRY(file_readcatinfo, conn, ((char *)(r->r[1]), conn, &(r->r[0]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]), &(r->r[4]), &(r->r[5])));
			pclear(conn->pool);
			break;
		case IMAGEENTRY_FILE_DELETE:
			err = CALLENTRY(file_delete, conn, ((char *)(r->r[1]), conn, &(r->r[0]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]), &(r->r[4]), &(r->r[5])));
			pclear(conn->pool);
			break;
		case IMAGEENTRY_FILE_CREATEFILE:
			err = CALLENTRY(file_createfile, conn, ((char *)(r->r[1]), r->r[2], r->r[3], (char *)(r->r[4]), (char *)(r->r[5]), conn));
			pclear(conn->pool);
			break;
		case IMAGEENTRY_FILE_CREATEDIR:
			err = CALLENTRY(file_createdir, conn, ((char *)(r->r[1]), r->r[2], r->r[3], conn));
			pclear(conn->pool);
			break;
		case IMAGEENTRY_FILE_READBLKSIZE:
			r->r[2] = FAKE_BLOCKSIZE;
			err = NULL;
			break;
		default:
			err = gen_error(UNSUPP, UNSUPPMESS);
	}


	EXIT(r, err);

	return err;
}

_kernel_oserror *imageentry_func_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;
	struct conn_info *conn = (struct conn_info *)(r->r[6]);

	(void)pw;

	ENTRY("Func    ", r->r[0] == 22 ? "" : (char *)(r->r[1]), r);

	switch (r->r[0]) {
		case IMAGEENTRY_FUNC_RENAME:
			err = CALLENTRY(func_rename, conn, ((char *)(r->r[1]), (char *)(r->r[2]), conn, &(r->r[1])));
			pclear(conn->pool);
			break;
		case IMAGEENTRY_FUNC_READDIR:
			err = CALLENTRY(func_readdirinfo, conn, (0, (char *)r->r[1], (void *)r->r[2], r->r[3], r->r[4], r->r[5], conn, &(r->r[3]), &(r->r[4])));
			pclear(conn->pool);
			break;
		case IMAGEENTRY_FUNC_READDIRINFO:
			err = CALLENTRY(func_readdirinfo, conn, (1, (char *)r->r[1], (void *)r->r[2], r->r[3], r->r[4], r->r[5], conn, &(r->r[3]), &(r->r[4])));
			pclear(conn->pool);
			break;
		case IMAGEENTRY_FUNC_NEWIMAGE:
			err = func_newimage(r->r[1],(struct conn_info **)&(r->r[1]));
			break;
		case IMAGEENTRY_FUNC_CLOSEIMAGE:
			conn = (struct conn_info *)(r->r[1]);
			err = CALLENTRY(func_closeimage, conn, (conn));
			break;
		case IMAGEENTRY_FUNC_READBOOT:
			/* This entry point is meaningless, but we must
			   support it as *ex et al call it. */
			r->r[2] = 0;
			err = NULL;
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


	EXIT(r, err);

	return err;
}
