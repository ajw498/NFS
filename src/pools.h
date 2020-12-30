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

