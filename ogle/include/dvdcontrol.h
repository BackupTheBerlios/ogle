#ifndef DVDCONTROL_H
#define DVDCONTROL_H

#include "dvd.h"


DVDResult_t DVDOpen(int msgqid);



/* info commands */

DVDResult_t DVDGetAllGPRMs(const DVDGPRMArray_t *Registers);
DVDResult_t DVDGetAllSPRMs(const DVDSPRMArray_t *Registers);

DVDResult_t DVDGetCurrentUOPS(const DVDUOP_t *uop);
DVDResult_t DVDGetAudioAttributes(DVDAudioStream_t StreamNr,
				  const DVDAudioAttributes_t *Attr);
DVDResult_t DVDGetAudioLanguage(DVDAudioStream_t StreamNr,
				const DVDLangID_t *Language);
DVDResult_t DVDGetCurrentAudio(const int *StreamsAvailable,
			       const DVDAudioStream_t *CurrentStream);
DVDResult_t DVDIsAudioStreamEnabled(DVDAudioStream_t StreamNr,
				    const DVDBool_t *Enabled);
DVDResult_t DVDGetDefaultAudioLanguage(const DVDLangID_t *Language,
				       const DVDAudioLangExt_t *AudioExtension);
DVDResult_t DVDGetCurrentAngle(const int *AnglesAvailable,
			       const DVDAngle_t *CurrentAngle);
DVDResult_t DVDGetCurrentVideoAttributes(const DVDVideoAttributes_t *Attr);

/* end info commands */


/* control commands */

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
DVDResult_t DVDGoUp(void);


DVDResult_t DVDForwardScan(double Speed);
DVDResult_t DVDBackwardScan(double Speed);

DVDResult_t DVDNextPGSearch(void);
DVDResult_t DVDPrevPGSearch(void);
DVDResult_t DVDTopPGSearch(void);

DVDResult_t DVDPTTSearch(DVDPTT_t PTT);
DVDResult_t DVDPTTPlay(DVDTitle_t Title, DVDPTT_t PTT);

DVDResult_t DVDTitlePlay(DVDTitle_t Title);

DVDResult_t DVDTimeSearch(DVDTimecode_t time);
DVDResult_t DVDTimePlay(DVDTitle_t Title, DVDTimecode_t time);

DVDResult_t DVDPauseOn(void);
DVDResult_t DVDPauseOff(void);
DVDResult_t DVDStop(void);



DVDResult_t DVDDefaultMenuLanguageSelect(DVDLangID_t Lang);

DVDResult_t DVDAudioStreamChange(DVDAudioStream_t StreamNr);

void DVDPerror(const char *str, DVDResult_t ErrCode);

/* end control commands */

#endif /* DVDCONTROL_H */
