#ifndef DVDBOOKMARKS_H_INCLUDED
#define DVDBOOKMARKS_H_INCLUDED

/* helper functions */
typedef struct DVDBookmark_s DVDBookmark_t;

DVDBookmark_t *DVDBookmarkOpen(unsigned char dvdid[16],
			       char *file, int create);
int DVDBookmarkGetNr(DVDBookmark_t *bm);
int DVDBookmarkGet(DVDBookmark_t *bm, int nr,
		   char **navstate, char **usercomment,
		   char *appname, char **appinfo);
int DVDBookmarkAdd(DVDBookmark_t *bm,
		   char *navstate, char *usercomment,
		   char *appname, char *appinfo);
int DVDBookmarkRemove(DVDBookmark_t *bm, int nr);
int DVDBookmarkSave(DVDBookmark_t *bm, int compressed);
void DVDBookmarkClose(DVDBookmark_t *bm);
/* end helper functions */

#endif /* DVDBOOKMARKS_H_INCLUDED */
