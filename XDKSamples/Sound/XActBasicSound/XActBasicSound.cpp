//-----------------------------------------------------------------------------
// File: XActBasicSound.cpp
//
// Desc: This sample demonstrates the basic functionality of XACT by showing
//       how to create and play sounds using the XACT runtime engine.

// Hist: 7.11.02 - New for August 2002 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XAct.h>
#include <dsstdfx.h>




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
HRESULT __cdecl main()
{
    HRESULT                 hr                  = S_OK;         // Return code
    IXACTEngine*            pXACT               = NULL;         // XACT Engine instance
    IXACTWaveBank*          pStreamingWaveBank  = NULL;         // XACT Wave Bank
    IXACTSoundBank*         pSoundBank          = NULL;         // XACT Sound Bank
    IXACTSoundSource*       pSoundSource        = NULL;         // XACT Sound Source

    // Set full 3D sound processing
    DirectSoundUseFullHRTF();

    // Initialize the XACT runtime parameters
    XACT_RUNTIME_PARAMETERS xrParams = {0};
    xrParams.dwMax2DHwVoices         = 128; // Maximum number of 2D hardware voices/streams XACT engine will use
    xrParams.dwMax3DHwVoices         = 32;  // Maximum number of 3D hardware voices/streams XACT engine will use, between 0 and 64
    xrParams.dwMaxConcurrentStreams  = 16;  // Maximum number of 2D voices XACT may use for streaming from the hard drive/DVD
    xrParams.dwMaxNotifications      = 0;   // Maximum number of notifications, 0 = default

    // Create the XACT runtime engine
    if( FAILED( hr = XACTEngineCreate( &xrParams, &pXACT ) ) )
    {
        return hr;
    }

    // Open the streaming wave bank file
    HANDLE hStreamingWaveBank = CreateFile( "D:\\media\\sounds\\XactSounds_streaming.xwb",
                                            GENERIC_READ, FILE_SHARE_READ, NULL,
                                            OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL );
    if( INVALID_HANDLE_VALUE == hStreamingWaveBank )
    {
        return XBAPPERR_MEDIANOTFOUND;
    }

    // Initialize the streaming wave bank parameters
    XACT_WAVEBANK_STREAMING_PARAMETERS wbParams = {0};
    wbParams.hFile                        = hStreamingWaveBank;
    wbParams.dwOffset                     = 0;
    wbParams.dwPacketSizeInMilliSecs      = 400;
    wbParams.dwPrimePacketSizeInMilliSecs = 400;

    // Register the streaming wave bank with XACT
    if( FAILED( hr = pXACT->RegisterStreamedWaveBank( &wbParams, &pStreamingWaveBank ) ) )
        return hr;

    // Load the sound bank
    BYTE* pbSoundBank = NULL;
    DWORD dwFileSize  = 0;
    if( FAILED( hr = XBUtil_LoadFile( "D:\\media\\sounds\\XactSounds.xsb", (VOID **)&pbSoundBank, &dwFileSize ) ) )
        return hr;

    // Register the sound bank with XACT
    if( FAILED( hr = pXACT->CreateSoundBank( pbSoundBank, dwFileSize, &pSoundBank ) ) )
        return hr;

    // Download the standard DirectSound effects image
    DSEFFECTIMAGELOC EffectLoc;
    EffectLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    EffectLoc.dwCrosstalkIndex   = GraphXTalk_XTalk;
    if( FAILED( XAudioDownloadEffectsImage( "d:\\media\\dsstdfx.bin", 
                                            &EffectLoc, 
                                            XAUDIO_DOWNLOADFX_EXTERNFILE, 
                                            NULL ) ) )
        return E_FAIL;

    // Create a 3D sound source to play our cues on
    if( FAILED( hr = pXACT->CreateSoundSource( XACT_FLAG_SOUNDSOURCE_3D, &pSoundSource ) ) )
        return hr;

    // Get the sound cue index from the sound bank
    DWORD dwSoundCueIndex = 0;
    if( FAILED( hr = pSoundBank->GetSoundCueIndexFromFriendlyName( "MusicMono",             // Null-terminated string representing the friendly
                                                                   &dwSoundCueIndex ) ) )   // Pointer to the returned SoundCue index for friendly name
    {
        return hr;
    }

    // Initialize XACT notification struct
    XACT_NOTIFICATION_DESCRIPTION xactNotificationDesc = {0};
    xactNotificationDesc.wType              = eXACTNotification_Stop;
    xactNotificationDesc.wFlags             = XACT_FLAG_NOTIFICATION_USE_SOUNDCUE_INDEX;
    xactNotificationDesc.u.pSoundBank       = pSoundBank;
    xactNotificationDesc.dwSoundCueIndex    = dwSoundCueIndex;
    xactNotificationDesc.hEvent             = NULL;

    // Register a stop notification with the XACT engine.
    // This will allow us to monitor when the cue stops playing.
    if( FAILED( hr = pXACT->RegisterNotification( &xactNotificationDesc ) ) )
        return hr;

    // Play the sound cue
    if( FAILED( hr = pSoundBank->Play( dwSoundCueIndex, pSoundSource, XACT_FLAG_SOUNDCUE_AUTORELEASE, NULL) ) )
        return hr;

    // Loop while sound cue is playing
    XACT_NOTIFICATION xactNotification = {0};
    while( pXACT->GetNotification( &xactNotificationDesc, &xactNotification ) != S_OK )
    {
        // Tell the XACT engine to do it's work
        XACTEngineDoWork();
    }
    
    // NULL XACT resource pointers
    pStreamingWaveBank = NULL;

    // Free allocated memory
    SAFE_DELETE_ARRAY( pbSoundBank );

    // Release interfaces
    SAFE_RELEASE( pSoundSource );
    SAFE_RELEASE( pSoundBank );
    SAFE_RELEASE( pXACT );

    // Reboot the Xbox console
    XLaunchNewImage( NULL, NULL );

    return S_OK;
}
