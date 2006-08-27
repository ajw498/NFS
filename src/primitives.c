/*
	$Id$

	Input and output routines for primitive XDR data types.
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
