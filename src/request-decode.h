/*
	$Id$
	$URL$

	Functions to implement decoding of recieved RPC calls.


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

#ifndef REQUEST_DECODE_H
#define REQUEST_DECODE_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#include "exports.h"


void *llmalloc(int size);

#define LOGNAME "Moonfish"

#define LOG_SERIOUS 1
#define LOG_MEM 2
#define LOG_ERROR 150
#define LOG_ACCESS 200
#define LOG_DATA 300

#define OUTOFMEM "Unable to allocate enough memory"

extern int logging;

void request_decode(struct server_conn *conn);

#endif

