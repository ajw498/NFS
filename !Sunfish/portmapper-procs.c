/*
	$Id$
	$URL$

	Portmapper procedures


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

#include "portmapper-procs.h"

void PMAPPROC_NULL(struct server_conn *conn)
{
	printf("PMAPPROC_NULL\n");
}

void PMAPPROC_SET(struct mapping *args, enum bool *res, struct server_conn *conn)
{
	printf("PMAPPROC_SET\n");
	*res = FALSE;
}

void PMAPPROC_UNSET(struct mapping *args, enum bool *res, struct server_conn *conn)
{
	printf("PMAPPROC_UNSET\n");
	*res = FALSE;
}

void PMAPPROC_GETPORT(struct mapping *args, int *res, struct server_conn *conn)
{
	printf("PMAPPROC_GETPORT\n");
	/* Decode invalid progs */
	if (args->prog == 100005 && args->vers == 3) {
		*res = 111;
	} else if (args->prog == 100005 && args->vers == 1) {
		*res = 111;
	} else if (args->prog == 100003 && args->vers == 2) {
		*res = 111;
	} else if (args->prog == 100003 && args->vers == 3) {
		*res = 111;
	} else {
		*res = 0;
	}
	if (args->prot != 17 /*6*/) {
		*res = 0;
	}
}

