#ifndef DVDBOOKMARKS_H_INCLUDED
#define DVDBOOKMARKS_H_INCLUDED

/* helper functions */
typedef struct DVDBookmark_s DVDBookmark_t;

DVDBookmark_t *DVDBookmarkOpen(const unsigned char dvdid[16],
			       const char *file, int create);
int DVDBookmarkGetNr(DVDBookmark_t *bm);
int DVDBookmarkGet(DVDBookmark_t *bm, int nr,
		   char **navstate, char **usercomment,
		   const char *appname, char **appinfo);
int DVDBookmarkAdd(DVDBookmark_t *bm,
		   const char *navstate, const char *usercomment,
		   const char *appname, const char *appinfo);
int DVDBookmarkSetAppInfo(DVDBookmark_t *bm, int nr,
			  const char *appname, const char *appinfo);
int DVDBookmarkRemove(DVDBookmark_t *bm, int nr);
int DVDBookmarkGetDiscComment(DVDBookmark_t *bm, char **disccomment);
int DVDBookmarkSetDiscComment(DVDBookmark_t *bm, const char *disccomment);
int DVDBookmarkSave(DVDBookmark_t *bm, int compressed);
void DVDBookmarkClose(DVDBookmark_t *bm);
/* end helper functions */

#endif /* DVDBOOKMARKS_H_INCLUDED */
