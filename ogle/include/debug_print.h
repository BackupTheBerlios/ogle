#ifndef DEBUG_PRINT_INCLUDED
#define DEBUG_PRINT_INCLUDED


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


#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 95)

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
