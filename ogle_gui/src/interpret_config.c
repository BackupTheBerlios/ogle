
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "debug_print.h"

static char *program_name;

static void(*add_keybinding)(char *, char *);
static void(*set_dvd_path)(char *);

void init_interpret_config(char *pgm_name,
		      void(*keybinding_cb)(char *, char *),
		      void(*dvd_path_cb)(char *))
{
  program_name = pgm_name;
  DNOTE("init_interpret_config\n");
  add_keybinding = keybinding_cb;
  set_dvd_path = dvd_path_cb;
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
	  ERROR("interpret_b(): more than one <action>\n");
	  break;
	}
	action = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	if(action == NULL) {
	  ERROR("interpret_b(): <action> empty\n");
	} else {
	  //fprintf(stderr, "action: %s\n", action);
	}
      } else if(!strcmp("key", cur->name)) {
	if(action == NULL) {
	  ERROR("interpret_b(): no <action>\n");
	  break;
	}
	key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	if(key == NULL) {
	  WARNING("interpret_b(): <key> empty\n");
	} else {
	  DNOTE("add_keybinding\n");
	  if(add_keybinding != NULL) {
	    add_keybinding(key, action);
	  }
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
	  DNOTE("set_dvd_path\n");
	  if(set_dvd_path != NULL) {
	    set_dvd_path(s);
	  }
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
      if(!strcmp("device", cur->name)) {
	interpret_device(doc, cur);
      }
    }
    cur = cur->next;
  }
}


static void interpret_ogle_conf(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  DNOTE("interpret_ogle_conf\n");

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
  
  DNOTE("interpret_oglerc: %s\n", filename);
  
  doc = xmlParseFile(filename);

  DNOTE("file parsed\n");

  if(doc != NULL) {

    cur = xmlDocGetRootElement(doc); 

    DNOTE("got root\n");

    while(cur != NULL) {
      DNOTE("cur != NULL\n");
      
      if(!xmlIsBlankNode(cur)) {
	if(!strcmp("ogle_conf", cur->name)) {
	  DNOTE("call interpret_ogle_conf\n");
	  interpret_ogle_conf(doc, cur);
	}
      }
      cur = cur->next;
    }
    
    xmlFreeDoc(doc);

    return 0;

  } else {

    WARNING("Couldn't load config file\n");

    return -1;

  }
}


int interpret_config(void)
{
  int r = 0;
  char *home;
  
  DNOTE("interpret_config: config file name: %s\n", CONFIG_FILE);

  if((r+= interpret_oglerc(CONFIG_FILE)) == -1) {
    ERROR("interpret_config(): Couldn't read "CONFIG_FILE"\n");
  }

  home = getenv("HOME");
  if(home == NULL) {
    WARNING("No $HOME\n");
  } else {
    char *rcpath = NULL;
    char rcfile[] = ".oglerc";
    
    rcpath = malloc(strlen(home)+strlen(rcfile)+2);
    strcpy(rcpath, home);
    strcat(rcpath, "/");
    strcat(rcpath, rcfile);
    
    if((r+= interpret_oglerc(rcpath)) == -1) {
      WARNING("interpret_config(): Couldn't read '%s'\n", rcpath);
    }
    
    free(rcpath);
  }
  
    
  
  return r;
}

