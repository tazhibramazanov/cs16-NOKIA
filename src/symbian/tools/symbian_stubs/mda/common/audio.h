#ifndef CS16_STUB_MDA_COMMON_AUDIO_H
#define CS16_STUB_MDA_COMMON_AUDIO_H

#include "e32std.h"

class TMdaAudioDataSettings
    {
public:
    enum TSampleRate
        {
        ESampleRate8000Hz = 0,
        ESampleRate11025Hz = 1,
        ESampleRate12000Hz = 2,
        ESampleRate16000Hz = 3,
        ESampleRate22050Hz = 4,
        ESampleRate24000Hz = 5,
        ESampleRate32000Hz = 6,
        ESampleRate44100Hz = 7
        };
    enum TChannels
        {
        EChannelsMono = 1,
        EChannelsStereo = 2
        };
    enum TFlags
        {
        ENoNetworkRouting = 0x00000001,
        EStreaming = 0x00000002,
        ENoAudio = 0x00000004,
        EAutoConnect = 0x00000008
        };
    TInt iCaps;
    TInt iVolume;
    TInt iMaxVolume;
    TSampleRate iSampleRate;
    TChannels iChannels;
    TInt iFlags;
    TMdaAudioDataSettings()
        : iCaps(0), iVolume(0), iMaxVolume(0),
          iSampleRate(ESampleRate8000Hz), iChannels(EChannelsMono), iFlags(0) {}
    };

#endif
