//-----------------------------------------------------------------------------
// File: UserInterface.h
//
// Desc: Matchmaking rendering functions
//
// Hist: 10.19.01 - Updated for Nov release
//       04.22.02 - New for May release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef MATCHMAKING_UI_H
#define MATCHMAKING_UI_H

#include "Common.h"
#include "XBFont.h"
#include "XBHelp.h"
#include "XBOnline.h"




//-----------------------------------------------------------------------------
// Name: class UserInterface
// Desc: Main UI class for Matchmaking sample
//-----------------------------------------------------------------------------
class UserInterface
{
    CXBFont m_Font;
    CXBHelp m_Help;

    CXBOnlineUI m_UI;

    static const D3DCOLOR COLOR_NORMAL    = CXBOnlineUI::COLOR_NORMAL;
    static const D3DCOLOR COLOR_HIGHLIGHT = CXBOnlineUI::COLOR_HIGHLIGHT;
    static const D3DCOLOR COLOR_GREEN     = CXBOnlineUI::COLOR_GREEN;

public:

    UserInterface( );

    HRESULT Initialize();

    // Accessors
    VOID __cdecl SetErrorStr( const WCHAR*, ... );

    // UI functions
    VOID RenderSelectMatch( DWORD dwCurrItem );
    VOID RenderOptiMatch(  SessionInfo&, DWORD dwCurrItem );
    VOID RenderSelectType( DWORD dwCurrItem );
    VOID RenderSelectStyle( DWORD dwCurrItem );
    VOID RenderSelectLevel( DWORD dwCurrItem );
    VOID RenderSelectName( DWORD dwCurrItem, const SessionNameList& );
    VOID RenderSelectSession( DWORD dwCurrItem, SessionList& );

    VOID RenderCreateAccount( BOOL bHasMachineAccount );
    VOID RenderSelectAccount( DWORD, XBUserList&, XUID & );
    VOID RenderLoggingOn();
    VOID RenderCancel();

    VOID RenderGameSearch( BOOL );
    VOID RenderRequestJoin();
    VOID RenderCreateSession();
    VOID RenderPlayGame( SessionInfo&, WCHAR* ,
                         WCHAR* , DWORD , 
                         DWORD , BOOL );
    VOID RenderDeleteSession();
    VOID RenderError();
    VOID RenderHelp();

    VOID RenderFinishFriendEnum();

private:

    // Disabled
    UserInterface( const UserInterface& );

};

#endif // MATCHMAKING_UI_H
