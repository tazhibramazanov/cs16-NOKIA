#ifndef CS16_AUDIO_H
#define CS16_AUDIO_H

/*
 * Audio.h -- нативный аудиомикшер (порт логики src/pc/cspc/Mixer.java).
 * Все звуки заранее загружаются в память и микшируются в один
 * CMdaAudioOutputStream. Голосовой PTT идёт через CMdaAudioInputStream.
 * Никаких MMAPI-объектов и аллокаций во время микширования.
 */

#include "Engine.h"

#include <mdaaudiooutputstream.h>
#include <mdaaudioinputstream.h>

class MAudioObserver
    {
public:
    virtual void VoiceRecorded(const TUint8* aPcm, TInt aLength) = 0;
    virtual void AudioError(TInt aError) = 0;
    };

class CAudioVoice
    {
public:
    TUint16* iData;        /* 16-bit signed, исходная частота */
    TInt iLength;
    TInt iRate;
    TUint8* iRaw;          /* 8-bit unsigned sources for 11025 Hz */
    TInt iRawLength;
    };

class CAudio : public CBase, public MMdaAudioOutputStreamCallback,
               public MMdaAudioInputStreamCallback
    {
public:
    static CAudio* NewL(RFs& aFileServer, const TDesC& aDirectory,
                        MAudioObserver& aObserver);
    ~CAudio();

    void ConstructL();
    void SetModDirectory(const TDesC& aModDir);

    /* Имя без .wav. Возвращает FALSE, если файл отсутствует. */
    TBool Play(const TPtrC8& aName, TInt aVolume);
    void PlayPcm(const TUint8* aPcm, TInt aLength, TInt aRate, TInt aVolume);
    void StopAll();
    void SetMasterVolume(TInt aVolume);
    TInt MasterVolume() const { return iMasterVolume; }

    TBool StartRecording();
    void StopRecording();
    TBool IsRecording() const { return iRecording; }

    TInt OutputRate() const { return KOutputRate; }
    TInt OutputError() const { return iOutputError; }

    /* CMdaAudioOutputStream observer */
    void MaoscOpenComplete(TInt aError);
    void MaoscBufferCopied(TInt aError, const TDesC8& aBuffer);
    void MaoscPlayComplete(TInt aError);

    /* CMdaAudioInputStream observer */
    void MaiscOpenComplete(TInt aError);
    void MaiscBufferCopied(TInt aError, const TDesC8& aBuffer);
    void MaiscRecordComplete(TInt aError);

private:
    enum
        {
        KOutputRate = 11025,
        KMaxVoices = 32,
        KMaxSounds = 320,
        KBlockFrames = 512,
        KRecordBlock = 4096,
        KMaxRecorded = 16384
        };

    CAudio(RFs& aFileServer, const TDesC& aDirectory, MAudioObserver& aObserver);

    struct TVoice
        {
        const TUint16* iData;
        TInt iPosition;
        TInt iStep;
        TInt iVolume;
        TInt iLength;
        TInt iRate;
        TBool iActive;
        };

    struct TSoundEntry
        {
        TPtrC8 iName;
        CAudioVoice* iSample;
        };

    CAudioVoice* LoadWaveL(const TPtrC8& aName);
    CAudioVoice* DecodeWaveL(HBufC8* aFile);
    void StartVoice(const CAudioVoice* aSample, TInt aVolume, TInt aRate);
    void MixBlock(TUint8* aDestination);
    void RequestNextBlock();

private:
    RFs& iFileServer;
    TPtrC iDirectory;
    HBufC* iDirectoryBuffer;
    HBufC* iModDirectoryBuffer;
    TPtrC iModDirectory;
    MAudioObserver& iObserver;

    CMdaAudioOutputStream* iOutputStream;
    CMdaAudioInputStream* iInputStream;
    TInt iOutputError;
    TInt iInputError;
    TBool iStreamOpen;
    TBool iRecording;
    TBool iOwnsOutputBuffer;
    HBufC8* iOutputBuffer;
    HBufC8* iInputBuffer;
    TUint8 iMixedBlock[KBlockFrames];
    TInt32 iMixer[KBlockFrames];

    TVoice iVoices[KMaxVoices];
    TSoundEntry iSounds[KMaxSounds];
    TInt iSoundCount;
    TInt iMasterVolume;
    TUint8 iRecorded[KMaxRecorded];
    TInt iRecordedLength;
    TInt iNextVoice;
    TRandom iRandom;
    };

#endif
