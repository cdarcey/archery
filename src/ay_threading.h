

#ifndef AY_THREADING_H
#define AY_THREADING_H

#include <stdint.h> // uint

//-----------------------------------------------------------------------------
// [SECTION] forward declarations (opaque types)
//-----------------------------------------------------------------------------

typedef struct _ayThread            ayThread;
typedef struct _ayMutex             ayMutex;
typedef struct _ayAtomicCounter     ayAtomicCounter;
typedef struct _ayCondition         ayCondition;
typedef struct _ayConditionVariable ayConditionVariable; 
typedef struct _ayCriticalSection   ayCriticalSection;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

typedef enum _ayThreadResult
{
    AY_THREAD_RESULT_FAIL    = 0,
    AY_THREAD_RESULT_SUCCESS = 1
} ayThreadResult;

typedef enum _ayAtomicsResult
{
    AY_ATOMICS_RESULT_FAIL    = 0,
    AY_ATOMICS_RESULT_SUCCESS = 1
} ayAtomicsResult;

//-----------------------------------------------------------------------------
// [SECTION] function pointer types
//-----------------------------------------------------------------------------

typedef void* (*ayThreadProcedure)(void*);

//-----------------------------------------------------------------------------
// [SECTION] thread api
//-----------------------------------------------------------------------------

ayThreadResult ay_create_thread(ayThreadProcedure ptProcedure, void* pData, ayThread** ppThreadOut);
void           ay_join_thread(ayThread* ptThread);
void           ay_destroy_thread(ayThread** ppThread);
void           ay_yield_thread(void);

//-----------------------------------------------------------------------------
// [SECTION] mutex api
//-----------------------------------------------------------------------------

ayThreadResult ay_create_mutex(ayMutex** ppMutexOut);
void           ay_destroy_mutex(ayMutex** ppMutex);
void           ay_lock_mutex(ayMutex* ptMutex);
void           ay_unlock_mutex(ayMutex* ptMutex);

//-----------------------------------------------------------------------------
// [SECTION] atomic api
//-----------------------------------------------------------------------------

ayAtomicsResult ay_create_atomic_counter(int64_t iValue, ayAtomicCounter** ppCounterOut);
void            ay_destroy_atomic_counter(ayAtomicCounter** ppCounter);
int64_t         ay_atomic_increment(ayAtomicCounter* ptCounter);
int64_t         ay_atomic_fetch_add(ayAtomicCounter* ptCounter, int64_t iValue);
int64_t         ay_atomic_load(ayAtomicCounter* ptCounter);

//-----------------------------------------------------------------------------
// [SECTION] conditional variable api
//-----------------------------------------------------------------------------

ayThreadResult ay_create_condition(ayConditionVariable** ppCondOut);
void           ay_destroy_condition(ayConditionVariable** ppCond);
void           ay_condition_wait(ayConditionVariable* ptCond, ayCriticalSection* ptCriticalSection);
void           ay_condition_signal(ayConditionVariable* ptCond);
void           ay_condition_broadcast(ayConditionVariable* ptCond);

//-----------------------------------------------------------------------------
// [SECTION] critical section api
//-----------------------------------------------------------------------------

ayThreadResult ay_create_critical_section(ayCriticalSection** ppCriticalSectionOut);
void ay_destroy_critical_section(ayCriticalSection** ppCriticalSection);
void ay_enter_critical_section(ayCriticalSection* ptCriticalSection);
void ay_leave_critical_section(ayCriticalSection* ptCriticalSection);

//-----------------------------------------------------------------------------
// [SECTION] utility api
//-----------------------------------------------------------------------------

void ay_sleep(uint32_t uMilliseconds);

#endif // AY_THREADING_H