#ifndef DVD_H
#define DVD_H

#include <inttypes.h>

typedef enum {
  DVD_E_OK = 0,
  DVD_E_NOT_IMPLEMENTED = 128
} DVDResult_t;


typedef enum {
  DVD_MENU_TITLE      = 2,
  DVD_MENU_ROOT       = 3,
  DVD_MENU_SUBPICTURE = 4,
  DVD_MENU_AUDIO      = 5,
  DVD_MENU_ANGLE      = 6,
  DVD_MENU_PART       = 7
} DVDMenuID_t;


typedef uint16_t DVDLangID_t;

typedef uint16_t DVDRegister;

typedef DVDRegister DVDGPRMArray_t[16];
typedef DVDRegister DVDSPRMArray_t[24];

typedef int DVDStream_t;

typedef struct {
  DVDAudioAppMode_t     AppMode;
  DVDAudioFormat_t      AudioFormat;
  DVDLangID_t           Language;
  DVDAudioLangExt_t     LanguageExtension;
  DVDBool_t             HasMultichannelInfo;
  DVDAudioSampleFreq_t  SampleFrequency;
  DVDAudioSampleQuant_t SampleQuantization;
  DVDChannelNumber_t    NumberOfChannels;
  
} DVDAudioAttributes_t;


#endif /* DVD_H */
