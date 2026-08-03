//-----------------------------------------------------------------------------
// File: UserInterface.h
//
// Desc: DownloadManager rendering functions
//
// Hist: 10.16.02 - New for Nov release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef USERINTERFACE_H
#define USERINTERFACE_H

#include "Common.h"
#include "XBFont.h"
#include "XBOnline.h"


static const D3DCOLOR COLOR_HIGHLIGHT = 0xffffff00; // Yellow
static const D3DCOLOR COLOR_GREEN     = 0xff00ff00; // Green
static const D3DCOLOR COLOR_NORMAL    = 0xffffffff; // White


    
    
//-----------------------------------------------------------------------------
// Name: class UserInterface
// Desc: Main UI class for DownloadManager sample
//-----------------------------------------------------------------------------
class UserInterface
{
    mutable CXBFont     m_Font;                      // Game font
    mutable CXBFont     m_OnlineIconsFont;

    WCHAR               m_strError[ MAX_ERROR_STR ]; // Generic err

    VOID GetRegionPosition( DWORD, FLOAT *, FLOAT * );
    VOID RenderRegionBorder( DWORD, const WCHAR * strName = NULL );

public:
    // UI functions
    VOID RenderError( BOOL bBootToDash = FALSE ) const;

    UserInterface();

    HRESULT Initialize();

    // Accessors
    VOID __cdecl SetErrorStr( const WCHAR*, ... );

    // UI functions
    VOID RenderCreateAccount( BOOL  );
    VOID RenderUserSelectAccount( DWORD, DWORD, DWORD, const XBUserList& );
    VOID RenderUserPreSignOn( DWORD );
    VOID RenderSigningOn( const XONLINE_USER * );
    VOID RenderUserPINEntry( DWORD, DWORD  );
    VOID RenderError( BOOL bBootToDash = FALSE );
    VOID RenderMainMenu();
    VOID RenderUserError( DWORD,  const WCHAR*, BOOL bBootToDash = FALSE );
    VOID RenderUserDone( DWORD, const WCHAR *, BOOL, BOOL );
    VOID RenderUserWaitForOthers( DWORD, const WCHAR *, BOOL );
    VOID RenderConfirmSponsor( DWORD, const XONLINE_USER & );
};




#endif // USERINTERFACE_H
