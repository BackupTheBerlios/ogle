#ifndef DVDCONTROL_H
#define DVDCONTROL_H

#include "dvd.h"


DVDResult_t DVDOpen(int msgqid);

DVDResult_t DVDLeftButtonSelect(void);
DVDResult_t DVDRightButtonSelect(void);
DVDResult_t DVDUpperButtonSelect(void);
DVDResult_t DVDLowerButtonSelect(void);

DVDResult_t DVDButtonActivate(void);
DVDResult_t DVDButtonSelect(int Button);
DVDResult_t DVDButtonSelectAndActivate(int Button);

DVDResult_t DVDMouseSelect(int x, int y);
DVDResult_t DVDMouseActivate(int x, int y);

DVDResult_t DVDMenuCall(DVDMenuID_t MenuId);
DVDResult_t DVDResume(void);
DVDResult_t DVDGoUP(void);

DVDResult_t DVDDefaultMenuLanguageSelect(DVDLangID_t Lang);

void DVDPerror(const char *str, DVDResult_t ErrCode);

#endif /* DVDCONTROL_H */
