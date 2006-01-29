/*
	$Id$
	$URL$

	Server connection handling code.


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



#include <swis.h>

/* The worstcase size of a header for read/write calls.
   If this value is not big enough, the data part will not be of
   an optimum size, but nothing bad will happen */
#define MAX_HDRLEN 416

#define MAX_DATABUFFER 32000 /*FIXME*/

/* The size to use for tx and rx buffers */
#define BUFFERSIZE (MAX_HDRLEN + MAX_DATABUFFER)

typedef _kernel_oserror os_error;



int conn_init(void);

void conn_poll(void);

void conn_close(void);
