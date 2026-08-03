//-------------------------------------------------------------------------------------
// File: XHVVoiceManager.cpp
//
// Desc: Wraps the XHV voice engine and provides a simple interface to the
//          game
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------
#include "XHVVoiceManager.h"
#include <assert.h>

//-------------------------------------------------------------------------------------
// Name: CXHVVoiceManager (ctor)
// Desc: Initializes member variables
//-------------------------------------------------------------------------------------
CXHVVoiceManager::CXHVVoiceManager()
{
    m_pXHVEngine            = NULL;
    m_pDSound               = NULL;
    m_dwMaxRemoteTalkers    = 0;
    m_dwNumRemoteTalkers    = 0;
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        m_bEnabledLocalTalkers[i]   = FALSE;
    }

    m_bRecordingNow         = FALSE;
    m_bPlayingNow           = FALSE;
    m_dwVoiceMailPort       = (DWORD)-1;
    m_dwVoiceMailDuration   = 0;
    m_dwVoiceMailBufferSize = 0;
    m_pVoiceMailBuffer      = NULL;
    m_bOutputToSpeakers     = FALSE;

}

//-------------------------------------------------------------------------------------
// Name: ~CXHVVoiceManager (dtor)
// Desc: Performs any final cleanup that is needed
//-------------------------------------------------------------------------------------
CXHVVoiceManager::~CXHVVoiceManager()
{
    Shutdown();
}

//-------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initializes the XHV voice manager
//-------------------------------------------------------------------------------------
HRESULT CXHVVoiceManager::Initialize( LPDIRECTSOUND8 pDSound,
                                      XHV_RUNTIME_PARAMS* pXHVParams,
                                      ITitleXHV *pITitleXHV )
{
    assert( pDSound );
    assert( pXHVParams );
    assert( pITitleXHV );

    m_pDSound = pDSound;

    // Create the engine
    HRESULT hr = XHVEngineCreate( pXHVParams, &m_pXHVEngine );
    if( FAILED( hr ) )
        return hr;

    // Enable voicechat and voice mail modes only
    m_pXHVEngine->EnableProcessingMode( XHV_VOICECHAT_MODE );

    m_pXHVEngine->EnableProcessingMode( XHV_VOICEMAIL_MODE );

    m_pXHVEngine->SetCallbackInterface( pITitleXHV );

    m_dwMaxRemoteTalkers    = pXHVParams->dwMaxRemoteTalkers;

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: Shutdown
// Desc: Shuts down the object
//-------------------------------------------------------------------------------------
HRESULT CXHVVoiceManager::Shutdown()
{
    m_pDSound = NULL;

    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        if( m_bEnabledLocalTalkers[ i ] )
        {
            UnregisterLocalTalker( i );
        }
    }

    if( m_pXHVEngine )
    {
        m_pXHVEngine->Release();
        m_pXHVEngine = NULL;
    }

    m_dwMaxRemoteTalkers    = 0;
    m_dwNumRemoteTalkers    = 0;

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: StartRecordVoiceMail
// Desc: Records voice mail, given port, max time, and recording buffer
//-------------------------------------------------------------------------------------
HRESULT CXHVVoiceManager::StartRecordVoiceMail( DWORD dwPort,
                                                DWORD dwMaxTimeMs ,
                                                BYTE* pRecordBuffer ,
                                                DWORD dwBufferSize )
{
    // we'll make sure that the buffer size matches the duration time expected
    DWORD dwExpectedBufferSize = XHVGetVoiceMailBufferSize(dwMaxTimeMs);
    assert(dwExpectedBufferSize == dwBufferSize);

    // initialize our voice mail variables
    m_dwVoiceMailPort       = dwPort;
    m_dwVoiceMailDuration   = dwMaxTimeMs;
    m_dwVoiceMailBufferSize = dwBufferSize;
    m_pVoiceMailBuffer      = pRecordBuffer;

    // Trigger recording of voice mail
    HRESULT hrStartRecord = m_pXHVEngine->VoiceMailRecord( m_dwVoiceMailPort,
                            m_dwVoiceMailDuration, m_dwVoiceMailBufferSize , 
                            m_pVoiceMailBuffer );

    // if result succeeds, turn the recording toggle on
    if ( SUCCEEDED( hrStartRecord ) )
    {
        m_bRecordingNow = TRUE;
    }

    return hrStartRecord;

}

//-------------------------------------------------------------------------------------
// Name: StartRecordVoiceMail
// Desc: Records voice mail, given port, max time, and recording buffer
//-------------------------------------------------------------------------------------    
HRESULT CXHVVoiceManager::PrepareVoiceMailPlay( DWORD dwPort,
                                                DWORD dwMaxTimeMs ,
                                                BYTE* pRecordBuffer ,
                                                DWORD dwBufferSize )
{
    // initialize our voice mail variables
    m_dwVoiceMailPort       = dwPort;
    m_dwVoiceMailDuration   = dwMaxTimeMs;
    m_dwVoiceMailBufferSize = dwBufferSize;
    m_pVoiceMailBuffer      = pRecordBuffer;

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: AlertOfVoiceMailStopped
// Desc: alerts manager of stopped voice mail
//-------------------------------------------------------------------------------------
VOID CXHVVoiceManager::AlertOfVoiceMailStopped()
{
    m_bRecordingNow = FALSE;
    m_bPlayingNow = FALSE;
    m_bOutputToSpeakers = FALSE;
}

//-------------------------------------------------------------------------------------
// Name: AlertOfVoiceMailDataReady
// Desc: alerts manager of voice mail data ready
//-------------------------------------------------------------------------------------
VOID CXHVVoiceManager::AlertOfVoiceMailDataReady()
{
    m_bRecordingNow = FALSE;
    m_bPlayingNow = FALSE;
    m_bOutputToSpeakers = FALSE;
}

//-------------------------------------------------------------------------------------
// Name: PlayRecordedVoiceMail
// Desc: plays back Recorded voice mail, possible to speakers.
//-------------------------------------------------------------------------------------
HRESULT CXHVVoiceManager::PlayRecordedVoiceMail( BOOL bOutputToSpeakers )
{
    assert( m_pVoiceMailBuffer );
    assert( m_dwVoiceMailBufferSize );
    assert( !m_bRecordingNow );

    m_bOutputToSpeakers = bOutputToSpeakers;

    HRESULT hrPlay = m_pXHVEngine->VoiceMailPlay( m_dwVoiceMailPort ,
                                                  m_dwVoiceMailBufferSize ,
                                                  m_pVoiceMailBuffer ,
                                                  m_bOutputToSpeakers );

    // if result succeeds, turn the recording toggle on
    if ( SUCCEEDED( hrPlay ) )
    {
        m_bPlayingNow = TRUE;
    }

    return hrPlay;
}

//-------------------------------------------------------------------------------------
// Name: StopVoiceMail
// Desc: stop voice mail operation.
//-------------------------------------------------------------------------------------
HRESULT CXHVVoiceManager::StopVoiceMail()
{
    // make sure we are currently recording or playing
    assert( m_bRecordingNow || m_bPlayingNow );

    HRESULT hrStop = m_pXHVEngine->VoiceMailStop( m_dwVoiceMailPort );

    if ( SUCCEEDED( hrStop ) )
    {
        m_bRecordingNow = FALSE;
        m_bPlayingNow = FALSE;
    }

    return hrStop;
}

//-------------------------------------------------------------------------------------
// Name: StopVoiceMail
// Desc: stop voice mail operation.
//-------------------------------------------------------------------------------------
HRESULT CXHVVoiceManager::ClearVoiceMail()
{
    HRESULT hrClear = S_OK;

    if ( m_bRecordingNow || m_bPlayingNow )
    {
        hrClear = StopVoiceMail();
    }

    m_bRecordingNow         = FALSE;
    m_bPlayingNow           = FALSE;
    m_dwVoiceMailPort       = (DWORD)-1;
    m_dwVoiceMailDuration   = 0;
    m_dwVoiceMailBufferSize = 0;
    m_pVoiceMailBuffer      = NULL;
    m_bOutputToSpeakers     = FALSE;

    return hrClear;
}

//-------------------------------------------------------------------------------------
// Name: RegisterLocalTalker
// Desc: Registers a local talker
//-------------------------------------------------------------------------------------
HRESULT CXHVVoiceManager::RegisterLocalTalker( DWORD dwPort )
{
    HRESULT hr = S_OK;

    hr = m_pXHVEngine->RegisterLocalTalker( dwPort );
    if( FAILED( hr ) )
        return hr;

    m_bEnabledLocalTalkers[ dwPort ] = TRUE;

    return hr;
}

//-------------------------------------------------------------------------------------
// Name: UnregisterLocalTalker
// Desc: Unregisters a local talker
//-------------------------------------------------------------------------------------
HRESULT CXHVVoiceManager::UnregisterLocalTalker( DWORD dwPort )
{
    HRESULT hr = S_OK;

    m_bEnabledLocalTalkers[ dwPort ] = FALSE;

    hr = m_pXHVEngine->UnregisterLocalTalker( dwPort );
    if( FAILED( hr ) )
        return hr;

    return hr;
}
