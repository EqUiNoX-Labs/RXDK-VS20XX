//-----------------------------------------------------------------------------
// File: UserInterface.cpp
//
// Desc: Auth rendering functions
//
// Hist: 10.12.01 - New for Nov release
//       05.13.02 - Updated for June release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBOnline.h>
#include "UserInterface.h"
#include <cassert>




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const D3DCOLOR COLOR_HIGHLIGHT = 0xffffff00; // Yellow
const D3DCOLOR COLOR_GREEN     = 0xff00ff00; // Green
const D3DCOLOR COLOR_NORMAL    = 0xffffffff; // White
const FLOAT    REGION_WIDTH    = 250.0f;
const FLOAT    REGION_HEIGHT   = 164.0f;
const FLOAT    REGION_X        =  64.0f;
const FLOAT    REGION_Y        =  80.0f;
const FLOAT    REGION_GAP      =   8.0f;





//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects
//-----------------------------------------------------------------------------
HRESULT UserInterface::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "OnlineFont.xpr" ) ) )
        return E_FAIL;

    // Create online icons font
    if( FAILED( m_OnlineIconsFont.Create( "OnlineIconsFont.xpr" ) ) )
    {
        OUTPUT_DEBUG_STRING( "Failed to load fonts\n" );
        return E_FAIL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderUserSelectAccount()
// Desc: Display list of accounts
//-----------------------------------------------------------------------------
VOID UserInterface::RenderUserSelectAccount( DWORD dwUserIndex, 
                                             DWORD dwCurrItem, DWORD dwTopItem,
                                             const XONLINE_USER* pUserList,
                                             DWORD dwNumUsers )
{
    assert( NULL != pUserList );
    assert( dwNumUsers > 0 );

    FLOAT fXtop, fYtop;
    GetRegionPosition( dwUserIndex, &fXtop, &fYtop );
    RenderRegionBorder( dwUserIndex );

    m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop, COLOR_NORMAL, 
                     L"Select an account", XBFONT_CENTER_X );

    FLOAT fYdelta = 30.0f;
    FLOAT fYItem  = fYtop + fYdelta;
    DWORD j = 0;
    for( DWORD i = dwTopItem; i < dwNumUsers && j < MAX_ACCOUNTS_DISPLAYED; ++i, ++j )
    {
        DWORD dwColor;
        
        if( i == dwCurrItem )
        {
            dwColor = COLOR_HIGHLIGHT;
            // Show selected item with little triangle
            m_Font.DrawText( fXtop + 30.0f, fYItem, 0xff00ff00, GLYPH_RIGHT_HAND, XBFONT_RIGHT );
        }
        else
            dwColor = COLOR_NORMAL;

        // Convert user name to WCHAR string
        WCHAR strUserName[ XONLINE_GAMERTAG_SIZE ];
        swprintf( strUserName, L"%S", pUserList[i].szGamertag );

        m_Font.DrawText( fXtop + 30.0f, fYItem, dwColor, strUserName );
        fYItem += fYdelta;
   }
    
    // Show scroll arrows
    BOOL bShowTopArrow = dwTopItem > 0;
    BOOL bShowBtmArrow = dwTopItem + MAX_ACCOUNTS_DISPLAYED < dwNumUsers;
    if( bShowTopArrow )
        m_Font.DrawText( fXtop + REGION_WIDTH - 16.0f, 
                         fYtop + 15.0f, COLOR_GREEN, GLYPH_UP_HAND );
    if( bShowBtmArrow )
        m_Font.DrawText( fXtop + REGION_WIDTH - 16.0f, 
                         fYtop + (fYdelta * MAX_ACCOUNTS_DISPLAYED ) + 16.0f, 
                         COLOR_GREEN, GLYPH_DOWN_HAND );


    m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT - fYdelta, 
                     COLOR_NORMAL, GLYPH_A_BUTTON L"Select Account", XBFONT_CENTER_X );

}




//-----------------------------------------------------------------------------
// Name: RenderSigningOn()
// Desc: Display "signing on" animation
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSigningOn( const XONLINE_USER *pUsers )
{
    // If a user list has been passed, display the users and their
    // controller assignments
    if( pUsers ) 
    {
        FLOAT fYtop   = 150.0f;
        FLOAT fYdelta =  30.0f;

        m_Font.DrawText( 320, 100, COLOR_NORMAL, L"Authenticating Users", 
                         XBFONT_CENTER_X );
        
        // Show list of logon users
        for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
        {
            WCHAR strText[ XONLINE_GAMERTAG_SIZE + 64 ];
            
            if( pUsers[i].xuid.qwUserID != 0 )
            {
                if( XOnlineIsUserGuest( pUsers[i].xuid.dwUserFlags ) )
                {
                    wsprintfW( strText, L"Controller %lu: %S (guest)", i + 1, 
                               pUsers[i].szGamertag );
                }
                else
                {
                    wsprintfW( strText, L"Controller %lu: %S", i + 1, 
                               pUsers[i].szGamertag );
                }
            }
            else
            {
                wsprintfW( strText, L"Controller %lu: NONE", i + 1 );
            }
            
            m_Font.DrawText( 170, fYtop + (fYdelta * i), COLOR_NORMAL, strText );
        }
        m_Font.DrawText( 320, 340, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel", 
                         XBFONT_CENTER_X );
    }
    else
    {
        m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Authenticating Xbox Account",
                         XBFONT_CENTER_X );
        m_Font.DrawText( 320, 260, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel",
                         XBFONT_CENTER_X );
    }
}




//-----------------------------------------------------------------------------
// Name: RenderUserPINEntry()
// Desc: Display PIN entry screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderUserPINEntry( DWORD dwUserIndex,
                                        DWORD dwNumChars )
{
    assert( dwNumChars <= XONLINE_PASSCODE_LENGTH );

    FLOAT fXtop, fYtop;
    GetRegionPosition( dwUserIndex, &fXtop, &fYtop );
    RenderRegionBorder( dwUserIndex );

    WCHAR strPinText[XONLINE_PASSCODE_LENGTH+1];
    
    for( DWORD i = 0; i < dwNumChars; ++i )
        strPinText[i] = L'*';
    strPinText[dwNumChars] = L'\0';

    m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT / 2 - 60.0f, 
                     COLOR_NORMAL, L"Enter Passcode", XBFONT_CENTER_X );

    m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT / 2 - 30.0f, 
                     COLOR_HIGHLIGHT, strPinText, XBFONT_CENTER_X );

    
    // Button descriptions
    m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT - 30.0f, 
                     COLOR_NORMAL, GLYPH_A_BUTTON L"Submit   " GLYPH_B_BUTTON L"Back", XBFONT_CENTER_X );

}




//-----------------------------------------------------------------------------
// Name: RenderUserDone()
// Desc: Display Signon result
//-----------------------------------------------------------------------------
VOID UserInterface::RenderUserDone( DWORD dwUserIndex, const WCHAR *strName,
                                    BOOL bSignedOn, BOOL bVoice )
{
    FLOAT fXtop, fYtop;
    GetRegionPosition( dwUserIndex, &fXtop, &fYtop );
    RenderRegionBorder( dwUserIndex, strName );

    m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT / 2, 
                     COLOR_NORMAL,
                     bSignedOn ? L"Signed On" : L"Not Signed On", XBFONT_CENTER_X );

     // Button descriptions
    if( bSignedOn )
        m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT - 30.0f, 
            COLOR_NORMAL, L"Press " GLYPH_A_BUTTON L" to Sign Off", XBFONT_CENTER_X );
    if( bVoice )
        XBOnline_RenderOnlineNotificationIcon( &m_OnlineIconsFont, 
                                               fXtop + REGION_WIDTH/2  - 20.0f, 
                                               fYtop + REGION_HEIGHT/2 - 35.0f,
                                               ONLINEICON_PLAYER_VOICE );
}




//-----------------------------------------------------------------------------
// Name: RenderUserError()
// Desc: Display user specific error
//-----------------------------------------------------------------------------
VOID UserInterface::RenderUserError( DWORD dwUserIndex, const WCHAR *strError,
                                     BOOL bBootToDash )
{
    FLOAT fXtop, fYtop;
    GetRegionPosition( dwUserIndex, &fXtop, &fYtop );
    RenderRegionBorder( dwUserIndex );

    m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT/2 - 60.0f,
                     COLOR_NORMAL, strError, XBFONT_CENTER_X );

     // Button descriptions
    if( bBootToDash )
        m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT - 30.0f, 
                         COLOR_NORMAL, GLYPH_A_BUTTON L"Continue  " GLYPH_X_BUTTON L"Dash", XBFONT_CENTER_X );
    else
        m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT - 30.0f, 
                         COLOR_NORMAL, L"Press " GLYPH_A_BUTTON L" to continue", XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderUserPreSignOn()
// Desc: Display controller selection screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderUserPreSignOn( DWORD dwUserIndex )
{
    FLOAT fXtop, fYtop;
    GetRegionPosition( dwUserIndex, &fXtop, &fYtop );
    RenderRegionBorder( dwUserIndex );

    m_Font.DrawText( fXtop + REGION_WIDTH / 2, 
                     fYtop + REGION_HEIGHT / 2, COLOR_NORMAL, 
                     L"Press " GLYPH_A_BUTTON L" to Play", XBFONT_CENTER_X );    
}




//-----------------------------------------------------------------------------
// Name: RenderUserWaitForOthers()
// Desc: Waiting for other users display
//-----------------------------------------------------------------------------
VOID UserInterface::RenderUserWaitForOthers( DWORD dwUserIndex,
                                             const WCHAR *strName, 
                                             BOOL bReadyForSignOn )
{
    FLOAT fXtop, fYtop;
    GetRegionPosition( dwUserIndex, &fXtop, &fYtop );
    RenderRegionBorder( dwUserIndex, strName );

    if( bReadyForSignOn )
    {
        m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT / 2,
                         COLOR_NORMAL, L"Press " GLYPH_A_BUTTON L" to Sign On", 
                         XBFONT_CENTER_X );
    }
    else
    {
        m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT / 2,
                         COLOR_NORMAL, L"Waiting for others...", 
                         XBFONT_CENTER_X );
    }
}




//-----------------------------------------------------------------------------
// Name: RenderConfirmSponsor()
// Desc: Confirm selection of a sponsor account (for guest sign on)
//-----------------------------------------------------------------------------
VOID UserInterface::RenderConfirmSponsor( DWORD dwUserIndex,
                                          const XONLINE_USER & Sponsor )
{
    FLOAT fXtop, fYtop;
    GetRegionPosition( dwUserIndex, &fXtop, &fYtop );
    RenderRegionBorder( dwUserIndex );

    WCHAR strText[80 + 2*XONLINE_GAMERTAG_SIZE];
    wsprintfW( strText, L"%S\nhas already been selected", Sponsor.szGamertag );
    m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT/2 - 60.0f,
                     COLOR_NORMAL, strText, XBFONT_CENTER_X );

    m_Font.DrawText( fXtop + REGION_WIDTH / 2, fYtop + REGION_HEIGHT/2,
                     COLOR_NORMAL, L"Press " GLYPH_A_BUTTON L" to sign on as a guest", XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: GetRegionPosition()
// Desc: Return the origin on the user region based on a controller
//-----------------------------------------------------------------------------
VOID UserInterface::GetRegionPosition( DWORD dwUserIndex, 
                                       FLOAT* pfX, FLOAT* pfY )
{
    assert( dwUserIndex < 4 );

    switch( dwUserIndex )
    {
        case 0: 
            (*pfX) = REGION_X; 
            (*pfY) = REGION_Y;
            break;
        case 1:
            (*pfX) = REGION_X + REGION_WIDTH +  REGION_GAP; 
            (*pfY) = REGION_Y;
            break;
        case 2:
            (*pfX) = REGION_X; 
            (*pfY) = REGION_Y + REGION_HEIGHT + REGION_GAP; 
            break;
        case 3:
            (*pfX) = REGION_X + REGION_WIDTH  + REGION_GAP; 
            (*pfY) = REGION_Y + REGION_HEIGHT + REGION_GAP;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: RenderRegionBorder()
// Desc: Render border around a user region
//-----------------------------------------------------------------------------
VOID UserInterface::RenderRegionBorder( DWORD dwUserIndex, const WCHAR* strName )
{
    FLOAT fLeft, fTop;
    GetRegionPosition( dwUserIndex, &fLeft, &fTop );
    fLeft = floorf(fLeft) - 0.5f;
    fTop  = floorf(fTop)  - 0.5f;

    if( strName )
        m_Font.DrawText( fLeft + REGION_WIDTH / 2, fTop, COLOR_NORMAL,
                         strName, XBFONT_CENTER_X );

    D3DXVECTOR4 pVertices[4];
    pVertices[0] = D3DXVECTOR4( fLeft,                fTop,                 1.0f, 1.0f );
    pVertices[1] = D3DXVECTOR4( fLeft + REGION_WIDTH, fTop,                 1.0f, 1.0f );
    pVertices[2] = D3DXVECTOR4( fLeft + REGION_WIDTH, fTop + REGION_HEIGHT, 1.0f, 1.0f );
    pVertices[3] = D3DXVECTOR4( fLeft,                fTop + REGION_HEIGHT, 1.0f, 1.0f );

    // Render the surrounding rectangle
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW );
    D3DDevice::SetTexture( 0, NULL );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    D3DDevice::SetRenderState( D3DRS_TEXTUREFACTOR, COLOR_NORMAL );
    D3DDevice::DrawVerticesUP( D3DPT_LINELOOP, 4, pVertices, sizeof(D3DXVECTOR4) );
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
}




//-----------------------------------------------------------------------------
// Name: RenderMessage()
// Desc: Display an error or other message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderMessage( WCHAR* strMessage, BOOL bBootToDash )
{
    m_Font.DrawText( 320, 200, COLOR_NORMAL, strMessage, XBFONT_CENTER_X );
    m_Font.DrawText( 320, 300, COLOR_NORMAL, L"Press " GLYPH_A_BUTTON L" to continue", 
                     XBFONT_CENTER_X );
    
    if( bBootToDash )
    {
        m_Font.DrawText( 320, 360, COLOR_NORMAL, 
                         L"Press " GLYPH_X_BUTTON L" to boot to the Xbox Dashboard",
                         XBFONT_CENTER_X );
    }
}




