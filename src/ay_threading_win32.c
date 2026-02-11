


//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------
#include <Windows.h>
#include "ay_threading.h"

// Research Order:

// Look at Win32 CreateThread docs
// Look at Win32 CRITICAL_SECTION docs
// Look at Win32 CONDITION_VARIABLE docs
// Think about how to map Win32 → archery API

// TODO: needs
//  -> create & destory thread
//  -> create & destroy mutex
//  -> InterlockedIncrement / InterlockedExchangeAdd (atomics)
//  -> signal object managment
//  -> critical sections & condition variable

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct  _ayThread
{
    int placeholder;
} ayThread;




// TODO: we need to learn to wrap every function so that api in ay_threading.h is platform agnostic
// create thread will need a wrapper, likely needs to be an opaque pointer with "create and destory"
// very simialar to ayGraphicsData in rasterize files

HANDLE CreateThread(
  [in, optional]  LPSECURITY_ATTRIBUTES   lpThreadAttributes, // not important for us
  [in]            SIZE_T                  dwStackSize,        // not important for us
  [in]            LPTHREAD_START_ROUTINE  lpStartAddress,
  [in, optional]  __drv_aliasesMem LPVOID lpParameter,
  [in]            DWORD                   dwCreationFlags,
  [out, optional] LPDWORD                 lpThreadId
);