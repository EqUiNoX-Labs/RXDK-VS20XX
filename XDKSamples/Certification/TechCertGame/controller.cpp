//-----------------------------------------------------------------------------
// File: Controller.cpp
//
// Desc: Handles game input
//
// Hist: 04.06.01 - New for May XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "Controller.h"




//-----------------------------------------------------------------------------
// Statics
//-----------------------------------------------------------------------------
INT   Controller::m_iPrimaryController = -1;
BOOL  Controller::m_bIsThumbstick1Calibrated = FALSE;
FLOAT Controller::m_fMaxX1 = 0.0f;
FLOAT Controller::m_fMinX1 = 0.0f;
FLOAT Controller::m_fMaxY1 = 0.0f;
FLOAT Controller::m_fMinY1 = 0.0f;




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const FLOAT fINITIAL_THUMB_DEADZONE = 0.35f;   // pre-calibration
const FLOAT fFINAL_THUMB_DEADZONE   = 0.14f;   // post-calibration




//-----------------------------------------------------------------------------
// Name: GetPrimaryController()
// Desc: The primary controller is the first controller used by a player.
//       If the primary controller has been removed, returns NULL until
//       a new controller is used. In demo mode, the primary controller
//       is cleared, and any controller may become the primary controller
//       by being the first controller to have a button activated.
//-----------------------------------------------------------------------------
const XBGAMEPAD* Controller::GetPrimaryController() // static
{
    // TCR Controller Selection

    // If primary controller has been set and hasn't been removed, use it
    const XBGAMEPAD* pGamepad = NULL;
    if( m_iPrimaryController != -1 )
    {
        pGamepad = &g_Gamepads[ m_iPrimaryController ];
        if( pGamepad->hDevice != NULL )
        {
            // See if controller has performed dynamic calibration
            CheckCalibration( pGamepad );

            return pGamepad;
        }
    }

    // Primary controller 1) hasn't been set, 
    //                    2) has been cleared, or
    //                    3) has been removed

    // Examine each inserted controller to see if any is being used
    for( DWORD i=0; i < XGetPortCount(); ++i )
    {
        pGamepad = &g_Gamepads[i];
        if( pGamepad->hDevice != NULL )
        {
            // If the Start button is pressed, we found the primary controller
            if( pGamepad->wButtons & XINPUT_GAMEPAD_START )
            {
                m_iPrimaryController = i;
                return pGamepad;
            }
        }
    }

    // Either no controllers are inserted, or a controller has been
    // inserted but not used yet
    return NULL;
}




//-----------------------------------------------------------------------------
// Name: CheckCalibration()
// Desc: Determine if the controller thumbstick has been calibrated. We
//       only care about the left thumbstick in the game, so that's the only
//       thumbstick we check. It's impossible to know when the thumbstick has
//       been physically calibrated, but one reasonable method is checking to
//       see if the thumbstick has been pushed in every direction.
//-----------------------------------------------------------------------------
VOID Controller::CheckCalibration( const XBGAMEPAD* pGamepad ) // static
{
    if( m_bIsThumbstick1Calibrated )
        return;

    if( pGamepad->fX1 > m_fMaxX1 )
        m_fMaxX1 = pGamepad->fX1;
    if( pGamepad->fX1 < m_fMinX1 )
        m_fMinX1 = pGamepad->fX1;

    if( pGamepad->fY1 > m_fMaxY1 )
        m_fMaxY1 = pGamepad->fY1;
    if( pGamepad->fY1 < m_fMinY1 )
        m_fMinY1 = pGamepad->fY1;

    if( m_fMaxX1 ==  1.0f &&
        m_fMinX1 == -1.0f &&
        m_fMaxY1 ==  1.0f &&
        m_fMinY1 == -1.0f )
    {
        m_bIsThumbstick1Calibrated = TRUE;
    }
}




//-----------------------------------------------------------------------------
// Name: SetVibration()
// Desc: Sets the primary controller a'rumblin
//-----------------------------------------------------------------------------
VOID Controller::SetVibration( const XBGAMEPAD* pGamepad, 
                               FLOAT fLeft, FLOAT fRight ) // static
{
    if( pGamepad == NULL || pGamepad->hDevice == NULL )
        return;

    assert( fLeft >= 0.0f );
    assert( fLeft <= 1.0f );
    assert( fRight >= 0.0f );
    assert( fRight <= 1.0f );
    
    // Initial state of feedback struct is empty. Struct must be global
    // because XInputSetState uses the structure asynchronously.
    static XINPUT_FEEDBACK InputFeedback = { 0 };

    // One-time creation of event handle
    if( InputFeedback.Header.hEvent == NULL )
        InputFeedback.Header.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    
    // Use the event handle method to determine if previous call to
    // XInputSetState has completed. We can't call XInputSetState again until
    // the previous XINPUT_FEEDBACK struct has been consumed by the controller.
    else
        WaitForSingleObject( InputFeedback.Header.hEvent, INFINITE );
    
    // Set up vibration parameters
    FLOAT fMAX_WORD = 65535.0f;
    InputFeedback.Rumble.wLeftMotorSpeed = WORD( fLeft * fMAX_WORD );
    InputFeedback.Rumble.wRightMotorSpeed = WORD( fRight * fMAX_WORD );

    // Send vibration info to controller
    DWORD dwStatus = XInputSetState( pGamepad->hDevice, &InputFeedback );
    (VOID)dwStatus; // ignore errors
}




//-----------------------------------------------------------------------------
// Name: IsAnyButtonActive()
// Desc: TRUE if any button on any controller
//-----------------------------------------------------------------------------
BOOL Controller::IsAnyButtonActive() // static
{
    for( DWORD i=0; i < XGetPortCount(); ++i )
    {
        const XBGAMEPAD* pGamepad = &g_Gamepads[i];
        if( pGamepad->hDevice != NULL )
        {
            if( IsAnyButtonActive( pGamepad ) )
                return TRUE;
        }
    }
    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: IsAnyAOrSTARTActive()
// Desc: TRUE if any A or START button pressed on any controller
//-----------------------------------------------------------------------------
BOOL Controller::IsAnyAOrSTARTActive() // static
{
    for( DWORD i=0; i < XGetPortCount(); ++i )
    {
        const XBGAMEPAD* pGamepad = &g_Gamepads[i];
        if( pGamepad->hDevice != NULL )
        {
            if( pGamepad->bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] ||
                pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
                return TRUE;
        }
    }
    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: IsAnyButtonActive()
// Desc: TRUE if given controller is available and any button on the control
//       is pressed
//-----------------------------------------------------------------------------
BOOL Controller::IsAnyButtonActive( const XBGAMEPAD* pGamepad ) // static
{
    if( pGamepad == NULL )
        return FALSE;

    // Check digital buttons
    if( pGamepad->wButtons )
        return TRUE;

    // Check analog buttons
    for( DWORD i = 0; i < 8; ++i )
    {
        if( pGamepad->bAnalogButtons[ i ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
            return TRUE;
    }

    // If the controller has been calibrated, use a smaller deadzone
    FLOAT fDeadzone = m_bIsThumbstick1Calibrated ? fFINAL_THUMB_DEADZONE : 
                                                   fINITIAL_THUMB_DEADZONE;

    // Check thumbsticks
    if( pGamepad->fX1 >  fDeadzone ||
        pGamepad->fX1 < -fDeadzone ||
        pGamepad->fY1 >  fDeadzone ||
        pGamepad->fY1 < -fDeadzone )
    {
        return TRUE;
    }

    if( pGamepad->fX2 >  fINITIAL_THUMB_DEADZONE ||
        pGamepad->fX2 < -fINITIAL_THUMB_DEADZONE ||
        pGamepad->fY2 >  fINITIAL_THUMB_DEADZONE ||
        pGamepad->fY2 < -fINITIAL_THUMB_DEADZONE )
    {
        return TRUE;
    }

    // Nothing active
    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: ClearPrimaryController()
// Desc: Allows the next controller that's activated to become the new
//       primary controller
//-----------------------------------------------------------------------------
VOID Controller::ClearPrimaryController() // static
{
    // TCR Controller Selection
    m_iPrimaryController = -1;
    m_bIsThumbstick1Calibrated = FALSE;
    m_fMaxX1 = 0.0f;
    m_fMinX1 = 0.0f;
    m_fMaxY1 = 0.0f;
    m_fMinY1 = 0.0f;
}




//-----------------------------------------------------------------------------
// Name: HavePrimaryController()
// Desc: TRUE if we have a primary controller active
//-----------------------------------------------------------------------------
BOOL Controller::HavePrimaryController() // static
{
    return( m_iPrimaryController != -1 );
}




//-----------------------------------------------------------------------------
// Name: AnyAdded()
// Desc: TRUE if any controllers have been inserted since the last call
//       to XGetDevices( XDEVICE_TYPE_MEMORY_UNIT ) or XGetDeviceChanges()
//-----------------------------------------------------------------------------
BOOL Controller::AnyAdded() // static
{
    for( DWORD i=0; i < XGetPortCount(); i++ )
    {
        if( g_Gamepads[i].bInserted )
            return TRUE;
    }
    return FALSE;
}
