/*
	$Id$
	$URL$

	Prototype server code that can run in a taskwindow rather than as a module.


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



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <socklib.h>
#include <time.h>
#include <sys/time.h>
#include <swis.h>
#include <sys/errno.h>
#include <unixlib.h>
#include <stdarg.h>
#include <ctype.h>


#include "rpc-structs.h"
#include "rpc-process1.h"
#include "rpc-process2.h"

#include "portmapper-recv.h"
#include "mount1-recv.h"
#include "mount3-recv.h"
#include "nfs2-recv.h"

#include "rpc-decode.h"

#include "serverconn.h"





/* Buffer to allocate linked list structures from. Should be significanly
   faster than using the RMA, doesn't require freeing each element of
   the list, and won't fragment */
static char malloc_buffer[MAX_DATABUFFER];
/* The start of the next location to return for linked list malloc */
static char *nextmalloc;


/* Log an entire data packet */
static void logdata(int rx, char *buf, int len)
{
	int i;

	printf("%s data (%d): xid %.2x%.2x%.2x%.2x\n", rx ? "rx" : "tx", len, buf[0], buf[1], buf[2], buf[3]);
	for (i=0; i<(len & ~3); i+=4) printf("  %.2x %.2x %.2x %.2x\n", buf[i], buf[i+1], buf[i+2], buf[i+3]);
	for (i=0; i<(len & 3); i++) printf("  %.2x\n", buf[(len &~3) + i]);
}



/* Allocate a chunk from the linklist buffer */
void *llmalloc(int size)
{
	void *mem;

	abort();

	if (nextmalloc + size > malloc_buffer + sizeof(malloc_buffer)) {
		mem = NULL;
	} else {
		mem = nextmalloc;
		nextmalloc += size;
	}

	return mem;
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
	printf("syslogged: %s\n", syslogbuf);
}

int main(void)
{
	if (conn_init()) return 1;

	while (1) {
		int ch;

		_swix(OS_Byte, _INR(0,2) | _OUT(2), 129, 10, 0, &ch);
		if (ch != 255) break;
		conn_poll();
	}

	conn_close();

	return 0;
}

