/*
	$Id$
	$URL$

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

#include "moduledefs.h"

#include "imageentry_func.h"
#include "imageentry_file.h"
#include "imageentry_bytes.h"
#include "imageentry_openclose.h"
#include "imageentry_args.h"

#include "rpc.h"

#if CMHG_VERSION < 542
#error cmhg out of date
#endif

#define IMAGE_FILETYPE 0x1B6

#define LOGNAME "Sunfish"
#define LOGENTRY 50
#define LOGEXIT  75
#define LOGERROR 25


#define SYSLOGF_BUFSIZE 1024
#define Syslog_LogMessage 0x4C880

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

void syslogf(char *logname, int level, char *fmt, ...)
{
	static char syslogbuf[SYSLOGF_BUFSIZE];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(syslogbuf, sizeof(syslogbuf), fmt, ap);
	va_end(ap);

	/* Ignore any errors, as there's not much we can do with them */
	_swix(Syslog_LogMessage, _INR(0,2), logname, syslogbuf, level);
}

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
	info_block.filetype = IMAGE_FILETYPE;
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

	rpc_init_header();

    return err;
}

_kernel_oserror *finalise(int fatal, int podule_base, void *private_word)
{
	os_error *err;

    (void)fatal;
    (void)podule_base;
    (void)private_word;

	if (enablelog) syslogf(LOGNAME, LOGENTRY, "Module finalisation %s %s %s", Module_Title, Module_VersionString, Module_Date);

    err = _swix(OS_FSControl, _INR(0,1), 36, IMAGE_FILETYPE);

    return err;
}

_kernel_oserror *imageentry_open_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;

	(void)pw;

	if (enablelog) logentry("Open    ", (char *)(r->r[1]), r);

	err = open_file((char *)r->r[1], r->r[0], (struct conn_info *)(r->r[6]), (unsigned int *)&(r->r[0]), (struct file_handle **)&(r->r[1]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]), (unsigned int *)&(r->r[4]));

	if (enablelog) logexit(r, err);

	return err;
}

_kernel_oserror *imageentry_getbytes_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;

	(void)pw;

	if (enablelog) logentry("GetBytes", "", r);

	err = get_bytes((struct file_handle *)(r->r[1]), (char *)(r->r[2]), r->r[3], r->r[4]);

	if (enablelog) logexit(r, err);

	return err;
}

_kernel_oserror *imageentry_putbytes_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err = NULL;

	(void)pw;

	if (enablelog) logentry("PutBytes", "", r);

	err = put_bytes((struct file_handle *)(r->r[1]), (char *)(r->r[2]), r->r[3], r->r[4]);

	if (enablelog) logexit(r, err);

	return err;
}

_kernel_oserror *imageentry_args_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;

	(void)pw;

	if (enablelog) logentry("Args    ", "", r);

	switch (r->r[0]) {
		case IMAGEENTRY_ARGS_WRITEEXTENT:
			err = args_writeextent((struct file_handle *)(r->r[1]),(unsigned int)(r->r[2]));
			break;
		case IMAGEENTRY_ARGS_READSIZE:
			err = args_readallocatedsize((struct file_handle *)(r->r[1]), (unsigned int *)&(r->r[2]));
			break;
		case IMAGEENTRY_ARGS_FLUSH:
			err = args_readdatestamp((struct file_handle *)(r->r[1]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]));
			break;
		case IMAGEENTRY_ARGS_ENSURESIZE:
			err = args_ensuresize((struct file_handle *)(r->r[1]),(unsigned int)(r->r[2]), (unsigned int *)&(r->r[2]));
			break;
		case IMAGEENTRY_ARGS_ZEROPAD:
			err = args_zeropad((struct file_handle *)(r->r[1]), (unsigned int)(r->r[2]), (unsigned int)(r->r[3]));
			break;
		case IMAGEENTRY_ARGS_READDATESTAMP:
			err = args_readdatestamp((struct file_handle *)(r->r[1]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]));
			break;
		default:
			err = gen_error(UNSUPP, UNSUPPMESS);
	}

	if (enablelog) logexit(r, err);

	return err;
}

_kernel_oserror *imageentry_close_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;

	(void)pw;

	if (enablelog) logentry("Close   ", "", r);

	err = close_file((struct file_handle *)(r->r[1]), r->r[2], r->r[3]);

	if (enablelog) logexit(r, err);

	return err;
}

_kernel_oserror *imageentry_file_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;

	(void)pw;

	if (enablelog) logentry("File    ", (char *)(r->r[1]), r);

	switch (r->r[0]) {
		case IMAGEENTRY_FILE_SAVEFILE:
			err = file_savefile((char *)(r->r[1]), r->r[2], r->r[3], (char *)(r->r[4]), (char *)(r->r[5]),  (struct conn_info *)(r->r[6]), (char **)(&(r->r[6])));
			break;
		case IMAGEENTRY_FILE_WRITECATINFO:
			err = file_writecatinfo((char *)(r->r[1]), r->r[2], r->r[3], r->r[5], (struct conn_info *)(r->r[6]));
			break;
		case IMAGEENTRY_FILE_READCATINFO:
			err = file_readcatinfo((char *)(r->r[1]), (struct conn_info *)(r->r[6]), &(r->r[0]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]), &(r->r[4]), &(r->r[5]));
			break;
		case IMAGEENTRY_FILE_DELETE:
			err = file_delete((char *)(r->r[1]), (struct conn_info *)(r->r[6]), &(r->r[0]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]), &(r->r[4]), &(r->r[5]));
			break;
		case IMAGEENTRY_FILE_CREATEFILE:
			err = file_createfile((char *)(r->r[1]), r->r[2], r->r[3], (char *)(r->r[4]), (char *)(r->r[5]),  (struct conn_info *)(r->r[6]));
			break;
		case IMAGEENTRY_FILE_CREATEDIR:
			err = file_createdir((char *)(r->r[1]), r->r[2], r->r[3], (struct conn_info *)(r->r[6]));
			break;
		case IMAGEENTRY_FILE_READBLKSIZE:
			err = file_readblocksize((unsigned int *)&(r->r[2]));
			break;
		default:
			err = gen_error(UNSUPP, UNSUPPMESS);
	}

	if (enablelog) logexit(r, err);

	return err;
}

_kernel_oserror *imageentry_func_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;

	(void)pw;

	if (enablelog) logentry("Func    ", r->r[0] == 22 ? "" : (char *)(r->r[1]), r);

	switch (r->r[0]) {
		case IMAGEENTRY_FUNC_RENAME:
			err = func_rename((char *)(r->r[1]), (char *)(r->r[2]), (struct conn_info *)(r->r[6]), &(r->r[1]));
			break;
		case IMAGEENTRY_FUNC_READDIR:
			err = func_readdirinfo(0, (char *)r->r[1], (void *)r->r[2], r->r[3], r->r[4], r->r[5], (struct conn_info *)(r->r[6]), &(r->r[3]), &(r->r[4]));
			break;
		case IMAGEENTRY_FUNC_READDIRINFO:
			err = func_readdirinfo(1, (char *)r->r[1], (void *)r->r[2], r->r[3], r->r[4], r->r[5], (struct conn_info *)(r->r[6]), &(r->r[3]), &(r->r[4]));
			break;
		case IMAGEENTRY_FUNC_NEWIMAGE:
			err = func_newimage(r->r[1],(struct conn_info **)&(r->r[1]));
			break;
		case IMAGEENTRY_FUNC_CLOSEIMAGE:
			err = func_closeimage((struct conn_info *)(r->r[1]));
			break;

		/* The following entrypoints are meaningless for NFS.
		   We could fake them, but I think it is better to give an error */
		case IMAGEENTRY_FUNC_READDEFECT:
		case IMAGEENTRY_FUNC_ADDDEFECT:
		case IMAGEENTRY_FUNC_READBOOT:
		case IMAGEENTRY_FUNC_WRITEBOOT:
		case IMAGEENTRY_FUNC_READUSEDSPACE:
		case IMAGEENTRY_FUNC_READFREESPACE:
		case IMAGEENTRY_FUNC_NAME:
		case IMAGEENTRY_FUNC_STAMP:
		case IMAGEENTRY_FUNC_GETUSAGE:
		default:
			err = gen_error(UNSUPP, UNSUPPMESS);
	}

	if (enablelog) logexit(r, err);

	return err;
}
