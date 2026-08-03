//-----------------------------------------------------------------------------
// File: UserInterface.h
//
// Desc: Friends rendering functions
//
// Hist: 10.20.01 - Updated for Nov release
//       03.14.02 - Updated for April release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef FRIENDS_UI_H
#define FRIENDS_UI_H

#include "Common.h"
#include "XBFont.h"
#include "XBHelp.h"
#include "XBOnline.h"



const D3DCOLOR COLOR_HIGHLIGHT = 0xffffff00; // Yellow
const D3DCOLOR COLOR_GREEN     = 0xff00ff00; // Green
const D3DCOLOR COLOR_NORMAL    = 0xffffffff; // White



//-----------------------------------------------------------------------------
// Name: class UserInterface
// Desc: Main UI class for Friends sample
//-----------------------------------------------------------------------------
class UserInterface
{
    mutable CXBFont m_Font;
    CXBFont         m_OnlineIconsFont;

    DWORD           m_dwUserIndex;

public:
    UserInterface( );
    
    HRESULT Initialize();
    
    // Accessors
    VOID SetUserIndex( DWORD dwUserIndex );
        
    // UI functions
    VOID RenderCreateAccount( BOOL bHasMachineAccount );
    VOID RenderSelectAccount( DWORD dwTopItem, DWORD dwCurrItem, 
                              const XONLINE_USER* acctList, DWORD dwNumAccounts);
    VOID RenderLoggingOn();
    VOID RenderFriendList( DWORD dwTopItem, DWORD dwCurrItem,
                           const WCHAR* strStatus, BOOL bCloaked );
    VOID RenderActionMenu( DWORD dwCurrItem, ActionList& Actions, DWORD dwFriendIndex );
    VOID RenderNewFriend( DWORD dwTopItem, DWORD dwCurrItem, 
                          XONLINE_USER* potentialFriendList, DWORD dwNumPotentialFriends );
    VOID RenderConfirmRemove( DWORD dwCurrItem, DWORD dwFriendIndex );        
    VOID RenderError( const WCHAR* );
    VOID RenderGameInviteIcon();
    VOID RenderGameInvite();
};


#endif // FRIENDS_UI_H
