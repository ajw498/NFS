/*
	$Id$

	Input and output routines for primitive XDR data types.
*/

#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#ifndef USE_TCPIPLIBS
#include <stdint.h>
#endif
#include <string.h>
#include <sys/types.h>

#include "pools.h"


#define OPAQUE_MAX 0x7FFFFFFF

#define OUTPUT 0
#define INPUT 1

typedef char opaque;

struct opaque {
	char *data;
	unsigned size;
};

typedef enum bool {
	FALSE = 0,
	TRUE = 1
} bool;

extern char *ibuf, *ibufend;
extern char *obuf, *obufend;

/* xbuf points to next input or outut byte
   xbufend points to byte after the end of buffer */

#define check_bufspace(input, x) do { \
 if (input) { \
  if (ibuf + (x) > ibufend) return 1; \
 } else { \
  if (obuf + (x) > obufend) return 1; \
 } \
} while (0)

#define input_byte() (*(ibuf++))

#define output_byte(x) (*(obuf++) = x)

#define input_bytes(size) (ibuf+=size,ibuf-size)

#define output_bytes(data, size) (memcpy(obuf, data, size),obuf+=size)


#define process_int32_macro(input, structbase, type) do { \
 check_bufspace(input, 4); \
 if (input) { \
  type tmp; \
  tmp  = input_byte() << 24; \
  tmp |= input_byte() << 16; \
  tmp |= input_byte() << 8; \
  tmp |= input_byte(); \
  (structbase) = tmp; \
 } else { \
  output_byte(((structbase) & 0xFF000000) >> 24); \
  output_byte(((structbase) & 0x00FF0000) >> 16); \
  output_byte(((structbase) & 0x0000FF00) >> 8); \
  output_byte(((structbase) & 0x000000FF)); \
 } \
} while (0)


#define process_int64_macro(input, structbase, type) do { \
 check_bufspace(input, 8); \
 if (input) { \
  type tmp; \
  tmp  = (uint64_t)(((uint64_t)input_byte()) << 56); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 48); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 40); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 32); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 24); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 16); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 8 ); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 0 ); \
  (structbase) = tmp; \
 } else { \
  output_byte((char)(((structbase) & 0xFF00000000000000) >> 56)); \
  output_byte((char)(((structbase) & 0x00FF000000000000) >> 48)); \
  output_byte((char)(((structbase) & 0x0000FF0000000000) >> 40)); \
  output_byte((char)(((structbase) & 0x000000FF00000000) >> 32)); \
  output_byte((char)(((structbase) & 0x00000000FF000000) >> 24)); \
  output_byte((char)(((structbase) & 0x0000000000FF0000) >> 16)); \
  output_byte((char)(((structbase) & 0x000000000000FF00) >> 8 )); \
  output_byte((char)(((structbase) & 0x00000000000000FF) >> 0 )); \
 } \
} while (0)

#define process_uint32_t(input, structbase, pool) process_unsigned(input, (unsigned *)(structbase), pool)
#define process_int32_t(input, structbase, pool) process_int(input, (int *)(structbase), pool)
#define process_uint32(input, structbase, pool) process_unsigned(input, (unsigned *)(structbase), pool)
#define process_int32(input, structbase, pool) process_int(input, (int *)(structbase), pool)
#define process_uint64(input, structbase, pool) process_uint64_t(input, (uint64_t *)(structbase), pool)
#define process_int64(input, structbase, pool) process_int64_t(input, (int64_t *)(structbase), pool)

int process_int(int input, int *structbase, struct pool *pool);

int process_unsigned(int input, unsigned *structbase, struct pool *pool);

int process_int64_t(int input, int64_t *structbase, struct pool *pool);

int process_uint64_t(int input, uint64_t *structbase, struct pool *pool);

int process_bool(int input, bool *structbase, struct pool *pool);

#define process_enum_macro(input, structbase, name) do { \
 check_bufspace(input, 4); \
 if (input) { \
  int tmp; \
  tmp  = input_byte() << 24; \
  tmp |= input_byte() << 16; \
  tmp |= input_byte() << 8; \
  tmp |= input_byte(); \
  (structbase) = (enum name)tmp; \
 } else { \
  output_byte(((structbase) & 0xFF000000) >> 24); \
  output_byte(((structbase) & 0x00FF0000) >> 16); \
  output_byte(((structbase) & 0x0000FF00) >> 8); \
  output_byte(((structbase) & 0x000000FF)); \
 } \
} while (0)


#define process_fixedarray_macro(input, structbase, type, maxsize, pool) do { \
  int i; \
  check_bufspace(input, (maxsize * sizeof(type) + 3) & ~3); \
  if (sizeof(type) == 1) { \
    if (input) { \
      memcpy((structbase), input_bytes(maxsize), maxsize); \
    } else { \
      output_bytes((structbase), maxsize); \
    } \
    for (i = (maxsize & 3); i < 4 && i != 0; i++) { \
      if (input) { \
        (void)input_byte(); \
      } else { \
        output_byte(0); \
      } \
    } \
  } else { \
    for (i = 0; i < maxsize; i++) { \
      if (process_##type(input, &((structbase)[i]), pool)) return 1; \
    } \
  } \
} while (0)

#define process_array_macro(input, structbase, type, maxsize, pool) do { \
  if (process_unsigned(input, &((structbase).size), pool)) return 1; \
  if ((structbase).size > maxsize) return 1; \
  if ((structbase).size > 0) { \
    int i; \
    check_bufspace(input, ((structbase).size * sizeof(type) + 3) & ~3); \
    if (sizeof(type) == 1) { \
      if (input) { \
        (structbase).data = (type *)input_bytes((structbase).size); \
      } else { \
        output_bytes((structbase).data, (structbase).size); \
      } \
      for (i = ((structbase).size & 3); i < 4 && i != 0; i++) { \
        if (input) { \
         (void)input_byte(); \
        } else { \
         output_byte(0); \
        } \
      } \
    } else { \
      if (input) { \
        (structbase).data = palloc((structbase).size * sizeof(type), pool); \
        if ((structbase).data == NULL) return 1; \
      } \
      for (i = 0; i < (structbase).size; i++) { \
        if (process_##type(input, &((structbase).data[i]), pool)) return 1; \
      } \
    } \
  } \
} while (0)

/* Only used in the non-taken branch of process_[fixed]array */
#define process_char(input, structbase, pool) 0
#define process_opaque(input, structbase, pool) 0

#define process_void(input, structbase, pool) 0

#define PR(ret) if (ret) return 1

#endif
