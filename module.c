#include "kernel.h"
#include <stdio.h>
#include <string.h>

#include "nfs-calls.h"
#include "mount-calls.h"
#include "portmapper-calls.h"
#include "rpc.h"

#include "moduledefs.h"
#include "imageentry_func.h"
#include "imagefs.h"


#if CMHG_VERSION < 542
#error cmhg out of date
#endif

#define UNUSED(x) x=x

extern int module_base_address;
/*typedef void (*veneer)(void);*/
typedef int veneer;

static struct {
	unsigned int information_word;
	unsigned int filetype;
	veneer imageentry_open;
	veneer imageentry_getbytes;
	veneer imageentry_putbytes;
	veneer imageentry_args;
	veneer imageentry_close;
	veneer imageentry_file;
	veneer imageentry_func;
} imagefs_info_block;

/* Must handle service redeclare
void tm_service(int service_number, _kernel_swi_regs *r, void *private_word)
{
    UNUSED(r);
    UNUSED(private_word);
} */

#include <stdarg.h>


#define LOGNAME "NFS"
#define LOGENTRY 50
#define LOGEXIT  75
#define LOGERROR 25

#define UNSUPP 1
#define UNSUPPMESS "Unsupported entry point called"

static int enablelog = 1;


os_error *gen_error(int num, char *msg)
{
	static os_error module_err_buf; /* FIXME - remove duplication with other files */
	module_err_buf.errnum = num;
	strcpy(module_err_buf.errmess,msg);
	return &module_err_buf;
}

void syslogf(char *logname, int level, char *fmt, ...)
{
	static char syslogbuf[1024];
	va_list ap;
	_kernel_swi_regs r;

	va_start(ap, fmt);
	vsnprintf(syslogbuf,1024,fmt,ap);
	va_end(ap);
	r.r[0] = (int)logname;
	r.r[1] = (int)syslogbuf;
	r.r[2] = level;
	_kernel_swi(0x4C880,&r,&r);
}

static void logexit(_kernel_swi_regs *r, os_error *err)
{
	if (err) {
		syslogf(LOGNAME, LOGERROR, "Error %x, %s", err->errnum, err->errmess);
	} else {
		syslogf(LOGNAME, LOGEXIT, "Exit %x %x %x %x %x %x %x",r->r[0],r->r[1],r->r[2],r->r[3],r->r[4],r->r[5],r->r[6]);
	}
}

static void logentry(char *entry, char *filename, _kernel_swi_regs *r)
{
	syslogf(LOGNAME, LOGENTRY, "%s %s %x %x %x %x %x %x %x", entry, filename, r->r[0], r->r[1], r->r[2], r->r[3], r->r[4], r->r[5], r->r[6]);	
}

_kernel_oserror *nfs_initialise(const char *cmd_tail, int podule_base, void *private_word)
{
	_kernel_swi_regs r;
	os_error *err;

    UNUSED(cmd_tail);
    UNUSED(podule_base);

	if (enablelog) syslogf(LOGNAME, LOGENTRY, "Module initialisation");

	imagefs_info_block.information_word = 0;
	imagefs_info_block.filetype = 0x001;
	imagefs_info_block.imageentry_open =     ((int)imageentry_open) - module_base_address;
	imagefs_info_block.imageentry_getbytes = ((int)imageentry_getbytes) - module_base_address;
	imagefs_info_block.imageentry_putbytes = ((int)imageentry_putbytes) - module_base_address;
	imagefs_info_block.imageentry_args =     ((int)imageentry_args) - module_base_address;
	imagefs_info_block.imageentry_close =    ((int)imageentry_close) - module_base_address;
	imagefs_info_block.imageentry_file =     ((int)imageentry_file) - module_base_address;
	imagefs_info_block.imageentry_func =     ((int)imageentry_func) - module_base_address;

	r.r[0] = 35;
	r.r[1] = module_base_address;
	r.r[2] = ((int)(&imagefs_info_block)) - module_base_address;
	r.r[3] = (int)private_word;
    err = _kernel_swi(0x29,&r,&r);

	rpc_init_header();

    return err;
}

_kernel_oserror *nfs_finalise(int fatal, int podule_base, void *private_word)
{
	_kernel_swi_regs r;
	os_error *err;

    UNUSED(fatal);
    UNUSED(podule_base);
    UNUSED(private_word);

	if (enablelog) syslogf(LOGNAME, LOGENTRY, "Module finalisation");

	r.r[0] = 36;
	r.r[1] = 0x001;
    err = _kernel_swi(0x29,&r,&r);

    return err;
}

_kernel_oserror *imageentry_open_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err = NULL;

	UNUSED(pw);

	if (enablelog) logentry("ImageEntry_Open", (char *)(r->r[1]), r);

	err = open_file((char *)r->r[1], r->r[0], (struct conn_info *)(r->r[6]), &(r->r[0]), &(r->r[1]), &(r->r[3]));
	r->r[2] = 1024;
	r->r[4] = (r->r[3] + 1023) & ~1023;

	if (enablelog) logexit(r, err);

	return err;
}

_kernel_oserror *imageentry_getbytes_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err = NULL;

	UNUSED(pw);

	if (enablelog) logentry("ImageEntry_GetBytes", "", r);

	err = get_bytes((struct file_handle *)(r->r[1]), (char *)(r->r[2]), r->r[3], r->r[4]);

	if (enablelog) logexit(r, err);

	return err;
}

_kernel_oserror *imageentry_putbytes_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err = NULL;

	UNUSED(pw);

	if (enablelog) logentry("ImageEntry_PutBytes", "", r);

	err = put_bytes((struct file_handle *)(r->r[1]), (char *)(r->r[2]), r->r[3], r->r[4]);

	if (enablelog) logexit(r, err);

	return err;
}

_kernel_oserror *imageentry_args_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err = NULL;

	UNUSED(pw);

	if (enablelog) logentry("ImageEntry_Args", "", r);

	switch (r->r[0]) {
		case IMAGEENTRY_ARGS_WRITEEXTENT:
		case IMAGEENTRY_ARGS_READSIZE:
		case IMAGEENTRY_ARGS_FLUSH:
		case IMAGEENTRY_ARGS_ENSURESIZE:
		case IMAGEENTRY_ARGS_ZEROPAD:
		case IMAGEENTRY_ARGS_READDATESTAMP:
		default:
			err = gen_error(UNSUPP, UNSUPPMESS);
	}

	if (enablelog) logexit(r, err);

	return err;
}

_kernel_oserror *imageentry_close_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err = NULL;

	UNUSED(pw);

	if (enablelog) logentry("ImageEntry_Close", "", r);

	err = close_file((struct file_handle *)(r->r[1]), r->r[2], r->r[3]);

	if (enablelog) logexit(r, err);

	return err;
}

_kernel_oserror *imageentry_file_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err = NULL;

	UNUSED(pw);

	if (enablelog) logentry("ImageEntry_File", (char *)(r->r[1]), r);

	switch (r->r[0]) {
		case IMAGEENTRY_FILE_SAVEFILE:
			err = file_savefile((char *)(r->r[1]), r->r[2], r->r[3], (char *)(r->r[4]), (char *)(r->r[5]),  (struct conn_info *)(r->r[6]), (char **)(&(r->r[6])));
			break;
		case IMAGEENTRY_FILE_WRITECATINFO:
			err = file_writecatinfo((char *)(r->r[1]), r->r[2], r->r[3], r->r[4], (struct conn_info *)(r->r[6]));
			break;
		case IMAGEENTRY_FILE_READCATINFO:
			err = file_readcatinfo((char *)(r->r[1]), (struct conn_info *)(r->r[6]), &(r->r[0]), &(r->r[2]), &(r->r[3]), &(r->r[4]), &(r->r[5]));
			break;
		case IMAGEENTRY_FILE_DELETE:
			err = file_delete((char *)(r->r[1]), (struct conn_info *)(r->r[6]), &(r->r[0]), &(r->r[2]), &(r->r[3]), &(r->r[4]), &(r->r[5]));
			break;
		case IMAGEENTRY_FILE_CREATEFILE:
			err = file_createfile((char *)(r->r[1]), r->r[2], r->r[3], (char *)(r->r[4]), (char *)(r->r[5]),  (struct conn_info *)(r->r[6]));
			break;
		case IMAGEENTRY_FILE_CREATEDIR:
			err = file_createdir((char *)(r->r[1]), r->r[2], r->r[3], (struct conn_info *)(r->r[6]));
			break;
		case IMAGEENTRY_FILE_READBLKSIZE:
			r->r[2] = 1024; /* Should this be the buffersize? Or ask NFS? */
			break;
		default:
			err = gen_error(UNSUPP, UNSUPPMESS);
	}

	if (enablelog) logexit(r, err);

	return err;
}

_kernel_oserror *imageentry_func_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err = NULL;

	UNUSED(pw);
	if (enablelog) logentry("ImageEntry_Func", r->r[0] == 22 ? "" : (char *)(r->r[1]), r);

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
