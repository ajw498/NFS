/*
	$Id$

	Common utility routines independant of NFS version


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


#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <swis.h>
#include <stdarg.h>
#include <stdio.h>
#include <unixlib.h>

#include "utils.h"

#include "sunfish.h"


/* Generate a RISC OS error block based on the given number and message */
os_error *gen_error(int num, char *msg, ...)
{
	static os_error module_err_buf;
	va_list ap;

	va_start(ap, msg);
	vsnprintf(module_err_buf.errmess, sizeof(module_err_buf.errmess), msg, ap);
	va_end(ap);
	module_err_buf.errnum = num;
	return &module_err_buf;
}

#define SYSLOGF_BUFSIZE 1024
#define Syslog_LogMessage 0x4C880

void syslogf(char *logname, int level, char *fmt, ...)
{
	static char syslogbuf[SYSLOGF_BUFSIZE];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(syslogbuf, sizeof(syslogbuf), fmt, ap);
	va_end(ap);

	/* Ignore any errors, as there's not much we can do with them */
	_swix(Syslog_LogMessage, _INR(0,2), logname, syslogbuf, level);

#ifdef DEBUGSERVER
	printf("%s\n",syslogbuf);
#endif
}

char *host_to_str(unsigned int host, struct pool *pool)
{
	char *str;

	str = palloc(16, pool);
	if (str == NULL) return NULL;

	snprintf(str, 16, "%d.%d.%d.%d",
	         (host & 0x000000FF) >> 0,
	         (host & 0x0000FF00) >> 8,
	         (host & 0x00FF0000) >> 16,
	         (host & 0xFF000000) >> 24);
	return str;
}

nstat oserr_to_nfserr(int errnum)
{
	switch (errnum) {
	case 0x117b4: return NFSERR_NOTEMPTY;
	case 0x117bd: return NFSERR_ACCESS;
	case 0x117c2: return NFSERR_SHARE_DENIED;
	case 0x117c3: return NFSERR_ACCESS;
	case 0x117c6: return NFSERR_NOSPC;
	case 0x80344a: return NFSERR_ROFS;
	case 0xb0: return NFSERR_XDEV;
	case 0xc1: return NFSERR_ACCESS;
	case 0xd6: return NFSERR_NOENT;
	}
	return NFSERR_IO;
}

/* Convert NFSv4 specific errors to their nearest NFSv2/NFSv3 equivalent */
nstat nfserr_removenfs4(nstat errnum)
{
	switch (errnum) {
	case NFSERR_OPENMODE:      return NFSERR_ACCESS;
	case NFSERR_LOCKED:        return NFSERR_ACCESS;
	case NFSERR_SHARE_DENIED:  return NFSERR_ACCESS;
	case NFSERR_GRACE:         return NFSERR_DELAY;
	}
	if (errnum > NFSERR_DELAY) return NFSERR_IO;
	return errnum;
}

static int localutf8 = 0;

char *filename_unixify(const char *name, unsigned int len, unsigned int *newlen, int escapewin, struct pool *pool)
{
	char *namebuffer;
	int i;
	int j;

	/* Allocate more space than needed, so the ,xyz can be added later if needed */
	namebuffer = palloc(len + sizeof(",xyz"), pool);
	if (namebuffer == NULL) return NULL;

	for (i = 0, j = 0; i < len; i++) {
		switch (name[i]) {
			case '/':
				namebuffer[j++] = '.';
				break;
			case 0xC2: /* UTF8 hard space is 0xC0 0xA0 */
				if (localutf8 && (i + 1 < len) && (name[i + 1] == 0xA0)) {
					namebuffer[j++] = ' ';
					i++;
				} else {
					namebuffer[j++] = 0xC2;
				}
				break;
			case 0xA0: /* Non UTF8 hard space */
				namebuffer[j++] = localutf8 ? 0xA0 : ' ';
				break;
			case '?':
				if (i + 2 < len && isxdigit(name[i+1]) && !islower(name[i+1]) && isxdigit(name[i+2]) && !islower(name[i+2])) {
					/* An escape sequence */
					i++;
					namebuffer[j] = (name[i] > '9' ? name[i] - 'A' + 10 : name[i] - '0') << 4;
					i++;
					namebuffer[j] |= name[i] > '9' ? name[i] - 'A' + 10 : name[i] - '0';
					switch (namebuffer[j]) {
					case '?':
						namebuffer[j] = '#';
						break;
					case '<':
						namebuffer[j] = '$';
						break;
					case '>':
						namebuffer[j] = '^';
						break;
					}
					j++;
				} else {
					namebuffer[j++] = escapewin ? '#' : name[i];
				}
				break;
			case '>':
				namebuffer[j++] = escapewin ? '^' : name[i];
				break;
			case '<':
				namebuffer[j++] = escapewin ? '$' : name[i];
				break;
			default:
				namebuffer[j++] = name[i];
		}
	}

	namebuffer[j] = '\0';
	*newlen = j;

	return namebuffer;
}

#ifndef MimeMap_Translate
#define MimeMap_Translate 0x50B00
#endif
#define MMM_TYPE_RISCOS 0
#define MMM_TYPE_RISCOS_STRING 1
#define MMM_TYPE_MIME 2
#define MMM_TYPE_DOT_EXTN 3

/* Use MimeMap to lookup a filetype from an extension */
int ext_to_filetype(char *ext, int defaultfiletype)
{
	os_error *err;
	int filetype;

	/* Try MimeMap to get a filetype, use the default type if the call fails */
	err = _swix(MimeMap_Translate,_INR(0,2) | _OUT(3), MMM_TYPE_DOT_EXTN, ext, MMM_TYPE_RISCOS, &filetype);
	if (err) filetype = defaultfiletype;

	return filetype;
}

/* Use MimeMap to lookup a mimetype from a filetype */
char *filetype_to_mimetype(int filetype, struct pool *pool)
{
	os_error *err;
	char *buffer;

	if ((buffer = palloc(128, pool)) == NULL) return NULL;

	/* Try MimeMap to get a mime-type, use a default type if the call fails */
	err = _swix(MimeMap_Translate,_INR(0,3), MMM_TYPE_RISCOS, filetype, MMM_TYPE_MIME, buffer);
	if (err) strcpy(buffer, "application/octet-stream");

	return buffer;
}

int filename_riscosify(const char *name, int namelen, char *buffer, int buflen, int *filetyperet, int defaultfiletype, int xyzext, int macforks, int escapewin)
{
	int i;
	int j;
	char *dotext = NULL;
	int filetype = -1;

	for (i = 0, j = 0; (i < namelen) && (j + 3 < buflen); i++) {
		if (name[i] == '.') {
			buffer[j++] = '/';
			dotext = buffer + j;
		} else if (name[i] == ' ') {
			/* spaces to hard spaces */
			if (localutf8) buffer[j++] = 0xC2;
			buffer[j++] = 0xA0;
		} else if (name[i] == ',') {
			if (xyzext != NEVER && namelen - i == 4
			     && isxdigit(name[i+1])
			     && isxdigit(name[i+2])
			     && isxdigit(name[i+3])) {
				char tmp[4];

				tmp[0] = name[i+1];
				tmp[1] = name[i+2];
				tmp[2] = name[i+3];
				tmp[3] = '\0';
				filetype = (int)strtol(tmp, NULL, 16);
				namelen -= sizeof(",xyz") - 1;
			} else {
				buffer[j++] = name[i];
			}
		} else if (name[i] == '!') {
			buffer[j++] = name[i];
		} else if ((name[i] == '?') || (escapewin && (name[i] == '#'))) {
			if (((namelen - i) >= 2) && isxdigit(name[i+1]) && isxdigit(name[i+2])) {
				buffer[j++] = '?';
				buffer[j++] = '3';
				buffer[j++] = 'F';
			} else {
				buffer[j++] = '?';
			}
		} else if (escapewin && (name[i] == '$')) {
			buffer[j++] = '<';
		} else if (escapewin && (name[i] == '^')) {
			buffer[j++] = '>';
		} else if (name[i] < '\''
		           || name[i] == '/'
		           || name[i] == ':'
		           || name[i] == '@'
		           || name[i] == '\\'
		           || name[i] == 127
		           || ((name[i] == 160) && (localutf8 == 0))) {
			int val;

			/* Turn illegal chars into ?XX escape sequences */
			buffer[j++] = '?';
			val = ((name[i] & 0xF0) >> 4);
			buffer[j++] = val < 10 ? val + '0' : (val - 10) + 'A';
			val = (name[i] & 0x0F);
			buffer[j++] = val < 10 ? val + '0' : (val - 10) + 'A';
		} else {
			/* All other chars translate unchanged */
			buffer[j++] = name[i];
		}
	}

	if (filetyperet) *filetyperet = filetype;

	if (i < namelen) return 0; /* Buffer overflow */

	buffer[j] = '\0';

	if (filetyperet) {
		if (xyzext != NEVER) {
			if (*filetyperet == -1) {
				/* No ,xyz found */
				if (macforks && (strncmp(name, "._", 2) == 0)) {
					*filetyperet = MACFORKS_FILETYPE;
				} else if (dotext) {
					*filetyperet = ext_to_filetype(dotext, defaultfiletype);
				} else {
					/* No ,xyz and no extension, so use default */
					*filetyperet = defaultfiletype;
				}
			}
		} else {
			*filetyperet = defaultfiletype;
		}
	}

	return j;
}

/* Convert a unix mode to RISC OS attributes */
unsigned int mode_to_attr(unsigned int mode)
{
	unsigned int attr;
	attr  = (mode & 0x100) >> 8; /* Owner read */
	attr |= (mode & 0x080) >> 6; /* Owner write */
	/* Set public read/write if either group or other bits are set */
	attr |= ((mode & 0x004) << 2) | ((mode & 0x020) >> 1); /* Public read */
	attr |= ((mode & 0x002) << 4) | ((mode & 0x010) << 1); /* Public write */
	return attr;
}

/* Convert RISC OS attributes to a unix mode */
unsigned attr_to_mode(unsigned attr, unsigned oldmode, unsigned umask, unsigned unumask)
{
	unsigned int newmode;
	newmode = oldmode & ~0666; /* Preserve existing type and execute bits */
	newmode |= unumask;
	newmode |= (attr & 0x01) << 8; /* Owner read */
	newmode |= (attr & 0x02) << 6; /* Owner write */
	newmode |= (attr & 0x10) << 1; /* Group read */
	newmode |= (attr & 0x20) >> 1; /* Group write */
	newmode |= (attr & 0x10) >> 2; /* Others read */
	newmode |= (attr & 0x20) >> 4; /* Others write */
	newmode &= ~umask;
	return newmode;
}


char *addfiletypeext(char *leafname, unsigned int len, int extfound, int newfiletype, unsigned int *newlen, int defaultfiletype, int xyzext, int unixexfiletype, int macforks, struct pool *pool)
{
	char *newleafname;

	if (xyzext == NEVER) {
		if (newlen) *newlen = len;
		return leafname;
	} else {
		/* Check if we need a new extension */
		int extneeded = 0;

		if (extfound) len -= sizeof(",xyz") - 1;

		newleafname = palloc(len + sizeof(",xyz"), pool);
		if (newleafname == NULL) return NULL;

		if (xyzext == ALWAYS) {
			/* Always add ,xyz */
			extneeded = 1;
		} else if (unixexfiletype && (newfiletype == UNIXEX_FILETYPE)) {
			extneeded = 0;
		} else if (macforks && (newfiletype == MACFORKS_FILETYPE) && (strncmp(leafname, "._", 2) == 0)) {
			extneeded = 0;
		} else {
			/* Only add an extension if needed */
			char *ext;

			ext = strrchr(leafname, '.');
			if (ext) {
				int mimefiletype;
				int extlen = len - (ext - leafname) - 1;

				memcpy(newleafname, ext + 1, extlen);
				newleafname[extlen] = '\0';
				mimefiletype = ext_to_filetype(newleafname, defaultfiletype);

				if (mimefiletype == newfiletype) {
					/* Don't need an extension */
					extneeded = 0;
				} else {
					/* A new ,xyz is needed */
					extneeded = 1;
				}
			} else {
				if (newfiletype == defaultfiletype) {
					/* Don't need an extension */
					extneeded = 0;
				} else {
					/* A new ,xyz is needed */
					extneeded = 1;
				}
			}
		}

		memcpy(newleafname, leafname, len);
		if (extneeded) {
			snprintf(newleafname + len, sizeof(",xyz"), ",%.3x", newfiletype);
			*newlen = len + sizeof(",xyz") - 1;
		} else {
			newleafname[len] = '\0';
			*newlen = len;
		}
	}

	return newleafname;
}

/* Convert a RISC OS load and execution address into a unix timestamp */
void loadexec_to_timeval(unsigned load, unsigned exec, unsigned *seconds, unsigned *nseconds, int nfs2)
{
	if ((load & 0xFFF00000) != 0xFFF00000) {
		/* A real load/exec address */
		*seconds = -1;
		*nseconds = -1;
	} else {
		uint64_t csecs;

		csecs = exec;
		csecs |= ((uint64_t)load & 0xFF) << 32;
		csecs -= 0x336e996a00LL; /* Difference between 1900 and 1970 */
		*seconds = ((unsigned)(csecs / 100) & 0xFFFFFFFF);
		*nseconds = ((unsigned)(csecs % 100) * (nfs2 ? 10000 : 10000000));
	}
}

/* Convert a unix timestamp into a RISC OS load and execution address */
void timeval_to_loadexec(unsigned seconds, unsigned nseconds, int filetype, unsigned *load, unsigned *exec, int nfs2)
{
	uint64_t csecs;

	csecs = seconds;
	csecs *= 100;
	csecs += ((uint64_t)nseconds / (nfs2 ? 10000ULL : 10000000ULL));
	csecs += 0x336e996a00ULL; /* Difference between 1900 and 1970 */
	*load = (unsigned)((csecs >> 32) & 0xFF);
	*load |= (0xFFF00000 | ((filetype & 0xFFF) << 8));
	*exec = (unsigned)(csecs & 0xFFFFFFFFULL);
}

#define Resolver_GetHost 0x46001

/* A version of gethostbyname that will timeout.
   Also handles IP addresses without needing a reverse lookup */
os_error *gethostbyname_timeout(char *host, unsigned long timeout, struct hostent **hp)
{
	unsigned long starttime;
	unsigned long endtime;
	os_error *err;
	int errnum;
	int quad1, quad2, quad3, quad4;

	if (sscanf(host, "%d.%d.%d.%d", &quad1, &quad2, &quad3, &quad4) == 4) {
		/* Host is an IP address, so doesn't need resolving */
		static struct hostent hostent;
		static unsigned int addr;
		static char *addr_list = (char *)&addr;

		addr = quad1 | (quad2 << 8) | (quad3 << 16) | (quad4 << 24);
		hostent.h_addr_list = &addr_list;
		hostent.h_length = sizeof(addr);

		*hp = &hostent;
		return NULL;
	}

	err = _swix(OS_ReadMonotonicTime, _OUT(0), &starttime);
	if (err) return err;

	do {
		err = _swix(Resolver_GetHost, _IN(0) | _OUTR(0,1), host, &errnum, hp);
		if (err) return err;

		if (errnum != EINPROGRESS) break;

		err = _swix(OS_ReadMonotonicTime, _OUT(0), &endtime);
		if (err) return err;

	} while (endtime - starttime < timeout);

	if (errnum == 0) return NULL; /* Host found */

	return gen_error(RPCERRBASE + 1, "Unable to resolve hostname '%s' (%d)", host, errnum);
}

char *encoding_getlocal(void)
{
	int alphabet;

	if (_swix(OS_Byte, _INR(0,1) | _OUT(1), 71, 127, &alphabet)) alphabet = 101;
	if (alphabet == 111) localutf8 = 1;
	switch (alphabet) {
		case 101: return "ISO-8859-1";
		case 102: return "ISO-8859-2";
		case 103: return "ISO-8859-3";
		case 104: return "ISO-8859-4";
		case 105: return "ISO-8859-5";
		case 106: return "ISO-8859-6";
		case 107: return "ISO-8859-7";
		case 108: return "ISO-8859-8";
		case 109: return "ISO-8859-9";
		case 110: return "ISO-IR-182";
		case 111: return "UTF-8";
		case 112: return "ISO-8859-15";
		case 113: return "ISO-8859-10";
		case 114: return "ISO-8859-13";
		case 115: return "ISO-8859-14";
		case 116: return "ISO-8859-16";
		case 120: return "CP866";
	}
	/* Default to Latin1 */
	return "ISO-8859-1";
}

