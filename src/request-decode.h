/*
	Functions to implement decoding of received RPC calls.


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

#ifndef REQUEST_DECODE_H
#define REQUEST_DECODE_H


#include "serverconn.h"
#include "pools.h"
#include "utils.h"

void request_decode(struct server_conn *conn);

typedef enum accept_stat (*decode_proc)(int proc, struct server_conn *conn);

enum bool portmapper_set(int prog, int vers, int prot, int port, decode_proc decode, struct pool *pool);

enum accept_stat portmapper_decodebody(int prog, int vers, int proc, int *hi, int *lo, struct server_conn *conn);

enum accept_stat nfs4_decode_proc(int proc, struct server_conn *conn);

#endif

