/*
	Input and output routines for primitive XDR data types.

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

#include "primitives.h"

int process_int(int input, int *structbase, struct pool *pool)
{
	(void)pool;
	process_int32_macro(input, (*structbase), int);
	return 0;
}

int process_unsigned(int input, unsigned *structbase, struct pool *pool)
{
	(void)pool;
	process_int32_macro(input, (*structbase), unsigned);
	return 0;
}

int process_int64_t(int input, int64_t *structbase, struct pool *pool)
{
	(void)pool;
	process_int64_macro(input, (*structbase), int64_t);
	return 0;
}

int process_uint64_t(int input, uint64_t *structbase, struct pool *pool)
{
	(void)pool;
	process_int64_macro(input, (*structbase), uint64_t);
	return 0;
}

int process_bool(int input, bool *structbase, struct pool *pool)
{
	(void)pool;
	process_enum_macro(input, (*structbase), bool);
	return 0;
}
