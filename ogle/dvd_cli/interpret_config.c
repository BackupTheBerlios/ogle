#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include <ogle/dvdcontrol.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "bindings.h"

extern DVDNav_t *nav;
extern char *dvd_path;

static void interpret_nav_defaults(xmlDocPtr doc, xmlNodePtr cur)
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
	//fprintf(stderr, "DefaultMenuLanguage = '%s'\n", s);
	lang = s[0] << 8 | s[1];
	DVDDefaultMenuLanguageSelect(nav, lang);
	free(s);
      }
    } else if(!strcmp("DefaultAudioLanguage", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	//fprintf(stderr, "DefaultAudioLanguage = %s\n", s);
	lang = s[0] << 8 | s[1];
	DVDDefaultAudioLanguageSelect(nav, lang);    
	free(s);
      }
    } else if(!strcmp("DefaultSubtitleLanguage", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	//fprintf(stderr, "DefaultSubtitleLanguage = %s\n", s);
	lang = s[0] << 8 | s[1];
	DVDDefaultSubpictureLanguageSelect(nav, lang);    
	free(s);
      } 
    } else if(!strcmp("ParentalCountry", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	//fprintf(stderr, "ParentalCountry = %s\n", s);
	country = s[0] << 8 | s[1];
	DVDParentalCountrySelect(nav, country);    
	free(s);
      } 
    } else if(!strcmp("ParentalLevel", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	//fprintf(stderr, "ParentalLevel = %s\n", s);
	plevel = atoi(s);
	DVDParentalLevelSelect(nav, plevel);    
	free(s);
      }
    }

    cur = cur->next;
  }

}


static void interpret_nav(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;

  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("defaults", cur->name)) {
	  interpret_nav_defaults(doc, cur);
      }
    }
    cur = cur->next;
  }
}


static void interpret_b(xmlDocPtr doc, xmlNodePtr cur)
{
  char *action = NULL;
  char *key = NULL;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("action", cur->name)) {
	if(action != NULL) {
	  fprintf(stderr,
		  "ERROR[dvd_cli]: interpret_b(): more than one <action>\n");
	  break;
	}
	action = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	if(action == NULL) {
	  fprintf(stderr, "ERROR[dvd_cli]: interpret_b(): <action> empty\n");
	} else {
	  //fprintf(stderr, "action: %s\n", action);
	}
      } else if(!strcmp("key", cur->name)) {
	if(action == NULL) {
	  fprintf(stderr, "ERROR[dvd_cli]: interpret_b(): no <action>\n");
	  break;
	}
	key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	if(key == NULL) {
	  fprintf(stderr, "WARNING[dvd_cli]: interpret_b(): <key> empty\n");
	} else {
	  add_keybinding(key, action);
	  //fprintf(stderr, "key: %s\n", key);	
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

static void interpret_bindings(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("b", cur->name)) {
	interpret_b(doc, cur);
      }
    }
    cur = cur->next;
  }
}

static void interpret_ui(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("bindings", cur->name)) {
	interpret_bindings(doc, cur);
      }
    }
    cur = cur->next;
  }
}

static void interpret_device(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("path", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(dvd_path != NULL) {
	    free(dvd_path);
	  }
	  dvd_path = strdup(s);
	  free(s);
	}
      }
    }
    cur = cur->next;
  }
}

static void interpret_dvd(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("nav", cur->name)) {
	interpret_nav(doc, cur);
      } else if(!strcmp("device", cur->name)) {
	interpret_device(doc, cur);
      }
    }
    cur = cur->next;
  }
}


static void interpret_ogle_conf(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("dvd", cur->name)) {
	interpret_dvd(doc, cur);
      } else if(!strcmp("user_interface", cur->name)) {
	interpret_ui(doc, cur);
      }
    }
    cur = cur->next;
  }
}


int interpret_oglerc(char *filename)
{
  xmlDocPtr doc;
  xmlNodePtr cur;
  
  doc = xmlParseFile(filename);

  if(doc != NULL) {

    cur = xmlDocGetRootElement(doc); 

    while(cur != NULL) {
      
      if(!xmlIsBlankNode(cur)) {
	if(!strcmp("ogle_conf", cur->name)) {
	  interpret_ogle_conf(doc, cur);
	}
      }
      cur = cur->next;
    }
    
    xmlFreeDoc(doc);

    return 0;

  } else {

    fprintf(stderr, "WARNING[dvd_cli]: Couldn't load config file\n");

    return -1;

  }
}


int interpret_config(void)
{
  int r = 0;
  char *home;


  if((r+= interpret_oglerc(CONFIG_FILE)) == -1) {
    fprintf(stderr,
	    "ERROR[dvd_cli]: interpret_config(): Couldn't read "CONFIG_FILE"\n");
  }

  home = getenv("HOME");
  if(home == NULL) {
    fprintf(stderr, "WARNING[dvd_cli]: No $HOME\n");
  } else {
    char *rcpath = NULL;
    char rcfile[] = ".oglerc";
    
    rcpath = malloc(strlen(home)+strlen(rcfile)+2);
    strcpy(rcpath, home);
    strcat(rcpath, "/");
    strcat(rcpath, rcfile);
    
    if((r+= interpret_oglerc(rcpath)) == -1) {
      fprintf(stderr,
	      "WARNING[dvd_cli]: interpret_config(): Couldn't read '%s'\n", rcpath);
    }
    
    free(rcpath);
  }
  
    
  
  return r;
}


