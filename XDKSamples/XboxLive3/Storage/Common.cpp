//-------------------------------------------------------------------------------------
// File: Common.cpp
//
// Desc: Holds common utility functions used by the Storage sample.
//
// Hist: 08.10.04 - New for Sept release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include <xtl.h>
#include <xonline.h>
#include <assert.h>
#include <XBOnlineTask.h>
#include "Common.h"


//-------------------------------------------------------------------------------------
// Name: BootToDash()
// Desc: Boot back into either the main or online dashes
//-------------------------------------------------------------------------------------
VOID BootToDash( DWORD dwReason )
{
    LD_LAUNCH_DASHBOARD ld;
    ZeroMemory( &ld, sizeof(ld) );
    ld.dwReason = dwReason;
    XLaunchNewImage( NULL, PLAUNCH_DATA( &ld ) );

    // XLaunchNewImage should never return
    assert( FALSE );
}


//-------------------------------------------------------------------------------------
// Name: WaitForTaskToComplete()
// Desc: Helper function for waiting for a task to complete
//       In a real game, this would be part of your game loop
//       Returns TRUE for success, FALSE for failure.
//       Optionally returns the results code in pHR
//-------------------------------------------------------------------------------------
BOOL WaitForTaskToComplete( CXBOnlineTask& Task, HRESULT* pHR )
{
    assert( pHR );
    HRESULT hrTask = XONLINETASK_S_RUNNING;

    while( hrTask == XONLINETASK_S_RUNNING )
    {
        // In a real game, this would be part of your game loop-
        // you wouldn't block on this.
        // The logon task should also be pumped inside this loop.

        hrTask = Task.Continue();
        if( FAILED( hrTask ))
        {
            *pHR = hrTask;
            return FALSE;
        }
        
        // put in a delay of about 1 frame so as not to
        // spam with MessageSendGetProgress
        Sleep( 15 );
    }
    
    if( hrTask != XONLINETASK_S_SUCCESS )
    {
        *pHR = hrTask;
        return FALSE;
    }

    *pHR = S_OK;
    return TRUE;
}