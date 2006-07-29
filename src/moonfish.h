/*
	$Id$


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

#ifndef MOONFISH_H
#define MOONFISH_H


#define LOGNAME "Moonfish"

#define LOG_SERIOUS 30
#define LOG_MEM 50
#define LOG_ERROR 150
#define LOG_ACCESS 200
#define LOG_DATA 250

#define OUTOFMEM "Unable to allocate enough memory"

/*The maximum size of our data buffers */
#define MAX_DATABUFFER 16*1024

extern int logging;

#define STATEID_NONE NULL
#define STATEID_ANY  ((struct stateid *)0xFFFFFFFF)

#define LEASE_TIMEOUT (30 * CLOCKS_PER_SEC)

#define EXPORTSREAD  "Choices:Moonfish.exports"
#define EXPORTSWRITE "<Choices$Write>.Moonfish.exports"

#define NE(x) do { \
	res->status = x; \
	if (res->status != NFS_OK) return SUCCESS; \
} while (0)

#define UE(x) do { \
	if ((x) == NULL) { \
		res->status = NFSERR_IO; \
		return SUCCESS; \
	} else { \
		res->status = NFS_OK; \
	} \
} while (0)


#define OE(x) do { \
	os_error *err = x; \
	if (err) { \
		res->status = oserr_to_nfserr(err->errnum); \
		syslogf(LOGNAME, LOG_ERROR, "Error: %x %s", err->errnum, err->errmess); \
		return SUCCESS; \
	} else { \
		res->status = NFS_OK; \
	} \
} while (0)

#define UR(x) do { \
	if ((x) == NULL) { \
		syslogf(LOGNAME, LOG_MEM, OUTOFMEM); \
		return NFSERR_IO; \
	} \
} while (0)

#define ER(x) do { \
	os_error *err = x; \
	if (x) { \
		syslogf(LOGNAME, LOG_SERIOUS, "%s", err->errmess); \
		return NULL; \
	} \
} while (0)

#define NR(x) do { \
	enum nstat status = x; \
	if (status != NFS_OK) return status; \
} while (0)

#define BR(x) do { \
	if ((x) == FALSE) return 1; \
} while (0)

#define OR(x) do { \
	os_error *err = x; \
	if (err) { \
		syslogf(LOGNAME, LOG_ERROR, "Error: %x %s", err->errnum, err->errmess); \
		return oserr_to_nfserr(err->errnum); \
	} \
} while (0)

#define NF(x) do { \
	res->status = x; \
	if (res->status != NFS_OK) goto failure; \
} while (0)

#define UF(x) do { \
	if ((x) == NULL) { \
		res->status = NFSERR_IO; \
		goto failure; \
	} else { \
		res->status = NFS_OK; \
	} \
} while (0)

#define OF(x) do { \
	os_error *err = x; \
	if (err) { \
		res->status = oserr_to_nfserr(err->errnum); \
		syslogf(LOGNAME, LOG_ERROR, "Error: %x %s", err->errnum, err->errmess); \
		goto failure; \
	} else { \
		res->status = NFS_OK; \
	} \
} while (0)

#define UU(x) do { \
	if ((x) == NULL) { \
		syslogf(LOGNAME, LOG_MEM, OUTOFMEM); \
		return NULL; \
	} \
} while (0)

#define N4(x) do { \
	res->status = (x); \
	if (res->status != NFS_OK) return res->status; \
} while (0)

#define O4(x) do { \
	os_error *err = (x); \
	if (err) { \
		res->status = oserr_to_nfserr(err->errnum); \
		syslogf(LOGNAME, LOG_ERROR, "Error: %x %s", err->errnum, err->errmess); \
		return res->status; \
	} else { \
		res->status = NFS_OK; \
	} \
} while (0)

#define U4(x) do { \
	if ((x) == NULL) { \
		res->status = NFSERR_SERVERFAULT; \
		syslogf(LOGNAME, LOG_MEM, OUTOFMEM); \
		return res->status; \
	} else { \
		res->status = NFS_OK; \
	} \
} while (0)


#endif

