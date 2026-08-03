//-----------------------------------------------------------------------------
// File: UserInterface.h
//
// Desc: Stats rendering functions
//
// Hist: 04.10.02 - New for May release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef STAT_UI_H
#define STAT_UI_H

#include "Common.h"
#include "XBFont.h"
#include "XBHelp.h"
#include "XBOnline.h"




//-----------------------------------------------------------------------------
// Name: class UserInterface
// Desc: Main UI class for Stats sample
//-----------------------------------------------------------------------------
class UserInterface
{
    CXBFont m_Font;
    CXBHelp m_Help;

    CXBOnlineUI m_UI;

    static const D3DCOLOR COLOR_NORMAL = CXBOnlineUI::COLOR_NORMAL;
    static const D3DCOLOR COLOR_HIGHLIGHT = CXBOnlineUI::COLOR_HIGHLIGHT;

public:

    UserInterface();

    HRESULT Initialize();

    // Accessors
    VOID __cdecl SetErrorStr( const WCHAR*, ... );

    // UI functions
    VOID RenderCreateAccount( BOOL );
    VOID RenderSelectAccount( DWORD, const XBUserList& );
    VOID RenderMainMenu( DWORD, WCHAR * );
    VOID RenderLoggingOn( const XONLINE_USER * );
    VOID RenderError( BOOL bBootToDash = FALSE );
    VOID RenderHelp();
    VOID RenderFriendEnum();
    VOID RenderFinishFriendEnum();
    VOID RenderSelectLevel( DWORD );
    VOID RenderEndGame( DWORD , PlayerList & );
    VOID RenderLeaderboard( const XUID *, DWORD, BOOL,
                            const PlayerList & );
    VOID RenderReadStats();
    VOID RenderWriteStats();
    VOID RenderResetStats( WCHAR* );

private:

    // Disabled
    UserInterface( const UserInterface& );

};

#endif // STAT_UI_H
