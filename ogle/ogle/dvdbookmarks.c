#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include <ogle/dvdbookmarks.h>

#define OGLE_RC_DIR ".ogle"
#define OGLE_BOOKMARK_DIR "bookmarks"

struct DVDBookmark_s {
  char *filename;
  xmlDocPtr doc;
};

static xmlDocPtr new_bookmark_doc(char *dvdid_str)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  
  if((doc = xmlNewDoc("1.0")) == NULL) {
    return NULL;
  }
  if((node = xmlNewDocNode(doc, NULL, "ogle_bookmarks", NULL)) == NULL) {
    fprintf(stderr, "apa\n");
  }
  if(xmlDocSetRootElement(doc, node)) {
    fprintf(stderr, "old root\n");
  }
  xmlNewProp(node, "dvddiscid", dvdid_str);
  
  return doc;
}

static xmlNodePtr get_bookmark(xmlDocPtr doc, xmlNodePtr cur, int nr)
{
  int n = 0;
  cur = cur->xmlChildrenNode;
  while(cur != NULL) {
    if((!xmlStrcmp(cur->name, (const xmlChar *)"bookmark"))) {
      if(n++ == nr) {
	return cur;
      }
    }
    cur = cur->next;
  }
  return NULL;  
}


/**
 * Open and and read the file that contains the bookmarks for a dvd.
 * @param dvdid This is the dvddiscid and will be used to locate the correct
 * file to read from.
 * @param file  Normally this should be NULL to use the standard bookmark file.
 * If the bookmarks should be read from a specific file, name it here.
 * The default filename used when none is specified is
 * $HOME/.ogle/bookmarks/<dvddiscid>
 *
 * @param create Set this to 1 if the bookmark file should be create in case
 * it doesn't exist. 0 if it shouldn't be created.
 * Setting it to 0 can be used if one wants to find out if there are
 * any bookmarks for a dvd. If there are none, NULL will be returned.
 *
 * @return If successful a handle is returned else NULL and errno is set if
 * the failure was file related.
 *
 */
DVDBookmark_t *DVDBookmarkOpen(unsigned char dvdid[16], char *file, int create)
{
  DVDBookmark_t *bm;
  char *home;
  xmlDocPtr doc = NULL;
  char *filename;
  int fd;
  char dvdid_str[33];
  int n;

  /* convert dvdid to text format */
  for(n = 0; n < 16; n++) {
    sprintf(&dvdid_str[n*2], "%02x", dvdid[n]);
  }
  fprintf(stderr, "id: '%s'\n", dvdid_str);
  if(file == NULL) {
    home = getenv("HOME");
    if(home == NULL) {
      fprintf(stderr, "No $HOME\n");
      return NULL;
    } else {
      char *bmpath = NULL;
      char bmdir[] = "/" OGLE_RC_DIR "/" OGLE_BOOKMARK_DIR "/";
      
      bmpath = malloc(strlen(home)+strlen(bmdir)+strlen(dvdid_str)+1);
      strcpy(bmpath, home);
      strcat(bmpath, bmdir);
      strcat(bmpath, dvdid_str);
      file = bmpath;
    }
  }
  filename = strdup(file);

  xmlKeepBlanksDefault(0);
  if((fd = open(filename, O_RDONLY)) != -1) {
    close(fd);
  } else {
    if(errno != ENOENT) {
      return NULL;
    } else {
      if((fd = open(filename, O_CREAT | O_RDONLY, 0644)) != -1) {
	close(fd);
	if((doc = new_bookmark_doc(dvdid_str)) == NULL) {
	  return NULL;
	}
      } else {
	return NULL;
      }
    } 
  }
  if(doc == NULL) {
    xmlNodePtr cur;
    xmlChar *prop;

    if((doc = xmlParseFile(filename)) == NULL) {
      return NULL;
    }
    if((cur = xmlDocGetRootElement(doc)) == NULL) {
      xmlFree(doc);
      return NULL;
    }
    if((prop = xmlGetProp(cur, "dvddiscid")) == NULL) {
      xmlFree(doc);
      return NULL;
    }
    if(xmlStrcmp(prop, (const xmlChar *)dvdid_str)) {
      xmlFree(prop);
      xmlFree(doc);
      return NULL;
    }
    xmlFree(prop);
      
  }

  if((bm = malloc(sizeof(DVDBookmark_t))) == NULL) {
    return NULL;
  }
  
  bm->filename = filename;
  bm->doc = doc;

  return bm;
}

/**
 * Retrieve a bookmark for the current disc.
 *
 * @param bm Handle from DVDBookmarkOpen.
 * @param nr The specific bookmark for this disc.
 * There can be several bookmarks for one disc.
 * Bookmarks are numbered from 0 and up.
 * To read all bookmarks for this disc, call this function
 * with numbers from 0 and up until -1 is returned.
 * @param navstate This is where a pointer to the navstate data
 * will be returned. You need to free() this when you are done with it.
 * @param usercomment This is where a pointer to the usercomment data
 * will be returned. You need to free() this when you are done with it.
 * @param appname If you supply the name of your application here, you will
 * get application specific data back in appinfo if there is a matching
 * entry for your appname.
 * @param appinfo This is where a pointer to the appinfo data
 * will be returned. You need to free() this when you are done with it.
 */
int DVDBookmarkGet(DVDBookmark_t *bm, int nr,
		   char **navstate, char **usercomment,
		   char *appname, char **appinfo)
{
  xmlNodePtr cur;
  xmlChar *data;
  if(!bm || !(bm->doc) || (nr < 0)) {
    return -1;
  }
  
  if((cur = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }

  cur = get_bookmark(bm->doc, cur, nr);
  if(cur == NULL) {
    return -1;
  }

  cur = cur->xmlChildrenNode;
  while(cur != NULL) {
    if((!xmlStrcmp(cur->name, (const xmlChar *)"navstate"))) {
      if(navstate != NULL) {
	data = xmlNodeListGetString(bm->doc, cur->xmlChildrenNode, 1);
	*navstate = strdup(data);
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"usercomment"))) {
      if(usercomment != NULL) {
	data = xmlNodeListGetString(bm->doc, cur->xmlChildrenNode, 1);
	*usercomment = strdup(data);
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"appinfo"))) {
      if((appname != NULL) && (appinfo != NULL)) {
	xmlChar *prop;
	if((prop = xmlGetProp(cur, "appname")) != NULL) {
	  if((!xmlStrcmp(prop, (const xmlChar *)appname))) {
	    data = xmlNodeListGetString(bm->doc, cur->xmlChildrenNode, 1);
	    *appinfo = strdup(data);
	    xmlFree(data);
	  }
	  xmlFree(prop);
	}
      }
    }
    cur = cur->next;
  }

  return 0;
}
 
/**
 * Add a bookmark for the current disc.
 *
 * @param bm Handle from DVDBookmarkOpen.
 * There can be several bookmarks for one disc.
 * The bookmark will be added at the end of the list of bookmarks
 * for this disc.
 * @param navstate The navstate data for the bookmark.
 * @param usercomment The usercomment data for the bookmark.
 * @param appname If you supply the name of your application here, you will
 * be able to supply application specific data in appinfo.
 * @param appinfo The appinfo data
 *
 * All char * should point to a nullterminated string.
 * The only char * that must be set is navstate, the rest can be NULL.
 *
 * @return 0 on success, -1 on failure.
 */
int DVDBookmarkAdd(DVDBookmark_t *bm,
		   char *navstate, char *usercomment,
		   char *appname, char *appinfo)
{
  xmlNodePtr cur;

  if(!bm || !navstate) {
    return -1;
  }

  if((cur = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }
  
  if((cur = xmlNewChild(cur, NULL, "bookmark", NULL)) == NULL) {
    return -1;
  }
  
  if((xmlNewChild(cur, NULL, "navstate", navstate)) == NULL) {
    return -1;
  }
  
  if(usercomment) {
    if((xmlNewChild(cur, NULL, "usercomment", usercomment)) == NULL) {
      return -1;
    }
  }

  if(appname && appinfo) {
    if((cur = xmlNewChild(cur, NULL, "appinfo", appinfo)) == NULL) {
      return -1;
    }
    xmlNewProp(cur, "appname", appname);    
  }
  
  return 0;						      
}

/**
 * Remove a bookmark for the current disc.
 *
 * @param bm Handle from DVDBookmarkOpen.
 * @param nr The specific bookmark in the list for this disc.
 * The nr is the same as in the DVDBookmarkGet function.
 * The numbering will change once you have removed a bookmark, the numbers
 * after the removed will decrease in value by one so there are no
 * gaps.
 * @return 0 on success, -1 on failure.
 */
int DVDBookmarkRemove(DVDBookmark_t *bm, int nr)
{
  xmlNodePtr cur;

  if(!bm || !(bm->doc) || (nr < 0)) {
    return -1;
  }
  
  if((cur = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }
  
  cur = get_bookmark(bm->doc, cur, nr);
  if(cur == NULL) {
    return -1;
  }
  
  xmlUnlinkNode(cur);
  xmlFreeNode(cur);

  return 0;
}

/**
 * Save the bookmark list for the current disc to file.
 * If the list doesn't contain any entries the file will be removed.
 *
 * @param bm Handle from DVDBookmarkOpen.
 * @param compressed If 1 the output file will be compressed and not human
 * readable. If 0 it will be normal indented text.
 * 0 is recommended if not disk space is very limited.
 * When opening a bookmark file with DVDBookmarkOpen the compression is
 * autodetected.
 *
 * @return 0 on success, -1 on failure.
 */
int DVDBookmarkSave(DVDBookmark_t *bm, int compressed)
{
  xmlNodePtr cur;
  int n;

  if(!bm || !(bm->filename) || !(bm->doc)) {
    return -1;
  }
  if(compressed) {
    xmlSetDocCompressMode(bm->doc, 9);
  } else {
    xmlSetDocCompressMode(bm->doc, 0);
  }
  if(xmlSaveFormatFile(bm->filename, bm->doc, 1) == -1) {
    return -1;
  }

  if((cur = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }
  
  n = 0;
  cur = cur->xmlChildrenNode;
  while(cur != NULL) {
    if((!xmlStrcmp(cur->name, (const xmlChar *)"bookmark"))) {
      n++;
    }
    cur = cur->next;
  }
  
  if(n == 0) {
    unlink(bm->filename);
  }

  return 0;
}

/**
 * Close and free memory used for holding the bookmark list.
 * @param bm The handle to be freed. It cannot be used again.
 */
void DVDBookmarkClose(DVDBookmark_t *bm)
{
  if(bm->filename) {
    free(bm->filename);
    bm->filename = NULL;
  }
  
  if(bm->doc) {
    xmlFreeDoc(bm->doc);
    bm->doc = NULL;
  }
  
  free(bm);
  
  return;
}
