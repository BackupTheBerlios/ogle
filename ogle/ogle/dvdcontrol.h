#ifndef DVDCONTROL_H
#define DVDCONTROL_H

#include <ogle/dvd.h>


typedef struct DVDNav_s DVDNav_t;

DVDResult_t DVDOpenNav(DVDNav_t **nav, int msgqid);
DVDResult_t DVDCloseNav(DVDNav_t *nav);

void DVDPerror(const char *str, DVDResult_t ErrCode);

/* info commands */

DVDResult_t DVDGetAllGPRMs(DVDNav_t *nav, const DVDGPRMArray_t *Registers);
DVDResult_t DVDGetAllSPRMs(DVDNav_t *nav, const DVDSPRMArray_t *Registers);

DVDResult_t DVDGetCurrentUOPS(DVDNav_t *nav, const DVDUOP_t *uop);
DVDResult_t DVDGetAudioAttributes(DVDNav_t *nav, DVDAudioStream_t StreamNr,
				  const DVDAudioAttributes_t *Attr);
DVDResult_t DVDGetAudioLanguage(DVDNav_t *nav, DVDAudioStream_t StreamNr,
				const DVDLangID_t *Language);
DVDResult_t DVDGetCurrentAudio(DVDNav_t *nav, const int *StreamsAvailable,
			       const DVDAudioStream_t *CurrentStream);
DVDResult_t DVDIsAudioStreamEnabled(DVDNav_t *nav, DVDAudioStream_t StreamNr,
				    const DVDBool_t *Enabled);
DVDResult_t DVDGetDefaultAudioLanguage(DVDNav_t *nav, const DVDLangID_t *Language,
				       const DVDAudioLangExt_t *AudioExtension);
DVDResult_t DVDGetCurrentAngle(DVDNav_t *nav, const int *AnglesAvailable,
			       const DVDAngle_t *CurrentAngle);
DVDResult_t DVDGetCurrentVideoAttributes(DVDNav_t *nav, const DVDVideoAttributes_t *Attr);

/* end info commands */


/* control commands */

DVDResult_t DVDLeftButtonSelect(DVDNav_t *nav);
DVDResult_t DVDRightButtonSelect(DVDNav_t *nav);
DVDResult_t DVDUpperButtonSelect(DVDNav_t *nav);
DVDResult_t DVDLowerButtonSelect(DVDNav_t *nav);

DVDResult_t DVDButtonActivate(DVDNav_t *nav);
DVDResult_t DVDButtonSelect(DVDNav_t *nav, int Button);
DVDResult_t DVDButtonSelectAndActivate(DVDNav_t *nav, int Button);

DVDResult_t DVDMouseSelect(DVDNav_t *nav, int x, int y);
DVDResult_t DVDMouseActivate(DVDNav_t *nav, int x, int y);

DVDResult_t DVDMenuCall(DVDNav_t *nav, DVDMenuID_t MenuId);
DVDResult_t DVDResume(DVDNav_t *nav);
DVDResult_t DVDGoUp(DVDNav_t *nav);


DVDResult_t DVDForwardScan(DVDNav_t *nav, double Speed);
DVDResult_t DVDBackwardScan(DVDNav_t *nav, double Speed);

DVDResult_t DVDNextPGSearch(DVDNav_t *nav);
DVDResult_t DVDPrevPGSearch(DVDNav_t *nav);
DVDResult_t DVDTopPGSearch(DVDNav_t *nav);

DVDResult_t DVDPTTSearch(DVDNav_t *nav, DVDPTT_t PTT);
DVDResult_t DVDPTTPlay(DVDNav_t *nav, DVDTitle_t Title, DVDPTT_t PTT);

DVDResult_t DVDTitlePlay(DVDNav_t *nav, DVDTitle_t Title);

DVDResult_t DVDTimeSearch(DVDNav_t *nav, DVDTimecode_t time);
DVDResult_t DVDTimePlay(DVDNav_t *nav, DVDTitle_t Title, DVDTimecode_t time);

DVDResult_t DVDPauseOn(DVDNav_t *nav);
DVDResult_t DVDPauseOff(DVDNav_t *nav);
DVDResult_t DVDStop(DVDNav_t *nav);



DVDResult_t DVDDefaultMenuLanguageSelect(DVDNav_t *nav, DVDLangID_t Lang);

DVDResult_t DVDAudioStreamChange(DVDNav_t *nav, DVDAudioStream_t StreamNr);



/* end control commands */

#endif /* DVDCONTROL_H */