/*
	$Id$
	$URL$

	Common bits for all entry points
*/

#ifndef IMAGEENTRY_COMMON_H
#define IMAGEENTRY_COMMON_H


#include <kernel.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "utils.h"

#ifdef NFS3
#include "nfs3-structs.h"
#else
#include "nfs2-structs.h"
#endif


#ifdef NFS3
#define commonfh_to_fh(fh, common) do { \
	(fh).data.data = (common).handle; \
	(fh).data.size = (common).size; \
} while (0)
#else
#define commonfh_to_fh(fh, common) do { \
	memcpy((fh).data, (common).handle, FHSIZE); \
} while (0)
#endif

#ifdef NFS3
#define fh_to_commonfh(common, fh) do { \
	memcpy((common).handle, (fh).data.data, (fh).data.size); \
	(common).size = (fh).data.size; \
} while (0)
#else
#define fh_to_commonfh(common, fh) do { \
	memcpy((common).handle, (fh).data, FHSIZE); \
} while (0)
#endif

struct objinfo {
	struct commonfh objhandle;
	struct fattr attributes;
};

/* Generate a RISC OS error block based on the NFS status code */
ENTRYFUNCDECL(os_error *, gen_nfsstatus_error, (enum nstat stat));

/* Convert a leafname into an nfs handle, following symlinks as necessary */
ENTRYFUNCDECL(os_error *, leafname_to_finfo, (char *leafname, unsigned int *len, int simple, int followsymlinks, struct commonfh *dirhandle, struct objinfo **finfo, enum nstat *status, struct conn_info *conn));

/* Convert a full filename/dirname into an nfs handle */
ENTRYFUNCDECL(os_error *, filename_to_finfo, (char *filename, int followsymlinks, struct objinfo **dinfo, struct objinfo **finfo, char **leafname, int *filetype, int *extfound, struct conn_info *conn));

/* Convert a unix timestamp into a RISC OS load and execution address */
ENTRYFUNCDECL(void, timeval_to_loadexec, (struct ntimeval *unixtime, int filetype, unsigned int *load, unsigned int *exec));

/* Convert a RISC OS load and execution address into a unix timestamp */
ENTRYFUNCDECL(void, loadexec_to_timeval, (unsigned int load, unsigned int exec, struct ntimeval *unixtime));


#endif

