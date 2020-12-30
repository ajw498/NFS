/*
	Prototype server code that can run in a taskwindow rather than as a module.


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

