/*
	$Id$

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
#include <time.h>
#include <sys/time.h>
#include <swis.h>
#include <sys/errno.h>
#ifdef USE_TCPIPLIBS
# include <socketlib.h>
# include <unixlib.h>
# include <riscos.h>
#endif
#include <stdarg.h>
#include <ctype.h>


#include "serverconn.h"







/* Log an entire data packet */
/*static void logdata(int rx, char *buf, int len)
{
	int i;

	printf("%s data (%d): xid %.2x%.2x%.2x%.2x\n", rx ? "rx" : "tx", len, buf[0], buf[1], buf[2], buf[3]);
	for (i=0; i<(len & ~3); i+=4) printf("  %.2x %.2x %.2x %.2x\n", buf[i], buf[i+1], buf[i+2], buf[i+3]);
	for (i=0; i<(len & 3); i++) printf("  %.2x\n", buf[(len &~3) + i]);
}*/


int main(void)
{
	if (conn_init()) {
		printf("Failed to initialise\n");
		return 1;
	}

	while (1) {
		int ch;
		int time = 1;

/*		printf("Poll\n");*/
		_swix(OS_Byte, _INR(0,2) | _OUT(2), 129, time, 0, &ch);
		if (ch != 255) break;
		conn_poll();
	}

	conn_close();

	return 0;
}

