#include "kernel.h"
#include <stdio.h>
#include <string.h>

#include "nfs-calls.h"
#include "mount-calls.h"
#include "portmapper-calls.h"
#include "rpc.h"
#include "readdir.h"

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

_kernel_oserror *nfs_initialise(const char *cmd_tail, int podule_base, void *private_word)
{
	_kernel_swi_regs r;

    UNUSED(cmd_tail);
    UNUSED(podule_base);

    /*printf("initialising...\n"); */

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
    if (_kernel_swi(0x29,&r,&r)) printf("err regsitering");

	rpc_init_header();
	syslogf("NFS",1,"initing");

    return NULL;
}

_kernel_oserror *nfs_finalise(int fatal, int podule_base, void *private_word)
{
	_kernel_swi_regs r;

    UNUSED(fatal);
    UNUSED(podule_base);
    UNUSED(private_word);

	r.r[0] = 36;
	r.r[1] = 0x001;
    _kernel_swi(0x29,&r,&r);
	syslogf("NFS",1,"finalising");
    /*printf("finalising...\n"); */
    return NULL;
}

_kernel_oserror *imageentry_open_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;
	UNUSED(pw);
	syslogf("NFS",1,"open");

	err = open_file((char *)r->r[1], r->r[0], (struct conn_info *)(r->r[6]), &(r->r[0]), &(r->r[1]), &(r->r[3]));
	r->r[2] = 1024;
	r->r[4] = (r->r[3] + 1023) & ~1023;
	return err;
}

_kernel_oserror *imageentry_getbytes_handler(_kernel_swi_regs *r, void *pw)
{
	UNUSED(pw);
	syslogf("NFS",1,"getbytes");

	return get_bytes((struct file_handle *)(r->r[1]), (char *)(r->r[2]), r->r[3], r->r[4]);
}

_kernel_oserror *imageentry_putbytes_handler(_kernel_swi_regs *r, void *pw)
{
	UNUSED(pw);
	syslogf("NFS",1,"putbytes");

	return put_bytes((struct file_handle *)(r->r[1]), (char *)(r->r[2]), r->r[3], r->r[4]);
}

_kernel_oserror *imageentry_args_handler(_kernel_swi_regs *r, void *pw)
{
	static _kernel_oserror err = {1,"nfs args bits not yet implemented"};
	UNUSED(pw);
	UNUSED(r);
	syslogf("NFS",1,"args");
	return &err;
}

_kernel_oserror *imageentry_close_handler(_kernel_swi_regs *r, void *pw)
{
	UNUSED(pw);

	syslogf("NFS",1,"close");
	return close_file((struct file_handle *)(r->r[1]), r->r[2], r->r[3]);
}

_kernel_oserror *imageentry_file_handler(_kernel_swi_regs *r, void *pw)
{
	static _kernel_oserror err = {1,""};
	UNUSED(pw);
	syslogf("NFS",1,"file %d",r->r[0]);
	switch (r->r[0]) {
	case IMAGEENTRY_FILE_SAVEFILE:
		return file_savefile((char *)(r->r[1]), r->r[2], r->r[3], (char *)(r->r[4]), (char *)(r->r[5]),  (struct conn_info *)(r->r[6]), (char **)(&(r->r[6])));
	case IMAGEENTRY_FILE_READCATINFO:
		return file_readcatinfo((char *)(r->r[1]), (struct conn_info *)(r->r[6]), &(r->r[0]), &(r->r[2]), &(r->r[3]), &(r->r[4]), &(r->r[5]));
	case IMAGEENTRY_FILE_DELETE:
		return file_delete((char *)(r->r[1]), (struct conn_info *)(r->r[6]), &(r->r[0]), &(r->r[2]), &(r->r[3]), &(r->r[4]), &(r->r[5]));
	default:
		sprintf(err.errmess,"nfs file %d '%s' not yet implemented",r->r[0],r->r[0] == 5? (char*)(r->r[1]) : "!!!");
	}
	return &err;
}

_kernel_oserror *imageentry_func_handler(_kernel_swi_regs *r, void *pw)
{
	static _kernel_oserror err2 = {1,"unknown func called"};

	UNUSED(pw);
	syslogf("NFS",1,"func %d",r->r[0]);

	switch (r->r[0]) {
	case IMAGEENTRY_FUNC_READDIRINFO:
		return func_readdirinfo(1, (const char *)r->r[1], (void *)r->r[2], r->r[3], r->r[4], r->r[5], (struct conn_info *)(r->r[6]), &(r->r[3]), &(r->r[4]));
	case IMAGEENTRY_FUNC_NEWIMAGE:
		return func_newimage(r->r[1],(struct conn_info **)&(r->r[1]));
	case IMAGEENTRY_FUNC_CLOSEIMAGE:
		return func_closeimage((struct conn_info *)(r->r[1]));
	case IMAGEENTRY_FUNC_RENAME:
		return file_rename((char *)(r->r[1]), (char *)(r->r[2]), (struct conn_info *)(r->r[6]));
	case IMAGEENTRY_FUNC_READDIR:
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
		return &err2;
	}
	return NULL;
}
