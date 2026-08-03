//-----------------------------------------------------------------------------
// File: P3HardwareTimer.cpp
//
// Desc: Stuctures and functions to handle the P3 hardware timer
//
// Hist: 1.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "P3HardwareTimer.h"


//-----------------------------------------------------------------------------
// Name: P3HardwareTimer
// Desc: Constructor
//-----------------------------------------------------------------------------
P3HardwareTimer::P3HardwareTimer()
: m_uiEventSampleIndex(0),
  m_uiReportIndex(0),
  m_bInterruptsDisabled(FALSE)
{
    m_Events[0] = INST_RETIRED;
    m_Events[1] = RESOURCE_STALLS;
};


//-----------------------------------------------------------------------------
// Name: SetEvents
// Desc: Set event controls 1 and 0
//-----------------------------------------------------------------------------
VOID P3HardwareTimer::SetEvents( P3Event Event0, P3Event Event1 )
{
    assert( Event0 < P3EVENT_MAX );
    assert( Event1 < P3EVENT_MAX );
    assert( g_P3EventInfos[Event0].CountersAllowed  == 0x00 ||
            g_P3EventInfos[Event0].CountersAllowed  == 0x10 );
    assert( g_P3EventInfos[Event1].CountersAllowed  == 0x01 ||
            g_P3EventInfos[Event1].CountersAllowed  == 0x10 );
    
    m_Events[0] = Event0;
    m_Events[1] = Event1;
}


//-----------------------------------------------------------------------------
// Name: StartTiming
// Desc: Start recording events
//-----------------------------------------------------------------------------
VOID P3HardwareTimer::StartTiming( BOOL bDisableInterrupts )
{
    m_uiEventSampleIndex = 0;
        
    P3SetEventCtrlSimple( 0, m_Events[0] );
    P3SetEventCtrlSimple( 1, m_Events[1] );
    P3EnableCounters( TRUE );

    P3SetCounter( 0, 0 );
    P3SetCounter( 1, 0 );

    m_bInterruptsDisabled = bDisableInterrupts;
    if( m_bInterruptsDisabled )
        P3EnableInterrupts( FALSE );

    

    StartTimer( "Calibration" );
    StopTimer( "Calibration" );

    m_CalibrationCycles = m_EventSamples[1].Cycles - m_EventSamples[0].Cycles;
    m_CalibrationCounters[0] = m_EventSamples[1].Counters[0] - m_EventSamples[0].Counters[0];
    m_CalibrationCounters[1] = m_EventSamples[1].Counters[1] - m_EventSamples[0].Counters[1];

    m_uiEventSampleIndex = 0;
}


//-----------------------------------------------------------------------------
// Name: StopTiming
// Desc: Stop recording events
//-----------------------------------------------------------------------------
VOID P3HardwareTimer::StopTiming()
{
    if( m_bInterruptsDisabled )
    {
        P3EnableInterrupts( TRUE );
        m_bInterruptsDisabled = FALSE;
    }

    MakeReports();
}


//-----------------------------------------------------------------------------
// Name: GetCounterName
// Desc: Gets the name of a set event counter
//-----------------------------------------------------------------------------
const CHAR* P3HardwareTimer::GetCounterName( UINT uiEvent ) const
{
    assert( uiEvent < 2 );
    return g_P3EventInfos[m_Events[uiEvent]].szName;
}


//-----------------------------------------------------------------------------
// Name: GetNumReports
// Desc: Gets the current number of reports
//-----------------------------------------------------------------------------
UINT P3HardwareTimer::GetNumReports() const
{
    return m_uiReportIndex;
}


//-----------------------------------------------------------------------------
// Name: GetReport
// Desc: Gets a report given an index
//-----------------------------------------------------------------------------
const P3EventSample* P3HardwareTimer::GetReport( UINT uiReport ) const
{
    assert( uiReport < GetNumReports() );
    return &m_Reports[uiReport];
}


//-----------------------------------------------------------------------------
// Name: MakeReports
// Desc: builds a report after timing
//-----------------------------------------------------------------------------
VOID P3HardwareTimer::MakeReports()
{
    // must be an even number of event samples
    assert( !(m_uiEventSampleIndex & 0x00000001));

    m_uiReportIndex = 0;

    BOOL* pbFinished = new BOOL[m_uiEventSampleIndex];
    ZeroMemory(pbFinished, m_uiEventSampleIndex * sizeof(BOOL));

    for( UINT i = 0; i < m_uiEventSampleIndex; i++ )
    {
        if( !pbFinished[i] )
        {
            // find corresponing end EventSample
            int last = -1;
            for( UINT j = i+1; j < m_uiEventSampleIndex; j++ )
            {
                if( !pbFinished[j] &&
                    strcmp( m_EventSamples[i].szName,
                            m_EventSamples[j].szName) == 0)
                {
                    last = j;
                }
            }
            assert( last != -1);

            assert( m_uiReportIndex < MAX_REPORTS );

            m_Reports[m_uiReportIndex].szName = m_EventSamples[i].szName;
            m_Reports[m_uiReportIndex].Cycles =
                m_EventSamples[last].Cycles - m_EventSamples[i].Cycles
                    - m_CalibrationCycles;
            m_Reports[m_uiReportIndex].Counters[0] =
                m_EventSamples[last].Counters[0] - m_EventSamples[i].Counters[0]
                    - m_CalibrationCounters[0];
            m_Reports[m_uiReportIndex].Counters[1] =
                m_EventSamples[last].Counters[1] - m_EventSamples[i].Counters[1]
                    - m_CalibrationCounters[1];

            m_uiReportIndex++;
            

            pbFinished[i] = TRUE;
            pbFinished[last] = TRUE;
        }
    }

    delete [] pbFinished;
}


//-----------------------------------------------------------------------------
// global pointer to the the timer class
//-----------------------------------------------------------------------------
P3HardwareTimer* g_pP3Timer = NULL;


//-----------------------------------------------------------------------------
// Name: InitP3HardwareTimer
// Desc: Initializes a global timer in non-cached memory
//-----------------------------------------------------------------------------
VOID InitP3HardwareTimer()
{
    assert( g_pP3Timer == NULL );
    g_pP3Timer =
        (P3HardwareTimer*)XPhysicalAlloc( sizeof(P3HardwareTimer),
                                          MAXULONG_PTR, 0,
                                          PAGE_READWRITE | PAGE_NOCACHE );
}

