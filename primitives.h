/*
	$Id: $

	Input and output routines for primitive data types.
*/

struct opaque {
	unsigned int size;
	char *data;
};

/* buf points to next input or outut byte
   bufend points to byte after the end of buffer */

#define check_bufspace(x) do { \
 if (buf + (x) > bufend) assert(0); /*FIXME*/ \
} while (0);

#define input_byte() (*(buf++))

#define output_byte(x) (*(buf++) = x)

#define input_bytes(size) (buf+=size,buf-size)

#define output_bytes(data, size) (memcpy(buf, data, size),buf+=size)


#define process_int(input,structbase) do { \
check_bufspace(4); \
if (input) { \
 structbase  = input_byte() << 24; \
 structbase |= input_byte() << 16; \
 structbase |= input_byte() << 8; \
 structbase |= input_byte(); \
} else { \
 output_byte((structbase & 0xFF000000) >> 24); \
 output_byte((structbase & 0x00FF0000) >> 16); \
 output_byte((structbase & 0x0000FF00) >> 8); \
 output_byte((structbase & 0x000000FF)); \
} while (0);

#define process_enum(input,structbase) process_int(input,structbase)

#define process_opaque(input,structbase,maxsize) do { \
 process_int(input,structbase.size); \
 assert(structbase.size <= maxsize); \
 check_bufspace(structbase.size); \
 if (input) { \
  structbase.data = input_bytes(structbase.size); \
 } else { \
  output_bytes(structbase.data, structbase.size); \
 } \
} while (0);

#define process_fixed_opaque(input,structbase,maxsize) do { \
 check_bufspace(maxsize); \
 if (input) { \
  structbase = input_bytes(maxsize); \
 } else { \
  output_bytes(structbase, maxsize); \
 } \
} while (0);

