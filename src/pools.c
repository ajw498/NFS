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

#include "pools.h"

#include <string.h>


#define POOL_INCR (8*1024)

/* Initialise a new pool. */
struct pool *pinit(struct pool *parent)
{
	struct pool *pool;

	pool = malloc(POOL_INCR);
	if (pool == NULL) return NULL;

	pool->next = NULL;
	pool->start = (char *)(pool + 1);
	pool->free = POOL_INCR - sizeof(struct pool);
	pool->children = NULL;

	if (parent) {
		pool->nextchild = parent->children;
		parent->children = pool;
	} else {
		pool->nextchild = NULL;
	}

	return pool;
}

/* Allocate some memory from a pool. */
void *palloc(size_t size, struct pool *pool)
{
	void *mem;
	struct pool *newpool = pool;
	size_t incr = POOL_INCR;

	size = (size + 3) & ~3;

	/* Search through the existing memory for space.
	   Most of the time there should be space in the first place we look. */
	while (newpool) {
		if (size <= newpool->free) {
			mem = newpool->start;
			newpool->start += size;
			newpool->free -= size;
			return mem;
		} else {
			newpool = newpool->next;
		}
	}

	/* Not enough free space found, so allocate some more. */
	if (size + sizeof(struct pool) > incr) incr = size + sizeof(struct pool);
	newpool = malloc(incr);
	if (newpool == NULL) return NULL;

	newpool->start = (char *)(newpool + 1);
	mem = newpool->start;
	newpool->start += size;
	newpool->free = incr - sizeof(struct pool) - size;
	newpool->next = pool->next;
	newpool->children = NULL;
	newpool->nextchild = NULL;
	pool->next = newpool;

	return mem;
}

/* strdup from a pool. */
char *pstrdup(char *src, struct pool *pool)
{
	char *dest;
	size_t len;

	len = strlen(src);
	dest = palloc(len + 1, pool);
	if (dest == NULL) return NULL;
	memcpy(dest, src, len + 1);
	return dest;
}

/* Free the pool and all its contents */
void pfree(struct pool *pool)
{
	struct pool *thispool;
	struct pool *nextpool;

	while (pool) {
		thispool = pool->children;

		while (thispool) {
			nextpool = thispool->nextchild;
			pfree(thispool);
			thispool = nextpool;
		}

		nextpool = pool->next;
		free(pool);
		pool = nextpool;
	}
}

/* Free the pool contents (but not the pool itself). */
void pclear(struct pool *pool)
{
	pfree(pool->next);
	pfree(pool->children);
	pool->next = NULL;
	pool->children = NULL;

	pool->start = (char *)(pool + 1);
	pool->free = POOL_INCR - sizeof(struct pool);
}

