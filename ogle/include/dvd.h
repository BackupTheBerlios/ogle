#ifndef DVD_H
#define DVD_H

#include <inttypes.h>

typedef enum {
  DVD_E_Ok = 0,
  DVD_E_NotImplemented = 128
} DVDResult_t;

/**
 * DVD Menu
 */
typedef enum {
  DVD_MENU_Title      = 2, /**< TBD */
  DVD_MENU_Root       = 3, /**< TBD */
  DVD_MENU_Subpicture = 4, /**< TBD */
  DVD_MENU_Audio      = 5, /**< TBD */
  DVD_MENU_Angle      = 6, /**< TBD */
  DVD_MENU_Part       = 7  /**< TBD */
} DVDMenuID_t;

typedef enum {
  UOP_FLAG_TitleOrTimePlay            = 0x00000001, 
  UOP_FLAG_ChapterSearchOrPlay        = 0x00000002, 
  UOP_FLAG_TitlePlay                  = 0x00000004, 
  UOP_FLAG_Stop                       = 0x00000008,  
  UOP_FLAG_GoUp                       = 0x00000010,
  UOP_FLAG_TimeOrChapterSearch        = 0x00000020, 
  UOP_FLAG_PrevOrTopPGSearch          = 0x00000040,  
  UOP_FLAG_NextPGSearch               = 0x00000080,   
  UOP_FLAG_ForwardScan                = 0x00000100,  
  UOP_FLAG_BackwardScan               = 0x00000200,
  UOP_FLAG_TitleMenuCall              = 0x00000400,
  UOP_FLAG_RootMenuCall               = 0x00000800,
  UOP_FLAG_SubPicMenuCall             = 0x00001000,
  UOP_FLAG_AudioMenuCall              = 0x00002000,
  UOP_FLAG_AngleMenuCall              = 0x00004000,
  UOP_FLAG_ChapterMenuCall            = 0x00008000,
  UOP_FLAG_Resume                     = 0x00010000,
  UOP_FLAG_ButtonSelectOrActivate     = 0x00020000,
  UOP_FLAG_StillOff                   = 0x00040000,
  UOP_FLAG_PauseOn                    = 0x00080000,
  UOP_FLAG_AudioStreamChange          = 0x00100000,
  UOP_FLAG_SubPicStreamChange         = 0x00200000,
  UOP_FLAG_AngleChange                = 0x00400000,
  UOP_FLAG_KaraokeAudioPresModeChange = 0x00800000,
  UOP_FLAG_VideoPresModeChange        = 0x01000000 
} DVDUOP_t;

typedef uint16_t DVDLangID_t;

typedef uint16_t DVDRegister;

typedef DVDRegister DVDGPRMArray_t[16];
typedef DVDRegister DVDSPRMArray_t[24];

typedef int DVDStream_t;
typedef int DVDAngle_t;
 
/**
 * The audio application mode
 */
typedef enum {
  DVD_AUDIO_APP_MODE_None     = 0, /**< app mode none     */
  DVD_AUDIO_APP_MODE_Karaoke  = 1, /**< app mode karaoke  */
  DVD_AUDIO_APP_MODE_Surround = 2, /**< app mode surround */
  DVD_AUDIO_APP_MODE_Other    = 3  /**< app mode other    */
} DVDAudioAppMode_t;

/**
 * The audio format
 */
typedef enum {
  DVD_AUDIO_FORMAT_AC3       = 0, /**< Dolby AC-3 */
  DVD_AUDIO_FORMAT_MPEG1     = 1, /**< MPEG-1 */
  DVD_AUDIO_FORMAT_MPEG1_DRC = 2, /**< MPEG-1 with dynamic range control */
  DVD_AUDIO_FORMAT_MPEG2     = 3, /**< MPEG-2 */
  DVD_AUDIO_FORMAT_MPEG2_DRC = 4, /**< MPEG-2 with dynamic range control */
  DVD_AUDIO_FORMAT_LPCM      = 5, /**< Linear Pulse Code Modulation */
  DVD_AUDIO_FORMAT_DTS       = 6, /**< Digital Theater Systems */
  DVD_AUDIO_FORMAT_SDDS      = 7, /**< Sony Dynamic Digital Sound */
  DVD_AUDIO_FORMAT_Other     = 8  /**< Other format*/
} DVDAudioFormat_t;

/**
 * Audio language extension
 */
typedef enum {
  DVD_AUDIO_LANG_EXT_NotSpecified       = 0, /**< TBD */
  DVD_AUDIO_LANG_EXT_NormalCaptions     = 1, /**< TBD */
  DVD_AUDIO_LANG_EXT_VisuallyImpaired   = 2, /**< TBD */
  DVD_AUDIO_LANG_EXT_DirectorsComments1 = 3, /**< TBD */
  DVD_AUDIO_LANG_EXT_DirectorsComments2 = 4  /**< TBD */
} DVDAudioLangExt_t;

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



typedef struct {
  DVDBool_t PanscanPermitted;
  DVDBool_t LetterboxPermitted;
  int AspectX;
  int AspectY;
  int FrameRate;
  int FrameHeight;
  DVDVideoCompression_t Compression;
  DVDBool_t Line21Field1InGop;
  DVDBool_t Line21Field2InGop;
  ...
} DVDVideoAttributes_t;

#endif /* DVD_H */
