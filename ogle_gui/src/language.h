#ifndef LANG_H
#define LANG_H

/* Ogle - A video player
 * Copyright (C) 2000, 2001 Vilhelm Bergman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

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
