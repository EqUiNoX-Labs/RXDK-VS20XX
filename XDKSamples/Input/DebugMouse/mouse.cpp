//-----------------------------------------------------------------------------
// File: Mouse.cpp
//
// Desc: Handles debug mouse.
//
// Hist: 04.07.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "mouse.h"


// Stores the current mask of all mice plugged in.
static DWORD                    g_dwMousePort           = 0;

// Stores the handles for all mice plugged in.
static HANDLE                   g_hMouseDevice[4]       = { 0, 0, 0, 0 };

// Stores the last packet number for each mouse.  
// We use this to know if the mouse data has changed.
static DWORD                    g_dwLastMousePacket[4]  = { 0, 0, 0, 0 };

// Stores tha actual mouse data.
XINPUT_STATE    g_MouseState[4];




//-----------------------------------------------------------------------------
// Name: XBInput_InitDebugMouse()
// Desc: Initialize Debug Mouse for use
//-----------------------------------------------------------------------------
HRESULT XBInput_InitDebugMouse()
{
    g_dwMousePort = XGetDevices( XDEVICE_TYPE_DEBUG_MOUSE );

    // We obtain the actual handle(s) below in the 
    // XInput_GetMouseInput function.

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: XBInput_GetMouseInput()
// Desc: Processes input from a debug mouse
//       Returns TRUE if ANY mouse that is plugged in changed state
//-----------------------------------------------------------------------------
DWORD XBInput_GetMouseInput()
{
    // See if a mouse is attached and get a handle to it, if it is.
    for( DWORD i=0; i < XGetPortCount(); i++ )
    {
        if( ( g_hMouseDevice[i] == NULL ) && ( g_dwMousePort & ( 1 << i ) ) ) 
        {
            // Get a handle to the device
            g_hMouseDevice[i] = XInputOpen( XDEVICE_TYPE_DEBUG_MOUSE, i, 
                                            XDEVICE_NO_SLOT, NULL );
        }
    }

    // Check if mouse or mice were removed or attached.
    // We'll get the handle(s) next frame in the above code.
    DWORD dwNumInsertions, dwNumRemovals;
    if( XGetDeviceChanges( XDEVICE_TYPE_DEBUG_MOUSE, &dwNumInsertions, 
                           &dwNumRemovals ) )
    {
        // Loop through all ports and remove any mice that have been unplugged
        for( DWORD i=0; i < XGetPortCount(); i++ )
        {

            if( ( dwNumRemovals && ( 1 << i ) ) && 
                ( g_hMouseDevice[i] != NULL ) )
            {
                XInputClose( g_hMouseDevice[i] );
                g_hMouseDevice[i] = NULL;
            }
        }

        // Set the bits for all of the mice plugged in.
        // We get the handles on the next pass through.
        g_dwMousePort = dwNumInsertions;
    }

    // Poll the mouse.
    DWORD bMouseMoved = 0;
    for( DWORD i=0; i < XGetPortCount(); i++ )
    {
        if( g_hMouseDevice[i] )
            XInputGetState( g_hMouseDevice[i], &g_MouseState[i] );

        if( g_dwLastMousePacket[i] != g_MouseState[i].dwPacketNumber )
        {
            bMouseMoved |= (1 << i); 
            g_dwLastMousePacket[i] = g_MouseState[i].dwPacketNumber;
        }
    }

    return bMouseMoved;
}


