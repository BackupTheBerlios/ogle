#ifndef DVDINFO_H
#define DVDINFO_H

#include "dvd.h"

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

#endif /* DVDINFO_H */
