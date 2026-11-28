#ifndef CS16_STUB_EIKCTL_H
#define CS16_STUB_EIKCTL_H

#include "e32std.h"
#include "e32base.h"
#include "eikcore.h"

class CEikMenuPane : public CBase
    {
public:
    TInt AddMenuItemL(TInt aCommandId, const TDesC& aText)
        { (void)aCommandId; (void)aText; return KErrNone; }
    void SetItemDimmed(TInt aCommandId, TBool aDimmed)
        { (void)aCommandId; (void)aDimmed; }
    };

#endif
