/*
	$Id$
	$URL$

	Mount v3 procedures


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

#include "mount3-procs.h"

enum accept_stat MOUNTPROC3_NULL(struct server_conn *conn)
{
	printf("MOUNTPROC3_NULL\n");
	return SUCCESS;
}

enum accept_stat MOUNTPROC3_MNT(string *args, struct mountres *res, struct server_conn *conn)
{
	printf("MOUNTPROC3_MNT\n");
	return SUCCESS;
}

enum accept_stat MOUNTPROC3_UMNT(string *args, struct server_conn *conn)
{
	printf("MOUNTPROC3_UMNT\n");
	return SUCCESS;
}

enum accept_stat MOUNTPROC3_UMNTALL(struct server_conn *conn)
{
	printf("MOUNTPROC3_UMNTALL\n");
	return SUCCESS;
}

