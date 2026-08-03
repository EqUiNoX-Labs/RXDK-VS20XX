//-----------------------------------------------------------------------------
// File: XbOnline.h
//
// Desc: Shortcut macros and helper functions for the Xbox online samples.
//       Requires linking with XONLINE[D][S].LIB
//
// Hist: 10.11.01 - New for November XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef XBONLINE_H
#define XBONLINE_H

#pragma warning( disable: 4786 ) // ignore STL identifier truncation
#include <xtl.h>
#include <vector>
#include <uix.h>
#include "xonline.h"
#include "XBResource.h"
#include "XBFont.h"




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD MAX_ERROR_STR = 1024;




//-----------------------------------------------------------------------------
// Name: XBOnline_GetUserList() / XBOnline_UpdateUserList
// Desc: Extract accounts from hard disk and MUs
//-----------------------------------------------------------------------------
typedef std::vector< XONLINE_USER > XBUserList;
HRESULT XBOnline_GetUserList( XBUserList& UserList );




//-----------------------------------------------------------------------------
// Name: XBOnline_GetUserList()
// Desc: Extract all accounts from hard disk and MUs
//-----------------------------------------------------------------------------
HRESULT XBOnline_GetUserList( XONLINE_USER* pUserList, DWORD* pdwNumUsers );




//-----------------------------------------------------------------------------
// Name: XBNet_OnlineInit() / XBNet_OnlineCleanup()
// Desc: Performs all required initialization/shutdown for online and 
//       network play
//-----------------------------------------------------------------------------
HRESULT XBNet_OnlineInit( BYTE cfgFlags = 0, 
                          XONLINE_STARTUP_PARAMS* pxosp = NULL );
HRESULT XBNet_OnlineCleanup();




//-----------------------------------------------------------------------------
// Name: XBOnline_RenderOnlineNotificationIcon()
// Desc: Render an online notification icon
//-----------------------------------------------------------------------------
enum ONLINEICON
{
    ONLINEICON_NONE = 0,                   // No icon
    ONLINEICON_FRIEND_ONLINE,              // Friend is online
    ONLINEICON_FRIEND_RECEIVEDREQUEST,     // Friend request received
    ONLINEICON_FRIEND_SENTREQUEST,         // Friend request sent
    ONLINEICON_FRIEND_RECEIVEDINVITE,      // Game invite from friend
    ONLINEICON_FRIEND_SENTINVITE,          // Game invite to friend
    ONLINEICON_PLAYER_VOICE,               // Player has voice capability        
    ONLINEICON_PLAYER_MUTED,               // Player has been muted
    ONLINEICON_PLAYER_TVCHAT,              // TV Chat -  no voice comm.               
};

VOID XBOnline_RenderOnlineNotificationIcon( CXBFont* pOnlineIconsFont, 
                                            FLOAT fLeft, FLOAT fTop, 
                                            ONLINEICON Icon );



//-----------------------------------------------------------------------------
// Name: XBOnline_GetOnlineHResultString()
// Desc: Display and online error HRESULT
//-----------------------------------------------------------------------------
const WCHAR* XBOnline_GetOnlineHResultString( HRESULT hr );




//-----------------------------------------------------------------------------
// Name: class CXBOnlineUI
// Desc: UI class for standard online UI
//-----------------------------------------------------------------------------
class CXBOnlineUI
{    
public:
    // number of icons and offset multiplier in index
    static const DWORD    ONLINEICON_OFFSET_MULTIPLIER = 20UL;
    static const DWORD    ONLINEICON_NUM_ICONS = 8;

    static const D3DCOLOR COLOR_HIGHLIGHT = 0xffffff00; // Yellow
    static const D3DCOLOR COLOR_GREEN     = 0xff00ff00; // Green
    static const D3DCOLOR COLOR_NORMAL    = 0xffffffff; // White

private:
    // Disabled
    CXBOnlineUI( const CXBOnlineUI& );

    mutable CXBFont     m_Font;                      // Game font
    mutable CXBFont     m_OnlineIconsFont;
    
    WCHAR               m_strError[ MAX_ERROR_STR ]; // Generic err
    WCHAR*              m_strHeader;                 // Header string

public:

    CXBOnlineUI();

    HRESULT Initialize();

    // Accessors
    VOID SetErrorStr( const WCHAR*, va_list );

    VOID SetHeader( WCHAR* strHeader ) { m_strHeader = strHeader; }

    // UI functions
    VOID RenderCreateAccount( BOOL bHasMachineAccount ) const;
    VOID RenderSelectAccount( DWORD, const XBUserList& ) const;
    VOID RenderLoggingOn( const XONLINE_USER *pUsers = NULL ) const;
    VOID RenderError( BOOL bBootToDash = FALSE ) const;
    VOID RenderHeader() const;
    VOID RenderOnlineNotificationIcon( FLOAT fLeft, FLOAT fTop, ONLINEICON Icon ) const;
};



//-----------------------------------------------------------------------------
// Name: class UIXFont
// Desc: A wrapper around a CXBFont class that works with UIX
//-----------------------------------------------------------------------------
class UIXFont : public ITitleFontRenderer
{
    CXBFont* m_pFont;
    DWORD    m_dwCurrentColor;

public:
    UIXFont( CXBFont* pFont )
    { 
        m_pFont          = pFont;
        m_dwCurrentColor = 0xffffffff;
    }

    virtual ULONG __stdcall Release()
    { 
        return 0; 
    }
    
    virtual HRESULT __stdcall SetHeight( DWORD Height ) 
    { 
        FLOAT fScale = (FLOAT)Height/m_pFont->m_fFontYAdvance;
        m_pFont->SetScaleFactors( fScale, fScale );
        return S_OK;
    }
    
    virtual HRESULT __stdcall SetColor( D3DCOLOR Color )
    {
        m_dwCurrentColor = (DWORD)Color;
        return S_OK;
    }
    
    virtual HRESULT __stdcall DrawText( const WCHAR* pText,
                                        DWORD X, DWORD Y, DWORD dwMaxWidth )
    {  
        if( dwMaxWidth == 0 )
            return m_pFont->DrawText( (FLOAT)X, (FLOAT)Y, m_dwCurrentColor, pText );
        else
        {
            // Because the GetTextSize function truncates the required text size when
            // converting it to an integer, the actual size required for the text being
            // drawn could be 0.999 pixels more (width and height) than UIX thinks.
            // If UIX thinks that the text perfectly fits (GetTextSize() == dwMaxWidth)
            // then it is quite likely that DrawText will think that the text is a
            // fraction of a pixel out of room. This will cause DrawText to insert an
            // ellipsis when UIX thought there was no need to word wrap.
            // The best way to avoid this situation is to lie to the DrawText code about
            // how much space is available - hence the + 1 on dwMaxWidth. That ensures
            // that DrawText will not insert an ellipsis on text that UIX thinks fits.
            return m_pFont->DrawText( (FLOAT)X, (FLOAT)Y, m_dwCurrentColor, pText,
                                      XBFONT_TRUNCATED, (FLOAT)dwMaxWidth + 1 );
        }
    }

    virtual HRESULT __stdcall GetTextSize( const WCHAR* pText,
                                           DWORD* pWidth, DWORD* pHeight )
    {
        FLOAT fWidth;
        FLOAT fHeight;
        m_pFont->GetTextExtent( pText, &fWidth, &fHeight );
        (*pWidth)  = (DWORD)fWidth;
        (*pHeight) = (DWORD)fHeight;
        return S_OK;
    }
};



#endif // XBONLINE_H
