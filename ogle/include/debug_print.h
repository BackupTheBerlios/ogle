#ifndef DEBUG_PRINT_INCLUDED
#define DEBUG_PRINT_INCLUDED

#include <stdio.h>

#ifdef DEBUG

unsigned int debug;
int debug_indent_level;

#define DINDENT(spaces) \
{ \
   debug_indent_level += spaces; \
   if(debug_indent_level < 0) { \
     debug_indent_level = 0; \
   } \
} 

#endif


#if defined(__GNUC__) && ( __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 95))

#define FATAL(str, args...) \
fprintf(stderr, "FATAL[%s]: " str, program_name, ## args)

#define ERROR(str, args...) \
fprintf(stderr, "ERROR[%s]: " str, program_name, ## args)

#define WARNING(str, ...) \
fprintf(stderr, "WARNING[%s]: " str, program_name, ## args)

#define NOTE(str, args...) \
fprintf(stderr, "Note[%s]: " str, program_name, ## args)

#define DNOTE(str, args...) \
fprintf(stderr, "Debug[%s]: " str, program_name, ## args)

#ifdef DEBUG

#define DPRINTFI(level, text...) \
if(debug >= level) \
{ \
  fprintf(stderr, "%*s", debug_indent_level, ""); \
  fprintf(stderr, ## text); \
}

#define DPRINTF(level, text...) \
if(debug >= level) \
{ \
  fprintf(stderr, ## text); \
}

#else /* DEBUG */

#define DINDENT(spaces)
#define DPRINTFI(level, text...)
#define DPRINTF(level, text...)

#endif /* DEBUG */

#else /* __GNUC__ < 2 */

/* TODO: these defines are not portable */

#define FATAL(format, ...) \
fprintf(stderr, "FATAL[%s]: " format, program_name , ##__VA_ARGS__)

#define ERROR(format, ...) \
fprintf(stderr, "ERROR[%s]: " format, program_name , ##__VA_ARGS__)

#define WARNING(format, ...) \
fprintf(stderr, "WARNING[%s]: " format, program_name , ##__VA_ARGS__)

#define NOTE(format, ...) \
fprintf(stderr, "Note[%s]: " format, program_name , ##__VA_ARGS__)

#define DNOTE(format, ...) \
fprintf(stderr, "Debug[%s]: " format, program_name , ##__VA_ARGS__)



#ifdef DEBUG

#define DPRINTFI(level, ...) \
if(debug >= level) \
{ \
  fprintf(stderr, "%*s", debug_indent_level, ""); \
  fprintf(stderr, __VA_ARGS__); \
}

#define DPRINTF(level, ...) \
if(debug >= level) \
{ \
  fprintf(stderr, __VA_ARGS__); \
}

#else /* DEBUG */

#define DINDENT(spaces)
#define DPRINTFI(level, ...)
#define DPRINTF(level, ...)

#endif /* DEBUG */

#endif /* __GNUC__ < 2 */

#ifdef DEBUG

#define DPRINTBITS(level, bits, value) \
{ \
  int n; \
  for(n = 0; n < bits; n++) { \
    DPRINTF(level, "%u", (value>>(bits-n-1)) & 0x1); \
  } \
}

#else /* DEBUG */

#define DPRINTBITS(level, bits, value)

#endif /* DEBUG */


#endif /* DEBUG_PRINT_INCLUDED */
