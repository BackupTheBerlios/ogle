#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>


static char *audio_device;


char *get_audio_device(void)
{
  return audio_device;
}

static void parse_device(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("path", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(audio_device != NULL) {
	    free(audio_device);
	  }
	  audio_device = strdup(s);
	  free(s);
	}
      }
    }
    cur = cur->next;
  }
}

static void parse_audio(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("device", cur->name)) {
	parse_device(doc, cur);
      }
    }
    cur = cur->next;
  }
}


static void parse_ogle_conf(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("audio", cur->name)) {
	parse_audio(doc, cur);
      }
    }
    cur = cur->next;
  }
}


int parse_oglerc(char *filename)
{
  xmlDocPtr doc;
  xmlNodePtr cur;
  
  doc = xmlParseFile(filename);

  if(doc != NULL) {

    cur = xmlDocGetRootElement(doc); 

    while(cur != NULL) {
      
      if(!xmlIsBlankNode(cur)) {
	if(!strcmp("ogle_conf", cur->name)) {
	  parse_ogle_conf(doc, cur);
	}
      }
      cur = cur->next;
    }
    
    xmlFreeDoc(doc);

    return 0;

  } else {

    fprintf(stderr, "WARNING[audio]: Couldn't load config file\n");

    return -1;

  }
}


int parse_config(void)
{
  int r = 0;
  char *home;


  if((r+= parse_oglerc(CONFIG_FILE)) == -1) {
    fprintf(stderr,
	    "ERROR[ogle_audio]: parse_config(): Couldn't read "CONFIG_FILE"\n");
  }

  home = getenv("HOME");
  if(home == NULL) {
    fprintf(stderr, "WARNING[ogle_audio]: No $HOME\n");
  } else {
    char *rcpath = NULL;
    char rcfile[] = ".oglerc";
    
    rcpath = malloc(strlen(home)+strlen(rcfile)+2);
    strcpy(rcpath, home);
    strcat(rcpath, "/");
    strcat(rcpath, rcfile);
    
    if((r+= parse_oglerc(rcpath)) == -1) {
      fprintf(stderr,
	      "WARNING[ogle_audio]: parse_config(): Couldn't read '%s'\n", rcpath);
    }
    
    free(rcpath);
  }
  
    
  
  return r;
}


