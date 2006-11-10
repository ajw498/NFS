/*
	$Id$

	Memory pool allocation routines


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

#ifndef POOLS_H
#define POOLS_H

#include <stdlib.h>

struct pool {
	struct pool *next;
	struct pool *children;
	struct pool *nextchild;
	char *start;
	size_t free;
	/* Actual memory follows */
};

/* Initialise a new pool, attaching it to parent if non-null. */
struct pool *pinit(struct pool *parent);

/* Allocate some memory from a pool. */
void *palloc(size_t size, struct pool *pool);

/* strdup from a pool. */
char *pstrdup(const char *src, struct pool *pool);

/* copy some memory to a pool. */
void *pmemdup(const void *src, size_t size, struct pool *pool);

/* Free the pool and all its contents */
void pfree(struct pool *pool);

/* Free the pool contents (but not the pool itself). */
void pclear(struct pool *pool);

#endif

