#include "kernel.h"
#include <stdio.h>
#include <string.h>

#include "nfs-calls.h"
#include "mount-calls.h"
#include "rpc.h"
#include "readdir.h"

#include "moduledefs.h"

#define UNUSED(x) x=x

extern int module_base_address;
/*typedef void (*veneer)(void);*/
typedef int veneer;

struct {
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

struct dir_entry {
	unsigned int load;
	int exec;
	int len;
	int attr;
	int type;
	char name[1];
};

/* Must handle service redeclare
void tm_service(int service_number, _kernel_swi_regs *r, void *private_word)
{
    UNUSED(r);
    UNUSED(private_word);
} */

_kernel_oserror *nfs_initialise(const char *cmd_tail, int podule_base, void *private_word)
{
	_kernel_swi_regs r;

    UNUSED(cmd_tail);
    UNUSED(podule_base);

    printf("initialising...\n");

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
    printf("finalising...\n");
    return NULL;
}

_kernel_oserror *imageentry_open_handler(_kernel_swi_regs *r, void *pw)
{
	static _kernel_oserror err = {1,"nfs open bits not yet implemented"};
	return &err;
}
_kernel_oserror *imageentry_getbytes_handler(_kernel_swi_regs *r, void *pw)
{
	static _kernel_oserror err = {1,"nfs getbytes bits not yet implemented"};
	return &err;
}
_kernel_oserror *imageentry_putbytes_handler(_kernel_swi_regs *r, void *pw)
{
	static _kernel_oserror err = {1,"nfs putbytes bits not yet implemented"};
	return &err;
}
_kernel_oserror *imageentry_args_handler(_kernel_swi_regs *r, void *pw)
{
	static _kernel_oserror err = {1,"nfs args bits not yet implemented"};
	return &err;
}
_kernel_oserror *imageentry_close_handler(_kernel_swi_regs *r, void *pw)
{
	static _kernel_oserror err = {1,"nfs close bits not yet implemented"};
	return &err;
}
_kernel_oserror *imageentry_file_handler(_kernel_swi_regs *r, void *pw)
{
	static _kernel_oserror err = {1,"nfs file bits not yet implemented"};
	return &err;
}

_kernel_oserror *imageentry_func_handler(_kernel_swi_regs *r, void *pw)
{
	static int handle = 0;
	struct dir_entry *entry;
	static _kernel_oserror err = {1,"unknown func called"};

	static string dir = {11,"/nfssandbox"};
	static struct fhstatus res;
	static struct readdirargs rddir;
	static struct readdirres rdres;
	static struct readdirok_entry direntry;
	static struct readdirok eof;
	int numentries = 0;

	switch (r->r[0]) {
	case 15:
		entry = (struct dir_entry *)(r->r[2]);
		init_header();
/*		NFSPROC3_NULL(NULL);*/
		MNTPROC_MNT(&dir, &res, NULL);
		memcpy(rddir.dir, res.u.directory, FHSIZE);
		rddir.count = 1024;
		rddir.cookie = 0;
		NFSPROC_READDIR(&rddir, &rdres, NULL);
		do {
			NFSPROC_READDIR_entry(&direntry,NULL);
			if (direntry.opted) {
				/*direntry.u.u.name.size = 3;
				direntry.u.u.name.data = "foo"; */
				if (direntry.u.u.name.size == 1 && direntry.u.u.name.data[0] == '.') {
					/* current dir */
				} else if (direntry.u.u.name.size == 2 && direntry.u.u.name.data[0] == '.' && direntry.u.u.name.data[1] == '.') {
					/* parent dir */
				} else {
					char *buf;
					entry->load = 0xFFF10200;
					entry->exec = 0;
					entry->len = 1023;
					entry->attr = 3;
					entry->type = 1;
					memcpy(entry->name,direntry.u.u.name.data,direntry.u.u.name.size);
					entry->name[direntry.u.u.name.size] = '\0';
/*					buf = entry;
					buf +=*/
					entry = (struct dir_entry *)((int)((((char *)entry) + 5*4 + direntry.u.u.name.size + 4)) & ~3);
					numentries++;
				}
			}
		} while (direntry.opted);
		NFSPROC_READDIR_eof(&eof, NULL);
		r->r[3] = numentries;
		r->r[4] = -1;
		break;
	case 21:
		r->r[1] = handle++;
		break;
	case 22:
		break;
	default:
		return &err;
	}
	return NULL;
}
