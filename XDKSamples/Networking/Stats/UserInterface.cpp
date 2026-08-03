//-----------------------------------------------------------------------------
// File: UserInterface.cpp
//
// Desc: Stats rendering functions
//
// Hist: 04.10.02 - New for May release
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




// Level names
static const WCHAR* const g_strLevels[NUM_LEVELS] =
{
    L"Sabotage",
    L"The Circle of Death",
    L"Gun Island",
    L"Curse of Siva"
};


//-----------------------------------------------------------------------------
// Name: UserInterface()
// Desc: Constructor
//-----------------------------------------------------------------------------
UserInterface::UserInterface()
{
    m_UI.SetHeader( L"Stats" );
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
// Name: RenderCreateAccount()
// Desc: Allow player to launch account creation tool
//-----------------------------------------------------------------------------
VOID UserInterface::RenderCreateAccount( BOOL bHasMachineAccount )
{
    m_UI.RenderCreateAccount( bHasMachineAccount );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderMainMenu()
// Desc: Render the main menu options
//-----------------------------------------------------------------------------
VOID UserInterface::RenderMainMenu( DWORD dwCurrItem, WCHAR *strUser )
{
    m_UI.RenderHeader();
    
    // Note: The strings here must match
    // the action enumeration in common.h
    const WCHAR* const strMenu[ACTION_MAX] =
    {
        L"New Statistics",
        L"Reset Statistics for %.*s",
        L"View Statistics for %.*s",
        L"View Overall Statistics",
        L"View Friends Statistics"
    };
    
    FLOAT fYtop = 140.0f;
    FLOAT fYdelta = 30.0f;
    
    WCHAR strItem[ 32 + XONLINE_GAMERTAG_SIZE ];

    for( DWORD i = 0; i < ACTION_MAX; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        wsprintfW( strItem, strMenu[i], XONLINE_GAMERTAG_SIZE, strUser );
        m_Font.DrawText( 220, fYtop + (fYdelta * i), dwColor, strItem );
    }
  
    // Show selected item with little triangle
    m_Font.DrawText( 220, fYtop + (fYdelta * dwCurrItem), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectAccount()
// Desc: Display list of accounts
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectAccount( DWORD dwCurrItem, 
                                         const XBUserList& UserList )
{
    m_UI.RenderSelectAccount( dwCurrItem, UserList );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectLevel()
// Desc: Display list of game levels
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectLevel( DWORD dwCurrItem ) 
{
    assert( dwCurrItem < NUM_LEVELS );

    m_UI.RenderHeader();
    
    
    FLOAT fYtop = 140.0f;
    FLOAT fYdelta = 30.0f;
    
    m_Font.DrawText( 320, 80, COLOR_NORMAL, L"Select Game Level", XBFONT_CENTER_X );

    for( DWORD i = 0; i < NUM_LEVELS; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 220, fYtop + (fYdelta * i), dwColor, g_strLevels[ i ] );
    }
  
    // Show selected item with little triangle
    m_Font.DrawText( 220, fYtop + (fYdelta * dwCurrItem), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}



//-----------------------------------------------------------------------------
// Name: RenderEndGame()
// Desc: Displayed an end of game list of players and stats
//-----------------------------------------------------------------------------
VOID UserInterface::RenderEndGame( DWORD dwLevel, 
                                   PlayerList & Players ) 
{
    assert( !Players.empty() );
    assert( dwLevel < NUM_LEVELS );

    m_UI.RenderHeader();
    
    
    FLOAT fYtop = 170.0f;
    FLOAT fYdelta = 30.0f;
    
    m_Font.DrawText( 320, 80, COLOR_NORMAL, g_strLevels[dwLevel], XBFONT_CENTER_X );

    m_Font.DrawText( 100, 140, COLOR_NORMAL, L"Player" );
    m_Font.DrawText( 300, 140, COLOR_NORMAL, L"Kills" );
    m_Font.DrawText( 350, 140, COLOR_NORMAL, L"Deaths" );
    m_Font.DrawText( 425, 140, COLOR_NORMAL, L"Assists" );

    
    for( DWORD i = 0; i < Players.size(); ++i )
    {
        WCHAR strText[32];
        lstrcpynW( strText, Players[i].strUserName, 12 );

        m_Font.DrawText( 100, fYtop + (fYdelta * i), COLOR_NORMAL, strText );
        wsprintfW( strText, L"%lu", Players[i].Stats.GetKills() );
        m_Font.DrawText( 315, fYtop + (fYdelta * i), COLOR_NORMAL, strText,
                         XBFONT_CENTER_X );
        wsprintfW( strText, L"%lu", Players[i].Stats.GetDeaths() );
        m_Font.DrawText( 380, fYtop + (fYdelta * i), COLOR_NORMAL, strText,
                         XBFONT_CENTER_X );
        wsprintfW( strText, L"%lu", Players[i].Stats.GetAssists() );
        m_Font.DrawText( 455, fYtop + (fYdelta * i), COLOR_NORMAL, strText,
                         XBFONT_CENTER_X );
        if( !Players[i].Stats.GetCompleted() )
            m_Font.DrawText( 500, fYtop + (fYdelta * i), COLOR_NORMAL,
                              L"(quit)" );
    }


    // Button descriptions
    m_Font.DrawText( 100, 350, COLOR_NORMAL, GLYPH_A_BUTTON L"Submit" );
    m_Font.DrawText( 300, 350, COLOR_NORMAL, GLYPH_B_BUTTON L"Back" );
    m_Font.DrawText( 400, 350, COLOR_NORMAL, GLYPH_Y_BUTTON L"Regenerate" );
    m_Font.DrawText( 400, 380, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderLoggingOn()
// Desc: Display "logging on" animation
//-----------------------------------------------------------------------------
VOID UserInterface::RenderLoggingOn( const XONLINE_USER *pUserList )
{
    m_UI.RenderLoggingOn( pUserList );
}




//-----------------------------------------------------------------------------
// Name: RenderFriendEnum()
// Desc: Display friend enumeration message 
//-----------------------------------------------------------------------------
VOID UserInterface::RenderFriendEnum()
{
    m_UI.RenderHeader();
    m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Enumerating Friends", 
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
// Name: RenderReadStats()
// Desc: Display stats retrieval message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderReadStats()
{
    m_UI.RenderHeader();
    m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Reading Statistics", 
                     XBFONT_CENTER_X );
    m_Font.DrawText( 320, 260, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel", 
                     XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderWriteStats()
// Desc: Display stats retrieval message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderWriteStats()
{
    m_UI.RenderHeader();
    m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Writing Statistics", 
                     XBFONT_CENTER_X );
    m_Font.DrawText( 320, 260, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel", 
                     XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderLeaderboard()
// Desc: Display a leaderboard
//-----------------------------------------------------------------------------
VOID UserInterface::RenderLeaderboard( const XUID * pxuidPivot, 
                                       DWORD dwLeaderboardID, 
                                       BOOL  bShowRating,
                                       const PlayerList & Players )
{
    m_UI.RenderHeader();
    FLOAT fYtop = 170.0f;
    FLOAT fYdelta = 30.0f;
    
    if( dwLeaderboardID == OVERALL_LEADERBOARD_ID )
        m_Font.DrawText( 320, 80, COLOR_NORMAL, L"Overall", XBFONT_CENTER_X );
    else
    {
        DWORD dwLevel = LeaderBoardIDToLevel( dwLeaderboardID );
        m_Font.DrawText( 320, 80, COLOR_NORMAL, g_strLevels[dwLevel], XBFONT_CENTER_X );
    }

    if( bShowRating )
    {
        m_Font.DrawText( 100, 140, COLOR_NORMAL, L"Player" );
        m_Font.DrawText( 375, 140, COLOR_NORMAL, L"Rating" );

        for( DWORD i = 0; i < Players.size(); ++i )
        {
            DWORD dwColor;
            
            if ( pxuidPivot && pxuidPivot->qwUserID == Players[i].xuid.qwUserID )
                dwColor = COLOR_HIGHLIGHT;
            else
                dwColor = COLOR_NORMAL;
            
            WCHAR strText[32];
            wsprintfW( strText, L"%lu.", Players[i].Stats.GetRank() ); 
            m_Font.DrawText( 70, fYtop + (fYdelta * i), dwColor, strText, 
                             XBFONT_CENTER_X );
            
            lstrcpynW( strText, Players[i].strUserName, 12 );
            m_Font.DrawText( 100, fYtop + (fYdelta * i), dwColor, strText );
            
            wsprintfW( strText, L"%I64d", Players[i].Stats.GetRating() );
            m_Font.DrawText( 405, fYtop + (fYdelta * i), dwColor, strText,
                             XBFONT_CENTER_X );
        }
    }
    else
    {
        m_Font.DrawText( 100, 140, COLOR_NORMAL, L"Player" );
        m_Font.DrawText( 250, 140, COLOR_NORMAL, L"Kills" ); 
        m_Font.DrawText( 300, 140, COLOR_NORMAL, L"Deaths" );
        m_Font.DrawText( 375, 140, COLOR_NORMAL, L"Assists" );
        m_Font.DrawText( 450, 110, COLOR_NORMAL, L"Completed /" );
        m_Font.DrawText( 450, 140, COLOR_NORMAL, L"Started" );
        
        
        for( DWORD i = 0; i < Players.size(); ++i )
        {
            DWORD dwColor;
            
            if ( pxuidPivot && pxuidPivot->qwUserID == Players[i].xuid.qwUserID )
                dwColor = COLOR_HIGHLIGHT;
            else
                dwColor = COLOR_NORMAL;
            
            WCHAR strText[32];
            wsprintfW( strText, L"%lu.", Players[i].Stats.GetRank() ); 
            
            m_Font.DrawText( 70, fYtop + (fYdelta * i), dwColor, strText, 
                             XBFONT_CENTER_X );
            
            lstrcpynW( strText, Players[i].strUserName, 12 );
            m_Font.DrawText( 100, fYtop + (fYdelta * i), dwColor, strText );
            wsprintfW( strText, L"%lu", Players[i].Stats.GetKills() );
            m_Font.DrawText( 265, fYtop + (fYdelta * i), dwColor, strText,
                             XBFONT_CENTER_X );
            wsprintfW( strText, L"%lu", Players[i].Stats.GetDeaths() );
            m_Font.DrawText( 330, fYtop + (fYdelta * i), dwColor, strText,
                             XBFONT_CENTER_X );
            wsprintfW( strText, L"%lu", Players[i].Stats.GetAssists() );
            m_Font.DrawText( 405, fYtop + (fYdelta * i), dwColor, strText,
                             XBFONT_CENTER_X );
            wsprintfW( strText, L"%lu / %lu", Players[i].Stats.GetCompleted(),
                                Players[i].Stats.GetStarted() );
            m_Font.DrawText( 500, fYtop + (fYdelta * i), dwColor, strText,
                             XBFONT_CENTER_X );
        }
    }

    // Button descriptions
    m_Font.DrawText( 100, 380, COLOR_NORMAL, GLYPH_B_BUTTON L"Back" );
    m_Font.DrawText( 190, 380, COLOR_NORMAL, GLYPH_BLACK_BUTTON L"Toggle Rating" );
    m_Font.DrawText( 400, 380, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );    
}




//-----------------------------------------------------------------------------
// Name: RenderResetStats()
// Desc: Display stat reset message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderResetStats( WCHAR * strUserName )
{
    m_UI.RenderHeader();
    m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Resetting Stats for", 
                     XBFONT_CENTER_X );
    m_Font.DrawText( 320, 230, COLOR_NORMAL, strUserName, 
                     XBFONT_CENTER_X );
    m_Font.DrawText( 320, 290, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel", 
                     XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderError()
// Desc: Display error message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderError( BOOL bBootToDash )
{
    m_UI.RenderError( bBootToDash );
}




//-----------------------------------------------------------------------------
// Name: RenderHelp()
// Desc: Display help
//-----------------------------------------------------------------------------
VOID UserInterface::RenderHelp() 
{
    m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
}



