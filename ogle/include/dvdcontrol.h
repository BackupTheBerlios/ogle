#ifndef DVDCONTROL_H
#define DVDCONTROL_H

#include "dvd.h"


DVDResult_t DVDLeftButtonSelect(int msgqid);
DVDResult_t DVDRightButtonSelect(int msgqid);
DVDResult_t DVDUpperButtonSelect(int msgqid);
DVDResult_t DVDLowerButtonSelect(int msgqid);

DVDResult_t DVDButtonActivate(int msgqid);
DVDResult_t DVDButtonSelect(int msgqid, int button);
DVDResult_t DVDButtonSelectAndActivate(int msgqid, int button);

DVDResult_t DVDMouseSelect(int msgqid, int x, int y);
DVDResult_t DVDMouseActivate(int msgqid, int x, int y);

DVDResult_t DVDMenuCall(int msgqid, DVDMenuID_t menuid);
DVDResult_t DVDResume(int msgqid);
DVDResult_t DVDGoUP(int msgqid);

DVDResult_t DVDDefaultMenuLanguageSelect(int msgqid, DVDLangID_t lang);

void DVDPerror(const char *str, DVDResult_t errcode);

#endif /* DVDCONTROL_H */
