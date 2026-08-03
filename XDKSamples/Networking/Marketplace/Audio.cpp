//------------------------------------------------------------------------------
// File: Audio.cpp
//
// Desc: Handles the classes responsible directly for the output of sound 
//       in marketplace.  Also wraps the Xbox High-level Voice (XHV) objects
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//------------------------------------------------------------------------------
#include "CommonInclude.h"

#define MAX_LOCAL_TALKERS 4

// Define a global audio manager; there is only one
AudioMgr g_AudioMgr;


//------------------------------------------------------------------------------
// Name: AudioMgr::AudioMgr()
// Desc: Constructs the audio manager class.  Initialize does the actual 
//       allocation of the XACT and XHV objects
//------------------------------------------------------------------------------
AudioMgr::AudioMgr()
{
    // ::Initialize takes care of these
    m_pXACT = NULL;
    m_pbWaveBank = NULL;
    m_pWaveBank = NULL;
    m_pbSoundBank = NULL;
    m_pSoundBank = NULL;
    m_pXHVEngine = NULL;
    m_pDirectSound = NULL;

    // fake xuid is for soundbit editing mode before we start the game
    m_fakeXUID.dwUserFlags = 0;
    m_fakeXUID.qwUserID = 0xbad;
}




//------------------------------------------------------------------------------
// Name: AudioMgr::Initialize()
// Desc: Initializes DSound, XACT, and the XHV objects
//------------------------------------------------------------------------------
VOID AudioMgr::Initialize( char *pszWavebank, char *pszSoundbank, char *pszDSPImage )
{
    DirectSoundUseFullHRTF();
    
    // Create the XACT runtime engine   
    XACT_RUNTIME_PARAMETERS xrParams;
    xrParams.dwMax2DHwVoices        = 64;
    xrParams.dwMax3DHwVoices        = 48;
    xrParams.dwMaxConcurrentStreams = 16;
    xrParams.dwMaxNotifications     = 0;

    if( FAILED( XACTEngineCreate( &xrParams, &m_pXACT ) ) )
        OutputDebugStringA("Could not create XACT Engine\n");
    
    DWORD dwFileSize;

    // Load the in memory wave bank and register it with XACT
    if( FAILED( XBUtil_LoadFile( pszWavebank, (VOID **)&m_pbWaveBank, &dwFileSize ) ) )
        OutputDebugStringA("Could not open wavebank\n");
    if( FAILED( m_pXACT->RegisterWaveBank( m_pbWaveBank, dwFileSize, &m_pWaveBank ) ) )
        OutputDebugStringA("Could not register wavebank\n");

    // Load the sound bank and register it with XACT
    if( FAILED( XBUtil_LoadFile( pszSoundbank, (VOID **)&m_pbSoundBank, &dwFileSize ) ) )
        OutputDebugStringA("Could not open soundbank\n");
    if( FAILED( m_pXACT->CreateSoundBank( m_pbSoundBank, dwFileSize, &m_pSoundBank ) ) )
        OutputDebugStringA("Could not register soundbank\n");

    // Load our DSP image 
    VOID* pDSPImage = NULL;
    if( FAILED( XBUtil_LoadFile( pszDSPImage, &pDSPImage, &dwFileSize ) ) )
        OutputDebugStringA("Could not load dsp image");

    // Register the DSP image with XACT
    DSEFFECTIMAGELOC dseil;    
    dseil.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    dseil.dwCrosstalkIndex   = GraphXTalk_XTalk;
    if( FAILED( m_pXACT->DownloadEffectsImage( pDSPImage, dwFileSize, &dseil, &m_pdspDesc ) ) )
        OutputDebugStringA("Could not process dsp image\n");

    // Free our copy of the DSP image- we'll let XACT handle it
    free( pDSPImage );
    
    // Create the DirectSound object
    DirectSoundCreate(NULL, &m_pDirectSound, NULL );
        
    // set the distance factor - this is how many units are in a meter in-game
    m_pDirectSound->SetDistanceFactor( MARKET_DISTANCE_FACTOR, DS3D_IMMEDIATE );
    
    // start the high level voice library
    StartXHV();


    DSBUFFERDESC dsbdesc;    
    DSMIXBINS mb;
    DSMIXBINVOLUMEPAIR pr[6] = { DSMIXBINVOLUMEPAIRS_REQUIRED_5CHANNEL_3D }; 

    mb.lpMixBinVolumePairs = pr;
    mb.dwMixBinCount = 5;

    ZeroMemory( &dsbdesc, sizeof( DSBUFFERDESC ) );
    dsbdesc.dwSize = sizeof( DSBUFFERDESC );        
    dsbdesc.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_FXIN2;
    dsbdesc.dwBufferBytes = 0;
    dsbdesc.lpwfxFormat = NULL;
    dsbdesc.lpMixBins = &mb;
    
    // create the set of buffers we are going to be using for remote talkers
    // we use these as an LRU cache based on source and method of voice

    // we have to create a limited number of these because we only have
    // so many user mixbins to route voice from XHV to the speakers

    for (WORD i = 0; i < MAX_SIMULTANEOUS_REMOTE_TALKERS; i++)
    {
        dsbdesc.dwInputMixBin = DSMIXBIN_FXSEND_1 + i;
        m_pDirectSound->CreateSoundBuffer(&dsbdesc, &m_pVoiceBuffer[i],NULL);
        m_pVoiceBuffer[i]->SetVolume( DSBVOLUME_MAX );   
        m_pVoiceBuffer[i]->SetMinDistance( MARKET_MIN_VOICE_DISTANCE, DS3D_IMMEDIATE);
        m_pVoiceBuffer[i]->SetPosition( 50.0f, 20.0f, 0.0f, DS3D_IMMEDIATE );
        m_pVoiceBuffer[i]->Play( 0, 0, 0 );

        m_pCurrentBufferOwner[i] = NULL;
    }
}




//------------------------------------------------------------------------------
// Name: AudioMgr::StartXHV()
// Desc: initializes the xbox high-level voice library
//------------------------------------------------------------------------------
VOID AudioMgr::StartXHV()
{
                    
    // Set up parameters for the Voice Chat engine
    XHV_RUNTIME_PARAMS xhvParams;
    ZeroMemory( &xhvParams, sizeof( XHV_RUNTIME_PARAMS ) );

    xhvParams.dwMaxLocalTalkers         = XGetPortCount();
    xhvParams.dwMaxRemoteTalkers        = MAX_SIMULTANEOUS_REMOTE_TALKERS;
    xhvParams.dwMaxCompressedBuffers    = 8;                   
    xhvParams.dwFlags                   = 0;
    xhvParams.pEffectImageDesc          = m_pdspDesc;
    xhvParams.dwEffectsStartIndex       = GraphVoice_Voice_0;
    xhvParams.bHeadphoneAlwaysOn        = false;
    xhvParams.dwOutOfSyncThreshold      = 10;
    xhvParams.bCustomVADProvided        = false;   

    // Create the engine and use the VoiceMgr object for callbacks
    XHVEngineCreate( &xhvParams, &m_pXHVEngine );    
    m_pXHVEngine->EnableProcessingMode( XHV_VOICECHAT_MODE );    
    m_pXHVEngine->SetCallbackInterface( &g_VoiceMgr );    
    m_pXHVEngine->SetMaxPlaybackStreamsCount( MAX_SIMULTANEOUS_REMOTE_TALKERS );     

    // Register each local talker immediately, but leave them inactive
    for( DWORD i = 0; i < MAX_LOCAL_TALKERS; i++ )
    {
        m_pXHVEngine->RegisterLocalTalker( i );
        m_pXHVEngine->SetProcessingMode( i, XHV_INACTIVE_MODE );
    }
}




//------------------------------------------------------------------------------
// Name: AudioMgr::StopXHV()
// Desc: releases the XHV object (we have to do this when we log in)
//------------------------------------------------------------------------------
VOID AudioMgr::StopXHV()
{
    m_pXHVEngine->Release();
}




//------------------------------------------------------------------------------
// Name: AudioMgr::CreateAmbientSound()
// Desc: Create an ambient sound - this is a sound with no position
//       in this sample, this is used for the marketplace murmur and the chime
//------------------------------------------------------------------------------
AmbientSound *AudioMgr::CreateAmbientSound() 
{
    AmbientSound *pSound = new AmbientSound;
    m_pSoundList.push_back( pSound );
    return pSound;
}




//------------------------------------------------------------------------------
// Name: AudioMgr::FreeSound()
// Desc: Creates a 3d positioned .wav sound and puts it in our sound list
//       in this sample, this is used for the fountain, and the walla-walla
//       sound for players who are talking but you can't receive from
//------------------------------------------------------------------------------
PositionedWaveBankSound *AudioMgr::CreatePositionedWaveBankSound() 
{
    PositionedWaveBankSound *pSound = new PositionedWaveBankSound;
    m_pSoundList.push_back( pSound );
    return pSound;
}




//------------------------------------------------------------------------------
// Name: AudioMgr::CreatePositionedVoiceSound()
// Desc: Creates a placeholder for a positioned voice sound
//       When we actually try to output data from it, we'll grab one of the
//       8 mixbins on a LRU basis and output our sound to it.
//------------------------------------------------------------------------------
PositionedVoiceSound *AudioMgr::CreatePositionedVoiceSound() 
{
    PositionedVoiceSound *pSound = new PositionedVoiceSound;
    m_pSoundList.push_back( pSound );
    return pSound;
}




//------------------------------------------------------------------------------
// Name: AudioMgr::FreeSound()
// Desc: frees a sound allocated in one of the createsound functions
//------------------------------------------------------------------------------
void AudioMgr::FreeSound( SingleSound *pSound )
{
    std::vector<SingleSound *>::iterator i;    

    for ( i = m_pSoundList.begin(); i != m_pSoundList.end(); i++ )
    {
        if ( (*i) == pSound )
        {            
            m_pSoundList.erase( i ) ;             
            delete pSound;
            break;
        }
    }

}




//------------------------------------------------------------------------------
// Name: AudioMgr::EnableAll()
// Desc: Enable all sounds if AutoEnable is set
//------------------------------------------------------------------------------
VOID AudioMgr::EnableAll()
{
    DWORD i;
    
    // make each talker active
    for( i = 0; i < MAX_LOCAL_TALKERS; i++ )
    {    
        m_pXHVEngine->SetProcessingMode( i, XHV_VOICECHAT_MODE );
    }
    
    for ( i = 0; i < m_pSoundList.size(); i++ )
    {
        if ( m_pSoundList[ i ]->AutoEnable() )
            m_pSoundList[ i ]->Enable();
    }
}




//------------------------------------------------------------------------------
// Name: AudioMgr::EnableAll()
// Desc: Disable all sounds
//------------------------------------------------------------------------------
VOID AudioMgr::DisableAll()
{
    DWORD i;

    // make each talker inactive
    for( i = 0; i < MAX_LOCAL_TALKERS; i++ )
    {    
        m_pXHVEngine->SetProcessingMode( i, XHV_INACTIVE_MODE );
    }
    
    for ( i = 0; i < m_pSoundList.size(); i++ )
    {
        m_pSoundList[ i ]->Disable();
    }
}




//------------------------------------------------------------------------------
// Name: AudioMgr::Update()
// Desc: Update orientation and position in XACT and DSound
//------------------------------------------------------------------------------
VOID AudioMgr::Update( FLOAT fDt, const D3DXVECTOR3 &vPos, const D3DXVECTOR3 &vFace )
{
    m_pXACT->SetListenerOrientation( vFace.x, vFace.y, 0.0f, 0,0,1, DS3D_DEFERRED );
    m_pXACT->SetListenerPosition( vPos.x, vPos.y, 0.0f, DS3D_DEFERRED );
    m_pDirectSound->SetOrientation( vFace.x, vFace.y, 0.0f, 0,0,1, DS3D_DEFERRED );
    m_pDirectSound->SetPosition( vPos.x, vPos.y, 0.0f, DS3D_DEFERRED );

    CommitSettings();

    XACTEngineDoWork();
    m_pXHVEngine->DoWork();    
}




//------------------------------------------------------------------------------
// Name: AudioMgr::CommitSettings
// Desc: Commit all settings on dsound and xact with the DS3D_DEFERRED flag
//------------------------------------------------------------------------------
VOID AudioMgr::CommitSettings()
{
    m_pXACT->CommitDeferredSettings();
    m_pDirectSound->CommitDeferredSettings(); 
}




//------------------------------------------------------------------------------
// Name: AudioMgr::Update()
// Desc: pump XACT and XHV 
//------------------------------------------------------------------------------
VOID AudioMgr::Update( float fDt )
{
     XACTEngineDoWork();
     m_pXHVEngine->DoWork();
}




//------------------------------------------------------------------------------
// Name: AudioMgr::SetSimulationMode()
// Desc: Sets the audio manager to use a fake xuid- this is for editing in 
//       the soundbit editor when we are forging voice packets
//------------------------------------------------------------------------------
VOID AudioMgr::SetSimulationMode( BOOL bMode )
{       
    DWORD i;

    if ( bMode == TRUE )
    {
        // register the local talker, and playback through the speakers
        for( i = 0; i < MAX_LOCAL_TALKERS; i++ )
        {            
            m_pXHVEngine->SetProcessingMode( i, XHV_VOICECHAT_MODE );
        }       
        m_pXHVEngine->RegisterRemoteTalker( m_fakeXUID );
        m_pXHVEngine->SetPlaybackPriority( m_fakeXUID, XHV_PLAYBACK_TO_SPEAKERS, XHV_PLAYBACK_PRIORITY_MAX );
    }
    else
    {        
        // stop playing through the speakers
        m_pXHVEngine->SetPlaybackPriority( m_fakeXUID, XHV_PLAYBACK_TO_SPEAKERS, XHV_PLAYBACK_PRIORITY_NEVER );
        m_pXHVEngine->UnregisterRemoteTalker( m_fakeXUID );
        for( i = 0; i < MAX_LOCAL_TALKERS; i++ )
        {
            m_pXHVEngine->SetProcessingMode( i, XHV_INACTIVE_MODE );
        }       
        
    }
    
    m_pXHVEngine->DoWork();
}




//------------------------------------------------------------------------------
// Name: AudioMgr::SimulationXUID()
// Desc: Returns the fake xuid we created for simulation mode
//------------------------------------------------------------------------------
XUID &AudioMgr::SimulationXUID() 
{ 
    return m_fakeXUID; 
}




//------------------------------------------------------------------------------
// Name: AudioMgr::SubmitVoicePacket()
// Desc: Submits a voice packet directly to the xhv engine
//------------------------------------------------------------------------------
VOID AudioMgr::SubmitVoicePacket( XUID xuid, BYTE *packet )
{   
    m_pXHVEngine->SubmitIncomingVoicePacket( xuid, packet, COMPRESSED_VOICE_SIZE );
}




//------------------------------------------------------------------------------
// Name: AmbientSound::AmbientSound()
// Desc: Constructs a 2d non-positioned sound - created through AudioMgr
//------------------------------------------------------------------------------
AmbientSound::AmbientSound()
{
    m_dwIndex = 0;
    m_bEnabled = FALSE;
}




//------------------------------------------------------------------------------
// Name: AmbientSound::~AmbientSound()
// Desc: Just stop playing the sound since we haven't allocated a sound source
//------------------------------------------------------------------------------
AmbientSound::~AmbientSound()
{
    Disable();
}




//------------------------------------------------------------------------------
// Name: AmbientSound::Initialize()
// Desc: Get the sound cue index for the sound we want to play and store it
//------------------------------------------------------------------------------
VOID AmbientSound::Initialize( char *pszSoundName )
{
    if( FAILED( g_AudioMgr.m_pSoundBank->GetSoundCueIndexFromFriendlyName( pszSoundName, &m_dwIndex ) ) )
       OutputDebugStringA("Couldn't open ambient sound file\n");
}




//------------------------------------------------------------------------------
// Name: AmbientSound::SetVolume()
// Desc: Set the volume on category 0 which this sample uses for ambient
//       this is an attenuation, so 0 is max volume
//------------------------------------------------------------------------------
VOID AmbientSound::SetVolume( float fVol )
{
    // category 0 is the ambient noise
    g_AudioMgr.m_pXACT->SetMasterVolume( 0, (LONG) ( fVol * 100.0f ) );
}




//------------------------------------------------------------------------------
// Name: AmbientSound::Enable()
// Desc: Start the sound playing
//------------------------------------------------------------------------------
VOID AmbientSound::Enable()
{
    m_bEnabled = TRUE;
    g_AudioMgr.m_pSoundBank->Play( m_dwIndex, NULL, XACT_FLAG_SOUNDCUE_AUTORELEASE, NULL );     
}




//------------------------------------------------------------------------------
// Name: AmbientSound::Disable()
// Desc: Stop the sound from playing
//------------------------------------------------------------------------------
VOID AmbientSound::Disable()
{
    m_bEnabled = FALSE;
    g_AudioMgr.m_pSoundBank->Stop( m_dwIndex, XACT_FLAG_SOUNDCUE_IMMEDIATE, NULL );
}




//------------------------------------------------------------------------------
// Name: PositionedWaveBankSound::PositionedWaveBankSound()
// Desc: create a 3d positioned sound that plays a .wav - AudioMgr calls this
//------------------------------------------------------------------------------
PositionedWaveBankSound::PositionedWaveBankSound()
{
    m_dwIndex = 0;
    m_bEnabled = FALSE;
    m_p3DSoundSource = NULL;
}




//------------------------------------------------------------------------------
// Name: PositionedWaveBankSound::~PositionedWaveBankSound()
// Desc: Clean up the DSound source for a positioned wav sound
//------------------------------------------------------------------------------
PositionedWaveBankSound::~PositionedWaveBankSound()
{
    Disable();

    if ( m_p3DSoundSource != NULL )
    {
        m_p3DSoundSource->StopSoundCues();        
        m_p3DSoundSource->Release();
        m_p3DSoundSource = NULL;
    }
}




//------------------------------------------------------------------------------
// Name: PositionedWaveBankSound::Initialize()
// Desc: Allocate a positioned 3d .wav sound
//------------------------------------------------------------------------------
VOID PositionedWaveBankSound::Initialize( char *pszSoundName, const D3DXVECTOR3 &vPos )
{
    if( FAILED( g_AudioMgr.m_pXACT->CreateSoundSource( XACT_FLAG_SOUNDSOURCE_3D, &m_p3DSoundSource ) ) )
        OutputDebugStringA("Couldn't create PositionedWaveBank 3d soundsource\n");
    if( FAILED( g_AudioMgr.m_pSoundBank->GetSoundCueIndexFromFriendlyName( pszSoundName, &m_dwIndex ) ) )
        OutputDebugStringA("Couldn't open PositionedWaveBank sound file\n");
        
    m_p3DSoundSource->SetPosition( vPos.x, vPos.y, 0.0f, DS3D_DEFERRED );        
}




//------------------------------------------------------------------------------
// Name: PositionedWaveBankSound::Move()
// Desc: Move the sound source 
//------------------------------------------------------------------------------
VOID PositionedWaveBankSound::Move( const D3DXVECTOR3 &vPos )
{
    m_p3DSoundSource->SetPosition( vPos.x, vPos.y, 0.0f, DS3D_DEFERRED );  
}




//------------------------------------------------------------------------------
// Name: PositionedWaveBankSound::Enable()
// Desc: Start the sound playing
//------------------------------------------------------------------------------
VOID PositionedWaveBankSound::Enable()
{
    m_bEnabled = TRUE;
    g_AudioMgr.m_pSoundBank->Play( m_dwIndex, m_p3DSoundSource, XACT_FLAG_SOUNDCUE_AUTORELEASE, NULL );     
}




//------------------------------------------------------------------------------
// Name: PositionedWaveBankSound::Disable()
// Desc: Stop the sound playing
//------------------------------------------------------------------------------
VOID PositionedWaveBankSound::Disable()
{
    m_bEnabled = FALSE;
    g_AudioMgr.m_pSoundBank->Stop( m_dwIndex, XACT_FLAG_SOUNDCUE_IMMEDIATE, NULL );
}




//------------------------------------------------------------------------------
// Name: PositionedVoiceSound::PositionedVoiceSound()
// Desc: Create a positioned voice sound - only AudioMgr should call this
//------------------------------------------------------------------------------
PositionedVoiceSound::PositionedVoiceSound()
{
    m_xuid.qwUserID     = 0;
    m_wVoiceBufferIdx   = IDX_NO_BUFFER;
    m_fTimeLastUsed     = NULL;
    m_vSavedPos         = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    m_wPriorMethod      = VOM_NONE;
}



//------------------------------------------------------------------------------
// Name: PositionedVoiceSound::~PositionedVoiceSound()
// Desc: Destruct a positioned voice sound - just make sure we aren't allocating
//       one of the mixbin buffers
//------------------------------------------------------------------------------
PositionedVoiceSound::~PositionedVoiceSound()
{
    if ( m_wVoiceBufferIdx != IDX_NO_BUFFER )
    {
        g_AudioMgr.m_pCurrentBufferOwner[ m_wVoiceBufferIdx ] = NULL;
        m_wVoiceBufferIdx = IDX_NO_BUFFER;
        g_AudioMgr.m_pXHVEngine->UnregisterRemoteTalker( m_xuid );        
    }
}




//------------------------------------------------------------------------------
// Name: PositionedVoiceSound::Initialize()
// Desc: Set the XUID of the user of this sound
//------------------------------------------------------------------------------
VOID PositionedVoiceSound::Initialize( XUID xuid )
{
    m_xuid = xuid;
}




//------------------------------------------------------------------------------
// Name: PositionedVoiceSound::Move()
// Desc: Move the positioned voice sound
//------------------------------------------------------------------------------
VOID PositionedVoiceSound::Move( const D3DXVECTOR3 &vPos )
{
    m_vSavedPos = vPos;
    if ( m_wVoiceBufferIdx != IDX_NO_BUFFER )
    {
        g_AudioMgr.m_pVoiceBuffer[ m_wVoiceBufferIdx ]->SetPosition( vPos.x, vPos.y, 0.0f, DS3D_DEFERRED );
    }    
}




//------------------------------------------------------------------------------
// Name: PositionedVoiceSound::Initialize()
// Desc: Enables the sound
//------------------------------------------------------------------------------
VOID PositionedVoiceSound::Enable()
{
    m_bEnabled = TRUE;
}




//------------------------------------------------------------------------------
// Name: PositionedVoiceSound::Initialize()
// Desc: Disables the sound
//------------------------------------------------------------------------------
VOID PositionedVoiceSound::Disable()
{
    m_bEnabled = FALSE;
}




//------------------------------------------------------------------------------
// Name: PositionedVoiceSound::SubmitVoicePacket()
// Desc: SubmitVoicePacket submits a voice packet to a positionedVoiceSound
//       Because we only have 8 mixbins that we use here, we go through the list
//       of mixbin sound sources, find one that hasn't been used recently, and
//       set it up to play our voice packet.  (Move it to the location of the
//       player, make it play appropriately through headphones or speakers).
//------------------------------------------------------------------------------
VOID PositionedVoiceSound::SubmitVoicePacket( BYTE *pData, DWORD dwSize, FLOAT fTime, BYTE byMethod )
{
    WORD wBufferIdx = 0, i;
    DWORD j;
    FLOAT fMinTime = fTime;

    // If we aren't enabled, return
    if (!m_bEnabled) return;
    
    // see if we currently have a buffer or if we need to release anyways.
    // The reason we would need to release anyways, even if we already own a 
    // buffer is that the latency changes quite a bit between server-forwarded
    // and peer-to-peer voice.  To avoid the skipping and breaking up, we need
    // to grab a new buffer when the method changes.

    if ( ( m_wVoiceBufferIdx == IDX_NO_BUFFER ) || (byMethod != m_wPriorMethod ) )
    {
        // If we don't currently own a buffer we need to find one

        if ( m_wVoiceBufferIdx == IDX_NO_BUFFER )
        {
            // loop through the buffers, and find the oldest one or one
            // that is not being used currently

            for ( i = 0; i < MAX_SIMULTANEOUS_REMOTE_TALKERS; i++ )
            {
                if ( g_AudioMgr.m_pCurrentBufferOwner[ i ] == NULL )
                {
                    wBufferIdx = i;
                    break;
                }
                if ( g_AudioMgr.m_pCurrentBufferOwner[ i ]->m_fTimeLastUsed < fMinTime )
                {
                    wBufferIdx = i;
                    fMinTime = g_AudioMgr.m_pCurrentBufferOwner[ i ]->m_fTimeLastUsed;
                }
            }   
        }
        else
            wBufferIdx = m_wVoiceBufferIdx;
        
        // If there was an old player associated with this buffer, we need to unregister them

        if ( g_AudioMgr.m_pCurrentBufferOwner[ wBufferIdx ] != NULL )
        {
            g_AudioMgr.m_pXHVEngine->UnregisterRemoteTalker( g_AudioMgr.m_pCurrentBufferOwner[ wBufferIdx ]->m_xuid );
            g_AudioMgr.m_pCurrentBufferOwner[ wBufferIdx ]->m_wVoiceBufferIdx = IDX_NO_BUFFER;              
            g_AudioMgr.m_pXHVEngine->DoWork();            
        }    
                
        // now, register the new player
        
        g_AudioMgr.m_pXHVEngine->RegisterRemoteTalker( m_xuid );
        g_AudioMgr.m_pCurrentBufferOwner[ wBufferIdx ] = this;
        g_AudioMgr.m_pXHVEngine->DoWork();

        m_wVoiceBufferIdx = wBufferIdx;       
        m_wPriorMethod = VOM_NONE;  
        
        // reset the buffer setting to normal distance
        g_AudioMgr.m_pVoiceBuffer[ wBufferIdx ]->SetMinDistance( MARKET_MIN_VOICE_DISTANCE, DS3D_DEFERRED );
        g_AudioMgr.m_pVoiceBuffer[ wBufferIdx ]->SetMaxDistance( 200.0f, DS3D_DEFERRED );       

        Move( m_vSavedPos );
    }
    
    // prior method is none if we want to force a reset, or if we just changed buffers (in the previous if-block)

    if ( m_wPriorMethod == VOM_NONE ) 
    {    
        // first we reset the buffer to 'normal params'
        DSMIXBINVOLUMEPAIR pr[2];
        DSMIXBINS mb;
        mb.dwMixBinCount = 1;
        mb.lpMixBinVolumePairs = pr;
        pr[0].dwMixBin = DSMIXBIN_FXSEND_1 + wBufferIdx;

        // turn off headphones and speakers 
        for ( j = 0; j < 4; j++ )
            g_AudioMgr.m_pXHVEngine->SetPlaybackPriority( m_xuid, j, XHV_PLAYBACK_PRIORITY_NEVER );
        g_AudioMgr.m_pXHVEngine->SetPlaybackPriority( m_xuid, XHV_PLAYBACK_TO_SPEAKERS, XHV_PLAYBACK_PRIORITY_NEVER );
        pr[0].lVolume = DSBVOLUME_MIN;
        g_AudioMgr.m_pXHVEngine->SetMixBinMapping( m_xuid, XHV_PLAYBACK_TO_SPEAKERS, &mb );
    
        g_AudioMgr.m_pVoiceBuffer[ wBufferIdx ]->SetMinDistance( MARKET_MIN_VOICE_DISTANCE, DS3D_DEFERRED );
        
        // now, based on the method of speech, we tweak the buffer settings for the voice
        switch( byMethod )
        {
        case VOM_COMMUNICATOR:
            // through the communicator, use XHV native stuff
            for ( j = 0; j < 4; j++ )
                g_AudioMgr.m_pXHVEngine->SetPlaybackPriority( m_xuid, j, XHV_PLAYBACK_PRIORITY_MAX );       
            break;
        case VOM_PODIUM:
            // through the podium, we just have a longer range distance, then we execute the VOM_NORMAL case
            g_AudioMgr.m_pVoiceBuffer[ wBufferIdx ]->SetMinDistance( MARKET_PODIUM_VOICE_DISTANCE, DS3D_DEFERRED );            
        case VOM_NORMAL:
            // normal voice goes through the speakers
            g_AudioMgr.m_pXHVEngine->SetPlaybackPriority( m_xuid, XHV_PLAYBACK_TO_SPEAKERS, XHV_PLAYBACK_PRIORITY_MAX );
            pr[0].lVolume = DSBVOLUME_MAX;
            g_AudioMgr.m_pXHVEngine->SetMixBinMapping( m_xuid, XHV_PLAYBACK_TO_SPEAKERS, &mb );
            break;                   
        }              
    }
    
    g_AudioMgr.CommitSettings();
    g_AudioMgr.m_pXHVEngine->DoWork();

    // set the time last used for the buffer, and my previous method
    m_fTimeLastUsed = fTime;
    m_wPriorMethod = byMethod;

    // submit all the packets to the sound source, now that we know we had one
    for ( i = 0; i < dwSize; i++ )
        g_AudioMgr.m_pXHVEngine->SubmitIncomingVoicePacket( m_xuid, pData + COMPRESSED_VOICE_SIZE * i, COMPRESSED_VOICE_SIZE );    
}

