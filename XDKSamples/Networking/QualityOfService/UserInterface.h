//-----------------------------------------------------------------------------
// File: UserInterface.h
//
// Desc: QualityOfService rendering functions
//
// Hist: 05.24.02 - New for June release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef QUALITYOFSERVICE_UI_H
#define QUALITYOFSERVICE_UI_H

#include "Common.h"
#include "XBFont.h"
#include "XBHelp.h"
#include "XBOnline.h"




//-----------------------------------------------------------------------------
// Name: class UserInterface
// Desc: Main UI class for QualityOfService sample
//-----------------------------------------------------------------------------
class UserInterface
{
    CXBFont m_Font;
    CXBHelp m_Help;

    CXBOnlineUI m_UI;
    const WCHAR* m_strSessionName;

    static const D3DCOLOR COLOR_NORMAL    = CXBOnlineUI::COLOR_NORMAL;
    static const D3DCOLOR COLOR_HIGHLIGHT = CXBOnlineUI::COLOR_HIGHLIGHT;
    static const D3DCOLOR COLOR_GREEN     = CXBOnlineUI::COLOR_GREEN;

public:

    UserInterface( );

    HRESULT Initialize();

    // Accessors
    VOID __cdecl SetErrorStr( const WCHAR*, ... );
    VOID SetSessionName( const WCHAR* );

    // UI functions
    VOID RenderSysLinkSearch();
    VOID RenderCreateAccount( BOOL bHasMachineAccount );
    VOID RenderSelectAccount( DWORD, XBUserList& );
    VOID RenderSigningIn();
    VOID RenderOnlineCreate();
    VOID RenderOnlineSearch();
    VOID RenderMode( DWORD dwCurrItem );
    VOID RenderListenParams( DWORD dwCurrItem, BOOL, DWORD, DWORD, DWORD, BOOL );
    VOID RenderSliderBar( DWORD i, FLOAT, FLOAT, FLOAT, FLOAT, FLOAT );
    VOID RenderSessionList( DWORD dwCurrItem, const XNADDR&, const SessionList&, 
                            const XNQOS*, const XNQOS*, DWORD, BOOL );
    VOID RenderProbeData( DWORD dwCurrItem, const WCHAR* strSession, 
                          const XNADDR&, const SessionList&, 
                          const XNQOS*, const XNQOS*, BOOL );

    VOID RenderError();
    VOID RenderHelp();

private:

    // Disabled
    UserInterface( const UserInterface& );

};

#endif // QUALITYOFSERVICE_UI_H
