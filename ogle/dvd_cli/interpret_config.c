#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "bindings.h"
#include "debug_print.h"
#include "interpret_config.h"

extern char *dvd_path;


static void interpret_b(xmlDocPtr doc, xmlNodePtr cur)
{
  char *action = NULL;
  char *key = NULL;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("action", cur->name)) {
	if(action != NULL) {
	  ERROR("interpret_b(): more than one <action>\n", 0);
	  break;
	}
	action = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	if(action == NULL) {
	  ERROR("interpret_b(): <action> empty\n", 0);
	} else {
	  //fprintf(stderr, "action: %s\n", action);
	}
      } else if(!strcmp("key", cur->name)) {
	if(action == NULL) {
	  ERROR("interpret_b(): no <action>\n", 0);
	  break;
	}
	key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	if(key == NULL) {
	  WARNING("interpret_b(): <key> empty\n", 0);
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
  int fd;

  if((fd = open(filename, O_RDONLY)) == -1) {
    if(errno != ENOENT) {
      perror(filename);
    }
    return -1;
  } else {
    close(fd);
  }
  
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
    return -1;
  }
}


int interpret_config(void)
{
  int config_read = 0;
  char *home;


  if(interpret_oglerc(CONFIG_FILE) != -1) {
    config_read |= 1;
  }

  home = getenv("HOME");
  if(home == NULL) {
    WARNING("No $HOME\n", 0);
  } else {
    char *rcpath = NULL;
    char rcfile[] = ".oglerc";
    
    rcpath = malloc(strlen(home)+strlen(rcfile)+2);
    strcpy(rcpath, home);
    strcat(rcpath, "/");
    strcat(rcpath, rcfile);
    
    if(interpret_oglerc(rcpath) != -1) {
      config_read |= 2;
    }
    if(!config_read) {
      ERROR("interpret_config(): Couldn't read '%s'\n", rcpath);
    }
    free(rcpath);
  }
  
  if(!config_read) {
    ERROR("interpret_config(): Couldn't read "CONFIG_FILE"\n", 0);
    return -1;
  }
  
  return 0;
}


