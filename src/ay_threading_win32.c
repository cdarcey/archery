

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "ay_threading.h"
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>

//-----------------------------------------------------------------------------
// [SECTION] internal structs (platform-specific implementation)
//-----------------------------------------------------------------------------

typedef struct _ayThreadData
{
	ayThreadProcedure ptProcedure;
	void*             pData;
} ayThreadData;

struct _ayThread
{
	HANDLE        tHandle;
	ayThreadData* ptData;
	uint64_t      uID;
};

struct _ayMutex
{
	HANDLE tHandle;
};

struct _ayAtomicCounter
{
	volatile LONG64 iValue;
};

struct _ayConditionVariable
{
	CONDITION_VARIABLE tHandle;
};

struct _ayCriticalSection
{
    CRITICAL_SECTION tHandle;
};


//-----------------------------------------------------------------------------
// [SECTION] internal helpers
//-----------------------------------------------------------------------------

static DWORD WINAPI
thread_procedure_wrapper(LPVOID lpParam)
{
	ayThreadData* ptData = (ayThreadData*)lpParam;
	ptData->ptProcedure(ptData->pData);
	return 0;
}

//-----------------------------------------------------------------------------
// [SECTION] thread implementation
//-----------------------------------------------------------------------------

ayThreadResult
ay_create_thread(ayThreadProcedure ptProcedure, void* pData, ayThread** ppThreadOut)
{
    ayThreadData* ptData = (ayThreadData*)malloc(sizeof(ayThreadData));
	if(!ptData) return AY_THREAD_RESULT_FAIL;
		
	ptData->ptProcedure = ptProcedure;
	ptData->pData = pData;

	HANDLE tHandle = CreateThread(NULL, 0, thread_procedure_wrapper, ptData, 0, NULL);
	if(!tHandle)
	{
		free(ptData);
		return AY_THREAD_RESULT_FAIL;
	}

	DWORD tID = GetThreadId(tHandle);
		
	*ppThreadOut = (ayThread*)malloc(sizeof(ayThread));
	(*ppThreadOut)->ptData = ptData;
	(*ppThreadOut)->tHandle = tHandle;
	(*ppThreadOut)->uID = (uint64_t)tID;
		
	return AY_THREAD_RESULT_SUCCESS;
}

void
ay_join_thread(ayThread* ptThread)
{
  	WaitForSingleObject(ptThread->tHandle, INFINITE);
}

void
ay_destroy_thread(ayThread** ppThread)
{
	if(!ppThread || !*ppThread) return;
		
	ay_join_thread(*ppThread);
	CloseHandle((*ppThread)->tHandle);
	free((*ppThread)->ptData);
	free(*ppThread);
	*ppThread = NULL;
}

void
ay_yield_thread(void)
{
	SwitchToThread();
}

//-----------------------------------------------------------------------------
// [SECTION] mutex implementation
//-----------------------------------------------------------------------------

ayThreadResult
ay_create_mutex(ayMutex** ppMutexOut)
{
	HANDLE tHandle = CreateMutex(NULL, FALSE, NULL);
	if(!tHandle) return AY_THREAD_RESULT_FAIL;

	*ppMutexOut = (ayMutex*)malloc(sizeof(ayMutex));
	(*ppMutexOut)->tHandle = tHandle;
		
	return AY_THREAD_RESULT_SUCCESS;
}

void
ay_destroy_mutex(ayMutex** ppMutex)
{
	if(!ppMutex || !*ppMutex) return;
		
	CloseHandle((*ppMutex)->tHandle);
	free(*ppMutex);
	*ppMutex = NULL;
}

void
ay_lock_mutex(ayMutex* ptMutex)
{
	DWORD dwWaitResult = WaitForSingleObject(ptMutex->tHandle, INFINITE);
	// TODO: add proper assert
	if(dwWaitResult != WAIT_OBJECT_0)
	{
		printf("Mutex lock failed: %lu\n", GetLastError());
	}
}

void
ay_unlock_mutex(ayMutex* ptMutex)
{
	if(!ReleaseMutex(ptMutex->tHandle))
	{
		printf("ReleaseMutex error: %lu\n", GetLastError());
	}
}

//-----------------------------------------------------------------------------
// [SECTION] atomic implementation
//-----------------------------------------------------------------------------

ayAtomicsResult
ay_create_atomic_counter(int64_t iValue, ayAtomicCounter** ppCounterOut)
{
	*ppCounterOut = (ayAtomicCounter*)malloc(sizeof(ayAtomicCounter));
	if(!*ppCounterOut) return AY_ATOMICS_RESULT_FAIL;
		
	(*ppCounterOut)->iValue = iValue;
	return AY_ATOMICS_RESULT_SUCCESS;
}

void
ay_destroy_atomic_counter(ayAtomicCounter** ppCounter)
{
	if(!ppCounter || !*ppCounter) return;
		
	free(*ppCounter);
	*ppCounter = NULL;
}

int64_t
ay_atomic_increment(ayAtomicCounter* ptCounter)
{
	return InterlockedIncrement64(&ptCounter->iValue);
}

int64_t
ay_atomic_fetch_add(ayAtomicCounter* ptCounter, int64_t iValue)
{
	return InterlockedExchangeAdd64(&ptCounter->iValue, iValue);
}

int64_t
ay_atomic_load(ayAtomicCounter* ptCounter)
{
	return InterlockedCompareExchange64(&ptCounter->iValue, 0, 0);
}

//-----------------------------------------------------------------------------
// [SECTION] conditionals implementation
//-----------------------------------------------------------------------------

ayThreadResult 
ay_create_condition(ayConditionVariable** pptConditionVariableOut)
{
	*pptConditionVariableOut = (ayConditionVariable*)malloc(sizeof(ayConditionVariable));
	InitializeConditionVariable(&(*pptConditionVariableOut)->tHandle);
	return AY_THREAD_RESULT_SUCCESS;
}

void 
ay_destroy_condition(ayConditionVariable** pptConditionVariable)
{
	free((*pptConditionVariable));
	*pptConditionVariable = NULL;
}

void 
ay_condition_wait(ayConditionVariable* ptConditionVariable, ayCriticalSection* ptCriticalSection)
{
	SleepConditionVariableCS(&ptConditionVariable->tHandle, &ptCriticalSection->tHandle, INFINITE);
}

void 
ay_condition_signal(ayConditionVariable* ptConditionVariable)
{
	WakeConditionVariable(&ptConditionVariable->tHandle);
}

void 
ay_condition_broadcast(ayConditionVariable* ptConditionVariable)
{
	WakeAllConditionVariable(&ptConditionVariable->tHandle);
}

//-----------------------------------------------------------------------------
// [SECTION] critical section implementation
//-----------------------------------------------------------------------------

ayThreadResult
ay_create_critical_section(ayCriticalSection** ppCriticalSectionOut)
{
    *ppCriticalSectionOut = (ayCriticalSection*)malloc(sizeof(ayCriticalSection));
    if(!*ppCriticalSectionOut) return AY_THREAD_RESULT_FAIL;
    
    InitializeCriticalSection(&(*ppCriticalSectionOut)->tHandle);
    return AY_THREAD_RESULT_SUCCESS;
}

void
ay_destroy_critical_section(ayCriticalSection** ppCriticalSection)
{
    if(!ppCriticalSection || !*ppCriticalSection) return;
    
    DeleteCriticalSection(&(*ppCriticalSection)->tHandle);
    free(*ppCriticalSection);
    *ppCriticalSection = NULL;
}

void
ay_enter_critical_section(ayCriticalSection* ptCriticalSection)
{
    EnterCriticalSection(&ptCriticalSection->tHandle);
}

void
ay_leave_critical_section(ayCriticalSection* ptCriticalSection)
{
    LeaveCriticalSection(&ptCriticalSection->tHandle);
}

//-----------------------------------------------------------------------------
// [SECTION] utility implementation
//-----------------------------------------------------------------------------

void
ay_sleep(uint32_t uMilliseconds)
{
	Sleep((DWORD)uMilliseconds);
}