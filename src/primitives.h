/*
	$Id$

	Input and output routines for primitive XDR data types.
*/

#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <string.h>

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
 if (ibuf + (x) > ibufend) goto buffer_overflow; \
 } else { \
 if (obuf + (x) > obufend) goto buffer_overflow; \
 } \
} while (0)

#define input_byte() (*(ibuf++))

#define output_byte(x) (*(obuf++) = x)

#define input_bytes(size) (ibuf+=size,ibuf-size)

#define output_bytes(data, size) (memcpy(obuf, data, size),obuf+=size)


#define process_int(input, structbase) do { \
 check_bufspace(input, 4); \
 if (input) { \
  int tmp; \
  tmp  = input_byte() << 24; \
  tmp |= input_byte() << 16; \
  tmp |= input_byte() << 8; \
  tmp |= input_byte(); \
  structbase = tmp; \
 } else { \
  output_byte((structbase & 0xFF000000) >> 24); \
  output_byte((structbase & 0x00FF0000) >> 16); \
  output_byte((structbase & 0x0000FF00) >> 8); \
  output_byte((structbase & 0x000000FF)); \
 } \
} while (0)

#define process_unsigned(input, structbase) do { \
 check_bufspace(input, 4); \
 if (input) { \
  unsigned tmp; \
  tmp  = input_byte() << 24; \
  tmp |= input_byte() << 16; \
  tmp |= input_byte() << 8; \
  tmp |= input_byte(); \
  structbase = tmp; \
 } else { \
  output_byte((structbase & 0xFF000000U) >> 24); \
  output_byte((structbase & 0x00FF0000U) >> 16); \
  output_byte((structbase & 0x0000FF00U) >> 8); \
  output_byte((structbase & 0x000000FFU)); \
 } \
} while (0)

#define process_uint32_t(input, structbase) process_unsigned(input, structbase)
#define process_int32_t(input, structbase) process_int(input, structbase)
#define process_uint32(input, structbase) process_unsigned(input, structbase)
#define process_int32(input, structbase) process_int(input, structbase)


#define process_int64_t(input, structbase) do { \
 check_bufspace(input, 8); \
 if (input) { \
  uint64_t tmp; \
  tmp  = (uint64_t)(((uint64_t)input_byte()) << 56); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 48); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 40); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 32); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 24); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 16); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 8 ); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 0 ); \
  structbase = tmp; \
 } else { \
  output_byte((char)((structbase & 0xFF00000000000000) >> 56)); \
  output_byte((char)((structbase & 0x00FF000000000000) >> 48)); \
  output_byte((char)((structbase & 0x0000FF0000000000) >> 40)); \
  output_byte((char)((structbase & 0x000000FF00000000) >> 32)); \
  output_byte((char)((structbase & 0x00000000FF000000) >> 24)); \
  output_byte((char)((structbase & 0x0000000000FF0000) >> 16)); \
  output_byte((char)((structbase & 0x000000000000FF00) >> 8 )); \
  output_byte((char)((structbase & 0x00000000000000FF) >> 0 )); \
 } \
} while (0)

#define process_uint64_t(input, structbase) do { \
 check_bufspace(input, 8); \
 if (input) { \
  int64_t tmp; \
  tmp  = (int64_t)(((int64_t)input_byte()) << 56); \
  tmp |= (int64_t)(((int64_t)input_byte()) << 48); \
  tmp |= (int64_t)(((int64_t)input_byte()) << 40); \
  tmp |= (int64_t)(((int64_t)input_byte()) << 32); \
  tmp |= (int64_t)(((int64_t)input_byte()) << 24); \
  tmp |= (int64_t)(((int64_t)input_byte()) << 16); \
  tmp |= (int64_t)(((int64_t)input_byte()) << 8 ); \
  tmp |= (int64_t)(((int64_t)input_byte()) << 0 ); \
  structbase = tmp; \
 } else { \
  output_byte((char)((structbase & 0xFF00000000000000) >> 56)); \
  output_byte((char)((structbase & 0x00FF000000000000) >> 48)); \
  output_byte((char)((structbase & 0x0000FF0000000000) >> 40)); \
  output_byte((char)((structbase & 0x000000FF00000000) >> 32)); \
  output_byte((char)((structbase & 0x00000000FF000000) >> 24)); \
  output_byte((char)((structbase & 0x0000000000FF0000) >> 16)); \
  output_byte((char)((structbase & 0x000000000000FF00) >> 8 )); \
  output_byte((char)((structbase & 0x00000000000000FF) >> 0 )); \
 } \
} while (0)

#define process_uint64(input, structbase) process_uint64_t(input, structbase)
#define process_int64(input, structbase) process_int64_t(input, structbase)

#define process_enum(input, structbase, name) do { \
 check_bufspace(input, 4); \
 if (input) { \
  int tmp; \
  tmp  = input_byte() << 24; \
  tmp |= input_byte() << 16; \
  tmp |= input_byte() << 8; \
  tmp |= input_byte(); \
  structbase = (enum name)tmp; \
 } else { \
  output_byte((structbase & 0xFF000000) >> 24); \
  output_byte((structbase & 0x00FF0000) >> 16); \
  output_byte((structbase & 0x0000FF00) >> 8); \
  output_byte((structbase & 0x000000FF)); \
 } \
} while (0)

#define process_bool(input, structbase) process_enum(input, structbase, bool)

#define process_fixedarray(input, structbase, type, maxsize) do { \
  int i; \
  check_bufspace(input, (maxsize * sizeof(type) + 3) & ~3); \
  if (sizeof(type) == 1) { \
    if (input) { \
      memcpy(structbase, input_bytes(maxsize), maxsize); \
    } else { \
      output_bytes(structbase, maxsize); \
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
      process_##type(input, structbase[i]); \
    } \
  } \
} while (0)

#define process_array(input, structbase, type, maxsize) do { \
  process_int(input, structbase.size); \
  if (structbase.size > maxsize) goto buffer_overflow; \
  if (structbase.size > 0) { \
    int i; \
    check_bufspace(input, (structbase.size * sizeof(type) + 3) & ~3); \
    if (sizeof(type) == 1) { \
      if (input) { \
        structbase.data = (type *)input_bytes(structbase.size); \
      } else { \
        output_bytes(structbase.data, structbase.size); \
      } \
      for (i = (structbase.size & 3); i < 4 && i != 0; i++) { \
        if (input) { \
         (void)input_byte(); \
        } else { \
         output_byte(0); \
        } \
      } \
    } else { \
      if (input) { \
        structbase.data = palloc(structbase.size * sizeof(type), conn->pool); \
        if (structbase.data == NULL) goto buffer_overflow; \
      } \
      for (i = 0; i < structbase.size; i++) { \
        process_##type(input, structbase.data[i]); \
      } \
    } \
  } \
} while (0)

/* Duplicate to avoid macro recursion */
#define process_array2(input, structbase, type, maxsize) do { \
  process_int(input, structbase.size); \
  if (structbase.size > maxsize) goto buffer_overflow; \
  if (structbase.size > 0) { \
    int ii; \
    check_bufspace(input, (structbase.size * sizeof(type) + 3) & ~3); \
    if (sizeof(type) == 1) { \
      if (input) { \
        structbase.data = (type *)input_bytes(structbase.size); \
      } else { \
        output_bytes(structbase.data, structbase.size); \
      } \
      for (ii = (structbase.size & 3); ii < 4 && ii != 0; ii++) { \
        if (input) { \
         (void)input_byte(); \
        } else { \
         output_byte(0); \
        } \
      } \
    } else { \
      if (input) { \
        structbase.data = palloc(structbase.size * sizeof(type), conn->pool); \
        if (structbase.data == NULL) goto buffer_overflow; \
      } \
      for (ii = 0; ii < structbase.size; ii++) { \
        process_##type(input, structbase.data[ii]); \
      } \
    } \
  } \
} while (0)

#define process_char(input,structbase)
#define process_opaque(input,structbase)
/* Only used in the non-taken branch of process_[fixed]array */

#define process_void(input,structbase) do { \
 if (0) goto buffer_overflow; \
} while (0)

#endif
