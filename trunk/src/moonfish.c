/*
	$Id$

	Main module entry points.


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

#include <kernel.h>
#include <stdio.h>
#include <stdarg.h>
#include <swis.h>

#include "moonfishdefs.h"
#include "moonfish.h"
#include "serverconn.h"
#include "utils.h"


#define EventV 16
#define Internet_Event 19
#define Socket_Async_Event 1
#define Socket_Broken_Event 3



_kernel_oserror *initialise(const char *cmd_tail, int podule_base, void *private_word)
{
	static _kernel_oserror initerr = {STARTUPERR, "Unable to initialise server, see syslog for details"};
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

	err = _swix(OS_CallAfter, _INR(0,2), 100, callevery, private_word);
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

	_swix(OS_Byte, _INR(0,1), 13, Internet_Event);
	_swix(OS_Release, _INR(0,2), EventV, internet_event, private_word);

	_swix(OS_RemoveTickerEvent, _INR(0,1), callevery, private_word);

	conn_close();

	return NULL;
}

/* Prevent reentrancy */
static volatile int ref = 0;

_kernel_oserror *callback_handler(_kernel_swi_regs *r, void *pw)
{
	int delay;
	int activity;

	(void)r;

	if (ref) return NULL;
	ref++;

	activity = conn_poll();

	if (activity) {
		/* If there was activity or there is still data waiting to
		   be written then we want to try again soon */
		delay = 1;
	} else {
		/* Otherwise wait a while as not much is going on */
		delay = 100;
	}

	ER(_swix(OS_RemoveTickerEvent, _INR(0,1), callevery, pw));
	ER(_swix(OS_CallAfter, _INR(0,2), delay, callevery, pw));

	ref--;

	return NULL;
}

_kernel_oserror *callevery_handler(_kernel_swi_regs *r, void *pw)
{
	(void)r;

	if (ref == 0) _swix(OS_AddCallBack, _INR(0,1), callback, pw);

	return NULL;
}

int internet_event_handler(_kernel_swi_regs *r, void *pw)
{
	if ((r->r[0] == Internet_Event) && (r->r[1] == Socket_Async_Event)) {
		if (conn_validsocket(r->r[2])) {
			/* This originally set a callback to do the work, as
			   that seemed the right thing to do from reading the
			   PRMs, and this gave better desktop responsiveness
			   and (bizarrely) was marginally faster.
			   However for some reason it seemed to trigger a
			   memory leak in the system heap which would
			   eventually bring the machine to its knees.
			   So it is now called directly. Allegedly the internet
			   stack does all its processing from a callback anyway
			   so it is safe to call non-reentrant SWIs etc.
			   Sigh. It would be nice if all this was actually
			   documented though rather than having to guess... */
			callback_handler(r, pw);
			return 0;
		}
	} else if ((r->r[0] == Internet_Event) && (r->r[1] == Socket_Broken_Event)) {
		if (conn_brokensocket(r->r[2])) {
			return 0;
		}
	}

	return 1;
}


