/*
	$Id: $

	Input and output routines for primitive data types.
*/

#ifndef PRIMITIVES_H
#define PRIMITIVES_H


struct opaque {
	unsigned int size;
	char *data;
};

struct opaqueint {
	unsigned int size;
	unsigned int *data;
};

#define OPAQUE_MAX 0xFFFFFFFF

typedef struct opaque string;

enum bool {
	FALSE = 0,
	TRUE = 1
};

extern char *buf, *bufend;

#include <assert.h>
#include <string.h>
#include <stdio.h>

#define OUTPUT 0
#define INPUT 1

/* buf points to next input or outut byte
   bufend points to byte after the end of buffer */

#define check_bufspace(x) do { \
 if (buf + (x) > bufend) goto buffer_overflow; \
} while (0)

#define input_byte() (*(buf++))

#define output_byte(x) (*(buf++) = x)

#define input_bytes(size) (buf+=size,buf-size)

#define output_bytes(data, size) (memcpy(buf, data, size),buf+=size)

/* FIXME: check for remaining assertions */

#define process_int(input, structbase, maxsize) do { \
 check_bufspace(4); \
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
/*printf("processing int " ###structbase ", %d\n",##structbase);*/\
} while (0)

#define process_enum(input, structbase, name) do { \
 check_bufspace(4); \
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
/*printf("processing enum " ###structbase ", %d\n",##structbase);*/\
} while (0)

#define process_opaque(input, structbase, maxsize) do { \
/*printf("processing opaque " ###structbase ", size = %d, maxsize = %d\n",##structbase.size,maxsize);*/\
 process_int(input, ##structbase.size, 0); \
 assert(##structbase.size <= maxsize); \
 if (##structbase.size > 0) { \
  int i; \
  check_bufspace((##structbase.size + 3) & ~3); \
  if (input) { \
   ##structbase.data = input_bytes(##structbase.size); \
  } else { \
   output_bytes(##structbase.data, ##structbase.size); \
  } \
  for (i = (##structbase.size & 3); i < 4 && i != 0; i++) { \
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
 check_bufspace(maxsize); \
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
 process_int(input, ##structbase.size, 0); \
 assert(##structbase.size <= maxsize); \
 if (##structbase.size > 0) { \
  int i; \
  check_bufspace(##structbase.size * 4); \
  for (i = 0; i < ##structbase.size; i++) { \
   if (input) { \
    /**/ \
   } else { \
    process_int(input, ##structbase.data[i], 0); \
   } \
  } \
 } \
} while (0)

#define process_string(input,structbase,maxsize) process_opaque(input,structbase,maxsize)
#define process_fixed_string(input,structbase,maxsize) process_fixed_opaque(input,structbase,maxsize)

#define process_void(input,structbase,maxsize) do { \
 if (0) goto buffer_overflow; \
} while (0)

#endif
