//-----------------------------------------------------------------------------
// File: UserInterface.cpp
//
// Desc: Friends rendering functions
//
// Hist: 10.20.01 - Updated for Nov release
//       01.21.02 - Updated for Feb release
//       02.15.02 - Updated for Mar release
//       03.11.02 - Update  for April release
//       04.24.02 - Update  for May release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "UserInterface.h"
#include "FriendsManager.h"
#include <cassert>




//-----------------------------------------------------------------------------
// Name: UserInterface()
// Desc: Constructor
//-----------------------------------------------------------------------------
UserInterface::UserInterface()
{
    m_dwUserIndex = 0;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects
//-----------------------------------------------------------------------------
HRESULT UserInterface::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
    {
        OUTPUT_DEBUG_STRING( "Failed to load fonts\n" );
        return E_FAIL;
    }

    // Create the online icons font
    if( FAILED( m_OnlineIconsFont.Create( "OnlineIconsFont.xpr" ) ) )
    {
        OUTPUT_DEBUG_STRING( "Failed to load online icons fonts\n" );
        return E_FAIL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderCreateAccount()
// Desc: Allow player to launch account creation tool
//-----------------------------------------------------------------------------
VOID UserInterface::RenderCreateAccount( BOOL bHasMachineAccount )
{
    WCHAR* strInfo;
    if( bHasMachineAccount )
    {
        strInfo = L"No online accounts exist on this Xbox.\n\n"
                  L"Run the XDK Launcher or Xbox OnlineDash\nto create accounts.\n\n"
                  L"Press " GLYPH_A_BUTTON L" to continue.";
    }
    else
    {
        strInfo = L"This Xbox does not have a machine account.\n\n"
                  L"Run the XDK Launcher or Xbox OnlineDash\nto create accounts.\n\n"
                  L"Press " GLYPH_A_BUTTON L" to continue.";
    }

    m_Font.DrawText( 320, 140, COLOR_NORMAL, strInfo, XBFONT_CENTER_X );

    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectAccount()
// Desc: Display list of accounts
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectAccount( DWORD dwTopItem, DWORD dwCurrItem, 
                                         const XONLINE_USER* UserList, DWORD dwNumUsers )
{
    DWORD dwBottomItem;
    assert( dwNumUsers );

    m_Font.DrawText( 320, 100, COLOR_NORMAL, L"Select an account",
                     XBFONT_CENTER_X );

    FLOAT fYtop   = 160.0f;
    FLOAT fYdelta =  30.0f;

    // Show list of user accounts

    dwBottomItem = dwTopItem + MAX_ACCOUNTS_DISPLAYED;
    if (dwBottomItem > dwNumUsers) dwBottomItem = dwNumUsers;

    for( DWORD i = dwTopItem; i < dwBottomItem; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;

        // Convert user name to WCHAR string
        WCHAR strUser[ XONLINE_GAMERTAG_SIZE ];
        swprintf( strUser, L"%S", UserList[i].szGamertag );

        m_Font.DrawText( 160, fYtop + (fYdelta * (i - dwTopItem) ), dwColor, strUser );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 120.0f, fYtop + (fYdelta * ( dwCurrItem - dwTopItem ) ), 0xff00ff00, GLYPH_RIGHT_TICK );

    BOOL bShowTopArrow = dwTopItem > 0;
    BOOL bShowBtmArrow = ( dwTopItem + MAX_ACCOUNTS_DISPLAYED <  dwNumUsers);
    if( bShowTopArrow )
        m_Font.DrawText( 115, 140, COLOR_GREEN, GLYPH_UP_TICK );
    if( bShowBtmArrow )
        m_Font.DrawText( 115, fYtop + MAX_ACCOUNTS_DISPLAYED * fYdelta, COLOR_GREEN, GLYPH_DOWN_TICK );

    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );

}




//-----------------------------------------------------------------------------
// Name: RenderLogginOn()
// Desc: Display login message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderLoggingOn()
{
    m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Authenticating Xbox Account",
                        XBFONT_CENTER_X );
    m_Font.DrawText( 320, 260, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel",
                        XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderFriendList()
// Desc: Display current list of friends
//-----------------------------------------------------------------------------
VOID UserInterface::RenderFriendList( DWORD dwTopItem, DWORD dwCurrItem,                                   
                                      const WCHAR* strStatus, BOOL bCloaked )
{
    if( g_FriendsManager.GetNumFriends( m_dwUserIndex ) == 0 )
    {
        m_Font.DrawText( 320, 140, COLOR_NORMAL, L"No friends",
                         XBFONT_CENTER_X );
        m_Font.DrawText( 320, 180, COLOR_NORMAL, L"Press " GLYPH_Y_BUTTON L" to add friend", 
                         XBFONT_CENTER_X );
        m_Font.DrawText( 320, 220, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel", 
                         XBFONT_CENTER_X );
        return;
    }
    
    m_Font.DrawText( 320, 70, COLOR_NORMAL, L"Friends List",
                     XBFONT_CENTER_X );
    
    const FLOAT fYtop = 120.0f;
    const FLOAT fYdelta = 30.0f;
    
    DWORD j = 0;
    for( DWORD i = dwTopItem; i < g_FriendsManager.GetNumFriends( m_dwUserIndex ) &&
        j < MAX_FRIENDS_DISPLAYED; ++i, ++j )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        if( dwCurrItem == i )
        {
            // Show selected item with little triangle
            m_Font.DrawText( 175, fYtop + (fYdelta * j ), 0xff00ff00,
                             GLYPH_RIGHT_TICK, XBFONT_RIGHT );
        }
        
        // Convert user name to wide string
        WCHAR strUser[ XONLINE_GAMERTAG_SIZE ];
        swprintf( strUser, L"%S", g_FriendsManager.GetFriend( m_dwUserIndex, i )->szGamertag );

        // Render the voice and status icons
        ONLINEICON VoiceIcon  = g_FriendsManager.GetFriendVoiceIcon( m_dwUserIndex, i );
        ONLINEICON StatusIcon = g_FriendsManager.GetFriendOnlineStateIcon( m_dwUserIndex, i );
        XBOnline_RenderOnlineNotificationIcon( &m_OnlineIconsFont, 
                                               160.0f, fYtop + (fYdelta * j ) - 5.0f, VoiceIcon );
        XBOnline_RenderOnlineNotificationIcon( &m_OnlineIconsFont, 
                                               190.0f, fYtop + (fYdelta * j ) - 5.0f, StatusIcon );
        m_Font.DrawText( 225, fYtop + (fYdelta * j), dwColor, strUser );
    }
    
    // Show scroll arrows
    BOOL bShowTopArrow = dwTopItem > 0;
    BOOL bShowBtmArrow = ( dwTopItem + MAX_FRIENDS_DISPLAYED < g_FriendsManager.GetNumFriends( m_dwUserIndex ) );
    if( bShowTopArrow )
        m_Font.DrawText( 170, 100, COLOR_GREEN, GLYPH_UP_TICK );
    if( bShowBtmArrow )
        m_Font.DrawText( 170, 270, COLOR_GREEN, GLYPH_DOWN_TICK );
    
    assert( dwCurrItem < g_FriendsManager.GetNumFriends( m_dwUserIndex ) );
    XONLINE_FRIEND* pFriend = g_FriendsManager.GetFriend( m_dwUserIndex, dwCurrItem );
    DWORD dwState = pFriend->dwFriendState;
    WCHAR strState[256] = { 0 };
    
    WCHAR strGameName[ MAX_TITLENAME_LEN ];
    g_FriendsManager.GetFriendTitleName( pFriend, XC_LANGUAGE_ENGLISH, 
                                         MAX_TITLENAME_LEN, strGameName );

    if( dwState & XONLINE_FRIENDSTATE_FLAG_ONLINE )
    {
        if( dwState & XONLINE_FRIENDSTATE_FLAG_PLAYING )
        {
            if( dwState & XONLINE_FRIENDSTATE_FLAG_JOINABLE )
            {
                wsprintfW( strState, L"Playing %.*s (joinable)", 128, strGameName );                     
            }
            else
            {
                wsprintfW( strState, L"Playing %.*s", 128, strGameName );                
            }
        }
        else 
        {
            wsprintfW( strState, L"In %.*s", 128, strGameName );            
        }
    }
    else
    {
        lstrcpyW( strState, L"Offline" );
    }
    
    if( dwState & XONLINE_FRIENDSTATE_FLAG_RECEIVEDREQUEST )
    {
        lstrcatW( strState, L"\nHas asked you to accept a friend request" );
    }
    else if( dwState & XONLINE_FRIENDSTATE_FLAG_SENTREQUEST )
    {
        lstrcatW( strState, L"\nRequest has been sent to friend" );
    }
    else 
    {
        if( dwState & XONLINE_FRIENDSTATE_FLAG_RECEIVEDINVITE )
        {
            lstrcatW( strState, 
            L"\nYou have received an invitation to play" );
        }
        else if( dwState & XONLINE_FRIENDSTATE_FLAG_SENTINVITE )
        {
            if( dwState & XONLINE_FRIENDSTATE_FLAG_INVITEACCEPTED )
            {
                lstrcatW( strState, 
                L"\nHas accepted your invitation to play" );
            }
            else if( dwState & XONLINE_FRIENDSTATE_FLAG_INVITEREJECTED )
            {
                lstrcatW( strState, 
                L"\nHas declined your invitation to play" );
            }
            else
            {
                lstrcatW( strState, 
                L"\nGame invitation has been sent to friend" );
            }
        }
    }
        
    m_Font.DrawText( 320, 290, COLOR_GREEN, strState, XBFONT_CENTER_X );
    
    // External status
    m_Font.DrawText( 320, 350, COLOR_GREEN, strStatus, XBFONT_CENTER_X );
    
    // Button descriptions
    m_Font.DrawText(  80, 380, COLOR_NORMAL, GLYPH_A_BUTTON L"Actions" );
    m_Font.DrawText( 200, 380, COLOR_NORMAL, GLYPH_Y_BUTTON L"New Friend" );
    
    if( bCloaked )
    {
        m_Font.DrawText( 360, 380, COLOR_NORMAL, GLYPH_BLACK_BUTTON L"Appear Online" );
    }
    else
    {
        m_Font.DrawText( 360, 380, COLOR_NORMAL, GLYPH_BLACK_BUTTON L"Appear Offline" );
    }

    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderActionMenu()
// Desc: Display the action menu     
//-----------------------------------------------------------------------------
VOID UserInterface::RenderActionMenu( DWORD dwCurrItem, // Index into Actions
                                      ActionList& Actions, 
                                      DWORD dwFriendIndex )
{
    assert( dwFriendIndex < g_FriendsManager.GetNumFriends( m_dwUserIndex ) );
    assert( !Actions.empty() );
    XONLINE_FRIEND* pFriend = g_FriendsManager.GetFriend( m_dwUserIndex, dwFriendIndex );

    FLOAT fYtop = 140.0f;
    FLOAT fYdelta = 30.0f;
    
    // Entries in strMenu must match the ACTIONS enumeration
    const WCHAR* const strMenu[] =
    {
        L"Invite %.*s to play this game",
        L"Revoke invitation to %.*s",
        L"Accept invite to play %.*s",
        L"Decline invite to play %.*s",
        L"Remove inviting friend from friends list",
        L"Accept friend request",
        L"Decline friend request",
        L"Never",
        L"Remove %.*s from friends list",
        L"Join %.*s in this game",
    };

    // Loop through the entries in Actions, each of which
    // is a value in the ACTIONS enum
    DWORD j = 0;

    for( ActionList::iterator i = Actions.begin(); i != Actions.end(); ++i )
    {
        DWORD dwColor = ( dwCurrItem == j ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        
        // Convert user name to wide string
        WCHAR strUser[ XONLINE_GAMERTAG_SIZE ];
        swprintf( strUser, L"%S", pFriend->szGamertag );
        
        WCHAR strItem[ 64 + XONLINE_GAMERTAG_SIZE + MAX_TITLENAME_LEN ];

        assert( *i < ACTION_MAX );

        
        WCHAR strGameName[ MAX_TITLENAME_LEN ];

        g_FriendsManager.GetFriendTitleName(
            pFriend,
            XC_LANGUAGE_ENGLISH, 
            MAX_TITLENAME_LEN, 
            strGameName );

        if (( *i == ACTION_GAME_INVITE_ACCEPT ) || 
            ( *i == ACTION_GAME_INVITE_DECLINE )) // an invite
        {
            wsprintfW( strItem, strMenu[ *i ], 24, strGameName );
        }
        else
            wsprintfW( strItem, strMenu[ *i ], 16, strUser );
        
        m_Font.DrawText( 100, fYtop + (fYdelta * j), dwColor, strItem );
        if( dwCurrItem == j )
        {
            // Show selected item with little triangle
            m_Font.DrawText( 100, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                             GLYPH_RIGHT_TICK, XBFONT_RIGHT );
        }

        ++j;
    }
    
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderNewFriend()
// Desc: Display list of potential new friends
//-----------------------------------------------------------------------------
VOID UserInterface::RenderNewFriend( DWORD dwTopItem, DWORD dwCurrItem,
                                     XONLINE_USER* potentialFriendList, 
                                     DWORD dwNumPotentialFriends )
{
    DWORD dwBottomItem;

    if( 0 == dwNumPotentialFriends )
    {
        m_Font.DrawText( 320, 140, COLOR_NORMAL, 
                         L"You have exhausted the list of potential friends.\n\n"
                         L"Everybody is your friend!\n\n"
                         L"Press " GLYPH_B_BUTTON L" to cancel",
                         XBFONT_CENTER_X );
        return;
    }
    
    m_Font.DrawText( 320, 140, COLOR_NORMAL, L"Potential new friends",
                     XBFONT_CENTER_X );
    
    FLOAT fYtop = 220.0f;
    FLOAT fYdelta = 30.0f;

    dwBottomItem = dwTopItem + MAX_POTENTIALS_DISPLAYED;
    if (dwBottomItem > dwNumPotentialFriends) dwBottomItem = dwNumPotentialFriends;


    // Show list of potential friends
    for( DWORD i = dwTopItem;  i < dwBottomItem; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        
        // Convert user name to WCHAR string
        WCHAR strUser[ XONLINE_GAMERTAG_SIZE ];
        swprintf( strUser, L"%S", potentialFriendList[i].szGamertag );
        
        m_Font.DrawText( 160.0f, fYtop + (fYdelta * (i - dwTopItem)), dwColor, strUser );
    }
        
    // Show selected item with little triangle
    m_Font.DrawText( 120, fYtop + (fYdelta * ( dwCurrItem - dwTopItem ) ), 0xff00ff00,
                     GLYPH_RIGHT_TICK );

    
    BOOL bShowTopArrow = dwTopItem > 0;
    BOOL bShowBtmArrow = ( dwTopItem + MAX_POTENTIALS_DISPLAYED <  dwNumPotentialFriends);
    if( bShowTopArrow )
        m_Font.DrawText( 115, fYtop - 20.0f, COLOR_GREEN, GLYPH_UP_TICK );
    if( bShowBtmArrow )
        m_Font.DrawText( 115, fYtop + MAX_POTENTIALS_DISPLAYED * fYdelta, COLOR_GREEN, GLYPH_DOWN_TICK );

    // Button descriptions
    m_Font.DrawText(  80, 410, COLOR_NORMAL, GLYPH_A_BUTTON L"Request" );
    m_Font.DrawText( 300, 410, COLOR_NORMAL, GLYPH_B_BUTTON L"Back" );
    m_Font.DrawText( 400, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}


//-----------------------------------------------------------------------------
// Name: RenderConfirmRemove()
// Desc: Display confirmation dialog for friend removal
//-----------------------------------------------------------------------------
VOID UserInterface::RenderConfirmRemove( DWORD dwCurrItem, DWORD dwFriendIndex )
{
    // Convert user name to wide string
    assert( dwFriendIndex < g_FriendsManager.GetNumFriends( m_dwUserIndex ) );
    XONLINE_FRIEND *pFriend = g_FriendsManager.GetFriend( m_dwUserIndex, dwFriendIndex );

    WCHAR strUser[ XONLINE_GAMERTAG_SIZE ];
    swprintf( strUser, L"%S", pFriend->szGamertag );
    
    // Build confirmation string
    WCHAR strConfirm[ XONLINE_GAMERTAG_SIZE + 64 ];
    wsprintfW( strConfirm, L"Are you sure you want to remove\n"
               L"'%.*s' from your friend list?", 16,
               strUser );
    
    m_Font.DrawText( 320, 140, COLOR_NORMAL, strConfirm, XBFONT_CENTER_X );
    
    const WCHAR* const strMenu[] =
    {
        L"Yes",
        L"No",
    };
    
    const FLOAT fYtop = 240.0f;
    const FLOAT fYdelta = 50.0f;
    
    for( DWORD i = 0; i < CONFIRM_REMOVE_MAX; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 280, fYtop + (fYdelta * i), dwColor, strMenu[i] );
    }
    
    // Show selected item with little triangle
    m_Font.DrawText( 280, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 360, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );
}




//-----------------------------------------------------------------------------
// Name: RenderGameInviteIcon()
// Desc: Render the game invitation icon
//-----------------------------------------------------------------------------
VOID UserInterface::RenderGameInviteIcon()
{
    XBOnline_RenderOnlineNotificationIcon( &m_OnlineIconsFont, 
                                           298.0f, 35.0f, ONLINEICON_FRIEND_RECEIVEDINVITE );
}




//-----------------------------------------------------------------------------
// Name: RenderGameInvite()
// Desc: Render the game invitate switch disc screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderGameInvite()
{
    m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Insert Game Disc", XBFONT_CENTER_X);
    m_Font.DrawText( 320, 300, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to return to the Friends Sample", XBFONT_CENTER_X);  
}



    
//-----------------------------------------------------------------------------
// Name: RenderError()
// Desc: Display error message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderError( const WCHAR* strError )
{
    m_Font.DrawText( 320, 200, COLOR_NORMAL, strError, XBFONT_CENTER_X );
    m_Font.DrawText( 320, 300, COLOR_NORMAL, L"Press " GLYPH_A_BUTTON L" to continue", 
                     XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: SetUserIndex()
// Desc: Set the users index for calls to the friend manager
//-----------------------------------------------------------------------------
VOID UserInterface::SetUserIndex( DWORD dwUserIndex )
{
    m_dwUserIndex = dwUserIndex;
}




