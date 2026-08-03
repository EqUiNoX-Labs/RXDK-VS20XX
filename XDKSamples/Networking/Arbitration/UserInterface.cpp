//-----------------------------------------------------------------------------
// File: UserInterface.cpp
//
// Desc: Matchmaking rendering functions
//
// Hist: 12.01.03 - Copied from Matchmaking sample
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "UserInterface.h"
#include <cassert>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_1, L"Display\nhelp" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_1, L"Select menu\nitem" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_1, L"Cancel" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_1, L"Menu navigation" },
};

#define NUM_HELP_CALLOUTS 4




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const WCHAR*  strANY          = L"Any";

const WCHAR*  strSHORT        = L"Short";
const WCHAR*  strMEDIUM       = L"Medium";
const WCHAR*  strLONG         = L"Long";

const WCHAR*  strBEGINNER     = L"Beginner";
const WCHAR*  strINTERMEDIATE = L"Intermediate";
const WCHAR*  strADVANCED     = L"Advanced";

const WCHAR*  strHEAVY        = L"Heavy";
const WCHAR*  strLIGHT        = L"Light";
const WCHAR*  strMIXED        = L"Mixed";




//-----------------------------------------------------------------------------
// Name: UserInterface()
// Desc: Constructor
//-----------------------------------------------------------------------------
UserInterface::UserInterface()
{
    m_UI.SetHeader( L"Arbitration" );
}




//-----------------------------------------------------------------------------
// Name: SetErrorStr()
// Desc: Set error string
//-----------------------------------------------------------------------------
VOID __cdecl UserInterface::SetErrorStr( const WCHAR* strFormat, ... )
{
    va_list pArgList;
    va_start( pArgList, strFormat );

    m_UI.SetErrorStr( strFormat, pArgList );

    va_end( pArgList );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects
//-----------------------------------------------------------------------------
HRESULT UserInterface::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return E_FAIL;

    // Create the help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return E_FAIL;

    return m_UI.Initialize();
}




//-----------------------------------------------------------------------------
// Name: RenderSelectMatch()
// Desc: Display matchmaking menu
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectMatch( DWORD dwCurrItem )
{
    m_UI.RenderHeader();

    m_Font.DrawText( 320, 140, COLOR_NORMAL, L"Find Game Session",
                     XBFONT_CENTER_X );

    const WCHAR* const strMatch[] =
    {
        L"QuickMatch",
        L"OptiMatch"
    };

    const FLOAT fYtop = 200.0f;
    const FLOAT fYdelta = 50.0f;

    for( DWORD i = 0; i < MATCH_MAX; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 260, fYtop + (fYdelta * i), dwColor, strMatch[i] );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 260, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderOptiMatch()
// Desc: Display OptiMatch screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderOptiMatch( SessionInfo& session,
                                     DWORD dwCurrItem )
{
    m_UI.RenderHeader();

    m_Font.DrawText( 320, 140, COLOR_NORMAL, L"Session Settings",
                     XBFONT_CENTER_X );

    const WCHAR* const strSetting[] =
    {
        L"Set Game Type",
        L"Set Player Level",
        L"Set Game Style",
        L"Set Session Name",
        L"Find Matching Session"
    };

    FLOAT fYtop = 200.0f;
    const FLOAT fYdelta = 40.0f;

    for( DWORD i = 0; i < CUSTOM_MAX; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : 
                                                COLOR_NORMAL;
        m_Font.DrawText( 160, fYtop + (fYdelta * i), dwColor, strSetting[i] );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 160, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );

    // Determine current settings as strings
    const WCHAR* strCurrent[ CUSTOM_MAX ];
    switch( session.GetGameType() )
    {
        case TYPE_ANY:    strCurrent[ CUSTOM_TYPE ] = strANY;    break;
        case TYPE_SHORT:  strCurrent[ CUSTOM_TYPE ] = strSHORT;  break;
        case TYPE_MEDIUM: strCurrent[ CUSTOM_TYPE ] = strMEDIUM; break;
        case TYPE_LONG:   strCurrent[ CUSTOM_TYPE ] = strLONG;   break;
        default:          assert( FALSE );                       break;
    }

  
    switch( session.GetGameStyle() )
    {
    case STYLE_ANY   : strCurrent[ CUSTOM_STYLE ] = strANY;   break;
    case STYLE_HEAVY : strCurrent[ CUSTOM_STYLE ] = strHEAVY; break;
    case STYLE_LIGHT : strCurrent[ CUSTOM_STYLE ] = strLIGHT; break;
    case STYLE_MIXED : strCurrent[ CUSTOM_STYLE ] = strMIXED; break;
    default:          assert( FALSE );                        break;
    }

    switch( session.GetPlayerLevel() )
    {
    case LEVEL_ANY          : strCurrent[ CUSTOM_LEVEL ] = strANY;          break;
    case LEVEL_BEGINNER     : strCurrent[ CUSTOM_LEVEL ] = strBEGINNER;     break;
    case LEVEL_INTERMEDIATE : strCurrent[ CUSTOM_LEVEL ] = strINTERMEDIATE; break;
    case LEVEL_ADVANCED     : strCurrent[ CUSTOM_LEVEL ] = strADVANCED;     break;
    default:                  assert( FALSE );                              break;
    }

    strCurrent[ CUSTOM_NAME ] = session.GetSessionName();
    strCurrent[ CUSTOM_FIND ] = L"";

    // Show current settings
    fYtop = 200.0f;
    for( DWORD i = 0; i < CUSTOM_MAX; ++i )
        m_Font.DrawText( 380, fYtop + (fYdelta * i), COLOR_GREEN, strCurrent[i] );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectType()
// Desc: Display game type screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectType( DWORD dwCurrItem )
{
    m_UI.RenderHeader();

    m_Font.DrawText( 320, 140, COLOR_NORMAL, L"Game Type",XBFONT_CENTER_X );

    const WCHAR* const strType[] =
    {
        strANY,
        strSHORT,
        strMEDIUM,
        strLONG
    };

    const FLOAT fYtop = 200.0f;
    const FLOAT fYdelta = 50.0f;

    for( DWORD i = 0; i < TYPE_MAX; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : 
                                                COLOR_NORMAL;
        m_Font.DrawText( 260, fYtop + (fYdelta * i), dwColor, strType[i] );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 260, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectLevel()
// Desc: Display player rating screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectLevel( DWORD dwCurrItem )
{
    m_UI.RenderHeader();

    m_Font.DrawText( 320, 140, COLOR_NORMAL, L"Player Level", XBFONT_CENTER_X );

    const WCHAR* const strLevel[] =
    {
        strANY,
        strBEGINNER,
        strINTERMEDIATE,
        strADVANCED
    };

    const FLOAT fYtop = 200.0f;
    const FLOAT fYdelta = 50.0f;

    for( DWORD i = 0; i < LEVEL_MAX; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : 
                                                COLOR_NORMAL;
        m_Font.DrawText( 260, fYtop + (fYdelta * i), dwColor, strLevel[i] );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 260, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectStyle()
// Desc: Display game style screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectStyle( DWORD dwCurrItem )
{
    m_UI.RenderHeader();

    m_Font.DrawText( 320, 140, COLOR_NORMAL, L"Game Style", XBFONT_CENTER_X );

    const WCHAR* const strStyle[] =
    {
        strANY,
        strHEAVY,
        strLIGHT,
        strMIXED
    };

    const FLOAT fYtop = 200.0f;
    const FLOAT fYdelta = 50.0f;

    for( DWORD i = 0; i < STYLE_MAX; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : 
                                                COLOR_NORMAL;
        m_Font.DrawText( 260, fYtop + (fYdelta * i), dwColor, strStyle[i] );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 260, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectName()
// Desc: Display game name screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectName( DWORD dwCurrItem, 
                                      const SessionNameList& SessionNames )
{
    assert( dwCurrItem < SessionNames.size() );

    m_UI.RenderHeader();
    m_Font.DrawText( 320, 100, COLOR_NORMAL, L"Session Name", XBFONT_CENTER_X );

    const FLOAT fYtop = 160.0f;
    const FLOAT fYdelta = 40.0f;

    for( DWORD i = 0; i < SessionNames.size(); ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 260, fYtop + (fYdelta * i), dwColor, SessionNames[i].c_str() );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 260, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectSession()
// Desc: Display session name screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectSession( DWORD dwCurrItem, 
                                         SessionList& Sessions )
{
    assert( dwCurrItem < Sessions.size() );

    m_UI.RenderHeader();
    m_Font.DrawText( 320, 120, COLOR_NORMAL, L"Sessions", XBFONT_CENTER_X );

    const FLOAT fYtop      = 200.0f;
    const FLOAT fYdelta    =  40.0f;
    const FLOAT SESSION_POSITION =  70.0f;
    const FLOAT LEVEL_POSITION   = 210.0f;
    const FLOAT STYLE_POSITION   = 340.0f;
    const FLOAT TYPE_POSITION    = 420.0f;
    const FLOAT PLAYER_POSITION  = 480.0f;

    m_Font.DrawText( SESSION_POSITION, 160, COLOR_NORMAL,  L"Session", 0 );
    m_Font.DrawText( LEVEL_POSITION,   160, COLOR_NORMAL,  L"Level", 0 );
    m_Font.DrawText( STYLE_POSITION,   160, COLOR_NORMAL,  L"Style", 0 );
    m_Font.DrawText( TYPE_POSITION,    160, COLOR_NORMAL,  L"Type", 0 );
    m_Font.DrawText( PLAYER_POSITION,  160, COLOR_NORMAL,  L"Open", 0 );

    for( DWORD i = 0; i < Sessions.size(); ++i )
    {
        FLOAT fYPosition = fYtop + (fYdelta * i);
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;

        ULONGLONG qwGameType = Sessions[i].GetGameType();
        ULONGLONG qwGameStyle = Sessions[i].GetGameStyle();
        ULONGLONG qwPlayerLevel = Sessions[i].GetPlayerLevel();
        const WCHAR* strType = L"";
        const WCHAR* strStyle = L"";
        const WCHAR* strLevel = L"";

        switch( qwGameType )
        {
            case TYPE_SHORT:
                strType = L"Short";
                break;
            case TYPE_MEDIUM:
                strType = L"Med";
                break;
            case TYPE_LONG:
                strType = L"Long";
                break;
            default:
                assert(0);
        }

        switch( qwGameStyle )
        {
        case STYLE_ANY   : strStyle = strANY;   break;
        case STYLE_HEAVY : strStyle = strHEAVY; break;
        case STYLE_LIGHT : strStyle = strLIGHT; break;
        case STYLE_MIXED : strStyle = strMIXED; break;
        default:          assert( FALSE );      break;
        }

        switch( qwPlayerLevel )
        {
        case LEVEL_ANY          : strLevel = strANY;          break;
        case LEVEL_BEGINNER     : strLevel = strBEGINNER;     break;
        case LEVEL_INTERMEDIATE : strLevel = strINTERMEDIATE; break;
        case LEVEL_ADVANCED     : strLevel = strADVANCED;     break;
        default:                  assert( FALSE );            break;
        }

        m_Font.DrawText( SESSION_POSITION, fYPosition, dwColor, 
                         Sessions[i].GetSessionName() );
        m_Font.DrawText( LEVEL_POSITION,   fYPosition, dwColor, 
                         strLevel );

        m_Font.DrawText( STYLE_POSITION,   fYPosition, dwColor, strStyle ); 
        m_Font.DrawText( TYPE_POSITION,   fYPosition, dwColor, strType ); 

        WCHAR strPlayers[ 32 ];
        wsprintfW( strPlayers, L"%lu", Sessions[i].GetPublicAvail() );
        m_Font.DrawText( PLAYER_POSITION + 20.0f,   fYPosition, dwColor, 
                         strPlayers );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 80, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderCreateAccount()
// Desc: Allow player to launch account creation tool
//-----------------------------------------------------------------------------
VOID UserInterface::RenderCreateAccount( BOOL bHasMachineAccount )
{
    m_UI.RenderCreateAccount( bHasMachineAccount );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectAccount()
// Desc: Display list of accounts
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectAccount( DWORD dwCurrItem, 
                                         XBUserList& UserList,
                                         XUID & xuidAcceptedUser)
{
    assert( !UserList.empty() );

    m_UI.RenderHeader();

    m_Font.DrawText( 320, 140, COLOR_NORMAL, L"Select an account",
                     XBFONT_CENTER_X );

    FLOAT fYtop = 220.0f;
    FLOAT fYdelta = 30.0f;

    // Show list of user accounts
    for( DWORD i = 0; i < UserList.size(); ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;

        // Convert user name to WCHAR string
        WCHAR strUserName[ XONLINE_GAMERTAG_SIZE + 32 ];
        XBUtil_GetWide( UserList[i].szGamertag, strUserName, XONLINE_GAMERTAG_SIZE );

        if( XOnlineAreUsersIdentical( &xuidAcceptedUser, &UserList[i].xuid ) )
            wcscat( strUserName, L" (accepted invitation) ");
        m_Font.DrawText( 160, fYtop + (fYdelta * i), dwColor, strUserName );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 160, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderLogginOn()
// Desc: Display login message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderLoggingOn()
{
    m_UI.RenderLoggingOn();
}




//-----------------------------------------------------------------------------
// Name: RenderGameSearch()
// Desc: Display game search screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderGameSearch( BOOL bPreviouslyAccepted )
{
    m_UI.RenderHeader();
    if( bPreviouslyAccepted )
        m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Finding Session", 
                         XBFONT_CENTER_X );
    else
        m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Searching for Game Sessions", 
                         XBFONT_CENTER_X );

    m_Font.DrawText( 320, 260, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel", 
                     XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderRequestJoin()
// Desc: Display game join screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderRequestJoin()
{
    m_UI.RenderHeader();
    m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Joining Game", 
                     XBFONT_CENTER_X );
    m_Font.DrawText( 320, 260, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel", 
                     XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderCreateSession()
// Desc: Display game create screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderCreateSession()
{
    m_UI.RenderHeader();
    m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Registering Game Session", 
                     XBFONT_CENTER_X );
    m_Font.DrawText( 320, 260, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel", 
                     XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderPlayGame()
// Desc: Display game
//-----------------------------------------------------------------------------
VOID UserInterface::RenderPlayGame( SessionInfo& session, WCHAR* strUser,
                                    WCHAR* strStatus, DWORD dwPlayerCount,
                                    DWORD dwCurrItem, BOOL bInvitedToPlay,
                                    BOOL bIsHost )
{
    m_UI.RenderHeader();

    // Game name and player name
    WCHAR strGameInfo[ 32 + XATTRIB_SESSION_NAME_MAX_LEN + MAX_PLAYER_STR ];

    if( bInvitedToPlay )
        wsprintfW( strGameInfo, L"Your name: %.*s", MAX_PLAYER_STR, strUser );
    else
        wsprintfW( strGameInfo, L"Session name: %.*s\nYour name: %.*s", 
                   XATTRIB_SESSION_NAME_MAX_LEN, session.GetSessionName(), 
                   MAX_PLAYER_STR, strUser );

    m_Font.DrawText( 320, 100, COLOR_GREEN, strGameInfo, XBFONT_CENTER_X );
    
    if( !bInvitedToPlay )
    {
        // Determine current game type as string
        const WCHAR* strType = L"";
        switch( session.GetGameType() )
        {
            case TYPE_SHORT:  strType = strSHORT;  break;
            case TYPE_MEDIUM: strType = strMEDIUM; break;
            case TYPE_LONG:   strType = strLONG;   break;
            default:          assert( FALSE );     break;
        }
        
        // Determine current game style as string
        const WCHAR* strStyle = L"";
        switch( session.GetGameStyle() )
        {
        case STYLE_ANY   : strStyle = strANY;    break;
        case STYLE_HEAVY : strStyle = strHEAVY;  break;
        case STYLE_LIGHT : strStyle = strLIGHT;  break;
        case STYLE_MIXED : strStyle = strMIXED;  break;
        default:          assert( FALSE );       break;
        }

        const WCHAR* strLevel = L"";
        switch( session.GetPlayerLevel() )
        {
        case LEVEL_ANY          : strLevel = strANY;          break;
        case LEVEL_BEGINNER     : strLevel = strBEGINNER;     break;
        case LEVEL_INTERMEDIATE : strLevel = strINTERMEDIATE; break;
        case LEVEL_ADVANCED     : strLevel = strADVANCED;     break;
        default:                  assert( FALSE );            break;
        }      
        // Game info
        wsprintfW( strGameInfo, L"Type: %.*s, Level: %.*s, Style: %.*s",
            MAX_TYPE_STR, strType,
            MAX_LEVEL_STR, strLevel,
            MAX_STYLE_STR, strStyle );
        m_Font.DrawText( 320, 150, COLOR_GREEN, strGameInfo, XBFONT_CENTER_X );
    }

    // Number of players and current status
    wsprintfW( strGameInfo, L"Players in game: %lu", dwPlayerCount );
    m_Font.DrawText( 320, 180, COLOR_GREEN, strGameInfo, XBFONT_CENTER_X );
    m_Font.DrawText( 320, 220, COLOR_GREEN, strStatus, XBFONT_CENTER_X );

    // Game options menu
    const WCHAR* const strMenu[] =
    {
        L"Wave To Other Players",
        L"Leave Game",
        L"Invite Friends",
        L"Start Arbitrated Game",
    };

    FLOAT fYtop = 270.0f;
    FLOAT fYdelta = 35.0f;

    // Show menu
    DWORD menuCount = GAME_NON_HOST_MAX;
    if( bIsHost )
        menuCount = GAME_MAX;
    for( DWORD i = 0; i < menuCount; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 260, fYtop + (fYdelta * i), dwColor, strMenu[i] );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 260, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderWaitingForRegistration()
// Desc: Display a screen while waiting for the user to register.
//-----------------------------------------------------------------------------
VOID UserInterface::RenderWaitingForRegistration( SessionInfo& session, DWORD dwCurrItem )
{
    m_UI.RenderHeader();

    FLOAT fYtop = 280.0f;

    WCHAR buffer[1000];
    // Should print the remaining time that the game will wait - maximum of about
    // ten seconds.
    swprintf( buffer, L"Waiting for players to register" );
    m_Font.DrawText( 260, fYtop, COLOR_NORMAL, buffer );
}




//-----------------------------------------------------------------------------
// Name: RenderArbitratedGame()
// Desc: Display 'game' that has been created to use arbitrated submission of
// results.
//-----------------------------------------------------------------------------
VOID UserInterface::RenderArbitratedGame( SessionInfo& session, DWORD dwCurrItem, const DWORD* pScores,
                                            const WCHAR* const playerNames[], DWORD playerCount )
{
    m_UI.RenderHeader();

    FLOAT fyPlayerTop = 100;
    FLOAT fYdelta = 40.0f;

    for( DWORD i = 0; i < playerCount; ++i )
    {
        WCHAR buffer[1000];
        swprintf( buffer, L"%s - %d points", playerNames[i], pScores[i] );
        m_Font.DrawText( 260, fyPlayerTop + (fYdelta * i), COLOR_NORMAL, buffer );
    }

    // Game options menu
    const WCHAR* const strMenu[] =
    {
        L"Score Point",
        L"Score Point (not sent)",
        L"End Game",
    };

    FLOAT fYtop = 280.0f;

    // Show menu
    for( DWORD i = 0; i < ARBITRATED_MAX; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 260, fYtop + (fYdelta * i), dwColor, strMenu[i] );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 260, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderDeleteSession()
// Desc: Display game deletion screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderDeleteSession()
{
    m_UI.RenderHeader();
    m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Unregistering Game Session", 
                     XBFONT_CENTER_X );
    m_Font.DrawText( 320, 260, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel", 
                     XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderFinishFriendEnum()
// Desc: Display friend enumeration finishing message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderFinishFriendEnum()
{
    m_UI.RenderHeader();
    m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Finishing friend enumeration", 
                     XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderError()
// Desc: Display error message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderError()
{
    m_UI.RenderError();
}




//-----------------------------------------------------------------------------
// Name: RenderHelp()
// Desc: Display help
//-----------------------------------------------------------------------------
VOID UserInterface::RenderHelp() 
{
    m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
}

