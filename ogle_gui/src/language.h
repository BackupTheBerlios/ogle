#ifndef LANG_H
#define LANG_H

#include <gnome.h>
#include <ogle/dvdcontrol.h>
/**
 * Contains language information.
 */
 
char* language_name(DVDLangID_t lang);

char* language_code(DVDLangID_t lang);


typedef struct {
  char *name;            /**<  Language Name (English)  */
  char code_639_2b[4];   /**<  Country code: 639-2/B    */
  char code_639_2t[4];   /**<  Country code: 639-2/T    */
  char code_639_1[3];    /**<  Country code: 639-1      */
} langcodes_t;




#endif /* LANG_H */
