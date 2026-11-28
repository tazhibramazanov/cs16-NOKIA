#ifndef CS16_STUB_MDAAUDIOINPUTSTREAM_H
#define CS16_STUB_MDAAUDIOINPUTSTREAM_H

#include "e32std.h"
#include "e32base.h"
#include "mda/common/audio.h"

class MMdaAudioInputStreamCallback
    {
public:
    virtual ~MMdaAudioInputStreamCallback() {}
    virtual void MaiscOpenComplete(TInt aError) = 0;
    virtual void MaiscBufferCopied(TInt aError, const TDesC8& aBuffer) = 0;
    virtual void MaiscRecordComplete(TInt aError) = 0;
    };

class CMdaAudioInputStream : public CBase
    {
public:
    static CMdaAudioInputStream* NewL(MMdaAudioInputStreamCallback& aCallback)
        { (void)aCallback; return new (ELeave) CMdaAudioInputStream(); }
    static CMdaAudioInputStream* NewLC(MMdaAudioInputStreamCallback& aCallback)
        { return NewL(aCallback); }
    void Open(TMdaAudioDataSettings* aSettings) { (void)aSettings; }
    void ReadL(TDes8& aBuffer) { (void)aBuffer; }
    void SetAudioPropertiesL(TInt aSampleRate, TInt aChannels)
        { (void)aSampleRate; (void)aChannels; }
    void SetDataTypeL(TUint32 aFourCC) { (void)aFourCC; }
    void SetGain(TInt aGain) { (void)aGain; }
    TInt MaxGain() const { return 100; }
    void Stop() {}
    TInt Record() { return KErrNone; }
private:
    CMdaAudioInputStream() {}
    };

#endif
