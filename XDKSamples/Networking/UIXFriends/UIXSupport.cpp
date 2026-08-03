//-----------------------------------------------------------------------------
// File: UIXSupport.cpp
//
// Desc: Support functions for UIXFriends
//
// Hist: 7.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "UIXFriends.h"




const LPCSTR strWaveBankFile =  "d:\\media\\UIXDefault.xwb";
const LPCSTR strSoundBankFile = "d:\\media\\UIXDefault.xsb";




//-----------------------------------------------------------------------------
// Name: SetPlayerOnlineState()
// Desc: Broadcast updated User state for the world
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetPlayerOnlineState( DWORD dwUserIndex, DWORD dwState )
{
	assert( !m_bSilentLogon || dwUserIndex == 0 );

	const XNKID ZeroSessionID = {0};

	HRESULT hr = m_pLiveEngine->NotificationSetState( dwUserIndex, dwState,
#ifndef LIVE_AWARE_ONLY
		m_SessionID,
#else
		ZeroSessionID,
#endif
		0, NULL );

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

		// Check the state of each controller
		for( DWORD CtlrPort = 0; CtlrPort < XGetPortCount(); ++CtlrPort )
		{
			// When silently logged on, aggregate all state under user index 0
			DWORD UserPort;
			if ( !m_bSilentLogon )
				UserPort = CtlrPort;
			else
				UserPort = 0;

			if( m_Users[UserPort].bSignedOn &&
                XOnlineIsUserVoiceAllowed( m_Users[UserPort].dwUserFlags ) &&
                !m_Users[UserPort].bGuest )
			{

				DWORD dwState = m_Users[UserPort].dwState;

				// If either the microphone or the headphone was
				// removed since last call, the user no longer
				// has voice capability
				if( m_dwVoice[CtlrPort] )
				{
					if( ( dwMicrophoneRemovals & ( 1 << CtlrPort ) ) ||
						( dwHeadphoneRemovals  & ( 1 << CtlrPort ) ) )
					{
						m_dwVoice[CtlrPort] = FALSE;
						dwState &= ~XONLINE_FRIENDSTATE_FLAG_VOICE;
					}
				}
				else
				{
					// If both microphone and headphone are present, and
					// the user didn't have voice capability, add it
					if( ( m_dwMicrophoneState & ( 1 << CtlrPort ) ) &&
						( m_dwHeadphoneState  & ( 1 << CtlrPort ) ) )
					{
						m_dwVoice[CtlrPort] = TRUE;
						dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;
					}
				}

                if( dwState != m_Users[UserPort].dwState ) // State has changed...
				{
					if ( !m_bSilentLogon )
					{
						m_Users[UserPort].dwState = dwState;
						SetPlayerOnlineState( UserPort, m_Users[UserPort].dwState );
					}
					else
					{
						// Remove voice status when all communicators have been removed or
						// Add voice status if only one communicator is inserted
						if ( ( m_Users[UserPort].dwState & XONLINE_FRIENDSTATE_FLAG_VOICE &&
							   ( m_dwMicrophoneState == 0 && m_dwHeadphoneState == 0 ) ) ||
						   ( !( m_Users[UserPort].dwState & XONLINE_FRIENDSTATE_FLAG_VOICE ) &&
							   ( m_dwMicrophoneState != 0 && m_dwHeadphoneState != 0 ) ) )
						{	
							m_Users[UserPort].dwState = dwState;
							SetPlayerOnlineState( UserPort, m_Users[UserPort].dwState );
						}
					}
				}
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




