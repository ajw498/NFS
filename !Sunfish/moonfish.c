/*
	$Id$
	$URL$

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
#include <swis.h>

#include "moonfishdefs.h"

#include "serverconn.h"



#define SYSLOGF_BUFSIZE 1024
#define Syslog_LogMessage 0x4C880


void syslogf(char *logname, int level, char *fmt, ...)
{
	static char syslogbuf[SYSLOGF_BUFSIZE];
	va_list ap;

	if (level > 10) return;

	level = 1; /**/

	va_start(ap, fmt);
	vsnprintf(syslogbuf, sizeof(syslogbuf), fmt, ap);
	va_end(ap);

	/* Ignore any errors, as there's not much we can do with them */
	_swix(Syslog_LogMessage, _INR(0,2), logname, syslogbuf, level);
}

#define EventV 16
#define Internet_Event 19
#define Socket_Async_Event 1



_kernel_oserror *initialise(const char *cmd_tail, int podule_base, void *private_word)
{
	static _kernel_oserror initerr = {1/*FIXME*/, "Unable to initialise server, see syslog for details"};
	_kernel_oserror *err;

	(void)cmd_tail;
	(void)podule_base;

	if (conn_init()) return &initerr;

	err = _swix(OS_Claim, _INR(0,2), EventV, internet_event, private_word);
	if (err) return err;
	err = _swix(OS_Byte, _INR(0,1), 14, Internet_Event);
	if (err) {
		_swix(OS_Release, _INR(0,2), EventV, internet_event, private_word);
		return err;
	}

	err = _swix(OS_CallAfter, _INR(0,2), 10, callevery, private_word);
	if (err) {
		_swix(OS_Byte, _INR(0,1), 13, Internet_Event);
		_swix(OS_Release, _INR(0,2), EventV, internet_event, private_word);
	}
	return err;
}

_kernel_oserror *finalise(int fatal, int podule_base, void *private_word)
{
	(void)fatal;
	(void)podule_base;

	_swix(OS_RemoveTickerEvent, _INR(0,1), callevery, private_word);

	_swix(OS_Byte, _INR(0,1), 13, Internet_Event);
	_swix(OS_Release, _INR(0,2), EventV, internet_event, private_word);

	conn_close();

/*	if (enablelog) syslogf(LOGNAME, LOGENTRY, "Module finalisation");*/


	return NULL;
}

_kernel_oserror *callback_handler(_kernel_swi_regs *r, void *pw)
{
	int delay;

	(void)r;
	(void)pw;

/*	do { */
		delay = conn_poll();
/*	} while (delay == 0);*/

	_swix(OS_CallAfter, _INR(0,2), delay, callevery, pw);

	return NULL;
}

_kernel_oserror *callback2_handler(_kernel_swi_regs *r, void *pw)
{
	int delay;

	(void)r;
	(void)pw;

/*	do {*/
		delay = conn_poll();
/*	} while (delay == 0);*/

	return NULL;
}

_kernel_oserror *callevery_handler(_kernel_swi_regs *r, void *pw)
{
	(void)r;

	_swix(OS_AddCallBack, _INR(0,1), callback, pw);

	return NULL;
}

int internet_event_handler(_kernel_swi_regs *r, void *pw)
{
	if (r->r[0] == Internet_Event && r->r[1] == Socket_Async_Event) {
		if (conn_validsocket(r->r[2])) {
			_swix(OS_AddCallBack, _INR(0,1), callback2, pw);
			return 0;
		}
	}

	return 1;
}


