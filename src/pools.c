/*
	Memory pool allocation routines


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
char *pstrdup(const char *src, struct pool *pool)
{
	char *dest;
	size_t len;

	len = strlen(src);
	dest = palloc(len + 1, pool);
	if (dest == NULL) return NULL;
	memcpy(dest, src, len + 1);
	return dest;
}

/* copy some memory to a pool. */
void *pmemdup(const void *src, size_t size, struct pool *pool)
{
	void *dest;

	dest = palloc(size, pool);
	if (dest == NULL) return NULL;
	memcpy(dest, src, size);
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

