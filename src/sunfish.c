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
#include <string.h>
#include <swis.h>
#ifdef USE_TCPIPLIBS
# include <unixlib.h>
# include <riscos.h>
#else
# include <strings.h>
#endif

#include "sunfishdefs.h"
#include "sunfish.h"

#include "imageentry_func.h"
#include "imageentry_file.h"
#include "imageentry_bytes.h"
#include "imageentry_openclose.h"
#include "imageentry_args.h"

#include "rpc.h"


extern int module_base_address;

extern void free_wrapper(void);

typedef int veneer;

struct fs_info_block {
	veneer fsname;
	veneer startuptext;
	veneer fsentry_open;
	veneer fsentry_getbytes;
	veneer fsentry_putbytes;
	veneer fsentry_args;
	veneer fsentry_close;
	veneer fsentry_file;
	unsigned information_word;
	veneer fsentry_func;
	veneer fsentry_gbpb;
	unsigned extra_information_word;
};

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

static const char *fsname = "Sunfish";

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

/* SWIs
All registers preserved unless stated. All SWIs are non-reentrant.

Sunfish_Mount
Create a new mount. Will first dismount any existing mount of the same name.
=> r0 disc name
   r1 special field (or 0 for none)
   r2 ptr to mountfile contents (null terminated)


Sunfish_dismount
Remove a mount.
=> r0 disc name
   r1 special field (or 0 for none)

Sunfish_ListMounts
Iterate through the list of currently mounted mounts.
=> r0 start value (0 for first item)
<= r0 next value (0 when last item reached)
   r1 disc name (or 0 if no item)
   r2 special field (or 0 if no special field)

Sunfish_Free
Called by free_wrapper for reason codes 0,1,2 with registers unchanged.

*/

struct fsmount {
	char *discname;
	char *specialfield;
	struct conn_info *conn;
	struct fsmount *next;
};

static struct fsmount *fsmounts = NULL;

static void fs_dismount_all(void)
{
	struct fsmount *mount = fsmounts;
	struct fsmount *next;

	while (mount) {
		next = mount->next;
		CALLENTRY(func_closeimage, mount->conn, (mount->conn));
		free(mount->discname);
		if (mount->specialfield) free(mount->specialfield);
		free(mount);
		mount = next;
	}
}

static void fs_dismount(char *discname, char *specialfield)
{
	struct fsmount *mount = fsmounts;
	struct fsmount **prevmount = &fsmounts;

	/* Search for a matching mount */
	while (mount) {
		if (strcasecmp(mount->discname, discname) == 0) {
			if (specialfield == NULL) {
				if (mount->specialfield == NULL) break;
			} else {
				if ((mount->specialfield != NULL) &&
				    (strcmp(specialfield, mount->specialfield) == 0)) {
					break;
				}
			}
		}
		prevmount = &(mount->next);
		mount = mount->next;
	}

	/* Remove mount if found */
	if (mount) {
		CALLENTRY(func_closeimage, mount->conn, (mount->conn));
		free(mount->discname);
		if (mount->specialfield) free(mount->specialfield);
		*prevmount = mount->next;
		free(mount);
	}
}

/* Filenames are of the form :discname.$.foo.bar
   We need to extract the discname and the foo.bar component, and then find
   a matching connection. */
static os_error *filename_to_conn(char *filename, char *specialfield, struct conn_info **conn, char **retfilename)
{
	struct fsmount *mount = fsmounts;
	size_t discnamelen;

	if (filename[0] == ':') filename++;

	*retfilename = strchr(filename, '$');
	if (*retfilename == NULL) return gen_error(INVALFILENAMEERR,"Invalid filename");

	discnamelen = *retfilename - filename;
	if (discnamelen && (filename[discnamelen - 1] == '.')) discnamelen--;

	(*retfilename)++;
	if (**retfilename == '.') (*retfilename)++;

	while (mount) {
		if (strncasecmp(mount->discname, filename, discnamelen) == 0) {
			if (specialfield == NULL) {
				if (mount->specialfield == NULL) break;
			} else {
				if ((mount->specialfield != NULL) &&
				    (strcasecmp(specialfield, mount->specialfield) == 0)) {
					break;
				}
			}
		}
		mount = mount->next;
	}

	if (mount) {
		*conn = mount->conn;
	} else {
		return gen_error(NODISCERR, "Disc not found");
	}

	return NULL;
}

/* Get the free space for the free module */
static os_error *getfree(char *id, unsigned *freelo, unsigned *freehi, unsigned *sizelo, unsigned *sizehi, unsigned *usedlo, unsigned *usedhi)
{
	char discname[256];
	char *hash;
	char *specialfield = NULL;
	char *filename = discname;
	struct conn_info *conn;
	char *retfilename;
	os_error *err;

	/* The id is specialfield#discname, or just discname if no special
	   field. Therefore we need to separate them, and convert to a
	   filename that filename_to_con can parse. */
	snprintf(discname, sizeof(discname), "%s.$", id);
	hash = strchr(discname, '#');
	if (hash) {
		specialfield = discname;
		*hash++ = '\0';
		filename = hash;
	}

	err = filename_to_conn(filename, specialfield, &conn, &retfilename);
	if (err) goto err;
	err = CALLENTRY(func_free, conn, (retfilename, conn, freelo, freehi, NULL, sizelo, sizehi, usedlo, usedhi));
	if (err) goto err;

	return NULL;
err:
	syslogf(LOGNAME, LOGERROR, "Error getting free information %x, %s", err->errnum, err->errmess);
	/* The free module doesn't like errors being returned */
	*freelo = 0;
	*freehi = 0;
	*sizelo = 1;
	*sizehi = 0;
	*usedlo = 1;
	*usedhi = 0;
	return NULL;
}

_kernel_oserror *swi_handler(int swi_offset, _kernel_swi_regs *r, void *pw)
{
	os_error *err;
	struct fsmount *mount;

	(void)pw;

	switch (swi_offset) {
	case Sunfish_Mount - Sunfish_00:
		/* Dismount any previous mount with the same name and special field */
		fs_dismount((char *)(r->r[0]), (char *)(r->r[1]));

		/* Create the new list entry */
		mount = malloc(sizeof(struct fsmount));
		if (mount == NULL) return gen_error(NOMEM,NOMEMMESS);
		mount->discname = strdup((char *)(r->r[0]));
		if (mount->discname == NULL) {
			free(mount);
			return gen_error(NOMEM,NOMEMMESS);
		}
		if (r->r[1]) {
			mount->specialfield = strdup((char *)(r->r[1]));
			if (mount->specialfield == NULL) {
				free(mount->discname);
				free(mount);
				return gen_error(NOMEM,NOMEMMESS);
			}
		} else {
			mount->specialfield = NULL;
		}

		/* Create the connection and mount the server */
		err = func_newimage(0, (char *)(r->r[2]), &(mount->conn));
		if (err) {
			if (mount->specialfield == NULL) free(mount->specialfield);
			free(mount->discname);
			free(mount);
			return err;
		}

		/* Add to linked list */
		mount->next = fsmounts;
		fsmounts = mount;
		break;
	case Sunfish_Dismount - Sunfish_00:
		fs_dismount((char *)(r->r[0]), (char *)(r->r[1]));
		break;
	case Sunfish_ListMounts - Sunfish_00:
		mount = (struct fsmount *)(r->r[0]);
		if (mount == NULL) mount = fsmounts;
		if (mount) {
			r->r[0] = (int)(mount->next);
			r->r[1] = (int)(mount->discname);
			r->r[2] = (int)(mount->specialfield);
		} else {
			r->r[0] = 0;
			r->r[1] = 0;
			r->r[2] = 0;
		}
		break;
	case Sunfish_Free - Sunfish_00:
		switch (r->r[0]) {
		case 0:
			/* NOP */
			break;
		case 1:
			/* Get device name */
			r->r[0] = strlen((char *)(r->r[3]));
			memcpy((char *)(r->r[2]), (char *)(r->r[3]), r->r[0] + 1);
			break;
		case 2: {
			/* Get free space */
			unsigned *buf = (unsigned *)(r->r[2]);

			err = getfree((char *)(r->r[3]), buf + 1, NULL, buf, NULL, buf + 2, NULL);
			if (err) return err;
			break;
			}
		case 4: {
			/* Get free space 64 bit */
			unsigned *buf = (unsigned *)(r->r[2]);

			err = getfree((char *)(r->r[3]), buf + 2, buf + 3, buf, buf + 1, buf + 4, buf + 5);
			if (err) return err;
			r->r[0] = 0;
			break;
			}
		}
		break;
	default:
		return error_BAD_SWI;
	}
	return NULL;
}

static os_error *declare_fs(void *private_word)
{
	struct imagefs_info_block info_block;
	struct fs_info_block fsinfo_block;
	os_error *err;

	info_block.information_word = 0;
	info_block.filetype = SUNFISH_FILETYPE;
	info_block.imageentry_open =     ((int)imageentry_open) - module_base_address;
	info_block.imageentry_getbytes = ((int)imageentry_getbytes) - module_base_address;
	info_block.imageentry_putbytes = ((int)imageentry_putbytes) - module_base_address;
	info_block.imageentry_args =     ((int)imageentry_args) - module_base_address;
	info_block.imageentry_close =    ((int)imageentry_close) - module_base_address;
	info_block.imageentry_file =     ((int)imageentry_file) - module_base_address;
	info_block.imageentry_func =     ((int)imageentry_func) - module_base_address;

	fsinfo_block.information_word = 0x80100000 | SUNFISH_FSNUMBER;
	fsinfo_block.extra_information_word = 0;
	fsinfo_block.fsname =           ((int)fsname) - module_base_address;
	fsinfo_block.startuptext =      ((int)fsname) - module_base_address;
	fsinfo_block.fsentry_open =     ((int)fsentry_open) - module_base_address;
	fsinfo_block.fsentry_getbytes = ((int)imageentry_getbytes) - module_base_address;
	fsinfo_block.fsentry_putbytes = ((int)imageentry_putbytes) - module_base_address;
	fsinfo_block.fsentry_args =     ((int)imageentry_args) - module_base_address;
	fsinfo_block.fsentry_close =    ((int)imageentry_close) - module_base_address;
	fsinfo_block.fsentry_file =     ((int)fsentry_file) - module_base_address;
	fsinfo_block.fsentry_func =     ((int)fsentry_func) - module_base_address;
	fsinfo_block.fsentry_gbpb = 0;

	err = _swix(OS_FSControl, _INR(0,3), 35, module_base_address, ((int)(&info_block)) - module_base_address, private_word);
	if (err) return err;

	return _swix(OS_FSControl, _INR(0,3), 12, module_base_address, ((int)(&fsinfo_block)) - module_base_address, private_word);
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

	err = _swix(Free_Register, _INR(0,2), SUNFISH_FSNUMBER, free_wrapper, 0);
	if (err) syslogf(LOGNAME, LOGERROR, "Error %x, %s", err->errnum, err->errmess);

	return declare_fs(private_word);
}

_kernel_oserror *finalise(int fatal, int podule_base, void *private_word)
{
	os_error *err1 = NULL;
	os_error *err2 = NULL;
	static int finalised = 0;

	(void)fatal;
	(void)podule_base;
	(void)private_word;

	if (enablelog) syslogf(LOGNAME, LOGENTRY, "Module finalisation");

	if (!finalised) {
		err1 = _swix(Free_DeRegister, _INR(0,2), SUNFISH_FSNUMBER, free_wrapper, 0);
		if (err1) syslogf(LOGNAME, LOGERROR, "Error %x, %s", err1->errnum, err1->errmess);

		err1 = _swix(OS_FSControl, _INR(0,1), 16, fsname);
		err2 = _swix(OS_FSControl, _INR(0,1), 36, SUNFISH_FILETYPE);
		fs_dismount_all();
	}
	finalised = 1;
	/* OS_FSControl 36 can return an error which we want to notify the caller
	   of, but that prevents the module from dying, and the next time
	   finalisation is called the FSControl call will fail as the imagefs
	   is no longer registered, so the module can never be killed.
	   So we make sure that we only deregister once. */

	rpc_free_all_buffers();

	if (err2) return err2;
	return err1;
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

_kernel_oserror *fsentry_open_handler(_kernel_swi_regs *r, void *pw)
{
	os_error *err;
	struct conn_info *conn;
	char *filename;
	unsigned info_word;


	(void)pw;

	ENTRY("Open    ", (char *)(r->r[1]), r);

	err = filename_to_conn((char *)(r->r[1]), (char *)(r->r[6]), &conn, &filename);
	if (err) return err;

	CONNENTRY(conn);

	/* Set bit 29 of r0, indicating it is a directory. Will be cleared by
	   open_file if it is not. */
	info_word = 0x20000000;
	err = CALLENTRY(open_file, conn, (filename, r->r[0], conn, &info_word, (struct file_handle **)&(r->r[1]), (unsigned int *)&(r->r[2]), (unsigned int *)&(r->r[3]), (unsigned int *)&(r->r[4])));
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

_kernel_oserror *fsentry_file_handler(_kernel_swi_regs *r, void *pw)
{
	struct conn_info *conn;
	char *filename;
	os_error *err;

	(void)pw;

	ENTRY("FSFile  ", (char *)(r->r[1]), r);

	err = filename_to_conn((char *)(r->r[1]), (char *)(r->r[6]), &conn, &filename);
	if (err) return err;

	err = file_handler(filename, conn, r);

	EXIT(r, err);

	return err;
}

static os_error *func_handler(char *filename, struct conn_info *conn, _kernel_swi_regs *r)
{
	os_error *err = NULL;
	char *to;

	switch (r->r[0]) {
		case FSENTRY_FUNC_SETCURDIR:
		case FSENTRY_FUNC_SETLIBDIR:
			break;
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
		case FSENTRY_FUNC_READNAMEANDBOOT:
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
			r->r[3] = 0;
			r->r[4] = 0;
			break;
		case FSENTRY_FUNC_RESOLVEWILDCARD:
			r->r[4] = -1;
			break;
		case IMAGEENTRY_FUNC_READBOOT:
			/* This entry point is meaningless, but we must
			   support it as *ex et al call it. */
			r->r[2] = 0;
			break;

		case IMAGEENTRY_FUNC_READFREESPACE:
			CONNENTRY(conn);
			err = CALLENTRY(func_free, conn, (filename, conn, (unsigned *)&(r->r[0]), NULL, (unsigned *)&(r->r[1]), (unsigned *)&(r->r[2]), NULL, NULL, NULL));
			CONNEXIT(conn);
			break;
		case IMAGEENTRY_FUNC_READFREESPACE64:
			CONNENTRY(conn);
			err = CALLENTRY(func_free, conn, (filename, conn, (unsigned *)&(r->r[0]), (unsigned *)&(r->r[1]), (unsigned *)&(r->r[2]), (unsigned *)&(r->r[3]), (unsigned *)&(r->r[4]), NULL, NULL));
			CONNEXIT(conn);
			break;

		/* The following entrypoints are meaningless for NFS.
		   We could fake them, but I think it is better to give an error */
		case IMAGEENTRY_FUNC_READDEFECT:
		case IMAGEENTRY_FUNC_ADDDEFECT:
		case IMAGEENTRY_FUNC_WRITEBOOT:
		case IMAGEENTRY_FUNC_READUSEDSPACE:
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

_kernel_oserror *fsentry_func_handler(_kernel_swi_regs *r, void *pw)
{
	struct conn_info *conn = NULL;
	char *filename;
	os_error *err = NULL;

	(void)pw;


	switch (r->r[0]) {
		case IMAGEENTRY_FUNC_RENAME:
		case IMAGEENTRY_FUNC_READDIR:
		case IMAGEENTRY_FUNC_READDIRINFO:
		case FSENTRY_FUNC_READDIRINFO:
		case IMAGEENTRY_FUNC_READFREESPACE:
		case IMAGEENTRY_FUNC_READFREESPACE64:
			ENTRY("FSFunc  ", (char *)(r->r[1]), r);
			err = filename_to_conn((char *)(r->r[1]), (char *)(r->r[6]), &conn, &filename);
			break;
		default:
			ENTRY("FSFunc  ", "", r);
			break;
	}

	if (err == NULL) err = func_handler(filename, conn, r);

	EXIT(r, err);

	return err;
}
