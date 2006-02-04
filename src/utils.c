/*
	$Id$
	$URL$

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


void (*error_func)(void) = NULL;

/* Generate a RISC OS error block based on the given number and message */
os_error *gen_error(int num, char *msg, ...)
{
	static os_error module_err_buf;
	va_list ap;

	if (error_func) error_func();

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

char *filename_unixify(char *name, unsigned int len, unsigned int *newlen, struct pool *pool)
{
	char *namebuffer;
	int i;
	int j;

	namebuffer = palloc(len + 1, pool);
	if (namebuffer == NULL) return NULL;

	for (i = 0, j = 0; i < len; i++) {
		switch (name[i]) {
			case '/':
				namebuffer[j++] = '.';
				break;
			case 160: /*hard space*/
				namebuffer[j++] = ' ';
				break;
			case '?':
				if (i + 2 < len && isxdigit(name[i+1]) && !islower(name[i+1]) && isxdigit(name[i+2]) && !islower(name[i+2])) {
					/* An escape sequence */
					i++;
					namebuffer[j] = (name[i] > '9' ? name[i] - 'A' + 10 : name[i] - '0') << 4;
					i++;
					namebuffer[j++] |= name[i] > '9' ? name[i] - 'A' + 10 : name[i] - '0';
				} else {
					namebuffer[j++] = name[i];
				}
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
int lookup_mimetype(char *ext, int defaultfiletype)
{
	os_error *err;
	int filetype;

	/* Try MimeMap to get a filetype, use the default type if the call fails */
	err = _swix(MimeMap_Translate,_INR(0,2) | _OUT(3), MMM_TYPE_DOT_EXTN, ext, MMM_TYPE_RISCOS, &filetype);
	if (err) filetype = defaultfiletype;

	return filetype;
}


int filename_riscosify(char *name, int namelen, char *buffer, int buflen, int *filetype, int defaultfiletype, int xyzext)
{
	int i;
	int j;
	char *dotext = NULL;

	*filetype = -1;

	for (i = 0, j = 0; (i < namelen) && (j + 3 < buflen); i++) {
		if (name[i] == '.') {
			buffer[j++] = '/';
			dotext = buffer + j;
		} else if (name[i] == ' ') {
			buffer[j++] = 160; /* spaces to hard spaces */
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
				*filetype = (int)strtol(tmp, NULL, 16);
				namelen -= sizeof(",xyz") - 1;
			} else {
				buffer[j++] = name[i];
			}
		} else if (name[i] == '!') {
			buffer[j++] = name[i];
		} else if (name[i] < '\''
				|| name[i] == '/'
				|| name[i] == ':'
				|| name[i] == '?'
				|| name[i] == '@'
				|| name[i] == '\\'
				|| name[i] == 127
				|| name[i] == 160) {
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

	if (i < namelen) return 0; /* Buffer overflow */

	buffer[j++] = '\0';

	if (xyzext != NEVER) {
		if (*filetype == -1) {
			/* No ,xyz found */
			if (dotext) {
				*filetype = lookup_mimetype(dotext, defaultfiletype);
			} else {
				/* No ,xyz and no extension, so use default */
				*filetype = defaultfiletype;
			}
		}
	} else {
		*filetype = defaultfiletype;
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
unsigned int attr_to_mode(unsigned int attr, unsigned int oldmode, struct conn_info *conn)
{
	unsigned int newmode;
	newmode = oldmode & ~0666; /* Preserve existing type and execute bits */
	newmode |= conn->unumask;
	newmode |= (attr & 0x01) << 8; /* Owner read */
	newmode |= (attr & 0x02) << 6; /* Owner write */
	newmode |= (attr & 0x10) << 1; /* Group read */
	newmode |= (attr & 0x20) >> 1; /* Group write */
	newmode |= (attr & 0x10) >> 2; /* Others read */
	newmode |= (attr & 0x20) >> 4; /* Others write */
	newmode &= ~(conn->umask);
	return newmode;
}


char *addfiletypeext(char *leafname, unsigned int len, int extfound, int newfiletype, unsigned int *newlen, int defaultfiletype, int xyzext, struct pool *pool)
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
		} else {
			/* Only add an extension if needed */
			char *ext;

			ext = strrchr(leafname, '.');
			if (ext) {
				int mimefiletype;
				int extlen = len - (ext - leafname) - 1;

				memcpy(newleafname, ext + 1, extlen);
				newleafname[extlen] = '\0';
				mimefiletype = lookup_mimetype(newleafname, defaultfiletype);

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

