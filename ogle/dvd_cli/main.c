/* Ogle - A video player
 * Copyright (C) 2000, 2001 Håkan Hjort
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include <X11/Xlib.h>

#include <ogle/dvdcontrol.h>
#include "xsniffer.h"
#include "bindings.h"


DVDNav_t *nav;

int msgqid;
extern int win;

static char *program_name;
void usage()
{
  fprintf(stderr, "Usage: %s [-m <msgid>] path\n", 
          program_name);
}



void parse_nav_defaults(xmlDocPtr doc, xmlNodePtr cur, DVDNav_t *nav)
{
  //  DVDResult_t res;
  DVDLangID_t lang;
  DVDCountryID_t country;
  DVDParentalLevel_t plevel;
  xmlChar *s;

  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!strcmp("DefaultMenuLanguage", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	fprintf(stderr, "DefaultMenuLanguage = '%s'\n", s);
	lang = s[0] << 8 | s[1];
	DVDDefaultMenuLanguageSelect(nav, lang);
	free(s);
      }
    } else if(!strcmp("DefaultAudioLanguage", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	fprintf(stderr, "DefaultAudioLanguage = %s\n", s);
	lang = s[0] << 8 | s[1];
	DVDDefaultAudioLanguageSelect(nav, lang);    
	free(s);
      }
    } else if(!strcmp("DefaultSubpictureLanguage", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	fprintf(stderr, "DefaultSubpictureLanguage = %s\n", s);
	lang = s[0] << 8 | s[1];
	DVDDefaultSubpictureLanguageSelect(nav, lang);    
	free(s);
      } 
    } else if(!strcmp("ParentalCountry", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	fprintf(stderr, "ParentalCountry = %s\n", s);
	country = s[0] << 8 | s[1];
	DVDParentalCountrySelect(nav, country);    
	free(s);
      } 
    } else if(!strcmp("ParentalLevel", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	fprintf(stderr, "ParentalLevel = %s\n", s);
	plevel = atoi(s);
	DVDParentalLevelSelect(nav, plevel);    
	free(s);
      }
    }

    cur = cur->next;
  }

}



void eval_action(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s;

  
  if((s = xmlGetProp(cur, "Play"))) {
    fprintf(stderr, "Play = %s\n", s);
    //add_key(s, action_play);
    free(s);
  }

  
}


void parse_nav(xmlDocPtr doc, xmlNodePtr cur, DVDNav_t *nav)
{
  cur = cur->xmlChildrenNode;

  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("defaults", cur->name)) {
	  parse_nav_defaults(doc, cur, nav);
      }
    }
    cur = cur->next;
  }
}


void parse_b(xmlDocPtr doc, xmlNodePtr cur)
{
  char *action = NULL;
  char *key = NULL;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("action", cur->name)) {
	if(action != NULL) {
	  fprintf(stderr, "ERROR: parse_b(): more than one <action>\n");
	  break;
	}
	action = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	if(action == NULL) {
	  fprintf(stderr, "ERROR: parse_b(): <action> empty\n");
	} else {
	  fprintf(stderr, "action: %s\n", action);
	}
      } else if(!strcmp("key", cur->name)) {
	if(action == NULL) {
	  fprintf(stderr, "ERROR: parse_b(): no <action>\n");
	  break;
	}
	key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	if(key == NULL) {
	  fprintf(stderr, "WARNING: parse_b(): <key> empty\n");
	} else {
	  add_keybinding(key, action);
	  fprintf(stderr, "key: %s\n", key);	
	  free(key);
	}
      }
    }
    cur = cur->next;
  }
  if(action != NULL) {
    free(action);
  }
}

void parse_bindings(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("b", cur->name)) {
	parse_b(doc, cur);
      }
    }
    cur = cur->next;
  }
}

void parse_ui(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("bindings", cur->name)) {
	parse_bindings(doc, cur);
      }
    }
    cur = cur->next;
  }
}

void parse_dvd(xmlDocPtr doc, xmlNodePtr cur, DVDNav_t *nav)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("nav", cur->name)) {
	parse_nav(doc, cur, nav);
      } 
    }
    cur = cur->next;
  }
}


void parse_ogle_conf(xmlDocPtr doc, xmlNodePtr cur, DVDNav_t *nav)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("dvd", cur->name)) {
	parse_dvd(doc, cur, nav);
      } else if(!strcmp("user_interface", cur->name)) {
	parse_ui(doc, cur);
      }
    }
    cur = cur->next;
  }
}



int
main (int argc, char *argv[])
{
  char *dvd_path;
  int c;
  
  program_name = argv[0];

  /* Parse command line options */
  while ((c = getopt(argc, argv, "m:h?")) != EOF) {
    switch (c) {
    case 'm':
      msgqid = atoi(optarg);
      break;
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }
  
  if(argc - optind > 1){
    usage();
    exit(1);
  }
  
  if(argc - optind == 1){
    dvd_path = argv[optind];
  } else { 
    dvd_path = "/dev/dvd";
  }
  
  if(msgqid !=-1) { // ignore sending data.
    DVDResult_t res;
    sleep(1);
    res = DVDOpenNav(&nav, msgqid);
    if(res != DVD_E_Ok ) {
      DVDPerror("DVDOpen:", res);
      exit(1);
    }
  }
  
  {
    xmlDocPtr doc;
    xmlNodePtr cur;
    DVDResult_t res;
    char *home;
    
    home = getenv("HOME");
    if(home == NULL) {
      fprintf(stderr, "cli: no $HOME\n");
    } else {
      // parse config file
      char *rcpath = NULL;
      char rcfile[] = ".oglerc";
      
      rcpath = malloc(strlen(home)+strlen(rcfile)+2);
      strcpy(rcpath, home);
      strcat(rcpath, "/");
      strcat(rcpath, rcfile);
      
      doc = xmlParseFile(rcpath);
      if(doc != NULL) {
	cur = xmlDocGetRootElement(doc);
	
	
	parse_ogle_conf(doc, cur, nav);      
      } else {
	fprintf(stderr, "WARNING: dvd_cli: Couldn't load config file\n");
      }
      
      free(rcfile);
    }
    
    res = DVDSetDVDRoot(nav, dvd_path);
    if(res != DVD_E_Ok) {
      DVDPerror("DVDSetDVDRoot:", res);
      exit(1);
    }
    
    //    sleep(1);
    DVDRequestInput(nav,
		    INPUT_MASK_KeyPress | INPUT_MASK_ButtonPress |
		    INPUT_MASK_PointerMotion);
  }
  
  xsniff_mouse(NULL);
  exit(0);
}


