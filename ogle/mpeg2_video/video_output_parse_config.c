#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>



static void parse_display(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("width_mm", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  free(s);
	}
      } else if(!strcmp("height_mm", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  free(s);
	}
      } else if(!strcmp("width_px", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  free(s);
	}
      } else if(!strcmp("height_px", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  free(s);
	}
      } else if(!strcmp("geometry_src", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  free(s);
	}
      } else if(!strcmp("resolution_src", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  free(s);
	}
      } 
    }
    cur = cur->next;
  }
}

static void parse_video(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("display", cur->name)) {
	parse_display(doc, cur);
      } else if(!strcmp("window", cur->name)) {
	parse_window(doc, cur);
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

    fprintf(stderr, "WARNING[ogle_vo]: Couldn't load config file\n");

    return -1;

  }
}


int parse_config(void)
{
  int r = 0;
  char *home;


  if((r+= parse_oglerc(CONFIG_FILE)) == -1) {
    fprintf(stderr,
	    "ERROR[ogle_vo]: parse_config(): Couldn't read "CONFIG_FILE"\n");
  }

  home = getenv("HOME");
  if(home == NULL) {
    fprintf(stderr, "WARNING[ogle_vo]: No $HOME\n");
  } else {
    char *rcpath = NULL;
    char rcfile[] = ".oglerc";
    
    rcpath = malloc(strlen(home)+strlen(rcfile)+2);
    strcpy(rcpath, home);
    strcat(rcpath, "/");
    strcat(rcpath, rcfile);
    
    if((r+= parse_oglerc(rcpath)) == -1) {
      fprintf(stderr,
	      "NOTE[ogle_vo]: parse_config(): Couldn't read '%s'\n", rcpath);
    }
    
    free(rcpath);
  }
  
    
  
  return r;
}


