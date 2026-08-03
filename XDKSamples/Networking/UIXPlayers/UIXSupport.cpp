//-----------------------------------------------------------------------------
// File: UIXSupport.cpp
//
// Desc: Support functions for UIXPlayers
//
// Hist: 7.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "UIXPlayers.h"




const LPCSTR strWaveBankFile =  "d:\\media\\UIXDefault.xwb";
const LPCSTR strSoundBankFile = "d:\\media\\UIXDefault.xsb";




//-----------------------------------------------------------------------------
// Name: SetPlayerOnlineState()
// Desc: Broadcast updated User state for the world
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetPlayerOnlineState( DWORD dwUserIndex, DWORD dwState )
{
    if ( dwState )
    {
        // If we are in a joinable game then set this flag.
        dwState |= XONLINE_FRIENDSTATE_FLAG_JOINABLE;
    }
    HRESULT hr = m_pLiveEngine->NotificationSetState( dwUserIndex, dwState,
                      m_FakeSessionID, 0, NULL );

    // Normally NotificationSetState will always succeed, however if the user
    // has been logged out (due to network problems or a duplicate logon for
    // instance) and the title has not yet been notified of this, then this
    // function can fail. Such failures can be safely ignored.
    (VOID)hr; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: CheckDeviceStates()
// Desc: Check for voice peripheral state changes and update online state
//       This function is called once per frame as soon the title is online
// Be sure to set other flags, such as XONLINE_FRIENDSTATE_FLAG_JOINABLE
// and XONLINE_FRIENDSTATE_FLAG_PLAYING as appropriate.
//-----------------------------------------------------------------------------
VOID CXBoxSample::CheckDeviceStates()
{
    DWORD dwMicrophoneInsertions;
    DWORD dwMicrophoneRemovals;
    DWORD dwHeadphoneInsertions;
    DWORD dwHeadphoneRemovals;

    BOOL bMicrophoneChanges = XGetDeviceChanges( XDEVICE_TYPE_VOICE_MICROPHONE,
                                                 &dwMicrophoneInsertions,
                                                 &dwMicrophoneRemovals );
    BOOL bHeadphoneChanges = XGetDeviceChanges( XDEVICE_TYPE_VOICE_HEADPHONE,
                                                &dwHeadphoneInsertions,
                                                &dwHeadphoneRemovals );

    if( bMicrophoneChanges || bHeadphoneChanges )
    {
        // Update state for removals
        m_dwMicrophoneState &= ~dwMicrophoneRemovals;
        m_dwHeadphoneState  &= ~dwHeadphoneRemovals;

        // Then update state for new insertions
        m_dwMicrophoneState |= dwMicrophoneInsertions;
        m_dwHeadphoneState  |= dwHeadphoneInsertions;

        // Check the state of each local user
        for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
        {
            if( m_Users[i].bSignedOn &&
                XOnlineIsUserVoiceAllowed( m_Users[i].dwUserFlags ) &&
                !m_Users[i].bGuest )
             {
                DWORD dwState = 0;

                // If either the microphone or the headphone was
                // removed since last call, the user no longer
                // has voice capability
                if( m_Users[i].bVoice )
                {
                    if( ( dwMicrophoneRemovals & ( 1 << i ) ) ||
                        ( dwHeadphoneRemovals  & ( 1 << i ) ) )
                    {
                        m_Users[i].bVoice = FALSE;
                        dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE;
                    }
                }
                else
                {
                    // If both microphone and headphone are present, and
                    // the user didn't have voice capability, add it
                    if( ( m_dwMicrophoneState & ( 1 << i ) ) &&
                        ( m_dwHeadphoneState  & ( 1 << i ) ) )
                    {
                        m_Users[i].bVoice = TRUE;
                        dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE |
                                  XONLINE_FRIENDSTATE_FLAG_VOICE;
                    }
                }
                if( dwState ) // State has changed...
                    SetPlayerOnlineState( i, dwState );
            }
        }
    }
}




//-----------------------------------------------------------------------------
// Name: InitSound()
// Desc: Initialize XACT support for UIX
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitSound()
{
    XACT_RUNTIME_PARAMETERS Params;
    VOID*   pSoundBankData;
    VOID*   pWaveBankData;
    DWORD   dwWaveBankSize;
    DWORD   dwSoundBankSize;
    HRESULT hr;

    // Read the wave bank and the sound bank media files into memory
    if( FAILED( XBUtil_LoadFile( strWaveBankFile, &pWaveBankData, &dwWaveBankSize ) ) )
        return E_FAIL;

    if( FAILED( XBUtil_LoadFile( strSoundBankFile, &pSoundBankData, &dwSoundBankSize ) ) )
        return E_FAIL;

    // Create the Xact engine
    ZeroMemory( &Params, sizeof(Params) );
    Params.dwMaxConcurrentStreams = 16;
    Params.dwMax2DHwVoices        = 40;
    Params.dwMax3DHwVoices        = 10;

    hr = XACTEngineCreate( &Params, &m_pXactEngine );
    if( FAILED(hr) )
        return hr;

    // Register the wave bank
    hr = m_pXactEngine->RegisterWaveBank( pWaveBankData, dwWaveBankSize,
                                          &m_pWaveBank );
    if( FAILED(hr) )
    {
        m_pXactEngine->Release();
        m_pXactEngine = NULL;
        return hr;
    }

    // Register the sound bank
    hr = m_pXactEngine->CreateSoundBank( pSoundBankData, dwSoundBankSize, &m_pSoundBank );
    if( FAILED(hr) )
    {
        m_pXactEngine->Release();
        m_pXactEngine = NULL;
        return hr;
    }

    return S_OK;
}




