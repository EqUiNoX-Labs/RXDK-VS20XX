//-----------------------------------------------------------------------------
// File: UIXPlayers.cpp
//
// Desc: Demonstrates how to use UIX to logon and then display players lists.
// See readme.txt for more details.
//
// Hist: 5.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "UIXPlayers.h"
#include "XBOnline.h"
#include <dsstdfx.h>
#include "XHVVoiceManager.h"

// XHV and DSound objects for detecting when the player is talking.
CXHVVoiceManager    g_XHVVoiceManager;
LPDIRECTSOUND8      g_pDSound;
LPDSEFFECTIMAGEDESC pdsImageDesc;

// This global variable tracks which port was used to invoke the players
// list so that XHV can return appropriate information about who is talking
// remotely.
DWORD               g_InvocationPort;

//
// g_FakeGamerTags array used to create some names for players
//
const char* g_FakeGamerTags[] =
{
    "Billy bob",
    "My Gamertag",
    "WWWWWWWWWWWWWWW",
    "Whatever",
    "Fish bucket",
    "Caramel",
    "Sticky bun",
    "Dessert",
};




//-----------------------------------------------------------------------------
// CTitlePlayer
// class that implements ITitlePlayersListItem and stores some player information
//-----------------------------------------------------------------------------
#define BIT_FLAGS_BLUE_TEAM 0x00000001
#define BIT_FLAGS_RED_TEAM  0x00000002
#define BIT_FLAGS_TEAMS     ( BIT_FLAGS_BLUE_TEAM | BIT_FLAGS_RED_TEAM )
#define BIT_FLAGS_HAS_GUN   0x00000004

class CTitlePlayer : public ITitlePlayersListItem
{
public:
    CTitlePlayer( XUID X, const char* GamerTag, int portNumber = -1 )
    {
        Xuid = X;
        // A port number of -1 implies a user on another box
        m_Port = portNumber;

        // We generate random commstatus and score - games would generate
        // this information in whatever way is appropriate.
        // XHV is used to get commstatus for local players.
        commstatus = (UIX_VOICE_STATUS_TYPE)(rand() % 3);
        Score = (UINT)rand() % 21;

        // Note that this _snwprintf call also converts the gamertag to Unicode
        // We print the text we want displayed for the player. This may be just
        // the player's gamertag, or it may include other information.
        if( XOnlineIsUserGuest( Xuid.dwUserFlags ) )
        {
            // For guests, it is advisable to print something that indicates that
            // they are a guest. The 'Score' field should be omitted if it is
            // from a leaderboard, since that makes no sense for a guest.
            _snwprintf( ScoreText, ScoreTextSize, L"Guest of %S", GamerTag );
        }
        else
        {
            // For regular (non-guest) players you can just print the gamer tag
            // or you can display additional information, such as a ranking,
            // current score in this game, etc.
            _snwprintf( ScoreText, ScoreTextSize, L"%S (%u)", GamerTag, Score);
        }
        // Ensure that the string is definitely terminated.
        ScoreText[ScoreTextSize-1] = 0;

        // Create flags to indicate what team players are on. This can be used for
        // filtering who is displayed.
        Flags = 0;
        // For remote users just randomly choose a team.
        // For local users assign those on odd numbered ports to one team and those
        // on even numbered ports to the other team. This makes it easier to verify
        // that the list is displayed differently depending on who invoked it - just
        // do a two player logon in adjacent ports (i.e.; ports 0 and 1).
        if( portNumber >= 0 )
            Flags |= (portNumber & 1) ? BIT_FLAGS_BLUE_TEAM : BIT_FLAGS_RED_TEAM;
        else
            Flags |= (rand() % 2) ? BIT_FLAGS_BLUE_TEAM : BIT_FLAGS_RED_TEAM;
        Flags |= (rand() % 2) ? BIT_FLAGS_HAS_GUN : 0;
    }

    STDMETHODIMP_(CONST WCHAR*) GetName() CONST { return ScoreText; }
    STDMETHODIMP_(CONST XUID*)  GetXUID() CONST { return &Xuid; }
    STDMETHODIMP_(UIX_VOICE_STATUS_TYPE) GetVoiceStatus() CONST
    {
        // What is the voice status of the user specified by m_Port/Xuid?
        if( m_Port == -1 )
        {
            // Handle remote users
            // For a real game we need would to get this information about remote users.
            return commstatus;
        }
        else
        {
            // Handle local users

            // Guests can't use a communicator.
            if( XOnlineIsUserGuest( Xuid.dwUserFlags )  )
                return UIX_VOICE_STATUS_NONE;

            XHV_LOCAL_TALKER_STATUS UIX_talker_status;
            g_XHVVoiceManager.GetLocalTalkerStatus( m_Port, &UIX_talker_status);
            if( UIX_talker_status.communicatorStatus == XHV_VOICE_COMMUNICATOR_STATUS_INSERTED)
                return UIX_VOICE_STATUS_COMMUNICATOR;
            else
                return UIX_VOICE_STATUS_NONE;
        }
    }
    STDMETHODIMP_(BOOL) IsTalking() CONST
    {
        // Is the user specified by m_Port/Xuid talking to the user (specified by
        // g_InvocationPort)?
        if( m_Port == -1 )
        {
            // Handle remote users
            // This function returns whether the remote user is talking
            // to a particular user, so we have to pass in a valid port number.
            // We pass in the port number for whoever invoked the feature.
            if( g_InvocationPort >= 0 && g_InvocationPort < XONLINE_MAX_LOGON_USERS )
                return g_XHVVoiceManager.IsTalking( Xuid, g_InvocationPort );
            // If we don't have an invocation port then the players feature must have been
            // started by a guest, and by definition nobody is talking to a guest.
            return FALSE;
        }
        else
        {
            // Handle local users

            // Guests can't use a communicator.
            if( XOnlineIsUserGuest( Xuid.dwUserFlags ) )
                return FALSE;

            XHV_LOCAL_TALKER_STATUS UIX_talker_status;
            g_XHVVoiceManager.GetLocalTalkerStatus( m_Port, &UIX_talker_status);
            return UIX_talker_status.bIsTalking;
        }
    }
    STDMETHODIMP_(DWORD) GetBitflags() CONST { return Flags; }
    // User defined sort function to allow sorting players by whatever criteria the
    // game wants.
    STDMETHODIMP_(INT) Compare(CONST ITitlePlayersListItem *B) CONST
    {
        CTitlePlayer *pPlayer = (CTitlePlayer *)B;

        if(Score < pPlayer->Score)
            return -1;
        else if(Score == pPlayer->Score)
            return 0;
        else
            return 1;
    }

    DWORD GetPort() const
    {
        return m_Port;
    }

public:
    XUID    Xuid;
    UIX_VOICE_STATUS_TYPE commstatus;       // Random for remote users, obtained from XHV for local users
    UINT    Score;                          // Random between 0 and 20
    static const int ScoreTextSize = XONLINE_GAMERTAG_SIZE+10;
    WCHAR   ScoreText[ScoreTextSize];       // Textual representation of score
    DWORD   Flags;                          // Player flags
    DWORD   m_Port;                         // Port number, or -1 for remote users.
};

//
// Storage for players we create - we store them in a vector to simplify tracking
// them. We store pointers so that the actual objects never move around.
//
#include <vector>
using namespace std;
vector<CTitlePlayer *> g_Players;




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
    // We have to zero these before calling Reset().
    m_pLiveEngine = NULL;
    m_strMessage[0] = 0;
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

    // Other variables are cleared in the Reset() function, but this
    // variable must be left intact by Reset() so we clear it here.
    m_strMessage[0] = 0;

    // Initialize the fake session ID that we use for issuing invitations
    // with something non-zero.
    memcpy( &m_FakeSessionID, "FAKESESS", sizeof( m_FakeSessionID ) );

    // Initialization needed for UIX
    // Zero the work flags.
    m_dwLiveWorkFlags = 0;

    // Initialize the online library
    HRESULT hr;

    hr = XOnlineStartup( NULL );
    if( FAILED( hr ) )
    {
        XBUtil_DebugPrint( "XOnlineStartup failed (error 0x%x)", hr );
    }

    // Create a UI plugin object
    static UIXFont m_UIXFont( &m_Font );

    hr = UIXCreateUIPlugin( &m_UIXFont, &m_pUIPlugin );
    if( FAILED( hr ) )
    {
        XBUtil_DebugPrint( "Failed (error 0x%x)", hr );
    }

    // Create the live engine
    hr = UIXCreateLiveEngine( "d:\\media\\UIXPlayers.uix", XGetLanguage(), &m_pLiveEngine );
    if( FAILED( hr ) )
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
    m_pLiveEngine->EnableFeature( UIX_PLAYERS_FEATURE );

    // We want UIX to handle displaying notifications for things like
    // logoffs due to duplicate logons, and friend invites. If we set
    // this to FALSE then we will be responsible for displaying the
    // notifications.
    m_pLiveEngine->SetProperty( UIX_PROPERTY_DISPLAY_NOTIFICATIONS, TRUE );

    // We want UIX to put a Send Game Invite item in the friends menu. We
    // should set this property to FALSE if we don't have a game session or
    // if our session is full. This attribute defaults to TRUE
    m_pLiveEngine->SetProperty( UIX_PROPERTY_ALLOW_GAME_INVITES, TRUE );

    StartScreen( SCREEN_ERROR );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Reset to a known-good state. Logged off and at the main menu.
//-----------------------------------------------------------------------------
VOID CXBoxSample::Reset()
{
    StartScreen( SCREEN_ERROR );
    m_ScreenHeader = L"";
    m_ScreenFooter = L"";

    // If there are some players in the list, free their memory and unregister them.
    if( g_Players.size() )
    {
        // Get the ILivePlayersList interface
        ILivePlayersList *pPlayers;
        m_pLiveEngine->GetFeatureInterface(UIX_PLAYERS_FEATURE, NULL, (LPVOID *)&pPlayers);
        for( DWORD i = 0; i < g_Players.size(); ++i )
        {
            CTitlePlayer* pPlayer = g_Players[i];
            // Unregister that player - they will now show up as departed.
            pPlayers->UnregisterPlayer( pPlayer );
            // Now we can delete the player record.
            delete pPlayer;
        }

        // Set the g_Players vector to zero size.
        // Using resize instead of clear so that the vector just marks its memory as unused
        // instead of freeing it (VC++ 7.x behavior).
        g_Players.resize( 0 );
    }

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

    if( m_pLiveEngine )
    {
        if( m_bLoggedOn )
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
    m_bPlayersListOpen = FALSE;
    m_dwMicrophoneState = 0;
    m_dwHeadphoneState = 0;
    m_pLogonUsers = NULL;
    ZeroMemory( &m_Users, sizeof( m_Users ) );
    m_ScreenStackEntries = 0;
    m_FilterFlags = 0;
}




//-----------------------------------------------------------------------------
// Name: PushAndStartScreen()
// Desc: Push the current screen onto a stack and start another screen.
// This call should be followed by a call to PopScreen.
//-----------------------------------------------------------------------------
VOID CXBoxSample::PushAndStartScreen( WHICH_SCREEN whichScreen )
{
    assert( m_ScreenStackEntries < ( sizeof( m_ScreenStack ) / sizeof( m_ScreenStack[0] ) ) );
    m_ScreenStack[m_ScreenStackEntries] = ScreenRecord( m_WhichScreen, m_ScreenHeader, m_ScreenFooter );
    ++m_ScreenStackEntries;
    StartScreen( whichScreen );
}




//-----------------------------------------------------------------------------
// Name: PopScreen()
// Desc: Pop the last screen off of the stack so that we can return to it.
//-----------------------------------------------------------------------------
VOID CXBoxSample::PopScreen()
{
    assert( m_ScreenStackEntries > 0);
    --m_ScreenStackEntries;
    m_WhichScreen = m_ScreenStack[m_ScreenStackEntries].m_WhichScreen;
    m_ScreenHeader = m_ScreenStack[m_ScreenStackEntries].m_ScreenHeader;
    m_ScreenFooter = m_ScreenStack[m_ScreenStackEntries].m_ScreenFooter;
}




//-----------------------------------------------------------------------------
// Name: StartScreen()
// Desc: Do whatever initialization is required for the screen we are about to
// start.
//-----------------------------------------------------------------------------
VOID CXBoxSample::StartScreen( WHICH_SCREEN whichScreen, DWORD portNumber /*= -1*/ )
{
    // Record the screen that is now active.
    m_WhichScreen = whichScreen;
    // Default to no title.
    m_ScreenHeader = L"";
    m_ScreenFooter = L"";

    // Run any necessary startup code for this screen.
    switch ( m_WhichScreen )
    {
    case SCREEN_ERROR:
        break;

    case SCREEN_LOGON:
        break;

    case SCREEN_MAINMENU:
        break;

    case SCREEN_LOBBY:
        StartPlayersList( TRUE, TRUE, TRUE, portNumber, 0 );
        m_ScreenHeader = L"Lobby";
        // If you want custom text for the B button - which will normally say 'back' - then
        // you can set STR_PLAYERS_BACK_BUTTON in the loc\*.inf file to an empty string and
        // then write whatever text you want over top. You could then write different text
        // for the different uses of the players feature.
        m_ScreenFooter = GLYPH_Y_BUTTON L" Options   " GLYPH_A_BUTTON L" Start Match  ";
        break;

    case SCREEN_MYTEAM_ONLY:
        // Display all of the players who are on the same team as the player invoking
        // the players list.
        for( DWORD i = 0; i < g_Players.size(); ++i )
        {
            if( g_Players[i]->GetPort() == portNumber )
            {
                StartPlayersList( FALSE, FALSE, TRUE, portNumber, g_Players[i]->GetBitflags() & BIT_FLAGS_TEAMS );
                m_ScreenHeader = L"Press " GLYPH_X_BUTTON L" to cycle team display.";
                break;
            }
        }
        break;

    case SCREEN_GUEST_VIEW:
        StartPlayersList( FALSE, FALSE, TRUE, UIX_INVALID_VALUE, 0 );
        m_ScreenHeader = L"Guest view";
        break;

    case SCREEN_START_MENU_PLAYERS_LIST:
        StartPlayersList( FALSE, TRUE, TRUE, portNumber, 0 );
        m_ScreenHeader = L"Players list";
        break;

    case SCREEN_DEPARTED_PLAYERS_ONLY:
        StartPlayersList( FALSE, TRUE, FALSE, portNumber, 0 );
        m_ScreenHeader = L"Departed players list";
        break;

    case SCREEN_OPTIONS_OVERLAY:
        break;
    }
}




//-----------------------------------------------------------------------------
// Name: IsUIXScreen()
// Desc: Returns true if a UIX feature is in use, so that the game and shared
// menu display logic are all disabled.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::IsUIXScreen()
{
    return m_bLoggingOn || m_bPlayersListOpen;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Pass input to the UIX engine. We do this every frame, even when UIX is
    // not active, so that when a UIX feature starts up it can distinguish
    // between a button that was just pressed and a button that has been pressed
    // down for a while.

    // When we start the options screen we stop sending input to UIX so that the
    // input isn't processed twice.
    BOOL withholdInput = FALSE;
    if ( m_WhichScreen == SCREEN_OPTIONS_OVERLAY )
        withholdInput = TRUE;
    if( !withholdInput )
    {
        for( DWORD i = 0; i < XGetPortCount(); i++)
        {
            if( g_Gamepads[i].hDevice == NULL )
            {
                m_pLiveEngine->SetInput( i, NULL );
            }
            else
            {
                m_pLiveEngine->SetInput( i, &( g_InputStates[i] ) );
            }
        }
    }

    // Let the UIX engine do work. This also sets m_dwLiveWorkFlags
    // based on what UIX is doing, if anything.
    // This should be called every frame, even when UIX is dormant,
    // because UIX still watches for unexpected logoffs.
    m_pLiveEngine->DoWork( &m_dwLiveWorkFlags );

    if( g_XHVVoiceManager.IsInitialized() )
        g_XHVVoiceManager.DoWork();

    if( m_dwLiveWorkFlags & UIX_DOWORK_NOTIFICATIONS )
    {
        // Process the Live notifications
        // These only need to be handled if UIX_PROPERTY_DISPLAY_NOTIFICATIONS
        // was set to FALSE with SetProperty. If you do this - telling UIX
        // not to display notifications - then you have to handle displaying
        // them yourself.
        OutputDebugStringA("Notification received.\n");

        // This code gets information about the notifications so that they
        // can be displayed on the lower right corner of the screen. There
        // is no code to actually display them.
        // If you use UIX_NOTIFICATION_IN_GAME_FLASH then the notification will
        // only be sent once. If you use UIX_NOTIFICATION_MENU then the
        // notification will be sent until you start the friends feature.
        for ( DWORD PortIndex = 0; PortIndex < XONLINE_MAX_LOGON_USERS; ++PortIndex )
        {
            if( ( (UIX_PORT_0) << PortIndex ) & m_dwLiveWorkFlags )
            {
                // We've received a notification for the user on PortIndex
                DWORD Notification = 0;
                m_pLiveEngine->GetNotifications( PortIndex, UIX_NOTIFICATION_IN_GAME_FLASH, &Notification );
                if( Notification & UIX_DOWORK_NOTIFY_FRIEND_REQUEST )
                {
                    XBUtil_DebugPrint( "Friend request for port %d\n", PortIndex );
                }
                else if( Notification & UIX_DOWORK_NOTIFY_GAME_INVITE )
                {
                    XBUtil_DebugPrint( "Game invite for port %d\n", PortIndex );
                }
            }
        }
    }

    // Is the active feature exiting?
    if( m_dwLiveWorkFlags & UIX_DOWORK_FEATURE_EXIT )
    {
        // Process the exit codes.
        HandleFeatureExit();
    }

    // Has UIX requested a reboot?
    if( m_dwLiveWorkFlags & UIX_DOWORK_NEED_TO_REBOOT )
    {
        // Do any work needed prior to rebooting, then let UIX reboot.
        // You can pass in a context value that will be placed in
        // LD_LAUNCH_DASHBOARD::dwContext and will subsequently be passed
        // back to the you when the dashboard launches you. You can use
        // this to return to the same location in your menus.
        m_pLiveEngine->Reboot( 0 );
    }

    // If no UIX feature is using the input then we should process
    // the users input ourselves. We also process input if the lobby
    // screen is open (so we can start the game or invoke the options
    // screen) and if there is an overlay screen - such as the options
    // screen - active.
    if( m_WhichScreen == SCREEN_MYTEAM_ONLY ||
            m_WhichScreen == SCREEN_LOBBY || withholdInput ||
            ( m_dwLiveWorkFlags & UIX_DOWORK_PROCESSING_INPUT ) == 0 )
    {
        ProcessInput();
    }

    if( m_bLoggedOn )
    {
        // Update the online/voice state of each player.
        CheckDeviceStates();
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: HandleFeatureExit()
// Desc: Called whenever a feature exits, to handle the necessary state changes.
//-----------------------------------------------------------------------------
VOID CXBoxSample::HandleFeatureExit()
{
    UIX_EXIT_INFO exitInfo;
    m_pLiveEngine->GetExitInfo( &exitInfo );
    // exitInfo contains FeatureID, ExitCode, hr, and pExitData
    // Make sure that the filter flags are zeroed whenever a UIX feature
    // closes.
    m_FilterFlags = 0;

    switch ( exitInfo.ExitCode )
    {
        case UIX_EXIT_NONE:
            // This code implies that the feature has not exited. Thus it should
            // never be returned if you call GetExitInfo after receiving
            // UIX_DOWORK_FEATURE_EXIT
            assert( 0 );
            break;

        case UIX_EXIT_LOGON_SUCCESSFUL:
        {
            // We have successfully logged on one or more gamers.

            assert ( m_WhichScreen == SCREEN_LOGON );
            assert( exitInfo.pExitData );

            m_pLogonUsers = XOnlineGetLogonUsers();
            m_bLoggedOn = TRUE;
            m_bLoggingOn = FALSE;

            // Get the ILivePlayersList interface
            ILivePlayersList *pPlayers;
            m_pLiveEngine->GetFeatureInterface(UIX_PLAYERS_FEATURE, NULL, (LPVOID *)&pPlayers);

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
            
            // You can adjust the voicemail target using this property:
            // UIX_PROPERTY_VOICE_MAIL_TO_SPEAKERS

            // Authentication successful
            // Get the initial states for the headphone and
            // microphone devices
            m_dwMicrophoneState = XGetDevices( XDEVICE_TYPE_VOICE_MICROPHONE );
            m_dwHeadphoneState  = XGetDevices( XDEVICE_TYPE_VOICE_HEADPHONE );
            XONLINE_USER UserAccounts[XONLINE_MAX_STORED_ONLINE_USERS];
            DWORD dwNumUsers;
            XOnlineGetUsers( UserAccounts, &dwNumUsers );
            for( DWORD portNumber = 0; portNumber < XONLINE_MAX_LOGON_USERS; ++portNumber )
            {
                m_Users[portNumber].bSignedOn  = m_pLogonUsers[portNumber].xuid.qwUserID != 0;
                m_Users[portNumber].bVoice    =  FALSE;
                m_Users[portNumber].bGuest     = XOnlineIsUserGuest(  m_pLogonUsers[portNumber].xuid.dwUserFlags );
                m_Users[portNumber].dwUserFlags = m_pLogonUsers[portNumber].xuid.dwUserFlags;
                if( m_Users[portNumber].bSignedOn )
                {
                    // This swprintf call copies the gamer tag over and also converts it from
                    // CHAR to WCHAR. This makes dealing with the gamertag in print code
                    // easier.
                    swprintf( m_Users[portNumber].strGamertag, L"%S", m_pLogonUsers[portNumber].szGamertag );
                    m_strMessage[0] = 0;

                    if( !m_Users[portNumber].bGuest )
                    {
                        DWORD dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE;
                        m_Users[portNumber].bVoice = XOnlineIsUserVoiceAllowed( m_Users[portNumber].dwUserFlags ) &&
                            ( m_dwMicrophoneState & ( 1 << portNumber ) ) &&
                            ( m_dwHeadphoneState  & ( 1 << portNumber ) );

                        if( m_Users[portNumber].bVoice )
                            dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;

                        SetPlayerOnlineState( portNumber, dwState );

                        // Should really be checking for voice banning here with XOnlineIsUserVoiceAllowed.
                        g_XHVVoiceManager.RegisterLocalTalker( portNumber );
                        // 
                        g_XHVVoiceManager.SetProcessingMode( portNumber, XHV_VOICECHAT_MODE );
                        // We can only do this if nobody is voice banned
                        g_XHVVoiceManager.SetVoiceThroughSpeakers( portNumber, TRUE );
                    }

                    // Register all players on this box that logged on.
                    CTitlePlayer* pPlayer = new CTitlePlayer( m_pLogonUsers[portNumber].xuid, m_pLogonUsers[portNumber].szGamertag, portNumber );
                    g_Players.push_back(pPlayer);
                    pPlayers->RegisterPlayer(pPlayer);
                }
                else
                    m_Users[portNumber].strGamertag[0] = 0;
            }

            // Add some fake gamer tags
            const int NumFakeGamerTags = sizeof( g_FakeGamerTags ) / sizeof( g_FakeGamerTags[0] );
            for( DWORD i = 0; i < NumFakeGamerTags; ++i )
            {
                // Add some fake players to the list to flesh it out.
                XUID FakeXUID;
                FakeXUID.dwUserFlags = 0;
                FakeXUID.qwUserID = rand();
                CTitlePlayer* pPlayer = new CTitlePlayer( FakeXUID, g_FakeGamerTags[i], -1 );
                g_Players.push_back(pPlayer);
                pPlayers->RegisterPlayer(pPlayer);
            }

            // Remove some players from the list so that we have some departed players to display
            // If any of these players had guests then we have to remove the guests also.
            for( DWORD i = 0; i < NumFakeGamerTags / 4; ++i )
            {
                // Find the last player in the list.
                CTitlePlayer* pPlayer = g_Players[g_Players.size() - 1];
                // Unregister that player - they will now show up as departed.
                pPlayers->UnregisterPlayer( pPlayer );
                // Now we can delete the player record.
                delete pPlayer;
                // Remove the last entry from the vector.
                g_Players.resize( g_Players.size() - 1 );
            }

            StartScreen( SCREEN_MAINMENU );
            break;
        }

        case UIX_EXIT_LOGON_FAILED:
            // This exit code either means that logon failed or it means that
            // the users were logged off after a successful logon.
            // Because of this second use, this exit feature message can arrive
            // even when no feature is running.
            // Players can be logged off unexpectedly if they log on to a second
            // Xbox or if they lose their network connection.
            wsprintfW( m_strMessage, L"Authentication Failed with Error 0x%x", exitInfo.hr  );
            // Give specific information about known errors.
            switch ( exitInfo.hr )
            {
                case XONLINE_E_SILENT_LOGON_DISABLED:
                    wsprintfW( m_strMessage, L"Not signed in (auto-sign in disabled)" );
                    break;

                case XONLINE_E_SILENT_LOGON_NO_ACCOUNTS:
                    // If there are no Live accounts, display nothing.
                    m_strMessage[0] = 0;
                    break;

                case XONLINE_E_SILENT_LOGON_PASSCODE_REQUIRED:
                    wsprintfW( m_strMessage, L"Not signed in: passcode required" );
                    break;

                case XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON:
                    // UIX by default displays a popup in this case, so we don't need
                    // to display the error code, so I clear it.
                    m_strMessage[0] = 0;
                    break;
            }

            // We clear m_bLoggedOn because UIX has already logged us off
            // (if we were previously logged on) and we don't want to call
            // m_pLiveEngine->LogOff() and SetPlayerOnlineState().
            m_bLoggedOn = FALSE;
            Reset();
            break;

        case UIX_EXIT_LOGON_USER_EXIT:
            // This means the user backed out of the logon screen.
            StartScreen ( SCREEN_ERROR );
            m_bLoggingOn = FALSE;
            wsprintfW( m_strMessage, L"User backed out of logon" );
            break;




        case UIX_EXIT_PLAYERS_NORMAL_EXIT:
            m_bPlayersListOpen = FALSE;
            StartScreen( SCREEN_MAINMENU );
            break;

        default:
            assert(0);
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: LocalChatDataReady
// Desc: XHV Callback - called when a packet of voice data is ready to be
//          sent over the wire
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::LocalChatDataReady( DWORD dwPort, DWORD dwSize, PVOID pData )
{
    // Unimplemented in order to simplify this sample.
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CommunicatorStatusUpdate
// Desc: XHV Callback - called when the engine detects that the status of a
//          communicator has changed.  May not be called if a communicator
//          is quickly removed and re-inserted, but in that case there is
//          nothing the game has to do.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::CommunicatorStatusUpdate( DWORD dwPort, XHV_VOICE_COMMUNICATOR_STATUS status )
{
    // Mostly unimplemented in order to simplify this sample.
    if( status == XHV_VOICE_COMMUNICATOR_STATUS_INSERTED )
    {
        g_XHVVoiceManager.SetVoiceThroughSpeakers( dwPort, FALSE );
    }
    else if( status == XHV_VOICE_COMMUNICATOR_STATUS_REMOVED )
    {
        g_XHVVoiceManager.SetProcessingMode( dwPort, XHV_VOICECHAT_MODE );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: StartLogon
// Desc: Call this function to start the UIX logon feature.
//-----------------------------------------------------------------------------
VOID CXBoxSample::StartLogon()
{
    // Clear any old error messages.
    m_strMessage[0] = 0;

    // Declare and zero UIX_LOGON_PARAMS
    UIX_LOGON_PARAMS    LogonParams = {0};
    LogonParams.StructSize = sizeof( LogonParams );

    // Normal logon is typically used otherwise.
    LogonParams.LogonType = UIX_LOGON_TYPE_NORMAL;

    // Specify how many users should be allowed to logon. This can be
    // 1, 2, or 4.
    LogonParams.LogonUserCount = 4;

    // Specify one or more services, consecutively starting from index zero.
    LogonParams.LogonServiceIDs[0] = XONLINE_MATCHMAKING_SERVICE;
    LogonParams.LogonServiceIDs[1] = XONLINE_FEEDBACK_SERVICE;

    // Optionally specify logon state or a game invite to use for logon.

    // Start the process of logging on to Live
    HRESULT hr = m_pLiveEngine->StartFeature( UIX_LOGON_FEATURE, &LogonParams );

    if( FAILED( hr ) )
    {
        wsprintfW( m_strMessage, L"Logging On Failed with Error 0x%x", hr );
        StartScreen( SCREEN_ERROR );
    }
    else
    {
        m_bLoggingOn = TRUE;
        StartScreen( SCREEN_LOGON );
    }
}




//-----------------------------------------------------------------------------
// Name: StartPlayersList()
// Desc: Call this function to start the UIX Players List feature. The structure
// will be initialized appropriately based on the parameters you pass.
//-----------------------------------------------------------------------------
void CXBoxSample::StartPlayersList( BOOL LobbyMode, BOOL DisplayDeparted, BOOL DisplayActive, DWORD portNumber, int FilterFlags )
{
    // Specify which user invoked this feature.
    g_InvocationPort = portNumber;

    // Declare and zero UIX_LOGON_PARAMS
    UIX_PLAYERS_PARAMS    PlayersParams = {0};
    PlayersParams.StructSize = sizeof( PlayersParams );

    // The port of the controller used to control the player's screen
    // Set this to UIX_INVALID_VALUE if you want this value ignored.
    // It will also be ignored in UIX_PLAYERS_DISPLAY_LOBBY_MODE
    PlayersParams.UserPort = portNumber;

    // The player controlling port UserPort. They will be displayed as
    // the first player in the list, but won't be selectable (can be
    // highlighted, but nothing happens).
    PlayersParams.pPlayerControllingScreen = 0;
    if( portNumber != UIX_INVALID_VALUE )
    {
        // Find the record for the player that activated this feature.
        for( DWORD i = 0; i < g_Players.size(); ++i )
        {
            if( g_Players[i]->m_Port == portNumber )
                PlayersParams.pPlayerControllingScreen = g_Players[i];
        }
    }

    // The XUID of the player that should be selected the first time the screen is shown.
    PlayersParams.pSelectedPlayerXUID = 0;

    // Should the player list be sorted? If so, use the compare function in ITitlePlayersListItem
    PlayersParams.SortCurrentPlayersList = FALSE;

    // A flag that is masked against ITitlePlayersListItem::GetBitFlags() to filter which
    // players are shown. A value of zero implies that all players should be shown.
    PlayersParams.FilterFlags = FilterFlags;
    // Record the filter flags so we can do custom rendering based on them.
    m_FilterFlags = FilterFlags;

    // Can be UIX_PLAYERS_DISPLAY_CURRENT_PLAYERS, UIX_PLAYERS_DISPLAY_DEPARTED_PLAYERS,
    // or UIX_PLAYERS_DISPLAY_DEFAULT - which is
    // equal to (UIX_PLAYERS_DISPLAY_CURRENT_PLAYERS | UIX_PLAYERS_DISPLAY_DEPARTED_PLAYERS)
    // You can also optionally OR in UIX_PLAYERS_DISPLAY_LOBBY_MODE
    PlayersParams.DisplayType = 0;
    if( DisplayActive )
        PlayersParams.DisplayType |= UIX_PLAYERS_DISPLAY_CURRENT_PLAYERS;
    if( DisplayDeparted )
        PlayersParams.DisplayType |= UIX_PLAYERS_DISPLAY_DEPARTED_PLAYERS;
    assert( PlayersParams.DisplayType != 0 );
    if( LobbyMode )
        PlayersParams.DisplayType |= UIX_PLAYERS_DISPLAY_LOBBY_MODE;

    // Start the players feature.
    HRESULT hr = m_pLiveEngine->StartFeature( UIX_PLAYERS_FEATURE, &PlayersParams );
    if( SUCCEEDED( hr ) )
        m_bPlayersListOpen = TRUE;
}




//-----------------------------------------------------------------------------
// Name: ProcessInput()
// Desc: Processes user input, changing states and starting features as needed.
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessInput()
{
    switch ( m_WhichScreen )
    {
        case SCREEN_ERROR:
            if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
                StartLogon();
            break;

        case SCREEN_MAINMENU:
        {
            for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS ; ++i )
            {
                if( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
                {
                    // Start the lobby for the user who pressed the button.
                    StartScreen( SCREEN_LOBBY, i );
                    break;
                }

                if( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK] )
                {
                    // Start the guest version of the players list
                    StartScreen( SCREEN_GUEST_VIEW, i );
                    break;
                }

                // Only users that are logged on can logout, invoke the my-team list, invoke
                // the 'start-menu' players list and the 'departed players' players list.
                // For the lobby and guest-view it is harmless to allow any user to invoke them.
                if ( m_pLogonUsers[i].xuid.qwUserID && SUCCEEDED( m_pLogonUsers[i].hr ) )
                {
                    if( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
                    {
                        // Logoff.
                        Reset();
                        break;
                    }

                    if( !XOnlineIsUserGuest( m_pLogonUsers[i].xuid.dwUserFlags) )
                    {
                        // Start the players list in start-menu mode for the user who pressed the button.
                        if( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
                        {
                            StartScreen( SCREEN_START_MENU_PLAYERS_LIST, i );
                            break;
                        }

                        if( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
                        {
                            // Display the team of the user who pressed the button.
                            StartScreen( SCREEN_MYTEAM_ONLY, i );
                            break;
                        }

                        if( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
                        {
                            // Display departed players only.
                            StartScreen( SCREEN_DEPARTED_PLAYERS_ONLY, i );
                            break;
                        }
                    }
                }
            }
            break;
        }

        case SCREEN_LOBBY:
            // The UIX players feature is up and running at this point but we
            // still process a few button presses to implement our extra
            // lobby functions.
            for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS ; ++i )
            {
                if ( m_pLogonUsers[i].xuid.qwUserID && SUCCEEDED( m_pLogonUsers[i].hr ) )
                {
                    if( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
                    {
                        // End the feature and start the game. For the purposes of this
                        // sample we just end the feature.
                        m_pLiveEngine->EndFeature();
                        break;
                    }
                    if( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
                    {
                        PushAndStartScreen( SCREEN_OPTIONS_OVERLAY );
                    }
                }
            }
            break;

        case SCREEN_OPTIONS_OVERLAY:
            // The UIX players feature is up at this point but is dormant - we
            // are stealing all input in order to implement an options screen.
            for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS ; ++i )
            {
                if ( m_pLogonUsers[i].xuid.qwUserID && SUCCEEDED( m_pLogonUsers[i].hr ) )
                {
                    if( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
                    {
                        PopScreen();
                        // When we return from the options screen we need to call the UIX
                        // ClearInput() function to avoid double presses. Otherwise when
                        // we resume sending input to UIX it will see the B button still
                        // pressed down and will think it is a new press.
                        m_pLiveEngine->ClearInput();
                    }
                }
            }
            break;

        case SCREEN_MYTEAM_ONLY:
            for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS ; ++i )
            {
                if ( m_pLogonUsers[i].xuid.qwUserID && SUCCEEDED( m_pLogonUsers[i].hr ) )
                {
                    if( m_Gamepad[i].bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
                    {
                        // Cycle to the next set of filter flags.
                        switch( m_FilterFlags )
                        {
                        case BIT_FLAGS_BLUE_TEAM:
                            m_FilterFlags = BIT_FLAGS_RED_TEAM;
                            break;
                        case BIT_FLAGS_RED_TEAM:
                            m_FilterFlags = BIT_FLAGS_TEAMS;
                            break;
                        case BIT_FLAGS_TEAMS:
                            m_FilterFlags = BIT_FLAGS_BLUE_TEAM;
                            break;
                        }
                        // Get the ILivePlayersList interface
                        ILivePlayersList *pPlayers;
                        m_pLiveEngine->GetFeatureInterface(UIX_PLAYERS_FEATURE, NULL, (LPVOID *)&pPlayers);
                        // Update the flags that will be used to filter the players list.
                        pPlayers->SetFilterFlags( m_FilterFlags );

                        // For other changes, such as changes that would affect the sort order, you can
                        // call pPlayers->Refresh(). This forces a rebuild of the displayed list of players.
                    }
                }
            }
            break;

        default:
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: RenderScreenOptionsOverlay()
// Desc: Render the 'options screen' overtop of the existing screen. Since this
// is just a sample the options screen doesn't actually have any options.
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderScreenOptionsOverlay()
{
    // Set states
    D3DDevice::SetTexture( 0, NULL );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    D3DDevice::SetRenderState( D3DRS_ZENABLE,          FALSE ); 
    D3DDevice::SetRenderState( D3DRS_FILLMODE,         D3DFILL_SOLID ); 
    D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE ); 
    D3DDevice::SetRenderState( D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA );
    D3DDevice::SetRenderState( D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA );
    D3DDevice::SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE ); 
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW|D3DFVF_DIFFUSE );

    // Draw a background-filling quad, alpha blended so that the underlying screen
    // is still visible.
    D3DDISPLAYMODE mode;
    D3DDevice::GetDisplayMode( &mode );
    FLOAT fX1 = -0.5f;
    FLOAT fY1 = -0.5f;
    FLOAT fX2 = (FLOAT)mode.Width - 0.5f;
    FLOAT fY2 = (FLOAT)mode.Height - 0.5f;

    DWORD dwBackgroundColor = 0xB8000000;
    D3DDevice::Begin( D3DPT_QUADLIST );
    D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, dwBackgroundColor );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX1, fY1, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX2, fY1, 1.0f, 1.0f );
    D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, dwBackgroundColor );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX2, fY2, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX1, fY2, 1.0f, 1.0f );
    D3DDevice::End();

    // Now draw a smaller quad representing a popup screen.
    fX1 = 230 -0.5f;
    fY1 = 130 -0.5f;
    fX2 = 600 - 0.5f;
    fY2 = 380 - 0.5f;

    const DWORD dwPopupColor = 0xB8404080;
    D3DDevice::Begin( D3DPT_QUADLIST );
    D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, dwPopupColor );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX1, fY1, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX2, fY1, 1.0f, 1.0f );
    D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, dwPopupColor );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX2, fY2, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX1, fY2, 1.0f, 1.0f );
    D3DDevice::End();

    // And draw the options screen text.
    m_Font.Begin();
    m_Font.DrawText( 240, 140, 0xffffffff, L"Options screen.\nPress " GLYPH_B_BUTTON L" to go back", 0 );
    m_Font.End();
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background and clear the zbuffer
    RenderGradientBackground( 0xff404040, 0xff404080 );

    // Show title, menus, and UIX imagery
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );

    if( !IsUIXScreen() )
        m_Font.DrawText( 48, 46, 0xffffffff, L"UIXPlayers" );

    m_Font.SetScaleFactors( 1.0f, 1.0f );

    // Draw the menus.
    const WCHAR* text = L"";
    const WCHAR* text2 = L"";

    switch ( m_WhichScreen )
    {
    case SCREEN_MAINMENU:
        m_Font.DrawText( 320, 140, 0xffffffff,
                L"Press " GLYPH_A_BUTTON L" for the 'lobby'\n"
                L"Press " GLYPH_WHITE_BUTTON L" for your team only\n"
                L"Press " GLYPH_BLACK_BUTTON L" for the 'guest view'\n"
                L"Press " GLYPH_Y_BUTTON L" for the 'Start Menu' players list\n"
                L"Press " GLYPH_X_BUTTON L" for the departed players list\n"
                L"\n"
                L"Press " GLYPH_B_BUTTON L" to sign out",
                XBFONT_CENTER_X );
        break;

    case SCREEN_ERROR:
        m_Font.DrawText( 320, 140, 0xffffffff,
                L"Press " GLYPH_A_BUTTON L" to sign in",
                XBFONT_CENTER_X );
        break;
    }

    if( !IsUIXScreen() )
    {
        // Display any error or informative messages there may be.
        m_Font.DrawText( 320, 430, 0xffffffff, m_strMessage, XBFONT_CENTER_X );
    }

    m_Font.DrawText( 320, 140, 0xffffffff, text, XBFONT_CENTER_X );

    m_Font.DrawText( 320, 340, 0xffffffff, text2, XBFONT_CENTER_X );

    m_Font.End();


    // Rendering code needed for UIX

    if( m_dwLiveWorkFlags & UIX_DOWORK_NEED_TO_RENDER )
    {
        m_pLiveEngine->Render( m_pBackBuffer );
    }


    // Text that is drawn overtop of (after) the UIX screen
    m_Font.Begin();

    m_Font.SetScaleFactors( 1.2f, 1.2f );

    m_Font.DrawText( 40, 40, 0xffffffff, m_ScreenHeader, 0 );

    m_Font.SetScaleFactors( 1.0f, 1.0f );

    m_Font.DrawText( 40, 400, 0xffffffff, m_ScreenFooter, 0 );

    WCHAR* TeamsText = 0;
    if( m_FilterFlags == BIT_FLAGS_BLUE_TEAM )
        TeamsText = L"Blue Team";
    if( m_FilterFlags == BIT_FLAGS_RED_TEAM )
        TeamsText = L"Red Team";
    if( m_FilterFlags == BIT_FLAGS_TEAMS )
        TeamsText = L"Both Teams";
    // Optionally draw text describing which team is being displayed.
    // m_ScreenFooter shouldn't be set when using the filter, since
    // it is drawn in the same place.
    if( TeamsText )
        m_Font.DrawText( 40, 400, 0xffffffff, TeamsText, 0 );

    m_Font.End();


    // Draw the options screen over top if it is open.
    if( m_WhichScreen == SCREEN_OPTIONS_OVERLAY )
        RenderScreenOptionsOverlay();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
