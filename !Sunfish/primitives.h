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

extern char *buf, *bufend;

/* buf points to next input or outut byte
   bufend points to byte after the end of buffer */

#define check_bufspace(x) do { \
 if (buf + (x) > bufend) goto buffer_overflow; \
} while (0)

#define input_byte() (*(buf++))

#define output_byte(x) (*(buf++) = x)

#define input_bytes(size) (buf+=size,buf-size)

#define output_bytes(data, size) (memcpy(buf, data, size),buf+=size)


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
} while (0)

#define process_opaque(input, structbase, maxsize) do { \
 process_int(input, ##structbase.size, 0); \
 if (##structbase.size > maxsize) goto buffer_overflow; \
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
 if (##structbase.size > maxsize) goto buffer_overflow; \
 if (##structbase.size > 0) { \
  int i; \
  check_bufspace(##structbase.size * 4); \
  if (input) { \
  	##structbase.data = llmalloc(##structbase.size * 4); \
   if (##structbase.data == NULL) goto buffer_overflow; \
  } \
  for (i = 0; i < ##structbase.size; i++) { \
   process_int(input, ##structbase.data[i], 0); \
  } \
 } \
} while (0)

#define process_string(input,structbase,maxsize) process_opaque(input,structbase,maxsize)
#define process_fixed_string(input,structbase,maxsize) process_fixed_opaque(input,structbase,maxsize)

#define process_void(input,structbase,maxsize) do { \
 if (0) goto buffer_overflow; \
} while (0)

#endif
