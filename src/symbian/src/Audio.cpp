#include "Audio.h"

#include <e32std.h>
#include <e32base.h>
#include <mda/common/audio.h>
#include <mmf\common\mmffourcc.h>

_LIT(KDotWav, ".wav");

static void CleanupAudioVoice(TAny* aVoice)
    {
    CAudioVoice* voice = static_cast<CAudioVoice*>(aVoice);
    if (!voice)
        return;
    delete[] reinterpret_cast<TUint8*>(voice->iData);
    delete voice;
    }

static TUint32 ReadLe32(const TUint8* aPointer)
    {
    return (TUint32)aPointer[0] | ((TUint32)aPointer[1] << 8) |
           ((TUint32)aPointer[2] << 16) | ((TUint32)aPointer[3] << 24);
    }

static TUint16 ReadLe16(const TUint8* aPointer)
    {
    return (TUint16)((TUint16)aPointer[0] | ((TUint16)aPointer[1] << 8));
    }

CAudio::CAudio(RFs& aFileServer, const TDesC& aDirectory, MAudioObserver& aObserver)
    : iFileServer(aFileServer),
      iDirectory(aDirectory),
      iDirectoryBuffer(NULL),
      iModDirectoryBuffer(NULL),
      iObserver(aObserver),
      iOutputStream(NULL),
      iInputStream(NULL),
      iOutputError(KErrNone),
      iInputError(KErrNone),
      iStreamOpen(EFalse),
      iRecording(EFalse),
      iOwnsOutputBuffer(EFalse),
      iOutputBuffer(NULL),
      iInputBuffer(NULL),
      iSoundCount(0),
      iMasterVolume(100),
      iRecordedLength(0),
      iNextVoice(0)
    {
    Mem::FillZ(iVoices, sizeof(iVoices));
    Mem::FillZ(iSounds, sizeof(iSounds));
    Mem::FillZ(iMixedBlock, sizeof(iMixedBlock));
    Mem::FillZ(iMixer, sizeof(iMixer));
    Mem::FillZ(iRecorded, sizeof(iRecorded));
    }

CAudio* CAudio::NewL(RFs& aFileServer, const TDesC& aDirectory, MAudioObserver& aObserver)
    {
    CAudio* self = new (ELeave) CAudio(aFileServer, aDirectory, aObserver);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }

void CAudio::ConstructL()
    {
    iDirectoryBuffer = iDirectory.AllocL();
    iDirectory.Set(*iDirectoryBuffer);

    iOutputBuffer = HBufC8::NewL(KBlockFrames);
    iOwnsOutputBuffer = ETrue;
    iOutputBuffer->Des().SetLength(KBlockFrames);
    iOutputBuffer->Des().FillZ(KBlockFrames);

    iInputBuffer = HBufC8::NewL(KRecordBlock);
    iInputBuffer->Des().SetLength(KRecordBlock);
    iInputBuffer->Des().FillZ(KRecordBlock);

    iOutputStream = CMdaAudioOutputStream::NewL(*this);

    TMdaAudioDataSettings settings;
    settings.iCaps = 0;
    settings.iVolume = 0;
    settings.iMaxVolume = 0;
    settings.iSampleRate = TMdaAudioDataSettings::ESampleRate11025Hz;
    settings.iChannels = TMdaAudioDataSettings::EChannelsMono;
    settings.iFlags = TMdaAudioDataSettings::ENoNetworkRouting;
    iOutputStream->Open(&settings);
    }

CAudio::~CAudio()
    {
    if (iInputStream)
        {
        if (iRecording)
            iInputStream->Stop();
        delete iInputStream;
        iInputStream = NULL;
        }

    if (iOutputStream)
        {
        if (iStreamOpen)
            iOutputStream->Stop();
        delete iOutputStream;
        iOutputStream = NULL;
        }

    for (TInt i = 0; i < iSoundCount; i++)
        {
        CAudioVoice* voice = iSounds[i].iSample;
        if (voice)
            CleanupAudioVoice(voice);
        }

    if (iOwnsOutputBuffer)
        delete iOutputBuffer;
    delete iInputBuffer;
    delete iDirectoryBuffer;
    }

CAudioVoice* CAudio::DecodeWaveL(HBufC8* aFile)
    {
    CleanupStack::PushL(aFile);

    const TUint8* bytes = aFile->Ptr();
    const TInt length = aFile->Length();
    CAudioVoice* voice = NULL;

    if (length < 12 || bytes[0] != 'R' || bytes[1] != 'I' ||
        bytes[2] != 'F' || bytes[3] != 'F' ||
        bytes[8] != 'W' || bytes[9] != 'A' || bytes[10] != 'V' || bytes[11] != 'E')
        User::Leave(KErrCorrupt);

    TInt position = 12;
    TInt channels = 1;
    TInt bits = 8;
    TInt rate = KOutputRate;

    while (position + 8 <= length)
        {
        const TUint8* chunk = bytes + position;
        const TUint32 chunkLength = ReadLe32(chunk + 4);
        const TInt available = length - position - 8;

        if (chunk[0] == 'f' && chunk[1] == 'm' && chunk[2] == 't' && chunk[3] == ' ')
            {
            if (available < 16)
                User::Leave(KErrCorrupt);
            channels = ReadLe16(chunk + 10);
            rate = (TInt)ReadLe32(chunk + 12);
            bits = ReadLe16(chunk + 22);
            }
        else if (chunk[0] == 'd' && chunk[1] == 'a' && chunk[2] == 't' && chunk[3] == 'a')
            {
            TInt count = available;
            if (chunkLength < (TUint32)count)
                count = (TInt)chunkLength;

            const TInt bytesPerSample = (bits / 8 > 0 ? bits / 8 : 1) *
                                        (channels > 0 ? channels : 1);
            const TInt samples = count / bytesPerSample;
            if (samples <= 0)
                User::Leave(KErrCorrupt);

            voice = new (ELeave) CAudioVoice;
            voice->iData = NULL;
            voice->iLength = 0;
            voice->iRate = rate > 0 ? rate : KOutputRate;
            voice->iRaw = NULL;
            voice->iRawLength = 0;

            CleanupStack::PushL(TCleanupItem(CleanupAudioVoice, voice));
            voice->iData = reinterpret_cast<TUint16*>(new (ELeave) TUint8[samples * (TInt)sizeof(TUint16)]);
            Mem::FillZ(reinterpret_cast<TUint8*>(voice->iData), samples * (TInt)sizeof(TUint16));

            for (TInt i = 0; i < samples; i++)
                {
                const TInt offset = position + 8 + i * bytesPerSample;
                TInt sample;
                if (bits == 8)
                    sample = ((TInt)bytes[offset] - 128) << 8;
                else
                    sample = (TInt)(TInt16)ReadLe16(bytes + offset);
                voice->iData[i] = (TUint16)(TInt16)sample;
                }

            voice->iLength = samples;
            CleanupStack::Pop();
            break;
            }

        if (chunkLength > (TUint32)(length - position))
            break;
        position += 8 + (TInt)chunkLength + (TInt)(chunkLength & 1);
        }

    if (!voice)
        User::Leave(KErrCorrupt);

    CleanupStack::PopAndDestroy(aFile);
    return voice;
    }

CAudioVoice* CAudio::LoadWaveL(const TPtrC8& aName)
    {
    for (TInt i = 0; i < iSoundCount; i++)
        {
        if (iSounds[i].iName.Length() == aName.Length() &&
            iSounds[i].iName.Compare(aName) == 0)
            return iSounds[i].iSample;
        }

    if (iSoundCount >= KMaxSounds)
        return NULL;

    HBufC* path = HBufC::NewLC((iModDirectory.Length() > 0 ? iModDirectory.Length() : iDirectory.Length()) + aName.Length() + 8);
    TPtr pathPointer = path->Des();
    TBool useMod = EFalse;
    if (iModDirectory.Length() > 0)
        {
        pathPointer.Copy(iModDirectory);
        if (pathPointer.Length() > 0 && pathPointer[pathPointer.Length() - 1] != '\\' && pathPointer[pathPointer.Length() - 1] != '/')
            pathPointer.Append('\\');
        for (TInt i = 0; i < aName.Length(); i++)
            pathPointer.Append((TText)aName[i]);
        pathPointer.Append(KDotWav);
        RFile tf; if (tf.Open(iFileServer, pathPointer, EFileRead | EFileShareAny) == KErrNone) { tf.Close(); useMod = ETrue; }
        }
    if (!useMod)
        {
        pathPointer.Zero();
        pathPointer.Copy(iDirectory);
        if (pathPointer.Length() == 0)
            pathPointer.Append('\\');
        else
            {
            const TText last = pathPointer[pathPointer.Length() - 1];
            if (last != '\\' && last != '/')
                pathPointer.Append('\\');
            }
        for (TInt i = 0; i < aName.Length(); i++)
            pathPointer.Append((TText)aName[i]);
        pathPointer.Append(KDotWav);
        }

    HBufC8* file = ReadWholeFileL(iFileServer, *path);
    CAudioVoice* voice = DecodeWaveL(file);
    CleanupStack::PushL(TCleanupItem(CleanupAudioVoice, voice));

    const TInt sampleBytes = voice->iLength * (TInt)sizeof(TUint16);
    const TInt nameLength = aName.Length();
    TUint8* block = new (ELeave) TUint8[sampleBytes + nameLength + 1];
    Mem::Copy(block, voice->iData, sampleBytes);
    for (TInt i = 0; i < nameLength; i++)
        block[sampleBytes + i] = aName[i];
    block[sampleBytes + nameLength] = 0;

    delete[] reinterpret_cast<TUint8*>(voice->iData);
    voice->iData = reinterpret_cast<TUint16*>(block);
    voice->iRaw = NULL;
    voice->iRawLength = 0;

    iSounds[iSoundCount].iName.Set(block + sampleBytes, nameLength);
    iSounds[iSoundCount].iSample = voice;
    iSoundCount++;

    CleanupStack::Pop();
    CleanupStack::PopAndDestroy(path);
    return voice;
    }

void CAudio::StartVoice(const CAudioVoice* aSample, TInt aVolume, TInt aRate)
    {
    if (!aSample || aSample->iLength <= 0 || !aSample->iData)
        return;

    TInt slot = -1;
    TInt oldest = -1;
    TInt largestPosition = -1;
    for (TInt i = 0; i < KMaxVoices; i++)
        {
        if (!iVoices[i].iActive)
            {
            slot = i;
            break;
            }
        if (iVoices[i].iPosition > largestPosition)
            {
            largestPosition = iVoices[i].iPosition;
            oldest = i;
            }
        }

    if (slot < 0)
        {
        if (oldest >= 0)
            slot = oldest;
        else
            {
            slot = iNextVoice % KMaxVoices;
            iNextVoice++;
            }
        }

    TInt volume = aVolume;
    if (volume < 0)
        volume = 0;
    if (volume > 100)
        volume = 100;
    volume = volume * iMasterVolume / 100;
    volume = volume * 190 / 100;

    TInt sourceRate = aRate > 0 ? aRate : aSample->iRate;
    if (sourceRate <= 0)
        sourceRate = KOutputRate;

    iVoices[slot].iData = aSample->iData;
    iVoices[slot].iLength = aSample->iLength;
    iVoices[slot].iRate = sourceRate;
    iVoices[slot].iStep = (TInt)(((TInt64)sourceRate << 16) / (TInt64)KOutputRate);
    iVoices[slot].iPosition = 0;
    iVoices[slot].iVolume = volume;
    iVoices[slot].iActive = ETrue;
    }

TBool CAudio::Play(const TPtrC8& aName, TInt aVolume)
    {
    TInt error = KErrNone;
    CAudioVoice* voice = NULL;
    TRAPD(loadError, voice = LoadWaveL(aName));
    error = loadError;
    if (error != KErrNone)
        {
        iOutputError = error;
        iObserver.AudioError(error);
        return EFalse;
        }
    if (!voice)
        return EFalse;

    StartVoice(voice, aVolume, voice->iRate);
    return ETrue;
    }

void CAudio::PlayPcm(const TUint8* aPcm, TInt aLength, TInt aRate, TInt aVolume)
    {
    if (!aPcm || aLength <= 0 || aRate <= 0)
        return;

    const TInt maxSamples = KMaxRecorded / (TInt)sizeof(TUint16);
    TInt samples = aLength / (TInt)sizeof(TUint16);
    if (samples > maxSamples)
        samples = maxSamples;
    if (samples <= 0)
        return;

    TUint16* destination = reinterpret_cast<TUint16*>(iRecorded);
    for (TInt i = 0; i < samples; i++)
        destination[i] = (TUint16)(TInt16)ReadLe16(aPcm + i * (TInt)sizeof(TUint16));

    CAudioVoice voice;
    voice.iData = destination;
    voice.iLength = samples;
    voice.iRate = aRate;
    voice.iRaw = const_cast<TUint8*>(aPcm);
    voice.iRawLength = aLength;
    StartVoice(&voice, aVolume, aRate);
    }

void CAudio::StopAll()
    {
    for (TInt i = 0; i < KMaxVoices; i++)
        {
        iVoices[i].iActive = EFalse;
        iVoices[i].iPosition = 0;
        }
    }

void CAudio::SetMasterVolume(TInt aVolume)
    {
    if (aVolume < 0)
        aVolume = 0;
    if (aVolume > 100)
        aVolume = 100;
    iMasterVolume = aVolume;

    if (iOutputStream && iStreamOpen)
        {
        const TInt maximum = iOutputStream->MaxVolume();
        iOutputStream->SetVolume(maximum > 0 ? maximum * aVolume / 100 : 0);
        }
    }

TBool CAudio::StartRecording()
    {
    if (iRecording)
        return ETrue;

    iInputError = KErrNone;
    const TUint8* scratchStart = iRecorded;
    const TUint8* scratchEnd = scratchStart + KMaxRecorded;
    for (TInt i = 0; i < KMaxVoices; i++)
        {
        const TUint8* data = reinterpret_cast<const TUint8*>(iVoices[i].iData);
        if (iVoices[i].iActive && data >= scratchStart && data < scratchEnd)
            iVoices[i].iActive = EFalse;
        }

    iRecordedLength = 0;

    if (!iInputStream)
        {
        TInt error = KErrNone;
        TRAPD(createError, iInputStream = CMdaAudioInputStream::NewL(*this));
        error = createError;
        if (error != KErrNone || !iInputStream)
            {
            iInputStream = NULL;
            iInputError = error != KErrNone ? error : KErrNotReady;
            iObserver.AudioError(iInputError);
            return EFalse;
            }
        }

    TMdaAudioDataSettings settings;
    settings.iCaps = 0;
    settings.iVolume = 0;
    settings.iMaxVolume = 0;
    settings.iSampleRate = TMdaAudioDataSettings::ESampleRate8000Hz;
    settings.iChannels = TMdaAudioDataSettings::EChannelsMono;
    iRecording = ETrue;
    iInputStream->Open(&settings);
    return ETrue;
    }

void CAudio::StopRecording()
    {
    if (!iRecording)
        return;

    iRecording = EFalse;
    if (iRecordedLength > 0)
        {
        iObserver.VoiceRecorded(iRecorded, iRecordedLength);
        iRecordedLength = 0;
        }
    if (iInputStream)
        iInputStream->Stop();
    }

void CAudio::MixBlock(TUint8* aDestination)
    {
    for (TInt i = 0; i < KBlockFrames; i++)
        iMixer[i] = 0;

    for (TInt voice = 0; voice < KMaxVoices; voice++)
        {
        if (!iVoices[voice].iActive)
            continue;

        const TUint16* data = iVoices[voice].iData;
        const TInt length = iVoices[voice].iLength;
        if (!data || length <= 0)
            {
            iVoices[voice].iActive = EFalse;
            continue;
            }

        TInt position = iVoices[voice].iPosition;
        const TInt step = iVoices[voice].iStep;
        const TInt volume = iVoices[voice].iVolume;
        TBool done = EFalse;

        for (TInt i = 0; i < KBlockFrames; i++)
            {
            const TInt index = position >> 16;
            if (index >= length)
                {
                done = ETrue;
                break;
                }
            const TInt a = (TInt)(TInt16)data[index];
            const TInt b = index + 1 < length ? (TInt)(TInt16)data[index + 1] : a;
            const TInt fraction = position & 0xFFFF;
            /* Java int multiplication wraps on 32 bits; make that explicit. */
            const TInt product = (TInt)((TUint32)(b - a) * (TUint32)fraction);
            iMixer[i] += ((a + (product >> 16)) * volume) >> 8;
            position += step;
            }

        iVoices[voice].iPosition = position;
        if (done)
            iVoices[voice].iActive = EFalse;
        }

    for (TInt i = 0; i < KBlockFrames; i++)
        {
        TInt sample = iMixer[i];
        if (sample > 32767)
            sample = 32767;
        else if (sample < -32768)
            sample = -32768;
        iMixedBlock[i] = (TUint8)((sample >> 8) + 128);
        }

    Mem::Copy(aDestination, iMixedBlock, KBlockFrames);
    }

void CAudio::RequestNextBlock()
    {
    if (!iOutputStream || !iStreamOpen || !iOutputBuffer || iOutputError != KErrNone)
        return;

    MixBlock(const_cast<TUint8*>(iOutputBuffer->Des().Ptr()));

    TInt error = KErrNone;
    TRAPD(writeError, iOutputStream->WriteL(*iOutputBuffer));
    error = writeError;
    if (error != KErrNone)
        {
        iOutputError = error;
        iStreamOpen = EFalse;
        iObserver.AudioError(error);
        }
    }

void CAudio::MaoscOpenComplete(TInt aError)
    {
    if (aError != KErrNone)
        {
        iStreamOpen = EFalse;
        iOutputError = aError;
        iObserver.AudioError(aError);
        return;
        }

    iOutputError = KErrNone;
    iStreamOpen = ETrue;

    TInt error = KErrNone;
    TRAPD(propertyError, iOutputStream->SetAudioPropertiesL(
        TMdaAudioDataSettings::ESampleRate11025Hz,
        TMdaAudioDataSettings::EChannelsMono));
    error = propertyError;
    if (error == KErrNone)
        {
        TRAPD(typeError, iOutputStream->SetDataTypeL(KMMFFourCCCodePCMU8));
        error = typeError;
        }
    if (error != KErrNone)
        {
        iOutputError = error;
        iStreamOpen = EFalse;
        iObserver.AudioError(error);
        return;
        }

    SetMasterVolume(iMasterVolume);
    RequestNextBlock();
    }

void CAudio::MaoscBufferCopied(TInt aError, const TDesC8& /*aBuffer*/)
    {
    if (aError == KErrAbort)
        {
        iStreamOpen = EFalse;
        return;
        }
    if (aError != KErrNone)
        {
        iStreamOpen = EFalse;
        iOutputError = aError;
        iObserver.AudioError(aError);
        return;
        }
    RequestNextBlock();
    }

void CAudio::MaoscPlayComplete(TInt aError)
    {
    iStreamOpen = EFalse;
    if (aError == KErrNone || aError == KErrUnderflow || aError == KErrEof)
        {
        iOutputError = KErrNone;
        iStreamOpen = ETrue;
        RequestNextBlock();
        }
    else if (aError != KErrAbort)
        {
        iOutputError = aError;
        iObserver.AudioError(aError);
        }
    }

void CAudio::MaiscOpenComplete(TInt aError)
    {
    if (aError != KErrNone)
        {
        iRecording = EFalse;
        iInputError = aError;
        iObserver.AudioError(aError);
        return;
        }

    TInt error = KErrNone;
    TRAPD(propertyError, iInputStream->SetAudioPropertiesL(
        TMdaAudioDataSettings::ESampleRate8000Hz,
        TMdaAudioDataSettings::EChannelsMono));
    error = propertyError;
    if (error != KErrNone)
        {
        iInputError = error;
        iObserver.AudioError(error);
        error = KErrNone;
        }

    TRAPD(typeError, iInputStream->SetDataTypeL(KMMFFourCCCodePCM16));
    if (typeError != KErrNone)
        {
        iInputError = typeError;
        iObserver.AudioError(typeError);
        }

    iInputStream->SetGain(iInputStream->MaxGain());

    TPtr8 buffer = iInputBuffer->Des();
    TRAPD(readError, iInputStream->ReadL(buffer));
    if (readError != KErrNone)
        {
        iRecording = EFalse;
        iInputError = readError;
        iObserver.AudioError(readError);
        }
    }

void CAudio::MaiscBufferCopied(TInt aError, const TDesC8& aBuffer)
    {
    if (iRecording && iRecordedLength < KMaxRecorded)
        {
        TInt count = aBuffer.Length();
        if (count > KMaxRecorded - iRecordedLength)
            count = KMaxRecorded - iRecordedLength;
        Mem::Copy(iRecorded + iRecordedLength, aBuffer.Ptr(), count);
        iRecordedLength += count;
        }

    if (aError != KErrNone && aError != KErrAbort)
        {
        iRecording = EFalse;
        iInputError = aError;
        iObserver.AudioError(aError);
        return;
        }
    if (aError == KErrAbort)
        return;

    if (iRecordedLength >= KMaxRecorded)
        {
        iInputStream->Stop();
        return;
        }

    TPtr8 buffer = iInputBuffer->Des();
    TRAPD(readError, iInputStream->ReadL(buffer));
    if (readError != KErrNone)
        {
        iRecording = EFalse;
        iInputError = readError;
        iObserver.AudioError(readError);
        }
    }

void CAudio::SetModDirectory(const TDesC& aModDir)
    {
    delete iModDirectoryBuffer;
    iModDirectoryBuffer = NULL;
    if (aModDir.Length() == 0) { iModDirectory.Set(KNullDesC); return; }
    iModDirectoryBuffer = aModDir.Alloc();
    if (iModDirectoryBuffer) iModDirectory.Set(*iModDirectoryBuffer);
    }

void CAudio::MaiscRecordComplete(TInt aError)
    {
    const TBool deliver = iRecording;
    iRecording = EFalse;

    if (deliver && iRecordedLength > 0)
        iObserver.VoiceRecorded(iRecorded, iRecordedLength);
    iRecordedLength = 0;

    if (aError != KErrNone && aError != KErrAbort)
        {
        iInputError = aError;
        iObserver.AudioError(aError);
        }
    }
