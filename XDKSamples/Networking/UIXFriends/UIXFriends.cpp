//-----------------------------------------------------------------------------
// File: UIXFriends.cpp
//
// Desc: Demonstrates how to use UIX to logon and then manage friends.
// See readme.txt for more details.
//
// Hist: 5.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "UIXFriends.h"
#include "XBOnline.h"
#include <dsstdfx.h>
#include "XHVVoiceManager.h"

// XHV and DSound objects for voice mail support.
CXHVVoiceManager    g_XHVVoiceManager;
LPDIRECTSOUND8      g_pDSound;
LPDSEFFECTIMAGEDESC pdsImageDesc;

const CHAR PATH_TO_THIS_TITLE[] = "D:\\UIXFriends.xbe";

const XNKID ZeroSessionID = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    CXBoxSample xbApp;
    if( FAILED( xbApp.Create() ) )
        return;
    xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
    m_pLiveEngine = NULL;
    Reset();
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Select the font file based on the languages setting of the Xbox
    CHAR* strFontFile = "Font.xpr";
    switch( XGetLanguage() )
    {
        case XC_LANGUAGE_TCHINESE:
            strFontFile = "Font_cht.xpr";
            break;
        case XC_LANGUAGE_JAPANESE:
            strFontFile = "Font_jpn.xpr";
            break;
        case XC_LANGUAGE_KOREAN:
            strFontFile = "Font_kor.xpr";
            break;
    }

    // Create the font
    if( FAILED( m_Font.Create( strFontFile ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create DirectSound in order to download our DSP image, which
    // contains the voice mixer SRC effects
    DirectSoundCreate( NULL, &g_pDSound, NULL );

    // Tell DirectSound where the 3D Reverb and XTalk effects are
    DSEFFECTIMAGELOC dsImageLoc;
    dsImageLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    dsImageLoc.dwCrosstalkIndex = GraphXTalk_XTalk;

    // Download the DSP image
    XAudioDownloadEffectsImage( "dspimage", &dsImageLoc, XAUDIO_DOWNLOADFX_XBESECTION, &pdsImageDesc );

    m_WhichScreen = SCREEN_INTRO;
    m_bLoggedOn = FALSE;
    m_bLoggingOn = FALSE;
    m_pLogonUsers = NULL;
    m_strMessage[0] = 0;

	// Live Aware Only titles won't ever use a session ID
#ifndef LIVE_AWARE_ONLY
	// Initialize session ID to 0 == no session
	memset( &m_SessionID, 0, sizeof(m_SessionID));
#endif

    // Initialization needed for UIX
    // Zero the work flags.
    m_dwLiveWorkFlags = 0;

    // Initialize the online library
    HRESULT hr;

    hr = XOnlineStartup( NULL );
    if ( FAILED( hr ) )
    {
        XBUtil_DebugPrint( "XOnlineStartup failed (error 0x%x)", hr );
    }

    // Create a UI plugin object
    static UIXFont m_UIXFont( &m_Font );

    hr = UIXCreateUIPlugin( &m_UIXFont, &m_pUIPlugin );
    if ( FAILED( hr ) )
    {
        XBUtil_DebugPrint( "Failed (error 0x%x)", hr );
    }

    // Create the live engine
    hr = UIXCreateLiveEngine( "d:\\media\\UIXFriends.uix", XGetLanguage(), &m_pLiveEngine );
    if ( FAILED( hr ) )
    {
        XBUtil_DebugPrint( "Failed (error 0x%x)", hr );
    }

    // Create and hookup an audio plugin object
    hr = InitSound();
    if( SUCCEEDED(hr) )
    {
        ITitleAudioPlugin* pAudioPlugin;

        hr = UIXCreateAudioPlugin( m_pXactEngine, m_pSoundBank, &pAudioPlugin );

        if( SUCCEEDED(hr) )
            m_pLiveEngine->SetAudioPlugin( pAudioPlugin );
    }

    // Setup the UI plugin and enable the desired features
    m_pLiveEngine->SetUIPlugin( m_pUIPlugin );
    m_pLiveEngine->EnableFeature( UIX_LOGON_FEATURE );
    m_pLiveEngine->EnableFeature( UIX_FRIENDS_FEATURE );

    // We want UIX to handle displaying notifications for things like
    // logoffs due to duplicate logons, and friend invites. If we set
    // this to FALSE then we will be responsible for displaying the
    // notifications. This sample lets UIX display the notification
    // popup, and then displays text describing the invite from the Live
    // menu.
    m_pLiveEngine->SetProperty( UIX_PROPERTY_DISPLAY_NOTIFICATIONS, TRUE );

    // We want UIX to put a Send Game Invite item in the friends menu. We
    // should set this property to FALSE if we don't have a game session or
    // if our session is full. This attribute defaults to TRUE
    m_pLiveEngine->SetProperty( UIX_PROPERTY_ALLOW_GAME_INVITES, TRUE );

	// Determine the kind of logon to start, if any at this point
	// Look for accepted game invite if we are not Live Aware Only -or-
	// Look for UIXFRIENDS_LAUNCH_DATA and use current logon state if available
#ifndef LIVE_AWARE_ONLY
	XONLINE_ACCEPTED_GAMEINVITE gameInvite;
#endif
	DWORD dwLaunchDataType = 0;
	LAUNCH_DATA LaunchData;
	DWORD* pLaunchData = (DWORD*)&LaunchData;
	memset( &LaunchData, 0, sizeof( LAUNCH_DATA ) );
	memset( &m_LaunchData, 0, sizeof( UIXFRIENDS_LAUNCH_DATA ) );
	DWORD dwLaunch = XGetLaunchInfo( &dwLaunchDataType, &LaunchData );
	if ( dwLaunch == ERROR_SUCCESS && dwLaunchDataType == LDT_TITLE &&
		 pLaunchData[0] == UIXFRIENDS_LAUNCH_ID )
	{
		memcpy( &m_LaunchData, &LaunchData, sizeof( UIXFRIENDS_LAUNCH_DATA ) );
		// If we had a silent logon previously, we need to sign on silently
		// again to get UIX into the correct state
		// Otherwise, sign on using the retrieved state
		// NOTE: The presence of the logon state directs StartLogon to the
		// correct screen so we pass it for both cases
		if (m_LaunchData.bSilentLogon)
			StartLogon( TRUE, NULL, &m_LaunchData.LogonState );
		else
			StartLogon( FALSE, NULL, &m_LaunchData.LogonState );
	}
#ifndef LIVE_AWARE_ONLY
    // Cross-game invites must be supported by all Live titles, but
    // they make no sense for titles that only support Live Aware
    else if( XOnlineFriendsGetAcceptedGameInvite( &gameInvite ) == S_OK )
    {
        StartLogon( FALSE, &gameInvite );
    }
#endif // LIVE_AWARE_ONLY

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Reset to a known-good state. Signed out and at the main menu.
//-----------------------------------------------------------------------------
VOID CXBoxSample::Reset()
{
    // When signing out it is important to shut down XHV. That's because
    // signing in can adjust the Xbox clock, which causes problems
    // for XHV if it is running at the time.
    if( g_XHVVoiceManager.IsInitialized() )
    {
        // Set the UIX voice mail engine to zero, to make sure XHV is
        // shut down.
        m_pLiveEngine->SetProperty( UIX_PROPERTY_VOICE_MAIL_ENGINE, NULL );
        g_XHVVoiceManager.Shutdown();
    }

    m_WhichScreen = SCREEN_MAIN;
    if ( m_pLiveEngine )
    {
        if ( m_bLoggedOn )
        {
            for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
            {
                if( m_Users[i].bSignedOn && !m_Users[i].bGuest )
                    SetPlayerOnlineState( i, 0 );
            }
            m_pLiveEngine->LogOff();
        }
    }
    m_bLoggedOn = FALSE;
    m_bLoggingOn = FALSE;
    m_dwMicrophoneState = 0;
    m_dwHeadphoneState = 0;
    m_pLogonUsers = NULL;
    ZeroMemory( &m_Users, sizeof( m_Users ) );

#ifndef LIVE_AWARE_ONLY
	memset( &m_SessionID, 0, sizeof(m_SessionID));
#endif // LIVE_AWARE_ONLY
	m_bSilentLogon = FALSE;
	m_dwCtlrIdx = -1;
	m_dwUserIdx = -1;
}




BOOL CXBoxSample::IsUIXScreen()
{
    return m_WhichScreen == SCREEN_LOGON || m_WhichScreen == SCREEN_FRIENDS;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // If the user loses their connection they may want to find out about it.
    // If they are in the UIX logon or friends screen then UIX will tell them
    // as needed.
    if ( m_WhichScreen == SCREEN_MAIN &&
                !m_NetLink.IsActive() )
    {
        if ( !m_strMessage[0] )
            wcscpy( m_strMessage, L"This Xbox has lost its online connection" );
    }

    // Pass input to the UIX engine. We do this every frame, even when UIX is
    // not active, so that when a UIX feature starts up it can distinguish
    // between a button that was just pressed and a button that has been pressed
    // down for a while.
	for( DWORD i = 0; i < XGetPortCount(); i++)
    {
		if ( g_Gamepads[i].hDevice == NULL )
		{
			m_pLiveEngine->SetInput( i, NULL );
		}
		else
		{
			m_pLiveEngine->SetInput( i, &( g_InputStates[i] ) );
		}
    }

    // Let the UIX engine do work. This also sets m_dwLiveWorkFlags
    // based on what UIX is doing, if anything.
    // This should be called every frame, even when UIX is dormant,
    // because UIX still watches for unexpected logoffs.
    m_pLiveEngine->DoWork( &m_dwLiveWorkFlags );

    if( g_XHVVoiceManager.IsInitialized() )
        g_XHVVoiceManager.DoWork();

    if ( m_dwLiveWorkFlags & UIX_DOWORK_NOTIFICATIONS )
    {
        // Process the Live notifications
        // If UIX_PROPERTY_DISPLAY_NOTIFICATIONS is set to FALSE with
        // SetProperty then you must handle these notifications. If
        // UIX_PROPERTY_DISPLAY_NOTIFICATIONS is set to TRUE then
        // handling this message is optional.
        OutputDebugStringA("Notification received.\n");

        // This code gets information about the notifications so that they
        // can be displayed on the lower right corner of the screen. There
        // is no code to actually display them.
        // If you use UIX_NOTIFICATION_IN_GAME_FLASH then the notification will
        // only be sent once. If you use UIX_NOTIFICATION_MENU then the
        // notification will be sent until you start the friends feature.
        for ( DWORD PortIndex = 0; PortIndex < XONLINE_MAX_LOGON_USERS; ++PortIndex )
        {
            if ( ( (UIX_PORT_0) << PortIndex ) & m_dwLiveWorkFlags )
            {
                // We've received a notification for the user on PortIndex
                DWORD Notification = 0;
                m_pLiveEngine->GetNotifications( PortIndex, UIX_NOTIFICATION_IN_GAME_FLASH, &Notification );
                if ( Notification & UIX_DOWORK_NOTIFY_FRIEND_REQUEST )
                {
                    XBUtil_DebugPrint( "Friend request for port %d\n", PortIndex );
                }
                else if ( Notification & UIX_DOWORK_NOTIFY_GAME_INVITE )
                {
                    XBUtil_DebugPrint( "Game invite for port %d\n", PortIndex );
                }
            }
        }
    }

    // Is the active feature exiting?
    if ( m_dwLiveWorkFlags & UIX_DOWORK_FEATURE_EXIT )
    {
        // Process the exit codes.
        HandleFeatureExit();
    }

    // Has UIX requested a reboot?
    if ( m_dwLiveWorkFlags & UIX_DOWORK_NEED_TO_REBOOT )
    {
        // Do any work needed prior to rebooting, then let UIX reboot.
        // You can pass in a context value that will be placed in
        // LD_LAUNCH_DASHBOARD::dwContext and will subsequently be passed
        // back to the you when the dashboard launches you. You can use
        // this to return to the same location in your menus.
        m_pLiveEngine->Reboot( 0 );
    }

    // If no UIX feature is using the input then we should process
    // the users input ourselves.
    if ( ( m_dwLiveWorkFlags & UIX_DOWORK_PROCESSING_INPUT ) == 0 )
    {
        ProcessInput();
    }

	// UIX Friends Screen doesn't use X button and we overload it to
	// offer appear offline functionality on the friend's list
	// Titles will typically offer this functionality on a separate
	// "Online Options" menu, but this demonstrates how to layer game
	// functionality on top of UIX
	if ( m_Gamepad[m_dwCtlrIdx].bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
    {
        // Change player online status
		if ( m_Users[m_dwUserIdx].dwState & XONLINE_FRIENDSTATE_FLAG_ONLINE )
			m_Users[m_dwUserIdx].dwState &= ~XONLINE_FRIENDSTATE_FLAG_ONLINE;
		else
			m_Users[m_dwUserIdx].dwState |= XONLINE_FRIENDSTATE_FLAG_ONLINE;

		SetPlayerOnlineState( m_dwUserIdx, m_Users[m_dwUserIdx].dwState );
    }

    if ( m_bLoggedOn )
    {
        // Update the online/voice state of each player.
        CheckDeviceStates();
    }

    return S_OK;
}




VOID CXBoxSample::ShowLoginState()
{
    BOOL Displayed = FALSE;
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        if( m_Users[i].bSignedOn )
        {
            // For silent logons, there will only be one logged on user.
			// However, we will also cover the case of multiple logons by appending all users to this string
            assert( m_bLoggedOn );

#ifndef LIVE_AWARE_ONLY // If we support sessions, we display that here
			const XNKID zeroSessionID = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
			BOOL bInSession = !( memcmp( &m_SessionID, &zeroSessionID, sizeof( m_SessionID ) ) == 0 );
			if ( bInSession )
			{
				wsprintfW( m_strMessage, L"In Session 0x%x", m_SessionID );
				Displayed = TRUE;
			}
			else
			{
#endif
            if ( !Displayed )
			{
				wsprintfW( m_strMessage, L"Signed in %s", m_Users[i].strGamertag );
				Displayed = TRUE;
			}
			else
			{
				wsprintfW( m_strMessage, L"%s, %s", m_strMessage, m_Users[i].strGamertag );
			}

#ifndef LIVE_AWARE_ONLY
			}
#endif

        }
    }
    if ( !Displayed )
    {
        // Update the status message.
        wsprintfW( m_strMessage, L"Signed out" );
    }
}




//-----------------------------------------------------------------------------
// Name: HandleFeatureExit()
// Desc: Called whenever a feature exits, to handle the necessary state changes.
//-----------------------------------------------------------------------------
VOID CXBoxSample::HandleFeatureExit()
{
    UIX_EXIT_INFO exitInfo;
	BOOL bCrossGameInvite = FALSE;

    m_pLiveEngine->GetExitInfo( &exitInfo );
    // exitInfo contains FeatureID, ExitCode, hr, and pExitData

    switch ( exitInfo.ExitCode )
    {
        case UIX_EXIT_NONE:
            // This code implies that the feature has not exited. Thus it should
            // never be returned if you call GetExitInfo after receiving
            // UIX_DOWORK_FEATURE_EXIT
            assert( 0 );
            break;

        case UIX_EXIT_FRIENDS_JOIN_GAME:
        {
            // This means the user accepted an invite to join someone
            // playing this game.
            // This exit code can happen in two completely different ways. One
            // is if the user is in the friends feature and accepts a game invite.
            // The other is if when the game starts up it retrieves a cross-game
            // invite - in that case this exit code is returned from the logon
            // feature. In that case the logon status recording has to be
            // completed, which is why this feature falls through to the logon code.

            // We should now take steps to join them.
            OutputDebugStringA( "JOIN_GAME\n");

            if( m_WhichScreen != SCREEN_LOGON)
            {
#ifndef LIVE_AWARE_ONLY
				// Get information about the friend that invited us to join their game.
				XONLINE_FRIEND* pFriendData = ( XONLINE_FRIEND* )exitInfo.pExitData;  
				JoinSession( pFriendData->sessionID );
#endif // LIVE_AWARE_ONLY
                break;
            }
            // Intentionally missing break!
            // This exit code is returned if you start the logon feature with a cross-game
            // invite. In that case we want to proceed with the normal process of gathering
            // information about the successful logon, then join the game we've been invited
			// to.
			bCrossGameInvite = TRUE;
        }

        case UIX_EXIT_LOGON_SUCCESSFUL:
        {
            // We have successfully signed in one or more gamers.

            if ( m_WhichScreen == SCREEN_LOGON )
                m_WhichScreen = SCREEN_LIVE;
            assert( exitInfo.pExitData );

            m_pLogonUsers = XOnlineGetLogonUsers();
            m_bLoggedOn = TRUE;
            m_bLoggingOn = FALSE;

            // Initialize XHV now that we're logged on, so we can use it for
            // detecting who is talking.
            XHV_RUNTIME_PARAMS xhvParams = { 0 };
            xhvParams.dwMaxLocalTalkers         = XGetPortCount();
            xhvParams.dwMaxRemoteTalkers        = XHV_MAX_REMOTE_TALKERS;
            xhvParams.dwMaxCompressedBuffers    = 4;                    // 4 buffers per local talker
            xhvParams.dwFlags                   = 0;
            xhvParams.pEffectImageDesc          = pdsImageDesc;
            xhvParams.dwEffectsStartIndex       = GraphVoice_Voice_0;

            // The out-of-sync threshold allows the title to control how aggresive
            // XHV is about determining that we've lost synchronization with a
            // remote talker.  A voice packet that is received significantly before or
            // after its expected time is considered to be "out-of-sync."  If this
            // many consecutive packets from a given remote talker are determined to
            // be out-of-sync, that remote talker will be reset, causing a brief pause
            // in their voice playback.
            // This number should be roughly twice as many packets as the title holds
            // in their network send buffer.
            xhvParams.dwOutOfSyncThreshold      = 10;
            // Make sure XHV is not initialized. It is not legal to have XHV initialized
            // when signing in.
            assert( !g_XHVVoiceManager.IsInitialized() );
            g_XHVVoiceManager.Initialize( g_pDSound, &xhvParams, this );

            // Tell UIX that we want to use the voice-mail feature. This enables voice
            // mail options for game invites and friend requests.
            m_pLiveEngine->UseVoiceMail( UIX_VOICE_MAIL );
            // Give UIX a pointer to our XHV engine, to use for voice mail.
            m_pLiveEngine->SetProperty( UIX_PROPERTY_VOICE_MAIL_ENGINE, (DWORD)g_XHVVoiceManager.GetXHVEngine() );
            g_XHVVoiceManager.SetMaxPlaybackStreamsCount( NUM_XHV_PLAYBACK_STREAMS );

            // You can adjust the voicemail target using this property, sending it either
            // to speakers or the voice communicator.
            m_pLiveEngine->SetProperty( UIX_PROPERTY_VOICE_MAIL_TO_SPEAKERS, TRUE );

            // Authentication successful

			// Get the initial states for the headphone and
            // microphone devices
            m_dwMicrophoneState = XGetDevices( XDEVICE_TYPE_VOICE_MICROPHONE );
            m_dwHeadphoneState  = XGetDevices( XDEVICE_TYPE_VOICE_HEADPHONE );
            XONLINE_USER UserAccounts[XONLINE_MAX_STORED_ONLINE_USERS];
            DWORD dwNumUsers;
            XOnlineGetUsers( UserAccounts, &dwNumUsers );
            for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
            {
                m_Users[i].bSignedOn  = m_pLogonUsers[i].xuid.qwUserID != 0;
                m_Users[i].bVoice    =  FALSE;
                m_Users[i].bGuest     = XOnlineIsUserGuest(  m_pLogonUsers[i].xuid.dwUserFlags );
                m_Users[i].dwUserFlags = m_pLogonUsers[i].xuid.dwUserFlags;

				if( m_Users[i].bSignedOn )
                {
					// This swprintf call copies the gamer tag over and also converts it from
					// CHAR to WCHAR. This makes dealing with the gamertag in print code
					// easier.
					swprintf( m_Users[i].strGamertag, L"%S", m_pLogonUsers[i].szGamertag );

                    if( !m_Users[i].bGuest )
                    {
						// Set the initial player state

						m_Users[i].dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE;
                        if ( !m_bSilentLogon )
						{
							m_Users[i].bVoice = XOnlineIsUserVoiceAllowed( m_Users[i].dwUserFlags ) &&
								( m_dwMicrophoneState & ( 1 << i ) ) &&
								( m_dwHeadphoneState  & ( 1 << i ) );
						}
						else
						{
							// When using Silent Sign-on, any communicator indicates voice capability
							m_Users[i].bVoice = XOnlineIsUserVoiceAllowed( m_Users[i].dwUserFlags ) &&
								( m_dwMicrophoneState ) && ( m_dwHeadphoneState );
						}

                        if( m_Users[i].bVoice )
                            m_Users[i].dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;

                        SetPlayerOnlineState( i, m_Users[i].dwState );
                    }
                }
                else
                    m_Users[i].strGamertag[0] = 0;
            }

			ShowLoginState();

			// Always register all local talkers. After silent sign-on, any controller may
			// be used to bring up the Friends list
			for(int j = 0; j < XGetPortCount(); ++j)
			{
				// TODO: Should really be checking for voice banning here with XOnlineIsUserVoiceAllowed.
				g_XHVVoiceManager.RegisterLocalTalker( j );
				//
				g_XHVVoiceManager.SetProcessingMode( j, XHV_VOICECHAT_MODE );
				// We can only do this if nobody is voice banned
				g_XHVVoiceManager.SetVoiceThroughSpeakers( j, TRUE );
			}

#ifndef LIVE_AWARE_ONLY
			// After a retrieved-game state logon, attempt to join the old session, if possible
			if ( memcmp( &m_LaunchData.SessionID, &ZeroSessionID, sizeof( XNKID ) ) != 0 )
			{
				JoinSession( m_LaunchData.SessionID );
			}

			if ( bCrossGameInvite)
			{
		        XONLINE_FRIEND* pFriendData = ( XONLINE_FRIEND* )exitInfo.pExitData;  
				JoinSession( pFriendData->sessionID );
			}
#endif

            break;
        }

        case UIX_EXIT_LOGON_FAILED:
            // This exit code either means that logon failed or it means that
            // the users were signed out after a successful logon.
            // Because of this second use, this exit feature message can arrive
            // even when no feature is running.
            // Players can be signed out unexpectedly if they sign in to a second
            // Xbox or if they lose their network connection.
            wsprintfW( m_strMessage, L"Authentication Failed with Error 0x%x", exitInfo.hr  );
            // Give specific information about known errors.
            switch ( exitInfo.hr )
            {
                case XONLINE_E_SILENT_LOGON_DISABLED:
                    wsprintfW( m_strMessage, L"Not signed in (auto-sign in disabled)" );
					m_bSilentLogon = FALSE;
                    break;

                case XONLINE_E_SILENT_LOGON_NO_ACCOUNTS:
                    // If there are no Live accounts, display nothing.
                    m_strMessage[0] = 0;
					m_bSilentLogon = FALSE;
                    break;

                case XONLINE_E_SILENT_LOGON_PASSCODE_REQUIRED:
                    wsprintfW( m_strMessage, L"Not signed in: passcode required" );
					m_bSilentLogon = FALSE;
                    break;

                case XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON:
                    // UIX by default displays a popup in this case, so we don't need
                    // to display the error code, so I clear it.
                    m_strMessage[0] = 0;
                    break;
            }

            // We clear m_bLoggedOn because UIX has already signed us out
            // (if we were previously signed in) and we don't want to call
            // m_pLiveEngine->LogOff() and SetPlayerOnlineState().
            m_bLoggedOn = FALSE;
            Reset();
            break;

        case UIX_EXIT_LOGON_USER_EXIT:
            // This means the user backed out of the logon screen.
            m_WhichScreen = SCREEN_LIVE;
            m_bLoggingOn = FALSE;
            m_strMessage[0] = 0;
            break;

        // Friends feature exit codes:
        case UIX_EXIT_FRIENDS_NORMAL_EXIT:
            // This means the user backed out of the friends feature.
            m_WhichScreen = SCREEN_LIVE;
            ShowLoginState();
            break;

        case UIX_EXIT_FRIENDS_JOIN_GAME_CROSS_TITLE:
        {
            // This means the user accepted an invite to join another
            // title. They will get the dialog asking them to insert the DVD for the other
			// game. If they get here, it means they backed out of that dialog without
			// inserting the DVD.

            OutputDebugStringA( "JOIN_GAME_CROSS_TITLE\n" );
            // Get information about the friend that invited us to join their game.
            // You aren't required to do anything with this information.
            XONLINE_FRIEND* pFriendData = ( XONLINE_FRIEND* )exitInfo.pExitData;
            m_WhichScreen = SCREEN_LIVE;
            ShowLoginState();
            break;
        }

        case UIX_EXIT_FRIENDS_SIGNED_OUT:
            // This message is received when the user logs out from the
            // friends menu.
            OutputDebugStringA( "FRIENDS_SIGNED_OUT\n" );
            m_bLoggedOn = FALSE;
            Reset();
            m_WhichScreen = SCREEN_LIVE;
            ShowLoginState();
            break;
    }
}




VOID CXBoxSample::StartLogon( BOOL bSilentLogon, XONLINE_ACCEPTED_GAMEINVITE* pInvite, XONLINE_LOGON_STATE* pState )
{
    // Clear any old error messages.
    m_strMessage[0] = 0;

    // Declare and zero UIX_LOGON_PARAMS
    UIX_LOGON_PARAMS    LogonParams = {0};
    LogonParams.StructSize = sizeof( LogonParams );

    if ( bSilentLogon )
    {
        // Select the type of logon - normal, silent, etc.
        // Silent logon is typically used for live aware titles.
        LogonParams.LogonType = UIX_LOGON_TYPE_SILENT;

        // Specify how many users should be allowed to logon. Silent
        // logon always implies one user being signed in.
        LogonParams.LogonUserCount = 1;

		m_bSilentLogon = TRUE;

    }
    else
    {
        // Specify how many users should be allowed to logon. This can be
        // 1, 2, or 4.
#ifdef  LIVE_AWARE_ONLY
        LogonParams.LogonUserCount = 1;
#else
        LogonParams.LogonUserCount = 4;
#endif

        // Do a retrieved-state logon, a cross game invite, or a normal logon
		if ( pState )
		{
            // Handle retrieved state
#ifdef DEBUG  
			// Pause to provide time to reconnect debugger
			Sleep(7000);
#endif
            LogonParams.LogonType = UIX_LOGON_TYPE_RETRIEVED_STATE;
            LogonParams.LogonUserCount = 1; // REVIEW: Does this need to be set?
            LogonParams.pLogonState = pState;
		}
        else if ( pInvite )
        {
            // Handle cross-game invites.
            LogonParams.LogonType = UIX_LOGON_TYPE_RETRIEVED_GAME_INVITE;
            LogonParams.LogonUserCount = 1; // REVIEW: Does this need to be set?
            LogonParams.pGameInvite = pInvite;
        }
		else
            LogonParams.LogonType = UIX_LOGON_TYPE_NORMAL;
    }

    // Specify one or more services, consecutively starting from index zero.
    LogonParams.LogonServiceIDs[0] = XONLINE_MATCHMAKING_SERVICE;
    LogonParams.LogonServiceIDs[1] = XONLINE_FEEDBACK_SERVICE;

    // Optionally specify logon state or a game invite to use for logon.

    // Start the process of signing in to Live
    HRESULT hr = m_pLiveEngine->StartFeature( UIX_LOGON_FEATURE, &LogonParams );

    if ( FAILED( hr ) )
    {
        wsprintfW( m_strMessage, L"Signing in Failed with Error 0x%x", hr );
        m_WhichScreen = SCREEN_LIVE;
    }
    else
    {
        m_bLoggingOn = TRUE;
        // Let the user know that they are signing in. Only critical for silent
        // logon (might not be displayed otherwise).
        wcscpy( m_strMessage, L"Signing in..." );
        if ( pState || pInvite )
		{
			// Both of these states should seamlessly transfer the player
			// into the Live menu. But we do need to block and display
			// progress messages while the logon is in process.
			// Progress messages are handled in Render()
			m_WhichScreen = SCREEN_LIVE;	
		}
		else if ( !bSilentLogon )
        {
            // With silent logon we don't change menus to logon.
            // With a manual logon we go to the logon screen.
            m_WhichScreen = SCREEN_LOGON;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: ProcessInput()
// Desc: Processes user input, changing states and starting features as needed.
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessInput()
{
    switch ( m_WhichScreen )
    {
        case SCREEN_INTRO:
            if ( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] ||
                        ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_START ) )
            {
                m_WhichScreen = SCREEN_MAIN;

				// For all Live titles we start a silent logon (automatic logon)
                // as soon as the user does the first button press.
				// If the silent logon fails or if the user chooses to sign out,
				// the user will have the option to do a manual sign-on.
				// Note that we do not block on the logon before the main menu,
				// but rather continue into game UI and let the logon continue
				// asynchronously in the background.

                StartLogon( TRUE, 0 );
            }
            break;

        case SCREEN_MAIN:
            if ( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
            {
                m_WhichScreen = SCREEN_LIVE;
            }
            break;

        case SCREEN_LIVE:
            if ( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
            {
                m_WhichScreen = SCREEN_MAIN;
                break;
            }

            if ( m_bLoggedOn )
            {
                DWORD DesiredUserIndex = XONLINE_MAX_LOGON_USERS;
                DWORD InputType = 0;
				BOOL  SessionChangeRequested = FALSE;
                for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS ; ++i )
                {
                    // Only users that are signed in can manage their friends or logout,
					// except after a silent sign-on when any controller can be used
                    if ( ( m_pLogonUsers[i].xuid.qwUserID && SUCCEEDED( m_pLogonUsers[i].hr ) )
						 || m_bSilentLogon )
                    {
                        // Did this controller request managing friends?
                        if ( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
                        {
							if ( m_bSilentLogon || !XOnlineIsUserGuest( m_pLogonUsers[i].xuid.dwUserFlags ) )
							{
	                            InputType = XINPUT_GAMEPAD_A;
		                        DesiredUserIndex = i;
							}
                        }

                        // Did this controller request logout?
                        if ( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
                        {
                            InputType = XINPUT_GAMEPAD_X;
                            DesiredUserIndex = i;
                        }

#ifndef LIVE_AWARE_ONLY
						// If any user requests a session change, all users are either joined or removed from the current session
						if ( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
						{
							SessionChangeRequested = TRUE;
							DesiredUserIndex = XONLINE_MAX_LOGON_USERS;
						}
#endif // LIVE_AWARE_ONLY

						// This demonstrates the use of the retrieved state logon.
						// To do this, we reboot back to ourselves, passing launch data with
						// state and session information (if not Live Aware Only.)
						// In a real game, this would likely happen during sessions if the
						// game engine is in a separate XBE from the UI XBE, or if the
						// game is being launched from a launcher XBE that already has
						// Live Aware functionality. In all cases, you want to try to 
						// persist the Live connection seamlessly across the reboot.
						if ( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
						{
							LAUNCH_DATA LaunchData;
							UIXFRIENDS_LAUNCH_DATA* pULD = (UIXFRIENDS_LAUNCH_DATA*)&LaunchData;
							memset( &LaunchData, 0, sizeof( LAUNCH_DATA ) );
							pULD->dwID = UIXFRIENDS_LAUNCH_ID; // Fixed identifier for this sample
							pULD->bSilentLogon = m_bSilentLogon;
#ifndef LIVE_AWARE_ONLY
							memcpy( &(pULD->SessionID), &m_SessionID, sizeof( XNKID ) );
#endif
							if ( SUCCEEDED( XOnlineSaveLogonState( &( pULD->LogonState ) ) ) )
								XLaunchNewImage( PATH_TO_THIS_TITLE, &LaunchData );
						}

					}
                }

#ifndef LIVE_AWARE_ONLY
				if ( SessionChangeRequested )
				{
					BOOL bInSession = !( memcmp( &m_SessionID, &ZeroSessionID, sizeof( m_SessionID ) ) == 0 );
					if ( bInSession )
						LeaveSession();
					else
						CreateSession();
				}
#endif // LIVE_AWARE_ONLY

                // If a signed in person pressed a button, then let's process it.
                if ( DesiredUserIndex != XONLINE_MAX_LOGON_USERS )
                {
                    // Clear any old error messages.
                    m_strMessage[0] = 0;

                    if ( InputType == XINPUT_GAMEPAD_A )
                    {
                        // Declare and zero a UIX_FRIENDS_PARAMS struct
                        UIX_FRIENDS_PARAMS  FriendsParams = {0};
                        FriendsParams.StructSize = sizeof( FriendsParams );
						FriendsParams.UserPort = DesiredUserIndex;	

						// Remember who brought up the Friends menu
						if ( m_bSilentLogon )
							m_dwUserIdx = 0;
						else
							m_dwUserIdx = DesiredUserIndex;
						m_dwCtlrIdx = DesiredUserIndex;

                        // If you are hosting a game and you accept a game-invite from the friends
                        // list then UIX needs to pop up a message to confirm that you want to
                        // accept the game invite. This property controls that. 

						// Our sample has no real connection between clients and no real game, but
						// we simulate correct use of this property in a game that does not support
						// host migration.

                        m_pLiveEngine->SetProperty( UIX_PROPERTY_HOST_NO_MIGRATION, TRUE );


                        // FriendsParams.SelectedFriendXUID = XUID of friend to initially select in list
                        // Set this if you want to allow signout from the friend's feature.
#ifdef LIVE_AWARE_ONLY
                        // This probably only makes sense in a live aware only title, since a
                        // regular live title sould have sign-out options elsewhere.
                        FriendsParams.SignOutEnabled = TRUE;
#else
                        FriendsParams.SignOutEnabled = FALSE;
#endif

                        // Start the friend's feature
                        HRESULT hr = m_pLiveEngine->StartFeature( UIX_FRIENDS_FEATURE, &FriendsParams );

                        if ( FAILED( hr ) )
                        {
                            wsprintfW( m_strMessage, L"Managing Friends Failed with Error 0x%x", hr );
                            m_WhichScreen = SCREEN_LIVE;
                        }
                        else
                            m_WhichScreen = SCREEN_FRIENDS;
                    }

                    // If a signed in person pressed X then sign everybody out.
                    if ( InputType == XINPUT_GAMEPAD_X )
                    {
                        Reset();
                        // Stay on the LIVE screen.
                        m_WhichScreen = SCREEN_LIVE;
                        ShowLoginState();
                    }
                }
            }
            else if ( !m_bLoggingOn )
            {
                // If any user pressed A...
                if ( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
                {
                    StartLogon( FALSE );
                }
            }
            break;

        case SCREEN_LOGON:
        case SCREEN_FRIENDS:
			break;

        default:
            assert( 0 );
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background and clear the zbuffer
    RenderGradientBackground( 0xff404040, 0xff404080 );

    // Show title, frame rate, menus, and UIX imagery
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 48, 46, 0xffffffff, L"UIXFriends" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );

    // Draw the menus.
    const WCHAR* text = L"";

    // Buffer to print notification information to.
    WCHAR NotificationBuffer[2000];
    // Mark the buffer as being a zero length string - more efficient than going = "";
    // on the declaration.
    NotificationBuffer[0] = 0;
    // NotificationPointer points to the next place to print notification
    // information so that we can efficiently append to the buffer.
    WCHAR* NotificationPointer = NotificationBuffer;

    if ( m_WhichScreen == SCREEN_INTRO )
    {
        text = L"Press " GLYPH_A_BUTTON L" to exit this fancy intro...";
    }
    else if ( m_WhichScreen == SCREEN_MAIN )
    {
        text = L"Main Menu\n\n"
               L"Press " GLYPH_A_BUTTON L" for Live.\n"
               L"Press something else for the real game.\n";
    }
    else if ( m_WhichScreen == SCREEN_LIVE )
    {
        if ( m_bLoggedOn )
        {

#ifndef LIVE_AWARE_ONLY
			text = L"Live\n\n"
                   L"Press " GLYPH_A_BUTTON L" to manage friends.\n"
                   L"Press " GLYPH_X_BUTTON L" to sign off.\n"
				   L"Press " GLYPH_Y_BUTTON L" to leave or join session.\n"
				   L"Press " GLYPH_WHITE_BUTTON L" to reboot.";
#else
			text = L"Live\n\n"
                   L"Press " GLYPH_A_BUTTON L" to manage friends.\n"
                   L"Press " GLYPH_X_BUTTON L" to sign off.\n"
				   L"Press " GLYPH_WHITE_BUTTON L" to reboot.";
#endif

            // An appropriate notification icon should be displayed beside the Friends menu
            // entry to indicate that there is a pending request or invite.
            for ( DWORD PortIndex = 0; PortIndex < XONLINE_MAX_LOGON_USERS; ++PortIndex )
            {
                DWORD Notification = 0;
                m_pLiveEngine->GetNotifications( PortIndex, UIX_NOTIFICATION_MENU, &Notification );
                if ( Notification & UIX_DOWORK_NOTIFY_FRIEND_REQUEST )
                {
                    // wsprintfW returns the number of characters printed, so this function call
                    // prints the notification information and updates the output pointer.
                    NotificationPointer += wsprintfW( NotificationPointer,
                                    L"%s (controller %d) has a Friend Request\n",
                                    m_Users[PortIndex].strGamertag, PortIndex + 1 );
                }
                else if ( Notification & UIX_DOWORK_NOTIFY_GAME_INVITE )
                {
                    // wsprintfW returns the number of characters printed, so this function call
                    // prints the notification information and updates the output pointer.
                    NotificationPointer += wsprintfW( NotificationPointer,
                                    L"%s (controller %d) has a Game Invite\n",
                                    m_Users[PortIndex].strGamertag, PortIndex + 1 );
                }
            }
		}
        else if ( m_bLoggingOn )
        {
            text = L"Live\n\n"
                   L"Signing in...\n";
        }
        else
        {
            text = L"Live\n\n"
                   L"Press " GLYPH_A_BUTTON L" to sign in.\n";
        }
    }

    if ( !IsUIXScreen() )
    {
        // Display any error or informative messages there may be.
        m_Font.DrawText( 320, 430, 0xffffffff, m_strMessage, XBFONT_CENTER_X );
    }

    m_Font.DrawText( 320, 140, 0xffffffff, text, XBFONT_CENTER_X );

    m_Font.DrawText( 320, 340, 0xffffffff, NotificationBuffer, XBFONT_CENTER_X );

	m_Font.End();


    // Rendering code needed for UIX

    if ( m_dwLiveWorkFlags & UIX_DOWORK_NEED_TO_RENDER )
    {
        m_pLiveEngine->Render( m_pBackBuffer );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

#ifndef LIVE_AWARE_ONLY
//-----------------------------------------------------------------------------
// Name: ChangeSessionState()
// Desc: Reflects changes to player state whenever session changes
//-----------------------------------------------------------------------------
VOID CXBoxSample::ChangeSessionState( BOOL bJoiningSession )
{
	// Change state for all logged on users: all users must either join or
	// leave a session together

	// Sessions in this sample are always joinable. In a real multiplayer game
	// this flag would be set depending on whether
	// a. The title supported join-in-progress or some method for players to
	//    join a game after it had started (ex: spectator mode, waiting in lobby
	//    for the current round to end)
	// b. The session has available private slots for friends to join

	for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
	{
		if ( m_Users[ i ].bSignedOn )
		{
			if ( bJoiningSession )
				m_Users[i].dwState |= ( XONLINE_FRIENDSTATE_FLAG_JOINABLE | XONLINE_FRIENDSTATE_FLAG_PLAYING );
			else // leaving session
				m_Users[i].dwState &= ~( XONLINE_FRIENDSTATE_FLAG_JOINABLE | XONLINE_FRIENDSTATE_FLAG_PLAYING );

			SetPlayerOnlineState( i, m_Users[i].dwState );
		}
	}

	ShowLoginState();
}

//-----------------------------------------------------------------------------
// Name: JoinSession()
// Desc: For this sample, "joining" the game is nothing more than changing our 
//       current session ID to the same session ID as the remote friend and
//       then updating player state correctly. In game titles, this process
//       will be much more involved.
//-----------------------------------------------------------------------------
VOID CXBoxSample::JoinSession( XNKID sessionID )
{
	m_SessionID = sessionID;
	wsprintfW( m_strMessage, L"Joined Friend's Game: 0x%x", m_SessionID );
    m_WhichScreen = SCREEN_LIVE;
	
	// Propagate session state through notification system
	ChangeSessionState( TRUE );

	ShowLoginState();
}

//-----------------------------------------------------------------------------
// Name: CreateSession()
// Desc: For this sample, creating a game is as simple as creating a new
//       sessionID randomly and registering that with the system. In a real
//       Live game that supports multiplayer, this sessionID is usually 
//       generated by the matchmaking system
//-----------------------------------------------------------------------------
VOID CXBoxSample::CreateSession()
{
	// In this sample, a session ID is created randomly and there is no real connection between
	// players. In ordinary Live games, a session ID is typically obtained through matchmaking
	// and there is an elaborate process for connecting players to a host for a game session.
	XNetRandom( (BYTE*)&m_SessionID, sizeof( m_SessionID ) );
	
	// Propagate changes through notification system
	ChangeSessionState( TRUE );
}

//-----------------------------------------------------------------------------
// Name: LeaveSession()
// Desc: For this sample, leaving the game is nothing more than changing our 
//       current session ID to zero and then updating player state correctly.
//       In game titles, this process will be much more involved.
//-----------------------------------------------------------------------------
VOID CXBoxSample::LeaveSession()
{
	memset( &m_SessionID, 0, sizeof(m_SessionID));
	
	// Propagate changes through notification system
	ChangeSessionState( FALSE );
}
#endif // LIVE_AWARE_ONLY
