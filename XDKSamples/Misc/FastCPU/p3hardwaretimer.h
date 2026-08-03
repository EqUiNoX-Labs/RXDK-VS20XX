//-----------------------------------------------------------------------------
// File: P3HarwareTimer.h
//
// Desc: P3 Hardware timer control
//
// Hist: 1.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef P3_HARDWARE_TIMER
#define P3_HARDWARE_TIMER

#include <assert.h>
#include <xtl.h>
#include "P3HardwareCounters.h"


//-----------------------------------------------------------------------------
// Name: P3Event
// Desc: An event sampling
//-----------------------------------------------------------------------------
struct P3EventSample
{
    const char* szName;
    __int64     Cycles;
    __int64     Counters[2];
};


//-----------------------------------------------------------------------------
// Maximum number of Event Samplings and reports
//-----------------------------------------------------------------------------
#define MAX_EVENT_SAMPLES 200
#define MAX_REPORTS (MAX_EVENT_SAMPLES/2)


//-----------------------------------------------------------------------------
// Name: P3HardwareTimer
// Desc: A class that handles recording and summarizing P3 timing events
//-----------------------------------------------------------------------------
class P3HardwareTimer
{
public:

    // constructor
                         P3HardwareTimer();

    // set events to be timed
    VOID                 SetEvents( P3Event Event0, P3Event Event1 );
    
    // start and stop timing
    VOID                 StartTiming( BOOL bDisableInterrupts );
    VOID                 StopTiming();

    // start and stop a timer
    VOID                 StartTimer( const CHAR* szName );
    VOID                 StopTimer( const CHAR* szName );
    
    // get the name of a specific counter
    const char*          GetCounterName( UINT uiEvent ) const;
    
    // get report information
    UINT                 GetNumReports() const;
    const P3EventSample* GetReport( UINT uiReport ) const;
    
private:
    P3EventSample        m_EventSamples[MAX_EVENT_SAMPLES];
    UINT                 m_uiEventSampleIndex;
    P3EventSample        m_Reports[MAX_REPORTS];
    UINT                 m_uiReportIndex;
    BOOL                 m_bInterruptsDisabled; // interrupts disabled
    P3Event              m_Events[2];
    __int64              m_CalibrationCycles;
    __int64              m_CalibrationCounters[2];
    
    // build reports
    VOID                 MakeReports();
    
    // adds an event sampling
   VOID                  AddSample( const CHAR* szName );
};


//-----------------------------------------------------------------------------
// global pointer to the timer class
//-----------------------------------------------------------------------------
extern P3HardwareTimer* g_pP3Timer;


//-----------------------------------------------------------------------------
// Name: InitP3HardwareTimer
// Desc: Initializes a global timer in non-cached memory
//-----------------------------------------------------------------------------
VOID InitP3HardwareTimer();




//-----------------------------------------------------------------------------
// inlines
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Name: StartTimer
// Desc: Starts timing a section
//-----------------------------------------------------------------------------
VOID __forceinline P3HardwareTimer::StartTimer( const CHAR* szName )
{
    AddSample( szName );
}


//-----------------------------------------------------------------------------
// Name: P3StopTimer
// Desc: Stops timing a section
//-----------------------------------------------------------------------------
VOID __forceinline P3HardwareTimer::StopTimer( const CHAR* szName )
{
    AddSample( szName );
}


//-----------------------------------------------------------------------------
// Name: AddSample
// Desc: Adds a timing sample to the array of timing samples
//-----------------------------------------------------------------------------
VOID __forceinline P3HardwareTimer::AddSample( const CHAR* szName )
{
    assert( m_uiEventSampleIndex < MAX_EVENT_SAMPLES );
    m_EventSamples[m_uiEventSampleIndex].szName = szName; 
    
    m_EventSamples[m_uiEventSampleIndex].Cycles = P3GetCycles();
    m_EventSamples[m_uiEventSampleIndex].Counters[0] = P3GetCounter( 0 );
    m_EventSamples[m_uiEventSampleIndex].Counters[1] = P3GetCounter( 1 );

    m_uiEventSampleIndex++;
}


#endif // P3_HARDWARE_TIMER