//-----------------------------------------------------------------------------
// File: XHVVoiceManager.cpp
//
// Desc: Wraps the XHV voice engine and provides a simple interface to the 
//          game
//
// Hist: 05.06.03 - New for the June 2003 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "XHVVoiceManager.h"
#include <assert.h>


//-----------------------------------------------------------------------------
// Name: CXHVVoiceManager (ctor)
// Desc: Initializes member variables
//-----------------------------------------------------------------------------
CXHVVoiceManager::CXHVVoiceManager()
{
    m_pXHVEngine            = NULL;
    m_pDSound               = NULL;
    m_dwMaxRemoteTalkers    = 0;
    m_dwNumRemoteTalkers    = 0;
    m_pPlayerVoiceInfo      = NULL;
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        m_bEnabledLocalTalkers[i]   = FALSE;
        m_bVoiceThroughSpeakers[i]  = FALSE;
    }
}



//-----------------------------------------------------------------------------
// Name: ~CXHVVoiceManager (dtor)
// Desc: Performs any final cleanup that is needed
//-----------------------------------------------------------------------------
CXHVVoiceManager::~CXHVVoiceManager()
{
    Shutdown();
}


//-----------------------------------------------------------------------------
// Name: Initialize
// Desc: Initializes the XHV voice manager
//-----------------------------------------------------------------------------
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

    // Enable voicechat and loopback modes only
    m_pXHVEngine->EnableProcessingMode( XHV_LOOPBACK_MODE );
    m_pXHVEngine->EnableProcessingMode( XHV_VOICECHAT_MODE );
    // m_pXHVEngine->EnableProcessingMode( XHV_SR_MODE );

    m_pXHVEngine->SetCallbackInterface( pITitleXHV );

    m_dwMaxRemoteTalkers    = pXHVParams->dwMaxRemoteTalkers;

    // Allocate space for the PlayerVoiceInfo structs
    m_pPlayerVoiceInfo = new PlayerVoiceInfo[ m_dwMaxRemoteTalkers ];
    if( NULL == m_pPlayerVoiceInfo )
    {
        Shutdown();
        return E_OUTOFMEMORY;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Shutdown
// Desc: Shuts down the object
//-----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::Shutdown()
{
    m_pDSound = NULL;

    if( m_pPlayerVoiceInfo )
    {
        delete[] m_pPlayerVoiceInfo;
        m_pPlayerVoiceInfo = NULL;
    }

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


//-----------------------------------------------------------------------------
// Name: RegisterRemoteTalker
// Desc: Registers a new remote talker
//-----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::RegisterRemoteTalker( XUID xuidRemoteTalker )
{
    // XHV allows you to call RegisterRemoteTalker more than once with the
    // same XUID.  For convenience, we'll emulate that behavior
    if( SUCCEEDED( FindPlayerVoiceInfo( xuidRemoteTalker, NULL ) ) )
        return S_OK;

    // Verify we're within our limits
    assert( m_dwNumRemoteTalkers < m_dwMaxRemoteTalkers );

    // Register the remote talker with XHV
    HRESULT hr;
    hr = m_pXHVEngine->RegisterRemoteTalker( xuidRemoteTalker );
    if( FAILED( hr ) )
        return hr;

    const DSMIXBINVOLUMEPAIR dsmbvp = { DSMIXBIN_FXSEND_15, DSBVOLUME_MAX };
    const DSMIXBINS dsmb = { 1, &dsmbvp };
    // Set the speaker mixbin destination to be FXSEND_15, which should
    // route through an amplifying rate converter to the speakers
    m_pXHVEngine->SetMixBinMapping( xuidRemoteTalker,
                                    XHV_PLAYBACK_TO_SPEAKERS,
                                    &dsmb );

    // Set up a new PlayerVoiceInfo struct for the player
    PlayerVoiceInfo* pRemoteTalker = &m_pPlayerVoiceInfo[ m_dwNumRemoteTalkers ];
    pRemoteTalker->xuid = xuidRemoteTalker;
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        pRemoteTalker->bMuted[ i ]          = FALSE;
        pRemoteTalker->bRemoteMuted[ i ]    = FALSE;
        pRemoteTalker->priority[ i ]        = XHV_PLAYBACK_PRIORITY_NEVER;
    }

    ++m_dwNumRemoteTalkers;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: UnregisterRemoteTalker
// Desc: Unregisters an existing remote talker
//-----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::UnregisterRemoteTalker( XUID xuidRemoteTalker )
{
    // Find the entry to remove from our PlayerVoiceInfo list
    DWORD dwEntry;
    if( FAILED( FindPlayerVoiceInfo( xuidRemoteTalker, &dwEntry ) ) )
        return E_FAIL;

    // Remove the entry by overwriting it with the last entry in the list
    memcpy( &m_pPlayerVoiceInfo[ dwEntry ],
            &m_pPlayerVoiceInfo[ m_dwNumRemoteTalkers - 1 ],
            sizeof( PlayerVoiceInfo ) );
    --m_dwNumRemoteTalkers;

    // Unregister the remote talker with XHV
    HRESULT hr = m_pXHVEngine->UnregisterRemoteTalker( xuidRemoteTalker );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: ClearRemoteTalkers
// Desc: Unregisters all remaining remote talkers
//-----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::ClearRemoteTalkers()
{
    for( DWORD i = 0; i < m_dwNumRemoteTalkers; i++ )
    {
        m_pXHVEngine->UnregisterRemoteTalker( m_pPlayerVoiceInfo[ i ].xuid );
    }
    m_dwNumRemoteTalkers = 0;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: SetRemoteTalkerPriority 
// Desc: Sets the priority of a remote talker.
//-----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::SetRemoteTalkerPriority( XUID xuidRemoteTalker, 
                                                   DWORD dwPort, 
                                                   XHV_PLAYBACK_PRIORITY priority )
{
    DWORD dwEntry;
    if( FAILED( FindPlayerVoiceInfo( xuidRemoteTalker, &dwEntry ) ) )
        return E_FAIL;

    // Set our copy of the priority
    PlayerVoiceInfo* pInfo = &m_pPlayerVoiceInfo[ dwEntry ];
    pInfo->priority[ dwPort ] = priority;

    // Recalculate priorities for the remote talker
    if( FAILED( RecalculatePriorities( pInfo ) ) )
        return E_FAIL;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: RegisterLocalTalker
// Desc: Registers a local talker
//-----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::RegisterLocalTalker( DWORD dwPort )
{
    HRESULT hr = S_OK;

    hr = m_pXHVEngine->RegisterLocalTalker( dwPort );
    if( FAILED( hr ) )
        return hr;

    m_bEnabledLocalTalkers[ dwPort ] = TRUE;

    return hr;
}



//-----------------------------------------------------------------------------
// Name: UnregisterLocalTalker
// Desc: Unregisters a local talker
//-----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::UnregisterLocalTalker( DWORD dwPort )
{
    HRESULT hr = S_OK;

    m_bEnabledLocalTalkers[ dwPort ] = FALSE;

    hr = m_pXHVEngine->UnregisterLocalTalker( dwPort );
    if( FAILED( hr ) )
        return hr;
    
    return hr;
}



//-----------------------------------------------------------------------------
// Name: RecalculatePriorities
// Desc: Calculates and sets the appropriate priorities of the remote talker.
//       If the local talker has voice through speakers disabled, it simply
//          sets the priority based on the local talker's requested priority,
//          while accounting for muting.
//       If the local talker has voice through speakers enabled, it sets
//          the priority for that local talker to NEVER.  If the local player
//          has not muted and has not been muted by the remote talker, it sets 
//          the speaker priority to the greater of its current value and the
//          priority specified for the local talker.
//       Finally, it sets the speaker priority to the maxmium priority of all
//          non-muted local talkers.  Note this means that if multiple local
//          talkers have selected Voice Through Speakers, and a remote talker
//          has been muted by only one of them, that remote talker will be 
//          heard over the speakers.  This is the recommended behavior.
//-----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::RecalculatePriorities( PlayerVoiceInfo* pInfo )
{
    XHV_PLAYBACK_PRIORITY speakerPriority = XHV_PLAYBACK_PRIORITY_NEVER;
    for( DWORD dwPort = 0; dwPort < XGetPortCount(); dwPort++ )
    {
        if( !m_bEnabledLocalTalkers[ dwPort ] )
            continue;

        if( !m_bVoiceThroughSpeakers[ dwPort ] )
        {
            // If voice through speakers is disabled, set priority based on 
            // muting.
            XHV_PLAYBACK_PRIORITY priority = pInfo->priority[ dwPort ];
            if( pInfo->bMuted[ dwPort ] || 
                pInfo->bRemoteMuted[ dwPort ] )
            {
                priority = XHV_PLAYBACK_PRIORITY_NEVER;
            }
            m_pXHVEngine->SetPlaybackPriority( pInfo->xuid,
                                               dwPort,
                                               priority );
        }
        else
        {
            // If voice through speakers is enabled, turn off headset
            // and set maximum speaker priority based on muting.
            m_pXHVEngine->SetPlaybackPriority( pInfo->xuid,
                                               dwPort,
                                               XHV_PLAYBACK_PRIORITY_NEVER );

            if( !pInfo->bMuted[ dwPort ] &&
                !pInfo->bRemoteMuted[ dwPort ] &&
                pInfo->priority[ dwPort ] < speakerPriority )
            {
                speakerPriority = pInfo->priority[ dwPort ];
            }
        }
    }

    // Set speaker priority based on maximum non-muted priority across
    // all local talkers in voice through speakers mode.
    m_pXHVEngine->SetPlaybackPriority( pInfo->xuid,
                                       XHV_PLAYBACK_TO_SPEAKERS,
                                       speakerPriority );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: SetVoiceThroughSpeakers
// Desc: Turns voice-through-speakers mode off and on
//-----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::SetVoiceThroughSpeakers( DWORD dwPort, BOOL bVoiceThroughSpeakers )
{
    m_bVoiceThroughSpeakers[dwPort] = bVoiceThroughSpeakers;

    // Recalculate the priorities for each remote talker
    for( DWORD i = 0; i < m_dwNumRemoteTalkers; i++ )
    {
        RecalculatePriorities( &m_pPlayerVoiceInfo[ i ] );
    }

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: SetVoiceSpeakerVolume
// Desc: Sets the volume level of voice through speaker output
//----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::SetVoiceSpeakerVolume( FLOAT fVolume, DWORD dwFXIndex )
{
    assert( 0.0f <= fVolume && fVolume <= 1.0f );
    
    // Convert to DSP data format
    DWORD dwGainParam = DWORD( fVolume * 0x7FFFFF );
    return m_pDSound->SetEffectData( dwFXIndex,
                                     32,
                                     &dwGainParam,
                                     sizeof( DWORD ),
                                     DSFX_IMMEDIATE );
}



//-----------------------------------------------------------------------------
// Name: SetMute
// Desc: Mutes/UnMutes a remote talker for a given local talker
//-----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::SetMute( XUID xuidRemoteTalker, DWORD dwPort, BOOL bMuted )
{
    DWORD dwEntry;
    if( FAILED( FindPlayerVoiceInfo( xuidRemoteTalker, &dwEntry ) ) )
        return E_FAIL;

    PlayerVoiceInfo* pInfo = &m_pPlayerVoiceInfo[ dwEntry ];
    pInfo->bMuted[ dwPort ] = bMuted;
    RecalculatePriorities( pInfo );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: SetRemoteMute
// Desc: RemoteMutes/RemoteUnMutes a remote talker for a given local talker
//-----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::SetRemoteMute( XUID xuidRemoteTalker, DWORD dwPort, BOOL bRemoteMuted )
{
    DWORD dwEntry;
    if( FAILED( FindPlayerVoiceInfo( xuidRemoteTalker, &dwEntry ) ) )
        return E_FAIL;

    PlayerVoiceInfo* pInfo = &m_pPlayerVoiceInfo[ dwEntry ];
    pInfo->bRemoteMuted[ dwPort ] = bRemoteMuted;
    RecalculatePriorities( pInfo );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: IsTalking
// Desc: Determines whether a remote talker is currently talking to a local
//          talker.
//-----------------------------------------------------------------------------
BOOL CXHVVoiceManager::IsTalking( XUID xuidRemoteTalker, DWORD dwPort )
{
    if( !m_bEnabledLocalTalkers[ dwPort ] )
        return FALSE;

    // First, get info about local and remote talker
    BOOL bIsTalking = m_pXHVEngine->IsTalking( xuidRemoteTalker );
    XHV_LOCAL_TALKER_STATUS status;
    m_pXHVEngine->GetLocalTalkerStatus( dwPort, &status );
    BOOL bHasHeadset = ( status.communicatorStatus == XHV_VOICE_COMMUNICATOR_STATUS_INSERTED );
    
    // Although they're talking, a local player may not hear them due to
    // muting.
    if( bIsTalking )
    {
        DWORD dwEntry;
        if( FAILED( FindPlayerVoiceInfo( xuidRemoteTalker, &dwEntry ) ) )
            return FALSE;
        PlayerVoiceInfo* pInfo = &m_pPlayerVoiceInfo[ dwEntry ];

        // If the local player has muted the remote player, or vice versa,
        // they are not talking
        if( pInfo->bMuted[ dwPort ] ||
            pInfo->bRemoteMuted[ dwPort ] ||
            pInfo->priority[ dwPort ] == XHV_PLAYBACK_PRIORITY_NEVER )
            bIsTalking = FALSE;

        // If the local player doesn't have a headset, and is not in voice 
        // through speakers mode, they're not talking.
        if( !bHasHeadset &&
            !m_bVoiceThroughSpeakers[ dwPort ] )
            bIsTalking = FALSE;
    }

    return bIsTalking;
}



//-----------------------------------------------------------------------------
// Name: FindPlayerVoiceInfo
// Desc: Helper function to find an entry in the player voice info list
//-----------------------------------------------------------------------------
HRESULT CXHVVoiceManager::FindPlayerVoiceInfo( XUID xuidRemoteTalker, DWORD* pdwEntry )
{
    for( DWORD i = 0; i < m_dwNumRemoteTalkers; i++ )
    {
        if( XOnlineAreUsersIdentical( &m_pPlayerVoiceInfo[i].xuid,
                                      &xuidRemoteTalker ) )
        {
            if( pdwEntry )
            {
                *pdwEntry = i;
            }
            return S_OK;
        }
    }

    return E_FAIL;
}

