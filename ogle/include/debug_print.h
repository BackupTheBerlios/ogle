#ifndef DEBUG_PRINT_INCLUDED
#define DEBUG_PRINT_INCLUDED

#include <stdio.h>

/* Ugly hack to get the right identifier in the messages */
extern char *program_name;
extern int dlevel;
#define DEFAULT_DLEVEL 3

#define GET_DLEVEL() \
  {\
    char *dlevel_str = getenv("OGLE_DLEVEL"); \
    if(dlevel_str != NULL) \
      dlevel = atoi(dlevel_str); \
    else \
      dlevel = DEFAULT_DLEVEL; \
  }

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
if(dlevel > 0) fprintf(stderr, "FATAL[%s]: " str, program_name , ## args)

#define ERROR(str, args...) \
if(dlevel > 1) fprintf(stderr, "ERROR[%s]: " str, program_name , ## args)

#define WARNING(str, args...) \
if(dlevel > 2) fprintf(stderr, "WARNING[%s]: " str, program_name , ## args)

#define NOTE(str, args...) \
if(dlevel > 3) fprintf(stderr, "Note[%s]: " str, program_name , ## args)

#define DNOTE(str, args...) \
if(dlevel > 4) fprintf(stderr, "Debug[%s]: " str, program_name , ## args)

#define DNOTEC(str, args...) \
if(dlevel > 4) fprintf(stderr, str, ## args)

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
if(dlevel > 0) fprintf(stderr, "FATAL[%s]: " format, program_name , ##__VA_ARGS__)

#define ERROR(format, ...) \
if(dlevel > 1) fprintf(stderr, "ERROR[%s]: " format, program_name , ##__VA_ARGS__)

#define WARNING(format, ...) \
if(dlevel > 2) fprintf(stderr, "WARNING[%s]: " format, program_name , ##__VA_ARGS__)

#define NOTE(format, ...) \
if(dlevel > 3) fprintf(stderr, "Note[%s]: " format, program_name , ##__VA_ARGS__)

#define DNOTE(format, ...) \
if(dlevel > 4) fprintf(stderr, "Debug[%s]: " format, program_name , ##__VA_ARGS__)

#define DNOTEC(format, ...) \
if(dlevel > 4) fprintf(stderr, format, ##__VA_ARGS__)



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
