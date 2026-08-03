//-----------------------------------------------------------------------------
// File: UserInterface.h
//
// Desc: User interface for authentication
//
// Hist: 10.12.01 - New for Nov release
//       05.13.02 - Updated for June release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef USERINTERFACE_H
#define USERINTERFACE_H

#include <xtl.h>
#include <xonline.h>
#include "XBFont.h"




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD MAX_ACCOUNTS_DISPLAYED = 3;




//-----------------------------------------------------------------------------
// Name: class UserInterface
// Desc: Main UI class for Auth sample
//-----------------------------------------------------------------------------
class UserInterface
{
    CXBFont m_Font;            // Game font
    CXBFont m_OnlineIconsFont; // Font for online icons
    
    VOID    GetRegionPosition( DWORD, FLOAT*, FLOAT* );
    VOID    RenderRegionBorder( DWORD, const WCHAR* strName = NULL );

public:
    HRESULT Initialize();

    // UI functions
    VOID    RenderCreateAccount( BOOL );
    VOID    RenderUserSelectAccount( DWORD, DWORD, DWORD, const XONLINE_USER* pUserList, DWORD dwNumUsers );
    VOID    RenderUserPreSignOn( DWORD );
    VOID    RenderSigningOn( const XONLINE_USER* );
    VOID    RenderUserPINEntry( DWORD, DWORD  );
    VOID    RenderMessage( WCHAR* strMessage, BOOL bBootToDash = FALSE );
    VOID    RenderUserError( DWORD, const WCHAR*, BOOL bBootToDash = FALSE );
    VOID    RenderUserDone( DWORD, const WCHAR*, BOOL, BOOL );
    VOID    RenderUserWaitForOthers( DWORD, const WCHAR*, BOOL );
    VOID    RenderConfirmSponsor( DWORD, const XONLINE_USER & );
};


#endif // USERINTERFACE_H
