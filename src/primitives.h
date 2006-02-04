/*
	$Id$
	$URL$

	Input and output routines for primitive XDR data types.
*/

#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <string.h>

#define OPAQUE_MAX 0xFFFFFFFF

#define OUTPUT 0
#define INPUT 1

struct opaque {
	unsigned int size;
	char *data;
};

struct opaqueint {
	unsigned int size;
	unsigned int *data;
};


typedef struct opaque string;

enum bool {
	FALSE = 0,
	TRUE = 1
};

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


#define process_int(input, structbase, maxsize) do { \
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

#define process_uint64_t(input, structbase, maxsize) do { \
 check_bufspace(input, 8); \
 if (input) { \
  uint64_t tmp; \
  tmp  = (uint64_t)(((uint64_t)input_byte()) << 54); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 48); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 40); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 32); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 24); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 16); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 8 ); \
  tmp |= (uint64_t)(((uint64_t)input_byte()) << 0 ); \
  structbase = tmp; \
 } else { \
  output_byte((char)((structbase & 0xFF00000000000000) >> 54)); \
  output_byte((char)((structbase & 0x00FF000000000000) >> 48)); \
  output_byte((char)((structbase & 0x0000FF0000000000) >> 40)); \
  output_byte((char)((structbase & 0x000000FF00000000) >> 32)); \
  output_byte((char)((structbase & 0x00000000FF000000) >> 24)); \
  output_byte((char)((structbase & 0x0000000000FF0000) >> 16)); \
  output_byte((char)((structbase & 0x000000000000FF00) >> 8 )); \
  output_byte((char)((structbase & 0x00000000000000FF) >> 0 )); \
 } \
} while (0)

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

#define process_opaque(input, structbase, maxsize) do { \
 process_int(input, structbase.size, 0); \
 if (structbase.size > maxsize) goto buffer_overflow; \
 if (structbase.size > 0) { \
  int i; \
  check_bufspace(input, (structbase.size + 3) & ~3); \
  if (input) { \
   structbase.data = input_bytes(structbase.size); \
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
 } \
} while (0)

#define process_fixed_opaque(input,structbase,maxsize) do { \
 int i; \
 check_bufspace(input, maxsize); \
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
} while (0)

#define process_opaqueint(input, structbase, maxsize) do { \
 process_int(input, structbase.size, 0); \
 if (structbase.size > maxsize) goto buffer_overflow; \
 if (structbase.size > 0) { \
  int i; \
  check_bufspace(input, structbase.size * 4); \
  if (input) { \
  	structbase.data = palloc(structbase.size * 4, conn->pool); \
   if (structbase.data == NULL) goto buffer_overflow; \
  } \
  for (i = 0; i < structbase.size; i++) { \
   process_int(input, structbase.data[i], 0); \
  } \
 } \
} while (0)

#define process_string(input,structbase,maxsize) process_opaque(input,structbase,maxsize)
#define process_fixed_string(input,structbase,maxsize) process_fixed_opaque(input,structbase,maxsize)

#define process_void(input,structbase,maxsize) do { \
 if (0) goto buffer_overflow; \
} while (0)

#endif
