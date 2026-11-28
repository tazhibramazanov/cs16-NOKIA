#ifndef CS16_STUB_E32BASE_H
#define CS16_STUB_E32BASE_H

#include "e32std.h"

class CBase
    {
public:
    CBase() {}
    virtual ~CBase() {}
private:
    CBase(const CBase&);
    CBase& operator=(const CBase&);
    };

enum TPriority
    {
    EPriorityIdle = -100,
    EPriorityLow = -20,
    EPriorityStandard = 0,
    EPriorityUserInput = 10,
    EPriorityHigh = 20,
    EPriorityMuchMore = 100
    };

class TRequestStatus;

class CActive : public CBase
    {
public:
    CActive(TPriority) : iActive(EFalse) {}
    virtual void RunL() = 0;
    virtual void DoCancel() = 0;
    virtual TInt RunError(TInt aError) { return aError; }
    void SetActive() { iActive = ETrue; }
    TBool IsActive() const { return iActive; }
    void Cancel() { DoCancel(); }
    TRequestStatus& Status() { return iStatus; }
protected:
    TRequestStatus iStatus;
private:
    TBool iActive;
    };

class CActiveScheduler
    {
public:
    static void Add(CActive*) {}
    static void Start() {}
    static void Stop() {}
    };

class RTimer
    {
public:
    RTimer() : iHandle(0) {}
    void CreateL() { iHandle = 1; }
    void CreateLocalL() { iHandle = 1; }
    void CreateLocal() { iHandle = 1; }
    void HighRes() {}
    void After(TRequestStatus&, TInt) {}
    void Cancel() {}
    void Close() { iHandle = 0; }
    TInt Handle() const { return iHandle; }
private:
    TInt iHandle;
    };

#endif
