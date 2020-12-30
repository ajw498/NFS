/*

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

#ifndef MOONFISH_H
#define MOONFISH_H


#define LOGNAME "Moonfish"

#define LOG_SERIOUS 30
#define LOG_MEM 50
#define LOG_ERROR 100
#define LOG_ACCESS 200
#define LOG_DATA 250

#define OUTOFMEM "Unable to allocate enough memory"

extern int logging;

#define STATEID_NONE NULL
#define STATEID_ANY  ((struct stateid *)0xFFFFFFFF)

#define LEASE_TIMEOUT 30

#define EXPORTSREAD  "Choices:Moonfish.exports"
#define EXPORTSWRITE "<Choices$Write>.Moonfish.exports"

#define CHOICESREAD  "Choices:Moonfish.Choices"
#define CHOICESWRITE "<Choices$Write>.Moonfish.Choices"

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
	nstat status = x; \
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

