//-----------------------------------------------------------------------------
// File: VoiceManager.cpp
//
// Desc: Implementation of Voice Communicator support
//
// Hist: 01.17.02 - Created
//       08.20.02 - Overhauled for October XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "VoiceManager.h"
#include <cassert>
#include <stdio.h>
#include "XBFont.h"

// Global instance of the voice manager
CVoiceManager g_VoiceManager;

// The voice codec only operates on 8kHz data
extern const DWORD VOICE_SAMPLING_RATE = 8000;
// Ramp headphone up/down over some range of headroom
// Gains get multiplied by 8 in the SRC effect
static const FLOAT MAX_GAIN = 1.00f / 8.0f; // 1/8  = unity gain
static const FLOAT MIN_GAIN = 0.25f / 8.0f; // 1/32 = 1/4 gain
// Ramp headphone up/down over a period of time
static const DWORD MAX_RAMP_TIME = 340;     // 340 ms

// This set of DSMIXBINVOLUMEPAIRs controls where the voice of each remote
// chatter will get routed.  Once CVoiceManager is initialized with a
// DSEFFECTIMAGEDESC, we will update the Headset mixbin destinations based
// on the actual loaded DSP image.
static DSMIXBINVOLUMEPAIR g_dsmbvp[] =
{
    { DSMIXBIN_VOICE_0,         DSBVOLUME_MAX },    // Headset 0
    { DSMIXBIN_VOICE_1,         DSBVOLUME_MAX },    // Headset 1
    { DSMIXBIN_VOICE_2,         DSBVOLUME_MAX },    // Headset 2
    { DSMIXBIN_VOICE_3,         DSBVOLUME_MAX },    // Headset 3
    { DSMIXBIN_FRONT_CENTER,    DSBVOLUME_MIN },    // Voice Through Speakers
};
static const DWORD NUM_VOLUMEPAIRS = sizeof( g_dsmbvp ) / sizeof( g_dsmbvp[0] );

// Everything after the first 4 mixbins is considered a "Speaker" send
static DSMIXBINVOLUMEPAIR* g_dsmbvpSpeakers = &g_dsmbvp[XGetPortCount()];
static const DWORD NUM_SPEAKERPAIRS = NUM_VOLUMEPAIRS - XGetPortCount();



//-----------------------------------------------------------------------------
// Name: struct REMOTE_CHATTER
// Desc: Stores information and related data structures for a remote chatter 
//          in the chat session
//-----------------------------------------------------------------------------
struct REMOTE_CHATTER
{
    XUID                xuid;               // XUID of remote chatter
    XMediaObject*       pVoiceQueueXMO;     // Voice queue for chatter
    LPXVOICEDECODER     pDecoderXMO;        // 1-to-1 decoder for chatter
    LPDIRECTSOUNDSTREAM pOutputStream;      // DSound mixing stream
    BOOL                bIsTalking;         // TRUE if player is talking
    BYTE*               pbStreamBuffer;     // Buffer for stream packets
    CVoiceManager*      pVoiceManager;      // Pointer to CVoiceManager
    BYTE*               pbDriftBuffer;      // Buffer for drift compensation
};



#pragma pack(push,1)
//-----------------------------------------------------------------------------
// Name: struct COMPLETED_PACKET
// Desc: Structure used to store completed packets until the game calls 
//          ProcessVoice to retrieve them
//-----------------------------------------------------------------------------
struct COMPLETED_PACKET
{
    BYTE    Port;
    BYTE    abData[1];  // Variable length compressed data
};
#pragma pack(pop)



//-----------------------------------------------------------------------------
// Name: CVoiceManager (ctor)
// Desc: Initializes member variables
//-----------------------------------------------------------------------------
CVoiceManager::CVoiceManager()
{
    ZeroMemory( &m_cfg, sizeof( VOICE_MANAGER_CONFIG ) );

    // Set up chatters
    m_pChatters                     = NULL;
    m_bIsInChatSession              = FALSE;
    m_pLoopbackChatters             = NULL;

    // Set up communicators
    m_dwConnectedCommunicators      = 0;
    m_dwMicrophoneState             = 0;
    m_dwHeadphoneState              = 0;
    m_dwLoopback                    = 0;
    m_dwEnabled                     = 0x0000000F;
    m_bVoiceThroughSpeakers         = FALSE;

    // Set up buffers
    m_pbTempEncodedPacket           = NULL;
    m_pbCompletedPackets            = NULL;
    m_dwNumCompletedPackets         = 0;

#if _DEBUG
    m_dwCurrentLogEntry         = 0;
    m_dwTotalLogEntries         = 0;
    for( DWORD i = 0; i < s_dwNumLogEntries; i++ )
        m_strDebugLog[i][0] = L'\0';
#endif // _DEBUG
}




//-----------------------------------------------------------------------------
// Name: ~CVoiceManager (dtor)
// Desc: Verifies that object was shut down properly
//-----------------------------------------------------------------------------
CVoiceManager::~CVoiceManager()
{
    assert( m_pChatters                     == NULL &&
            m_pLoopbackChatters             == NULL &&
            m_pbTempEncodedPacket           == NULL &&
            m_pbCompletedPackets            == NULL );

    assert( m_dwConnectedCommunicators == 0 );
}



//-----------------------------------------------------------------------------
// Name: InitChatter
// Desc: Helper function to initialize a REMOTE_CHATTER struct
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::InitChatter( REMOTE_CHATTER*             pChatter, 
                                    XVOICE_QUEUE_XMO_CONFIG*    pVoiceQueueCfg,
                                    DSSTREAMDESC*               pdssd )
{
    HRESULT hr = S_OK;

    // Zero out the chatter
    ZeroMemory( pChatter, sizeof( REMOTE_CHATTER ) );

    pChatter->pVoiceManager = this;

    // Create the voice queue
    hr = XVoiceQueueCreateMediaObject( pVoiceQueueCfg, &pChatter->pVoiceQueueXMO );
    if( FAILED( hr ) )
        return hr;

    // Create the decoder
    hr = XVoiceCreateOneToOneDecoder( &pChatter->pDecoderXMO );
    if( FAILED( hr ) )
        return hr;

    // Create a stream with the specified context
    pdssd->lpvContext = pChatter;
    hr = DirectSoundCreateStream( pdssd, &pChatter->pOutputStream );
    if( FAILED( hr ) )
        return hr;

    // Set the stream headroom to 0
    pChatter->pOutputStream->SetHeadroom( 0 );

    // Allocate buffer for stream data
    pChatter->pbStreamBuffer = new BYTE[ m_dwBufferSize ];
    if( !pChatter->pbStreamBuffer )
        return E_OUTOFMEMORY;

    // Allocate drift compensation buffer and fill with silence
    pChatter->pbDriftBuffer = new BYTE[ m_dwPacketSize ];
    if( !pChatter->pbDriftBuffer )
        return E_OUTOFMEMORY;
    ZeroMemory( pChatter->pbDriftBuffer, m_dwPacketSize );

    // Start the stream with silence from the drift compensation buffer
    for( DWORD i = 0; i < NUM_PACKETS; i++ )
    {
        SubmitStreamPacket( i, pChatter );
    }

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: DSPMixBinToDSoundMixBin
// Desc: Converts a DSP mix bin (retrieved from a DSP effect state block) to a 
//          DirectSound mix bin constant (i.e., DSMIXBIN_FRONT_LEFT)
//-----------------------------------------------------------------------------
inline static DWORD DSPMixBinToDSoundMixBin( DWORD dwDSPMixBin )
{
    const DWORD DSP_MIXBIN_BASE = 0x1400;
    const DWORD DSP_MIXBIN_OFFSET = 0x20;

    // Make sure this is a valid DSP mixbin address
    assert( dwDSPMixBin >= DSP_MIXBIN_BASE &&
            dwDSPMixBin <= DSP_MIXBIN_BASE + DSMIXBIN_FXSEND_LAST * DSP_MIXBIN_OFFSET &&
            ( dwDSPMixBin - DSP_MIXBIN_BASE ) % DSP_MIXBIN_OFFSET == 0 );

    return ( dwDSPMixBin - DSP_MIXBIN_BASE ) / DSP_MIXBIN_OFFSET;
}



// Disable compiler warning about skipped initializations
#pragma warning( push )
#pragma warning( disable : 4533 )
//-----------------------------------------------------------------------------
// Name: Initialize
// Desc: Initializes the voice manager object
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::Initialize( VOICE_MANAGER_CONFIG* pConfig )
{
    HRESULT hr = S_OK;

    // Must have at least 1 remote player
    assert( pConfig->dwMaxRemotePlayers > 0 );

    // Voice packets must be a multiple of 20ms
    assert( pConfig->dwVoicePacketTime % 20 == 0 );

    // Grab the config parameters
    memcpy( &m_cfg, pConfig, sizeof( VOICE_MANAGER_CONFIG ) );

    // Calculate other useful constants
    m_dwPacketSize          = ( m_cfg.dwVoicePacketTime * VOICE_SAMPLING_RATE / 1000 ) * 2;
    m_dwBufferSize          = m_dwPacketSize * NUM_PACKETS;
    m_dwCompressedSize      = m_cfg.dwVoicePacketTime * 8 / 20 + 2;  // The +2 comes from the encoder
    m_dwCompletedPacketSize = m_dwCompressedSize + sizeof( BYTE );

    // Set up the wave format
    m_wfx.cbSize          = 0;
    m_wfx.nChannels       = 1;
    m_wfx.nSamplesPerSec  = VOICE_SAMPLING_RATE;
    m_wfx.wBitsPerSample  = 16;
    m_wfx.nBlockAlign     = m_wfx.wBitsPerSample / 8 * m_wfx.nChannels;
    m_wfx.nAvgBytesPerSec = m_wfx.nSamplesPerSec * m_wfx.nBlockAlign;
    m_wfx.wFormatTag      = WAVE_FORMAT_PCM;

    // Initialize communicators
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        hr = m_aVoiceCommunicators[i].Initialize( this );
        if( FAILED( hr ) )
            goto Cleanup;

        // Each communicator has an SRC effect for outputting mixed
        // voice data to system memory.  We can read from the DSP
        // image to find out what that mixbin is
        DSEFFECTMAP* pEffectMap = &m_cfg.pEffectImageDesc->aEffectMaps[ m_cfg.dwFirstSRCEffectIndex + i ];
        LPDSFX_SAMPLE_RATE_CONVERTER_STATE pSRCState = LPDSFX_SAMPLE_RATE_CONVERTER_STATE(pEffectMap->lpvStateSegment);
        m_adwHeadphoneSends[ i ] = DSPMixBinToDSoundMixBin( pSRCState->dwInMixbinPtrs[ 0 ] );

        // The SRC buffer should be twice as big as our buffers, because it
        // contains 32-bit samples instead of 16-bit
        assert( pEffectMap->dwScratchSize >= m_dwBufferSize * 2 );

        // Turn off mixbin headroom for this input mixbin
        pConfig->pDSound->SetMixBinHeadroom( m_adwHeadphoneSends[ i ], 0 );
    }

    // Voice Queue configuration
    XVOICE_QUEUE_XMO_CONFIG VoiceQueueCfg = {0};
    VoiceQueueCfg.dwMsOfDataPerPacket     = m_cfg.dwVoicePacketTime;
    VoiceQueueCfg.dwCodecBufferSize       = m_dwCompressedSize;

    // Update the stream mixbin configuration to reflect the correct headphone
    // sends
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        g_dsmbvp[ i ].dwMixBin = m_adwHeadphoneSends[ i ];
    }
    DSMIXBINS dsmb = { NUM_VOLUMEPAIRS, g_dsmbvp};
    m_bVoiceThroughSpeakers = FALSE;

    // Stream configuration
    DSSTREAMDESC dssd = {0};
    dssd.dwMaxAttachedPackets   = NUM_PACKETS;
    dssd.lpwfxFormat            = &m_wfx;
    dssd.lpMixBins              = &dsmb;
    dssd.dwFlags                = DSSTREAMCAPS_ACCURATENOTIFY;
    dssd.lpfnCallback           = StreamCallback;
    
    // Allocate space for each of our remote chatters
    m_pChatters = new REMOTE_CHATTER[ m_cfg.dwMaxRemotePlayers ];
    if( !m_pChatters )
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

    // Initialize each remote chatter
    for( DWORD i = 0; i < m_cfg.dwMaxRemotePlayers; i++ )
    {
        hr = InitChatter( &m_pChatters[i], &VoiceQueueCfg, &dssd );
        if( FAILED( hr ) )
            goto Cleanup;
    }

    // Allocate space for loopback chatters
    m_pLoopbackChatters = new REMOTE_CHATTER[ XGetPortCount() ];
    if( !m_pLoopbackChatters )
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

    // Configure our loopback chatters - we can limit the queue
    // buffering on these guys, since there's no jitter
    VoiceQueueCfg.dwInitialHighWaterMark = 2 * m_cfg.dwVoicePacketTime;
    VoiceQueueCfg.dwMinDelay             = 2 * m_cfg.dwVoicePacketTime;
    VoiceQueueCfg.dwMaxDelay             = 3 * m_cfg.dwVoicePacketTime;

    // Mixbin configuration for the loopback chatters - each loopback
    // chatter should just be outputting to the corresponding communicator's
    // mixbin
    dsmb.dwMixBinCount = 1;
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        dsmb.lpMixBinVolumePairs = &g_dsmbvp[i];
        hr = InitChatter( &m_pLoopbackChatters[ i ], &VoiceQueueCfg, &dssd );
        if( FAILED( hr ) )
            goto Cleanup;
    }

    // Allocate space for a temporary compressed packet
    m_pbTempEncodedPacket = new BYTE[ m_dwCompressedSize ];
    if( !m_pbTempEncodedPacket )
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

    // Allocate space to store completed packets until game retrieves them
    m_pbCompletedPackets = new BYTE[ m_cfg.dwMaxStoredPackets * m_dwCompletedPacketSize ];
    if( !m_pbCompletedPackets )
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

    // Calculate our expected latency
    DWORD dwExpectedLatency = 0;
    dwExpectedLatency += m_cfg.dwVoicePacketTime;    // One packet for microphone
    dwExpectedLatency += 1000 / 120;        // 1/2 frame to process
    dwExpectedLatency += 100;               // Assuming 100 ms transmit time
    dwExpectedLatency += 100;               // Guess at queue high water mark
    dwExpectedLatency += m_cfg.dwVoicePacketTime * NUM_PACKETS;   // Stream buffer
    dwExpectedLatency += m_cfg.dwVoicePacketTime * NUM_PACKETS;   // Headphone buffer
    VoiceLog( L"Initialized - Expected latency is %dms", dwExpectedLatency );

Cleanup:
    if( FAILED( hr ) )
        Shutdown();

    return hr;
}
#pragma warning( pop )


//-----------------------------------------------------------------------------
// Name: Shutdown
// Desc: Shuts down the voice manager
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::Shutdown()
{
    LeaveChatSession();

    // Tear down our remote chatters
    if( m_pChatters )
    {
        for( DWORD i = 0; i < m_cfg.dwMaxRemotePlayers; i++ )
        {
            if( m_pChatters[i].pVoiceQueueXMO )
                m_pChatters[i].pVoiceQueueXMO->Release();
            if( m_pChatters[i].pDecoderXMO )
                m_pChatters[i].pDecoderXMO->Release();
            if( m_pChatters[i].pOutputStream )
                m_pChatters[i].pOutputStream->Release();
            if( m_pChatters[i].pbStreamBuffer )
                delete[] m_pChatters[i].pbStreamBuffer;
            if( m_pChatters[i].pbDriftBuffer )
                delete[] m_pChatters[i].pbDriftBuffer;
        }

        delete[] m_pChatters;
        m_pChatters = NULL;
    }

    // Tear down our loopback chatters
    if( m_pLoopbackChatters )
    {
        for( DWORD i = 0; i < XGetPortCount(); i++ )
        {
            if( m_pLoopbackChatters[i].pVoiceQueueXMO )
                m_pLoopbackChatters[i].pVoiceQueueXMO->Release();
            if( m_pLoopbackChatters[i].pDecoderXMO )
                m_pLoopbackChatters[i].pDecoderXMO->Release();
            if( m_pLoopbackChatters[i].pOutputStream )
                m_pLoopbackChatters[i].pOutputStream->Release();
            if( m_pLoopbackChatters[i].pbStreamBuffer )
                delete[] m_pLoopbackChatters[i].pbStreamBuffer;
            if( m_pLoopbackChatters[i].pbDriftBuffer )
                delete[] m_pLoopbackChatters[i].pbDriftBuffer;
        }

        delete[] m_pLoopbackChatters;
        m_pLoopbackChatters = NULL;
    }

    // Tear down each local communicator
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        m_aVoiceCommunicators[i].Shutdown();
    }

    // Free our temporary compressed packet
    if( m_pbTempEncodedPacket )
    {
        delete[] m_pbTempEncodedPacket;
        m_pbTempEncodedPacket = NULL;
    }

    // Free the completed packet buffer
    if( m_pbCompletedPackets )
    {
        delete[] m_pbCompletedPackets;
        m_pbCompletedPackets = NULL;
    }

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: AddChatter
// Desc: Adds a chatter to the list of remote chatters
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::AddChatter( XUID xuidPlayer )
{
    assert( IsInChatSession() );
    assert( ChatterIndexFromXUID( xuidPlayer ) == m_cfg.dwMaxRemotePlayers );

    VoiceLog( L"Adding new chatter %I64x", xuidPlayer.qwUserID );

    // Find an open slot to use for this chatter
    DWORD dwNewChatterIndex;
    for( dwNewChatterIndex = 0; dwNewChatterIndex < m_cfg.dwMaxRemotePlayers; dwNewChatterIndex++ )
    {
        if( m_pChatters[ dwNewChatterIndex ].xuid.qwUserID == 0 )
            break;
    }
    assert( dwNewChatterIndex < m_cfg.dwMaxRemotePlayers );
    REMOTE_CHATTER* pNewChatter = &m_pChatters[ dwNewChatterIndex ];

    // Initialize the new chatter
    pNewChatter->xuid = xuidPlayer;
    RecalculateMixBins( pNewChatter );

    assert( ChatterIndexFromXUID( xuidPlayer ) < m_cfg.dwMaxRemotePlayers );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RemoveChatter
// Desc: Removes a chatter from the list of remote chatters
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::RemoveChatter( XUID xuidPlayer )
{
    assert( IsInChatSession() );
    assert( ChatterIndexFromXUID( xuidPlayer ) < m_cfg.dwMaxRemotePlayers );

    VoiceLog( L"Removing chatter %I64x", xuidPlayer.qwUserID );

    // If we've muted them, or they've muted us, pull them
    // out of the list.  Game code is responsible for
    // re-muting someone when they come back into the session
    for( WORD i = 0; i < XGetPortCount(); i++ )
    {
        if( IsPlayerMuted( xuidPlayer, i ) )
            UnMutePlayer( xuidPlayer, i );
        if( IsPlayerRemoteMuted( xuidPlayer, i ) )
            UnRemoteMutePlayer( xuidPlayer, i );
    }

    // Find the specified chatter
    DWORD chatterIndex = ChatterIndexFromXUID( xuidPlayer );

    // Flush the queue so it gets reset
    m_pChatters[ chatterIndex ].pVoiceQueueXMO->Flush();
    ZeroMemory( m_pChatters[ chatterIndex ].pbDriftBuffer, m_dwPacketSize );
    m_pChatters[ chatterIndex ].bIsTalking = FALSE;

    // Zero out the XUID so we know this slot is free
    ZeroMemory( &m_pChatters[ chatterIndex ].xuid, sizeof( XUID ) );
    
    assert( ChatterIndexFromXUID( xuidPlayer ) == m_cfg.dwMaxRemotePlayers );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: ResetChatter
// Desc: Resets the voice queue for a chatter - should be called whenever
//          a remote chatter re-inserts their communicator
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::ResetChatter( XUID xuidPlayer )
{
    assert( IsInChatSession() );
    assert( ChatterIndexFromXUID( xuidPlayer ) < m_cfg.dwMaxRemotePlayers );

    VoiceLog( L"Resetting chatter %I64x", xuidPlayer.qwUserID );

    // Find the specified chatter
    DWORD chatterIndex = ChatterIndexFromXUID( xuidPlayer );

    // Reset their voice queue
    m_pChatters[ chatterIndex ].pVoiceQueueXMO->Flush();
    m_pChatters[ chatterIndex ].bIsTalking = FALSE;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: CheckDeviceChanges
// Desc: Processes device changes to look for insertions and removals of
//          communicators
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::CheckDeviceChanges()
{
    DWORD dwMicrophoneInsertions;
    DWORD dwMicrophoneRemovals;
    DWORD dwHeadphoneInsertions;
    DWORD dwHeadphoneRemovals;

    // Must call XGetDevice changes to track possible removal and insertion
    // in one frame
    XGetDeviceChanges( XDEVICE_TYPE_VOICE_MICROPHONE,
                       &dwMicrophoneInsertions,
                       &dwMicrophoneRemovals );
    XGetDeviceChanges( XDEVICE_TYPE_VOICE_HEADPHONE,
                       &dwHeadphoneInsertions,
                       &dwHeadphoneRemovals );

    // Update state for removals
    m_dwMicrophoneState &= ~( dwMicrophoneRemovals );
    m_dwHeadphoneState  &= ~( dwHeadphoneRemovals );

    // Then update state for new insertions
    m_dwMicrophoneState |= ( dwMicrophoneInsertions );
    m_dwHeadphoneState  |= ( dwHeadphoneInsertions );

    for( WORD i = 0; i < XGetPortCount(); i++ )
    {
        // If either the microphone or the headphone was
        // removed since last call, remove the communicator
        if( m_dwConnectedCommunicators & ( 1 << i ) &&
            ( ( dwMicrophoneRemovals   & ( 1 << i ) ) ||
              ( dwHeadphoneRemovals    & ( 1 << i ) ) ) )
        {
            OnCommunicatorRemoved( i );
        }

        // If both microphone and headphone are present, and
        // we didn't have a communicator here last frame, and
        // the communicator is enabled, then register the insertion
        if( ( m_dwMicrophoneState         & ( 1 << i ) ) &&
            ( m_dwHeadphoneState          & ( 1 << i ) ) &&
            !( m_dwConnectedCommunicators & ( 1 << i ) ) &&
            ( m_dwEnabled                 & ( 1 << i ) ) )
        {
            OnCommunicatorInserted( i );
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnCompletedPacket
// Desc: Called whenever a packet is completed, so that we can do the 
//          appropriate thing with it (store to send to game, loopback, etc.)
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::OnCompletedPacket( DWORD dwControllerPort, VOID* pvData, DWORD dwSize )
{
    if( m_dwLoopback & ( 1 << dwControllerPort ) )
    {
        // If this communicator is in loopback mode, we'll submit the packet
        // to the corresponding loopback chatter
        XMEDIAPACKET xmp = {0};
        xmp.dwMaxSize = dwSize;
        xmp.pvBuffer = pvData;
        m_pLoopbackChatters[ dwControllerPort ].pVoiceQueueXMO->Process( &xmp, NULL );
    }
    else
    {
        if( IsInChatSession() )
        {
            // Store the completed packet until the game retrieves it
            if( m_dwNumCompletedPackets < m_cfg.dwMaxStoredPackets )
            {
                COMPLETED_PACKET* pPacket = (COMPLETED_PACKET *)( m_pbCompletedPackets + m_dwNumCompletedPackets * m_dwCompletedPacketSize );

                pPacket->Port = (BYTE)dwControllerPort;
                memcpy( pPacket->abData, pvData, dwSize);

                m_dwNumCompletedPackets++;
            }
            else
            {
                VoiceLog( L"No space to store encoded packet - need to call ProcessVoice() more frequently!" );
            }
        }
    }

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: OnCommunicatorInserted
// Desc: Called when we detect that a communicator has physically been 
//          inserted into a controller
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::OnCommunicatorInserted( DWORD dwControllerPort )
{
    // Tell the communicator to handle the insertion
    HRESULT hr = m_aVoiceCommunicators[ dwControllerPort ].OnInsertion( dwControllerPort );

    if( SUCCEEDED( hr ) )
    {
        // Reset the headphone for the communicator - this basically
        // just entails submitting initial packets to get it going
        hr = m_aVoiceCommunicators[ dwControllerPort ].ResetHeadphone();
        if( FAILED( hr ) )
            goto Cleanup;

        // Do the same for the microphone
        hr = m_aVoiceCommunicators[ dwControllerPort ].ResetMicrophone();
        if( FAILED( hr ) )
            goto Cleanup;

        // Now that everything has succeeded, we can mark the communicator
        // as being present.
        m_dwConnectedCommunicators |= ( 1 << dwControllerPort );

        // Notify the title that the communicator was inserted
        m_cfg.pfnCommunicatorCallback( dwControllerPort, 
                                       VOICE_COMMUNICATOR_INSERTED, 
                                       m_cfg.pCallbackContext );
    }

Cleanup:
    if( FAILED( hr ) )
    {
        // This could happen if the communicator gets removed immediately
        // after being inserted, and should be considered a normal scenario
        m_aVoiceCommunicators[ dwControllerPort ].OnRemoval();
    }

    return hr;
}



//-----------------------------------------------------------------------------
// Name: OnCommunicatorRemoved
// Desc: Called when we detect that a communicator has physically been
//          removed from a controller
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::OnCommunicatorRemoved( DWORD dwControllerPort )
{
    // Mark the communicator as having been removed
    m_dwConnectedCommunicators &= ~( 1 << dwControllerPort );
    m_aVoiceCommunicators[dwControllerPort].OnRemoval();

    // Notify the title that the communicator was removed
    m_cfg.pfnCommunicatorCallback( dwControllerPort, 
                                   VOICE_COMMUNICATOR_REMOVED, 
                                   m_cfg.pCallbackContext );

    // Flush the queue for the loopback chatter
    m_pLoopbackChatters[ dwControllerPort ].pVoiceQueueXMO->Flush();
    m_pLoopbackChatters[ dwControllerPort ].bIsTalking = FALSE;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: EnterChatSession
// Desc: Brings the box into the chat session
//-----------------------------------------------------------------------------
VOID CVoiceManager::EnterChatSession()
{
    if( IsInChatSession() )
        return;

    m_bIsInChatSession = TRUE;

    for( WORD i = 0; i < XGetPortCount(); i++ )
    {
        assert( m_MuteList[i].empty() );
        assert( m_RemoteMuteList[i].empty() );
    }
}



//-----------------------------------------------------------------------------
// Name: LeaveChatSession
// Desc: Leaves the chat session
//-----------------------------------------------------------------------------
VOID CVoiceManager::LeaveChatSession()
{
    if( !IsInChatSession() )
        return;

    // Remove all our remote chatters - this will
    // also un-mute/un-remote-mute them all
    for( DWORD i = 0; i < m_cfg.dwMaxRemotePlayers; i++ )
    {
        if( m_pChatters[i].xuid.qwUserID != 0 )
        {
            RemoveChatter( m_pChatters[i].xuid );
        }
    }

    m_bIsInChatSession = FALSE;
}



//-----------------------------------------------------------------------------
// Name: ChatterIndexFromXUID
// Desc: Finds the index into m_pChatters for the given player XUID.
//          Returns m_cfg.dwMaxRemotePlayers if not found
//-----------------------------------------------------------------------------
DWORD CVoiceManager::ChatterIndexFromXUID( XUID xuidPlayer )
{
    assert( xuidPlayer.qwUserID != 0 );

    for( DWORD i = 0; i < m_cfg.dwMaxRemotePlayers; i++ )
    {
        if( XOnlineAreUsersIdentical( &m_pChatters[i].xuid, &xuidPlayer ) )
            return i;
    }

    // Not found - return max
    return m_cfg.dwMaxRemotePlayers;
}



//-----------------------------------------------------------------------------
// Name: MutePlayer
// Desc: Handles muting a chatter
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::MutePlayer( XUID xuidPlayer, DWORD dwControllerPort )
{
    VoiceLog( L"Muting player %I64x for port %d", xuidPlayer.qwUserID, dwControllerPort );

    assert( IsInChatSession() );
    assert( !IsPlayerMuted( xuidPlayer, dwControllerPort ) );

    // Add them to our mute list
    m_MuteList[ dwControllerPort ].push_back( xuidPlayer );

    // Update mixbin settings to reflect the new mute settings
    DWORD dwChatterIndex = ChatterIndexFromXUID( xuidPlayer );
    assert( dwChatterIndex < m_cfg.dwMaxRemotePlayers );
    RecalculateMixBins( &m_pChatters[ dwChatterIndex ] );

    assert( IsPlayerMuted( xuidPlayer, dwControllerPort ) );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: UnMutePlayer
// Desc: Handles un-muting a chatter
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::UnMutePlayer( XUID xuidPlayer, DWORD dwControllerPort )
{
    VoiceLog( L"UnMuting player %I64x for port %d", xuidPlayer.qwUserID, dwControllerPort );

    assert( IsInChatSession() );
    assert( IsPlayerMuted( xuidPlayer, dwControllerPort ) );

    // Find the entry in our mute list and remove it
    for( MuteList::iterator it = m_MuteList[ dwControllerPort ].begin(); it < m_MuteList[ dwControllerPort ].end(); ++it )
    {
        if( XOnlineAreUsersIdentical( it, &xuidPlayer ) )
        {
            m_MuteList[ dwControllerPort ].erase( it );
            break;
        }
    }

    // Update mixbin settings to reflect the new mute settings
    DWORD dwChatterIndex = ChatterIndexFromXUID( xuidPlayer );
    assert( dwChatterIndex < m_cfg.dwMaxRemotePlayers );
    RecalculateMixBins( &m_pChatters[ dwChatterIndex ] );

    assert( !IsPlayerMuted( xuidPlayer, dwControllerPort ) );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: IsPlayerMuted
// Desc: Returns TRUE if the player is in our mute list
//-----------------------------------------------------------------------------
BOOL CVoiceManager::IsPlayerMuted( XUID xuidPlayer, DWORD dwControllerPort )
{
    // Look for the player in our mute list
    for( MuteList::iterator it = m_MuteList[ dwControllerPort ].begin(); it < m_MuteList[ dwControllerPort ].end(); ++it )
    {
        if( XOnlineAreUsersIdentical( it, &xuidPlayer ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}



//-----------------------------------------------------------------------------
// Name: RemoteMutePlayer
// Desc: Handles being muted by a different player
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::RemoteMutePlayer( XUID xuidPlayer, DWORD dwControllerPort )
{
    VoiceLog( L"Remote muted port %d by player %I64x", dwControllerPort, xuidPlayer.qwUserID );

    assert( IsInChatSession() );
    assert( !IsPlayerRemoteMuted( xuidPlayer, dwControllerPort ) );

    // Add them to our remote mute list
    m_RemoteMuteList[ dwControllerPort ].push_back( xuidPlayer );

    // Update mixbin settings to reflect the new mute settings
    DWORD dwChatterIndex = ChatterIndexFromXUID( xuidPlayer );
    assert( dwChatterIndex < m_cfg.dwMaxRemotePlayers );
    RecalculateMixBins( &m_pChatters[ dwChatterIndex ] );

    assert( IsPlayerRemoteMuted( xuidPlayer, dwControllerPort ) );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: UnRemoteMutePlayer
// Desc: Handles removing a remote-mute from another player
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::UnRemoteMutePlayer( XUID xuidPlayer, DWORD dwControllerPort )
{
    VoiceLog( L"Remote UnMuted port %d by player %I64x", dwControllerPort, xuidPlayer.qwUserID );

    assert( IsInChatSession() );
    assert( IsPlayerRemoteMuted( xuidPlayer, dwControllerPort ) );

    // Remove them from our remote mute list
    for( MuteList::iterator it = m_RemoteMuteList[ dwControllerPort ].begin(); it < m_RemoteMuteList[ dwControllerPort ].end(); ++it )
    {
        if( XOnlineAreUsersIdentical( it, &xuidPlayer ) )
        {
            m_RemoteMuteList[ dwControllerPort ].erase( it );
            break;
        }
    }

    // Update mixbin settings to reflect the new mute settings
    DWORD dwChatterIndex = ChatterIndexFromXUID( xuidPlayer );
    assert( dwChatterIndex < m_cfg.dwMaxRemotePlayers );
    RecalculateMixBins( &m_pChatters[ dwChatterIndex ] );

    assert( !IsPlayerRemoteMuted( xuidPlayer, dwControllerPort ) );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: IsPlayerRemoteMuted
// Desc: Returns TRUE if the player has been remotely muted
//-----------------------------------------------------------------------------
BOOL CVoiceManager::IsPlayerRemoteMuted( XUID xuidPlayer, DWORD dwControllerPort )
{
    for( MuteList::iterator it = m_RemoteMuteList[ dwControllerPort ].begin(); it < m_RemoteMuteList[ dwControllerPort ].end(); ++it )
    {
        if( XOnlineAreUsersIdentical( it, &xuidPlayer ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}



//-----------------------------------------------------------------------------
// Name: IsPlayerTalking
// Desc: Returns TRUE if the player is currently talking
//-----------------------------------------------------------------------------
BOOL CVoiceManager::IsPlayerTalking( XUID xuidPlayer )
{
    DWORD chatterIndex = ChatterIndexFromXUID( xuidPlayer );
    assert( chatterIndex < m_cfg.dwMaxRemotePlayers );

    return( m_pChatters[ chatterIndex ].bIsTalking );
}



//-----------------------------------------------------------------------------
// Name: ReceivePacket
// Desc: Handles receipt of a voice packet from the network
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::ReceivePacket( XUID xuidFromPlayer, VOID* pvData, INT nSize )
{
    assert( IsInChatSession() );
    assert( nSize == (INT)m_dwCompressedSize );

    DWORD chatterIndex = ChatterIndexFromXUID( xuidFromPlayer );
    if( chatterIndex < m_cfg.dwMaxRemotePlayers )
    {
        XMEDIAPACKET xmp = {0};

        // Send the packet to the queue
        xmp.pvBuffer  = pvData;
        xmp.dwMaxSize = nSize;
        m_pChatters[ chatterIndex ].pVoiceQueueXMO->Process( &xmp, NULL );
    }
    else
    {
        // This could happen if a remote player got our ADD_CHATTER message
        // and starts sending us voice, all before we get an ADD_CHATTER
        // message from them.  If it happens for a long period of time, it
        // probably means that an ADD_CHATTER message got lost somewhere.
        VoiceLog( L"Got packet from player %I64x, but no queue set up for them", xuidFromPlayer.qwUserID );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: EnableCommunicator
// Desc: Enables or disabled voice on the specified controller.  If a 
//          controller has voice DISABLED, then no voice will be sent to
//          that peripheral, and no input will be captured FROM the peripheral
//          To the game code, it will look as if there is no communicator
//          plugged in.  If a game has a scenario where they want to know
//          that a communicator is plugged in, but still want it disabled,
//          then you'd want to change this so that the communicator was still
//          recognized, but just never processed.  Requires more work on the
//          game side to differentiate an inserted-but-banned communicator.
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::EnableCommunicator( DWORD dwControllerPort, BOOL bEnabled )
{
    if( bEnabled )
    {
        // All we need to do is set the flag - if a communicator is currently
        // plugged in, it will be picked up in the next call to 
        // CheckDeviceChanges
        m_dwEnabled |= ( 1 << dwControllerPort );
    }
    else
    {
        m_dwEnabled &= ~( 1 << dwControllerPort );
        
        // Pretend the communicator was removed.  Having the enabled flag
        // cleared will prevent it from being re-added in CheckDeviceChanges
        if( m_dwConnectedCommunicators & ( 1 << dwControllerPort ) )
        {
            OnCommunicatorRemoved( dwControllerPort );
        }
    }
        
    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: SetVoiceMask
// Desc: Controls voice masking for the specified controller.
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::SetVoiceMask( DWORD dwControllerPort, XVOICE_MASK mask )
{
    return m_aVoiceCommunicators[ dwControllerPort ].m_pEncoderXMO->SetVoiceMask( 0, &mask );
}



//-----------------------------------------------------------------------------
// Name: SetLoopback
// Desc: Sets a voice communicator to loop back on itself, rather than 
//          producing data.  Useful for voice mask testing
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::SetLoopback( DWORD dwControllerPort, BOOL bLoopback )
{
    if( bLoopback )
    {
        m_dwLoopback |= ( 1 << dwControllerPort );

        m_pLoopbackChatters[ dwControllerPort ].pVoiceQueueXMO->Flush();
    }
    else
    {
        m_dwLoopback &= ~( 1 << dwControllerPort );
    }

    // Update mixbin settings to reflect loopback change
    for( DWORD i = 0; i < m_cfg.dwMaxRemotePlayers; i++ )
    {
        RecalculateMixBins( &m_pChatters[i] );
    }
        
    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: RecalculateMixBins
// Desc: Helper function to recalculate mix bin volumes
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::RecalculateMixBins( REMOTE_CHATTER* pChatter )
{
    DSMIXBINS dsmb = { NUM_VOLUMEPAIRS, g_dsmbvp };
    if( m_bVoiceThroughSpeakers )
    {
        // Voice Through Speakers ON:
        // Turn all headphone sends off, and check to see if any
        // player has muted or been muted by the remote chatter.
        // If anyone's muted, turn speaker sends off, else turn the
        // speaker sends on
        BOOL bMuted = FALSE;
        for( DWORD i = 0; i < XGetPortCount(); i++ )
        {
            if( IsPlayerMuted( pChatter->xuid, i ) ||
                IsPlayerRemoteMuted( pChatter->xuid, i ) )
            {
                bMuted = TRUE;
            }
            g_dsmbvp[i].lVolume = DSBVOLUME_MIN;
        }
        LONG lSpeakerVolume = bMuted ? DSBVOLUME_MIN : DSBVOLUME_MAX;
        for( DWORD i = XGetPortCount(); i < NUM_VOLUMEPAIRS; i++ )
        {
            g_dsmbvp[i].lVolume = lSpeakerVolume;
        }
    }
    else
    {
        // Voice Through Speakers OFF:
        // Set each headphone send appropriately based on whether or
        // not they've muted or been muted by the remote talker, and
        // whether or not that headphone is in loopback mode
        // Turn the speaker sends off
        for( DWORD i = 0; i < XGetPortCount(); i++ )
        {
            if( IsPlayerMuted( pChatter->xuid, i ) ||
                IsPlayerRemoteMuted( pChatter->xuid, i ) ||
                ( m_dwLoopback & ( 1 << i ) ) )
            {
                g_dsmbvp[i].lVolume = DSBVOLUME_MIN;
            }
            else
            {
                g_dsmbvp[i].lVolume = DSBVOLUME_MAX;
            }
        }
        for( DWORD i = XGetPortCount(); i < NUM_VOLUMEPAIRS; i++ )
        {
            g_dsmbvp[i].lVolume = DSBVOLUME_MIN;
        }
    }

    pChatter->pOutputStream->SetMixBinVolumes( &dsmb );
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: SetVoiceThroughSpeakers
// Desc: Toggles Voice Through Speakers off and on
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::SetVoiceThroughSpeakers( BOOL bEnabled )
{
    if( bEnabled == m_bVoiceThroughSpeakers )
        return S_OK;

    // Recalculate mix bin settings for all remote chatters
    m_bVoiceThroughSpeakers = bEnabled;
    for( DWORD i = 0; i < m_cfg.dwMaxRemotePlayers; i++ )
    {
        if( m_pChatters[i].xuid.qwUserID != 0 )
            RecalculateMixBins( &m_pChatters[i] );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetDriftCompensationPacket
// Desc: Fills out an XMEDIAPACKET pointing to the drift compensation buffer
//          for the specified chatter
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::GetDriftCompensationPacket( XMEDIAPACKET* pPacket, REMOTE_CHATTER* pChatter )
{
    ZeroMemory( pPacket, sizeof( XMEDIAPACKET ) );

    pPacket->pvBuffer   = pChatter->pbDriftBuffer;
    pPacket->dwMaxSize  = m_dwPacketSize;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: GetTemporaryPacket
// Desc: Fills out an XMEDIAPACKET pointing to a temporary (compressed) buffer
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::GetTemporaryPacket( XMEDIAPACKET* pPacket )
{
    ZeroMemory( pPacket, sizeof( XMEDIAPACKET ) );

    pPacket->pvBuffer   = m_pbTempEncodedPacket;
    pPacket->dwMaxSize  = m_dwCompressedSize;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: GetStreamPacket
// Desc: Fills out an XMEDIAPACKET pointing to the next stream packet for
//          the specified chatter
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::GetStreamPacket( XMEDIAPACKET* pPacket, REMOTE_CHATTER* pChatter, DWORD dwIndex )
{
    ZeroMemory( pPacket, sizeof( XMEDIAPACKET ) );

    pPacket->pvBuffer   = pChatter->pbStreamBuffer + dwIndex * m_dwPacketSize;
    pPacket->dwMaxSize  = m_dwPacketSize;
    pPacket->pContext   = (LPVOID)dwIndex;

    ZeroMemory( pPacket->pvBuffer, pPacket->dwMaxSize );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: SubmitStreamPacket
// Desc: Fills the next stream packet from the drift compensation buffer
//          and submits to the stream
//-----------------------------------------------------------------------------
inline
HRESULT CVoiceManager::SubmitStreamPacket( DWORD dwIndex, REMOTE_CHATTER* pChatter )
{
    XMEDIAPACKET xmp;
    GetStreamPacket( &xmp, pChatter, dwIndex );
    memcpy( xmp.pvBuffer, pChatter->pbDriftBuffer, m_dwPacketSize );
    return pChatter->pOutputStream->Process( &xmp, NULL );
}



//-----------------------------------------------------------------------------
// Name: MicrophonePacketCallback
// Desc: Called at DPC time when a packet has been completed by one of the
//          microphones.  We can just encode the packet and add it to our 
//          bundle of stored completed packets
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::MicrophonePacketCallback( DWORD dwPort, LPVOID pPacketContext, DWORD dwStatus )
{
    HRESULT             hr;
    CVoiceCommunicator* pCommunicator = &m_aVoiceCommunicators[ dwPort ];

    // If the packet failed or was flushed, it means that we either released
    // the device, or the device was removed. 
    if( dwStatus != XMEDIAPACKET_STATUS_SUCCESS )
    {
        assert( dwStatus == XMEDIAPACKET_STATUS_FAILURE ||
                dwStatus == XMEDIAPACKET_STATUS_FLUSHED );

        return S_FALSE;
    }

    // Save floating point state
    XSaveFloatingPointStateForDpc();

    // Grab the completed microphone packet
    XMEDIAPACKET        xmpMicrophone;
    pCommunicator->GetMicrophonePacket( &xmpMicrophone, (DWORD)pPacketContext );

    // We should only encode if we're currently in a chat session,
    // or if the microphone is in loopback mode
    if( IsInChatSession() || m_dwLoopback & ( 1 << dwPort ) )
    {
        // Grab a temporary compressed packet
        XMEDIAPACKET        xmpCompressed;
        DWORD               dwCompressedSize;
        GetTemporaryPacket( &xmpCompressed );
        xmpCompressed.pdwCompletedSize = &dwCompressedSize;

        // Run the encoder
        hr = pCommunicator->m_pEncoderXMO->ProcessMultiple( 1, &xmpMicrophone, 1, &xmpCompressed );
        assert( SUCCEEDED( hr ) );
        assert( dwCompressedSize == m_dwCompressedSize ||
                dwCompressedSize == 0 );

        // If the compressed size is greater than zero, that means we detected
        // voice.  Otherwise, it was silence
        if( dwCompressedSize > 0 )
        {
            // If voice was detected, increase the ramp time for lowering
            // headphone volume
            if( pCommunicator->m_dwRampTime < MAX_RAMP_TIME )
                pCommunicator->m_dwRampTime += m_cfg.dwVoicePacketTime;

            OnCompletedPacket( dwPort, xmpCompressed.pvBuffer, dwCompressedSize );
        }
        else
        {
            // If the mic was silent, decrease the ramp time so that we
            // start bringing the headphone volume back up
            if( pCommunicator->m_dwRampTime > 0 )
                pCommunicator->m_dwRampTime -= m_cfg.dwVoicePacketTime;
        }
    }

    // Re-submit the packet back to the microphone
    if( FAILED( pCommunicator->SubmitMicrophonePacket( &xmpMicrophone ) ) )
    {
        // This will only happen if the device has been removed,
        // in which case we'll pick it up on the next call to
        // CheckDeviceChanges
    }

    // Restore floating point state
    XRestoreFloatingPointStateForDpc();

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: StreamCallback
// Desc: Stream DPC callback function - called whenever one of the mixing
//          streams completes a packet, so that we can decode a new one and
//          re-submit
//-----------------------------------------------------------------------------
VOID CALLBACK StreamCallback( LPVOID pStreamContext, LPVOID pPacketContext, DWORD dwStatus )
{
    REMOTE_CHATTER* pThis = (REMOTE_CHATTER*)pStreamContext;
    pThis->pVoiceManager->StreamPacketCallback( pThis, pPacketContext, dwStatus );
}



//-----------------------------------------------------------------------------
// Name: StreamPacketCallback
// Desc: Called at DPC time whenever a stream has completed a packet (this
//          includes both regular chatter streams as well as loopback chatter
//          streams).  Attempts to get a new packet from the corresponding
//          queue, decode it, and resubmit to the stream.  If no packet is
//          available, submits a packet of silence
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::StreamPacketCallback( REMOTE_CHATTER* pChatter, LPVOID pPacketContext, DWORD dwStatus )
{
    if( dwStatus != XMEDIAPACKET_STATUS_SUCCESS )
    {
        // Streams don't fail
        assert( dwStatus == XMEDIAPACKET_STATUS_FLUSHED );
        return S_FALSE;
    }

    // Get the drift compensation packet
    XMEDIAPACKET xmpDriftPacket;
    GetDriftCompensationPacket( &xmpDriftPacket, pChatter );

    // Grab as many packets from the queue as it is willing to
    // produce. 
    // Zero Packets: The stream is running slightly faster than
    //      the queue and will re-use the packet stored in the 
    //      drift compensation buffer
    // One Packet: Normal scenario - copy output to drift
    //      compensation buffer and submit to stream
    // Two Packets: The stream is running slightly slower than
    //      the queue and will overwrite a packet in the
    //      drift compensation buffer, then submit the last
    //      packet to the stream.
    DWORD dwQueueStatus;
    pChatter->pVoiceQueueXMO->GetStatus( &dwQueueStatus );
    while( dwQueueStatus & XMO_STATUSF_ACCEPT_OUTPUT_DATA )
    {
        // Queue has a packet ready
        HRESULT         hr;
        XMEDIAPACKET    xmpCompressed;
        DWORD           dwSizeFromQueue;

        // Grab a temporary packet
        GetTemporaryPacket( &xmpCompressed );
        xmpCompressed.pdwCompletedSize = &dwSizeFromQueue;

        // Get a packet from the queue - this must succeed, since the
        // queue just told us it was ready to give us data
        hr = pChatter->pVoiceQueueXMO->Process( NULL, &xmpCompressed );
        if( dwSizeFromQueue > 0 )
        {
            assert( dwSizeFromQueue == m_dwCompressedSize );

            // The queue gave us a compressed packet - we need to decode it
            pChatter->bIsTalking = TRUE;

            // Save floating point state
            XSaveFloatingPointStateForDpc();

            hr = pChatter->pDecoderXMO->ProcessMultiple( 1, &xmpCompressed, 1, &xmpDriftPacket );
            assert( SUCCEEDED( hr ) );

            // Restore floating point state
            XRestoreFloatingPointStateForDpc();
        }
        else
        {
            pChatter->bIsTalking = FALSE;

            // Re-fill the drift compensation packet with silence
            ZeroMemory( xmpDriftPacket.pvBuffer, m_dwPacketSize );
        }

        pChatter->pVoiceQueueXMO->GetStatus( &dwQueueStatus );
    }

    if( FAILED( SubmitStreamPacket( (DWORD)pPacketContext, pChatter ) ) )
    {
        // The only way this would fail is if we submitted too many packets
        assert( FALSE );
    }

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: GetSRCInfo
// Desc: Retrieves information about the state of the specified SRC DSP effect
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::GetSRCInfo( DWORD dwControllerPort, 
                                   DWORD* pdwBufferSize,
                                   VOID** ppvBufferData, 
                                   DWORD* pdwWritePosition )
{
    // Get the state segment for the appropriate SRC effect
    DSEFFECTMAP* pEffectMap = &m_cfg.pEffectImageDesc->aEffectMaps[ m_cfg.dwFirstSRCEffectIndex + dwControllerPort ];
    LPCDSFX_SAMPLE_RATE_CONVERTER_PARAMS pSRCParams = LPCDSFX_SAMPLE_RATE_CONVERTER_PARAMS(pEffectMap->lpvStateSegment);

    if( pdwBufferSize )
        *pdwBufferSize = pEffectMap->dwScratchSize;

    if( ppvBufferData )
        *ppvBufferData = pEffectMap->lpvScratchSegment;

    if( pdwWritePosition )
        *pdwWritePosition = pSRCParams->dwScratchSampleOffset;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: SetSRCGain
// Desc: Sets the gain level for the SRC output, between 0 and 1.  Note that
//          the SRC effect scales output by 8x, so unity gain is really 1/8 or
//          0.125.
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::SetSRCGain( DWORD dwControllerPort, FLOAT fGain )
{
    assert( fGain >= 0.0f && fGain <= 1.0f );

    // Convert from IEEE float to DSP s.23 format
    DWORD dwGainParam = DWORD( fGain * 0x7FFFFF );
    return m_cfg.pDSound->SetEffectData( m_cfg.dwFirstSRCEffectIndex + dwControllerPort,
                                         32,   // offset to gain param
                                         &dwGainParam,
                                         sizeof( DWORD ),
                                         DSFX_IMMEDIATE );
 
}



//-----------------------------------------------------------------------------
// Name: HeadphonePacketCallback
// Desc: Called at DPC time whenever one of the communicator headphones
//          completes a packet.  We need to re-fill the packet from the SRC
//          output and re-submit to the headphone
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::HeadphonePacketCallback( DWORD dwPort, LPVOID pPacketContext, DWORD dwStatus )
{
    CVoiceCommunicator* pCommunicator = &m_aVoiceCommunicators[ dwPort ];

    // If the packet failed or was flushed, it means that we either released
    // the device, or the device was removed. 
    if( dwStatus != XMEDIAPACKET_STATUS_SUCCESS )
    {
        assert( dwStatus == XMEDIAPACKET_STATUS_FAILURE ||
                dwStatus == XMEDIAPACKET_STATUS_FLUSHED );

        return S_FALSE;
    }

    // Grab packets of audio data from the output of the Sample 
    // Rate Converter DSP effect.  Right now, the effect is outputting
    // 32 bit samples instead of 16, so we have to downconvert them.
    // In the future, the SRC effect will be able to output 16
    // bit samples, and this will no longer be necessary
    DWORD dwCircularBufferSize;
    DWORD dwWritePosition;
    VOID* pvBufferData;
    GetSRCInfo( (WORD)dwPort, &dwCircularBufferSize, &pvBufferData, &dwWritePosition );

    DWORD dwSrcPacketSize = m_dwPacketSize * 2;
    DWORD dwBytesAvailable = ( dwWritePosition + dwCircularBufferSize - pCommunicator->m_dwSRCReadPosition ) % dwCircularBufferSize;

    // Grab the completed headphone packet
    XMEDIAPACKET xmpHeadphone;
    pCommunicator->GetHeadphonePacket( &xmpHeadphone, (DWORD)pPacketContext );

    // We'll convert from 32-bit to 16-bit samples as we copy into the 
    // headphone buffer
    DWORD  dwSamplesToStream = m_dwPacketSize / sizeof( WORD );
    PLONG  pSrcData = (PLONG)( (BYTE*)pvBufferData + pCommunicator->m_dwSRCReadPosition );
    PWORD  pDestData = (PWORD)xmpHeadphone.pvBuffer;

    // If we're wrapping around to the start of our circular
    // buffer, then process the last little bit here
    if( dwSrcPacketSize > dwCircularBufferSize - pCommunicator->m_dwSRCReadPosition )
    {
        DWORD dwPartial = ( dwCircularBufferSize - pCommunicator->m_dwSRCReadPosition ) / sizeof( DWORD );
        for( DWORD dwSample = 0; dwSample < dwPartial; dwSample++ )
            *pDestData++ = WORD( *pSrcData++ >> 16 );

        assert( (PLONG)pSrcData == (PLONG)( (BYTE*)pvBufferData + dwCircularBufferSize ) );

        // Adjust source pointer and sample count for main loop below
        dwSamplesToStream -= dwPartial;
        pSrcData = (PLONG)pvBufferData;
    }

    // Now copy over the rest of the packet
    for( DWORD dwSample = 0; dwSample < dwSamplesToStream; dwSample++ )
        *pDestData++ = WORD( *pSrcData++ >> 16 );

    if( dwBytesAvailable >= dwSrcPacketSize )
    {
        // Adjust our read position
        pCommunicator->m_dwSRCReadPosition = ( pCommunicator->m_dwSRCReadPosition + dwSrcPacketSize ) % dwCircularBufferSize;
    }
    else
    {
        // This means that for some reason, there wasn't enough data in the 
        // SRC output buffer to fill up a headphone packet, so we probably
        // put some garbage in.  We re-set the communicator's SRC read
        // position to be half a packet before the current write position, so that 
        // when the next packet completes, we can be pretty sure that we'll
        // have a full packet
        // VoiceLog( L"Not enough data available for headphone %d!", dwPort );
        pCommunicator->m_dwSRCReadPosition = ( dwWritePosition + dwCircularBufferSize - dwSrcPacketSize / 2 ) % dwCircularBufferSize;
    }

    // Re-submit the packet to the headphone
    if( FAILED( pCommunicator->SubmitHeadphonePacket( &xmpHeadphone ) ) )
    {
        // This will only happen if the device has been removed,
        // in which case we'll pick it up on the next call to
        // CheckDeviceChanges
    }

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: ProcessVoice
// Desc: Main task pump to be called from the title - this should be called
//          once per frame.  If not, it's possible to lose voice data.
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::ProcessVoice()
{
    // Adjust the gain level of each headphone output based on whether or
    // not (and for how long) that player is talking
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        if( m_dwConnectedCommunicators & ( 1 << i ) )
        {
            FLOAT fGain = MAX_GAIN - ( m_aVoiceCommunicators[i].m_dwRampTime * ( MAX_GAIN - MIN_GAIN ) / MAX_RAMP_TIME );
            SetSRCGain( i, fGain );
        }
    }

    // Fire callbacks for all buffered packets
    if( m_cfg.pfnVoiceDataCallback )
    {
        for( DWORD i = 0; i < m_dwNumCompletedPackets; i++ )
        {
            COMPLETED_PACKET* pPacket = (COMPLETED_PACKET*)( m_pbCompletedPackets + i * m_dwCompletedPacketSize );
            m_cfg.pfnVoiceDataCallback( pPacket->Port,
                                        m_dwCompressedSize, 
                                        pPacket->abData, 
                                        m_cfg.pCallbackContext );
        }
    }
    m_dwNumCompletedPackets = 0;

    // Check for insertions and removals
    CheckDeviceChanges();

#if _DEBUG
    ValidateStateDbg();
#endif // _DEBUG
    return S_OK;
}




#if _DEBUG
//-----------------------------------------------------------------------------
// Name: VoiceLog
// Desc: Debug function for logging and displaying status messages
//-----------------------------------------------------------------------------
VOID CVoiceManager::VoiceLog( WCHAR* format, ... )
{
    va_list arglist;

    va_start( arglist, format );
    _vsnwprintf( m_strDebugLog[ m_dwCurrentLogEntry ], s_dwLogEntrySize, format, arglist );
    va_end( arglist );

    m_dwCurrentLogEntry = ( m_dwCurrentLogEntry + 1 ) % s_dwNumLogEntries;
    ++m_dwTotalLogEntries;
}



//-----------------------------------------------------------------------------
// Name: DrawColoredQuad
// Desc: Draws a quad at the specified location in a single color
//-----------------------------------------------------------------------------
VOID DrawColoredQuad( FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight, DWORD dwColor )
{
    typedef struct { XGVECTOR4 p; D3DCOLOR c; } BUFFER_VERTEX;
    BUFFER_VERTEX vQuad[4];
    vQuad[0].p = XGVECTOR4( fX, fY + fHeight, 1.0f, 1.0f );
    vQuad[0].c = dwColor;
    vQuad[1].p = XGVECTOR4( fX, fY, 1.0f, 1.0f );
    vQuad[1].c = dwColor;
    vQuad[2].p = XGVECTOR4( fX + fWidth, fY, 1.0f, 1.0f );
    vQuad[2].c = dwColor;
    vQuad[3].p = XGVECTOR4( fX + fWidth, fY + fHeight, 1.0f, 1.0f );
    vQuad[3].c = dwColor;

    IDirect3DDevice8::SetVertexShader( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );
    IDirect3DDevice8::SetRenderState( D3DRS_LIGHTING, FALSE );
    IDirect3DDevice8::SetRenderState( D3DRS_ZENABLE, FALSE );
    IDirect3DDevice8::SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    IDirect3DDevice8::SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    IDirect3DDevice8::SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    IDirect3DDevice8::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    IDirect3DDevice8::SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
    IDirect3DDevice8::SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
    IDirect3DDevice8::SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
    IDirect3DDevice8::DrawPrimitiveUP( D3DPT_QUADLIST, 1, vQuad, sizeof( BUFFER_VERTEX ) );
}



//-----------------------------------------------------------------------------
// Name: RenderBuffer
// Desc: Renders a visualization of a buffer with some filled, some empty
//          packets
//-----------------------------------------------------------------------------
VOID RenderBuffer( FLOAT fStartX, 
                   FLOAT fStartY, 
                   DWORD dwFilled, 
                   DWORD dwTotal, 
                   FLOAT fSize,
                   BOOL  bUp)
{
    FLOAT fX = fStartX;
    FLOAT fY = fStartY;

    DWORD dwEmptyColor = 0x80006000;
    DWORD dwFilledColor;
    if( dwFilled == 1 )
        dwFilledColor   = 0x80FF0000;
    else
        dwFilledColor   = 0x8000FF00;

    for( DWORD i = 0; i < dwTotal; i++ )
    {
        DrawColoredQuad( fX, fY, fSize, fSize, ( i < dwFilled ) ? dwFilledColor : dwEmptyColor );

        if( bUp )
            fY -= fSize;
        else
            fY += fSize;
    }
}


//-----------------------------------------------------------------------------
// Name: RenderDebugInfo
// Desc: Debug function for toggling debug display on/off
//-----------------------------------------------------------------------------
VOID CVoiceManager::RenderDebugInfo( CXBFont* pFont )
{
    // First render the local communicators
    FLOAT fStartX = 60.0f;
    FLOAT fSpacing = 140.0f;
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        if( m_dwConnectedCommunicators & ( 1 << i ) )
        {
            DrawColoredQuad( fStartX + fSpacing * i - 10, 360.0f, fSpacing / 2, 120.0f, 0xC0202020 );
            if( pFont )
                pFont->DrawText( fStartX + fSpacing * i, 440, 0xFFA0A0A0, L"Voice" );

            // Since the microphone processes at DPC time, it's always full
            DWORD dwMicrophoneFilled = NUM_PACKETS;
            if( pFont )
                pFont->DrawText( fStartX + fSpacing * i, 425, 0xFFA0A0A0, L"M" );
            RenderBuffer( fStartX + fSpacing * i, 420, dwMicrophoneFilled, NUM_PACKETS, 10, TRUE );

            // Check the SRC output buffer
            DWORD dwWritePosition;
            DWORD dwBufferSize;
            GetSRCInfo( (WORD)i, &dwBufferSize, NULL, &dwWritePosition );
            DWORD dwSRCFilled = ( ( dwWritePosition + dwBufferSize - m_aVoiceCommunicators[i].m_dwSRCReadPosition ) % dwBufferSize );
            if( pFont )
                pFont->DrawText( fStartX + fSpacing * i + 20, 425, 0xFFA0A0A0, L"S" );
            // RenderBuffer( fStartX + fSpacing * i + 20, 420, dwSRCFilled, dwBufferSize / ( 2 * m_dwPacketSize ), 10, TRUE );
            DrawColoredQuad( fStartX + fSpacing * i + 20, 430 - dwSRCFilled * 20.0f / dwBufferSize, 10, dwSRCFilled * 20.0f / dwBufferSize, 0x80FFFF00 );
            DrawColoredQuad( fStartX + fSpacing * i + 20, 410, 10, ( dwBufferSize - dwSRCFilled ) * 20.0f / dwBufferSize, 0x80006000 );

            // Since the headphone processes at DPC time, it's always full
            DWORD dwHeadphoneFilled = NUM_PACKETS;
            if( pFont )
                pFont->DrawText( fStartX + fSpacing * i + 40, 425, 0xFFA0A0A0, L"H" );
            RenderBuffer( fStartX + fSpacing * i + 40, 420, dwHeadphoneFilled, NUM_PACKETS, 10, TRUE );
        }
    }

    // Now render each remote chatter
    fStartX = 60.0f;
    fSpacing = 120.0f;
    DWORD dwSlot = 0;
    for( DWORD i = 0; i < m_cfg.dwMaxRemotePlayers; i++ )
    {
        if( m_pChatters[i].xuid.qwUserID == 0 )
            continue;

        DrawColoredQuad( fStartX + fSpacing * dwSlot - 10, 40, fSpacing, 100, 0xC0202020 );
        WCHAR str[100];
        swprintf( str, L"%I64x", m_pChatters[i].xuid.qwUserID );
        DWORD dwColor = m_pChatters[i].bIsTalking ? 0xFFFF6060 : 0xFFA0A0A0;

        if( pFont )
            pFont->DrawText( fStartX + fSpacing * dwSlot, 55, dwColor, str );

        // Since the stream processes at DPC time, it's always full
        DWORD dwStreamFilled = NUM_PACKETS;

        RenderBuffer( fStartX + fSpacing * dwSlot, 75, dwStreamFilled, NUM_PACKETS, 10, FALSE );
        ++dwSlot;
    }

    // Now render debug log text
    DrawColoredQuad( 70.0f, 160.0f, 450.0f, 30 * s_dwNumLogEntries + 20.0f, 0xC0202020 );
    for( DWORD i = 0; i < s_dwNumLogEntries; i++ )
    {
        if( pFont )
        {
            WCHAR strNum[6];
            swprintf( strNum, L"%d)", m_dwTotalLogEntries - s_dwNumLogEntries + i + 1 );
            pFont->DrawText( 80.0f, 170.0f + 30 * i, 0xFFA0A0A0, strNum );
            pFont->DrawText( 0xFFA0A0A0, m_strDebugLog[ ( i + m_dwCurrentLogEntry ) % s_dwNumLogEntries ] );
        }
    }
}



//-----------------------------------------------------------------------------
// Name: ValidateStateDbg
// Desc: Validates that the VoiceManager is in a consistent state
//-----------------------------------------------------------------------------
HRESULT CVoiceManager::ValidateStateDbg()
{
    // Only the bottom 4 bits should be used
    assert( !( m_dwConnectedCommunicators & 0xFFFFFFF0 ) );
    assert( !( m_dwMicrophoneState        & 0xFFFFFFF0 ) );
    assert( !( m_dwHeadphoneState         & 0xFFFFFFF0 ) );
    assert( !( m_dwLoopback               & 0xFFFFFFF0 ) );
    assert( !( m_dwEnabled                & 0xFFFFFFF0 ) );

    // Check the state of the chatters list
    for( DWORD i = 0; i < m_cfg.dwMaxRemotePlayers; i++ )
    {
        if( m_pChatters[i].xuid.qwUserID == 0 )
        {
            assert( !m_pChatters[i].bIsTalking );

            // The queue should be ready to accept input data, but
            // not accept output data
            DWORD dwStatus;
            m_pChatters[i].pVoiceQueueXMO->GetStatus( &dwStatus );
            assert( dwStatus == XMO_STATUSF_ACCEPT_INPUT_DATA );

            m_pChatters[i].pOutputStream->GetStatus( &dwStatus );
            assert( dwStatus == DSSTREAMSTATUS_PLAYING );
            continue;
        }

        // Make sure they're not duplicated in the chatters list
        for( DWORD j = i + 1; j < m_cfg.dwMaxRemotePlayers; j++ )
        {
            if( XOnlineAreUsersIdentical( &m_pChatters[i].xuid, &m_pChatters[j].xuid ) )
            {
                assert( FALSE && "Duplicate player in chatters list!" );
            }
        }

        // Make sure the stream is still happy
        DWORD dwStreamStatus;
        m_pChatters[i].pOutputStream->GetStatus( &dwStreamStatus );
        assert( dwStreamStatus == DSSTREAMSTATUS_PLAYING );
    }

    // Check the mute list and remote mute list - every user in them
    // should currently be in the chatters list
    for( WORD i = 0; i < XGetPortCount(); i ++ )
    {
        for( MuteList::iterator it = m_MuteList[i].begin(); it < m_MuteList[i].end(); ++it )
        {
            assert( ChatterIndexFromXUID( *it ) < m_cfg.dwMaxRemotePlayers );
        }
        for( MuteList::iterator it = m_RemoteMuteList[i].begin(); it < m_RemoteMuteList[i].end(); ++it )
        {
            assert( ChatterIndexFromXUID( *it ) < m_cfg.dwMaxRemotePlayers );
        }
    }

    return S_OK;
}
#endif // _DEBUG