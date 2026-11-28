#ifndef CS16_STUB_MDAAUDIOOUTPUTSTREAM_H
#define CS16_STUB_MDAAUDIOOUTPUTSTREAM_H

#include "e32std.h"
#include "e32base.h"
#include "mda/common/audio.h"

class MMdaAudioOutputStreamCallback
    {
public:
    virtual ~MMdaAudioOutputStreamCallback() {}
    virtual void MaoscOpenComplete(TInt aError) = 0;
    virtual void MaoscBufferCopied(TInt aError, const TDesC8& aBuffer) = 0;
    virtual void MaoscPlayComplete(TInt aError) = 0;
    };

class CMdaAudioOutputStream : public CBase
    {
public:
    static CMdaAudioOutputStream* NewL(MMdaAudioOutputStreamCallback& aCallback)
        { (void)aCallback; return new (ELeave) CMdaAudioOutputStream(); }
    static CMdaAudioOutputStream* NewLC(MMdaAudioOutputStreamCallback& aCallback)
        { return NewL(aCallback); }
    void Open(TMdaAudioDataSettings* aSettings) { (void)aSettings; }
    void WriteL(const TDesC8& aBuffer) { (void)aBuffer; }
    void SetAudioPropertiesL(TInt aSampleRate, TInt aChannels)
        { (void)aSampleRate; (void)aChannels; }
    void SetDataTypeL(TUint32 aFourCC) { (void)aFourCC; }
    void SetVolume(TInt aVolume) { (void)aVolume; }
    TInt MaxVolume() const { return 100; }
    void Stop() {}
    void Start() {}
    TInt Play() { return KErrNone; }
private:
    CMdaAudioOutputStream() {}
    };

#endif
