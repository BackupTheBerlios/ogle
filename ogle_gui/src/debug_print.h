#ifndef DEBUG_PRINT_INCLUDED
#define DEBUG_PRINT_INCLUDED



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


#endif /* __GNUC__ < 2 */

#endif /* DEBUG_PRINT_INCLUDED */
