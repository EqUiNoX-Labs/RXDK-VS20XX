//-----------------------------------------------------------------------------
// File: UserInterface.cpp
//
// Desc: ContentDownload rendering functions
//
// Hist: 09.04.01 - Updated for Nov release
//       04.05.02 - Added billable content and content details.
//                  Updated for the new HD/DVD content enumeration API
//       06.05.02 - Updated billing stuctures. Added removal of "bad" content
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "UserInterface.h"
#include "XBApp.h"
#include "XBConfig.h"
#include "XBStorageDevice.h"
#include <cassert>




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD MAX_CONTENT_DISPLAYED = 5;    // Number to show on screen

const D3DCOLOR COLOR_PROGRESS  = 0xff00ff00; // Green
const D3DCOLOR COLOR_BARBORDER = 0xff000000; // Black

const DWORD FVF_BARVERTEX = D3DFVF_XYZRHW;




//-----------------------------------------------------------------------------
// Name: UserInterface()
// Desc: Constructor
//-----------------------------------------------------------------------------
UserInterface::UserInterface()
{
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
}




//-----------------------------------------------------------------------------
// Name: RenderSelectAccount()
// Desc: Display list of accounts
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectAccount( DWORD dwCurrItem, 
                                        const XBUserList& UserList )
{
    assert( !UserList.empty() );

    m_Font.DrawText( 320, 100, COLOR_NORMAL, L"Select an account",
                     XBFONT_CENTER_X );

    FLOAT fYtop   = 160.0f;
    FLOAT fYdelta =  30.0f;

    // Show list of user accounts
    for( DWORD i = 0; i < UserList.size(); ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;

        // Convert user name to WCHAR string
        WCHAR strUserName[ XONLINE_GAMERTAG_SIZE ];
        swprintf( strUserName, L"%S", UserList[i].szGamertag );

        m_Font.DrawText( 160, fYtop + (fYdelta * i), dwColor, strUserName );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 120.0f, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00, GLYPH_RIGHT_TICK );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectDevice()
// Desc: Display available content enumeration devices
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectDevice( DWORD dwCurrItem )
{
    m_Font.DrawText( 320, 140, COLOR_NORMAL, L"Select Content Device",
                     XBFONT_CENTER_X );
    
    const WCHAR* const strDevice[] =
    {
            L"Hard Disk",
            L"Online Game Content Server",
            L"Online using Downloader"
    };
    
    const FLOAT fYtop = 200.0f;
    const FLOAT fYdelta = 50.0f;
    
    for( DWORD i = 0; i < sizeof( strDevice ) / sizeof( strDevice[0] ) ; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 260, fYtop + (fYdelta * i), dwColor, strDevice[i] );
    }
    
    // Show selected item with little triangle
    m_Font.DrawText( 260, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectContent()
// Desc: Display content list
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectContent( const ContentList& contentList,
                                         DWORD dwCurrItem,
                                         DWORD dwTopItem )
{
    if( contentList.empty() )
    {
        m_Font.DrawText( 320, 140, COLOR_NORMAL, L"No content available",
                         XBFONT_CENTER_X );
        m_Font.DrawText( 320, 180, COLOR_NORMAL, L"Press " GLYPH_A_BUTTON L" to continue", 
                         XBFONT_CENTER_X );
        return;
    }
    
    m_Font.DrawText( 320, 100, COLOR_NORMAL, L"Content List",
                     XBFONT_CENTER_X );
    
    m_Font.DrawText( 100, 140, COLOR_NORMAL, L"Press " GLYPH_A_BUTTON L" for more info" );
    
    const FLOAT fYtop = 200.0f;
    const FLOAT fYdelta = 40.0f;
    
    DWORD j = 0;
    for( DWORD i = dwTopItem; i < contentList.size() &&
        j < MAX_CONTENT_DISPLAYED; ++i, ++j )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        if( dwCurrItem == i )
        {
            // Show selected item with little triangle
            m_Font.DrawText( 200, fYtop + (fYdelta * ( dwCurrItem - dwTopItem ) ), 0xff00ff00,
                             GLYPH_RIGHT_TICK, XBFONT_RIGHT );
        }
        
        WCHAR strOfferingId[ 64 ];
        wsprintfW( strOfferingId, L"ID: 0x%I64X", contentList[i].GetId() );
        
        m_Font.DrawText( 200, fYtop + (fYdelta * j), dwColor, strOfferingId );
    }
    
    // Show scroll arrows
    BOOL bShowTopArrow = dwTopItem > 0;
    BOOL bShowBtmArrow = dwTopItem + MAX_CONTENT_DISPLAYED < contentList.size();
    if( bShowTopArrow )
        m_Font.DrawText( 170, 170, COLOR_GREEN, GLYPH_UP_TICK );
    if( bShowBtmArrow )
        m_Font.DrawText( 170, 390, COLOR_GREEN, GLYPH_DOWN_TICK );
}




//-----------------------------------------------------------------------------
// Name: RenderContentDetails()
// Desc: Display detailed information about selected content
//-----------------------------------------------------------------------------
VOID UserInterface::RenderContentDetails( ContentInfo& content,
                                          BOOL bBillingEnabled )
{
    // Display enum and details data
    WCHAR* strOfferingCost = NULL;
    WCHAR  strOfferingID [64] = L"";
    WCHAR* strOfferingTypeDesc = NULL;
    WCHAR  strOfferingType[ 64 ] = L"";
    WCHAR  strBitFlags[ 12 ] = L"";
    WCHAR  strOfferingFlags[ 12 ] = L"";
    WCHAR  strRating[ 12 ] = L"";
    WCHAR  strEnumDataBlob[64];
    WCHAR  strPackageBlocks[32] = L"";
    WCHAR  strInstallBlocks[32] = L"";
    WCHAR  strNumInstances[12] = L"";
    WCHAR  strPrice[64] = L"";
    WCHAR  strFreeMonths[64] = L"";
    WCHAR  strDuration[64] = L"";
    WCHAR* strFrequency = NULL;
    WCHAR  strDetailsDataBlob[128] = L"";

    // offing ID and free or pay
    if( content.GetPrice().fOfferingIsFree )
        strOfferingCost = L"Free";
    else
        strOfferingCost = L"Pay";
    wsprintfW( strOfferingID, L"0x%I64X, %s ", 
               content.GetId(), strOfferingCost );

    // offering type
    if( XONLINE_OFFERING_SUBSCRIPTION == content.GetOfferingType() )
        strOfferingTypeDesc = L"Subscription";
    else if( XONLINE_OFFERING_CONTENT == content.GetOfferingType() )
            strOfferingTypeDesc = L"Content";
    else
        strOfferingTypeDesc = L"Title Defined";
    wsprintfW( strOfferingType, L"0x%08X, %s", content.GetOfferingType(),
        strOfferingTypeDesc );

    // date and time information
    WCHAR strDate[32];
    WCHAR strTime[32];
    FILETIME ftActivationTime = content.GetActivationDate();
    CXBConfig::FormatDateTime( ftActivationTime, strDate, strTime );

    if( XONLINE_OFFERING_SUBSCRIPTION != content.GetOfferingType() )
    {
        // package size in blocks
        const DWORD dwBlockSize = CXBStorageDevice::GetBlockSize();
        DWORD dwPackageBlocks = ( content.GetPackageSize() + (dwBlockSize-1) )
                                / dwBlockSize;
        CXBConfig::FormatInt( dwPackageBlocks, strPackageBlocks );
        wcscat( strPackageBlocks, L" blocks" );
    
        // install size in blocks
        DWORD dwInstallBlocks = content.GetInstallSize();
        CXBConfig::FormatInt( dwInstallBlocks, strInstallBlocks );
        wcscat( strInstallBlocks, L" blocks" );
    }

    // bit flags and rating
    wsprintfW( strBitFlags, L"0x%08X", content.GetBitFlags() );
    wsprintfW( strOfferingFlags, L"0x%04X", content.GetOfferingFlags() );

    wsprintfW( strRating, L"%u", content.GetRating() );

    // enum data blob
    XBUtil_GetWide( (const CHAR*)content.GetEnumBlob(), strEnumDataBlob, 64 );

    if( !content.GetPrice().fOfferingIsFree )
    {
        // number of owned instaces
        wsprintfW( strNumInstances, L"%u", content.GetNumInstances());
    
        // price
        DWORD dwBufferSize;
        HRESULT hr;
        dwBufferSize = 64;
        hr = XOnlineOfferingPriceFormat( &content.GetPrice(),
                                         strPrice, &dwBufferSize, 0 );

        assert(SUCCEEDED(hr));
        (VOID)hr; // avoid compiler warnings in release code

        const WCHAR* strTax = NULL;
        switch(content.GetPrice().Tax)
        {
            case NO_TAX: strTax = L", No Tax"; break;
            case DEFAULT: strTax = L", Default"; break;
            case GST: strTax = L", GST"; break;
            case VAT: strTax = L", VAT"; break;
        }

        assert(strTax);
        wcscat(strPrice, strTax);
    }

    if( XONLINE_OFFERING_SUBSCRIPTION == content.GetOfferingType() )
    {
        // duration
        if( content.GetDuration() == 0)
            wsprintfW( strDuration, L"Non-terminating");
        else
            wsprintfW( strDuration, L"%u Months", content.GetDuration() );

        // free months
        wsprintfW( strFreeMonths, L"%u Months", content.GetFreeMonths() );

        // frequency
        switch( content.GetFrequency() )
        {
            case ONE_TIME_CHARGE:
                strFrequency = L"One Time Change";
                break;
            case MONTHLY:
                strFrequency = L"Monthly";
                break;
            case QUARTERLY:
                strFrequency = L"Quarterly";
                break;
            case BIANNUALLY:
                strFrequency = L"Biannually";
                break;
            case ANNUALLY:
                strFrequency = L"Annually";
                break;
            default:
                assert( FALSE );
        }
    }

    // details data blob
    XBUtil_GetWide( (const CHAR*)content.GetDetailsBlob(),
        strDetailsDataBlob, 128 );
    
    // Column 1
    FLOAT fPos = 60;
    m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Offering ID" );
    m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Offering Type" );
    m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Bit Flags" );
    m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Offering Flags" );
    m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Activation Date" );
    m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Rating" );
    if( XONLINE_OFFERING_SUBSCRIPTION != content.GetOfferingType() )
    {
        m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Package Size" );
        m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Install Size" );
    }
    m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Enum Data Blob:" );
    fPos+=20;

    if( !content.GetPrice().fOfferingIsFree )
    {
        m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Owned Instances" );
        m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Price" );
    }
    if( XONLINE_OFFERING_SUBSCRIPTION == content.GetOfferingType() )
    {
        m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Free for first" );
        m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Duration" );
        m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Charge Frequency" );
    }
    m_Font.DrawText( 100, fPos+=20, COLOR_NORMAL, L"Details Data Blob:" );
    fPos+=20;

    // Column 2
    fPos = 60;
    m_Font.DrawText( 280, fPos+=20,  COLOR_GREEN, strOfferingID );
    m_Font.DrawText( 280, fPos+=20, COLOR_GREEN, strOfferingType );
    m_Font.DrawText( 280, fPos+=20, COLOR_GREEN, strBitFlags );
    m_Font.DrawText( 280, fPos+=20, COLOR_GREEN, strOfferingFlags );
    m_Font.DrawText( 280, fPos+=20, COLOR_GREEN, strDate );
    m_Font.DrawText( 280, fPos+=20, COLOR_GREEN, strRating );
    if( XONLINE_OFFERING_SUBSCRIPTION != content.GetOfferingType() )
    {
        m_Font.DrawText( 280, fPos+=20, COLOR_GREEN, strPackageBlocks );
        m_Font.DrawText( 280, fPos+=20, COLOR_GREEN, strInstallBlocks );
    }
    fPos+=20;
    m_Font.DrawText( 320, fPos+=20, COLOR_GREEN, strEnumDataBlob,
                     XBFONT_CENTER_X );

    if( !content.GetPrice().fOfferingIsFree )
    {
        m_Font.DrawText( 280, fPos+=20, COLOR_GREEN, strNumInstances );
        m_Font.DrawText( 280, fPos+=20, COLOR_GREEN, strPrice );
    }
    if( XONLINE_OFFERING_SUBSCRIPTION == content.GetOfferingType() )
    {
        m_Font.DrawText( 280, fPos+=20, COLOR_GREEN, strFreeMonths );
        m_Font.DrawText( 280, fPos+=20, COLOR_GREEN, strDuration );
        m_Font.DrawText( 280, fPos+=20, COLOR_GREEN, strFrequency );

    }
    fPos+=20;
    m_Font.DrawText( 320, fPos+=20, COLOR_GREEN, strDetailsDataBlob,
                     XBFONT_CENTER_X );

    //
    // UI
    //
    if( XONLINE_OFFERING_SUBSCRIPTION == content.GetOfferingType() )
    {
        if(bBillingEnabled)
        {
            if(content.GetNumInstances() == 0)
                m_Font.DrawText( 320, 380, COLOR_NORMAL,
                                 L"Press " GLYPH_A_BUTTON L" to subscribe", XBFONT_CENTER_X );
            else
                m_Font.DrawText( 320, 380, COLOR_NORMAL,
                                 L"Press " GLYPH_A_BUTTON L" to cancel subscription",
                                 XBFONT_CENTER_X );
        }
        // cannot change subscription if purchase permissions are off
        else
        {
             m_Font.DrawText( 320, 380, COLOR_NORMAL,
                              L"Billing permissions disabled for this account.", XBFONT_CENTER_X );
        }

    }
    else if( XONLINE_OFFERING_CONTENT == content.GetOfferingType() )
    {
        // only pay the first time
        if(!content.GetPrice().fOfferingIsFree &&
            content.GetNumInstances() == 0 )
        {
            // cannot purchase if permissions are off
            if(bBillingEnabled)
            {
                m_Font.DrawText( 320, 380, COLOR_NORMAL,
                                 L"Press " GLYPH_A_BUTTON L" to purchase", XBFONT_CENTER_X );
            }
            else
            {
                m_Font.DrawText( 320, 380, COLOR_NORMAL,
                                 L"Billing permissions disabled for this account.", XBFONT_CENTER_X );
            }
        }

        else
            m_Font.DrawText( 320, 380, COLOR_NORMAL,
                             L"Press " GLYPH_A_BUTTON L" to install", XBFONT_CENTER_X );
    }
    else
        assert( FALSE );
    
    m_Font.DrawText( 320, 400, COLOR_NORMAL,
                     L"Press " GLYPH_B_BUTTON L" to return to list", XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderContentMetadata()
// Desc: Display detailed information about selected content
//-----------------------------------------------------------------------------
VOID UserInterface::RenderContentMetadata( ContentInfo& content )
{
    // display metadata

    WCHAR  strOfferingID [64] = L"";
    WCHAR  strBitFlags[ 12 ] = L"";
    
    WCHAR  strContentDirectory[MAX_PATH] = L"";
    WCHAR  strDisplayName[MAX_CONTENT_DISPLAY_NAME] = L"";
    
    wsprintfW( strOfferingID, L"0x%I64X", content.GetId() );

    // bit flags
    wsprintfW( strBitFlags, L"0x%08X", content.GetBitFlags() );

    // install directory
    XBUtil_GetWide( content.GetContentDirectory(),
                    strContentDirectory, MAX_PATH );
    lstrcpynW( strDisplayName, content.GetDisplayName(), MAX_CONTENT_DISPLAY_NAME );    
    
    // date and time information
    WCHAR strDate[32];
    WCHAR strTime[32];
    FILETIME ftDownloadTime = content.GetDownloadDate();
    CXBConfig::FormatDateTime( ftDownloadTime, strDate, strTime );
    
    // install size in blocks
    DWORD dwInstallBlocks = content.GetInstallSize( );
    WCHAR strInstallBlocks[32];
    CXBConfig::FormatInt( dwInstallBlocks, strInstallBlocks );
    wcscat( strInstallBlocks, L" blocks" );
    
    // Column 1
    m_Font.DrawText( 100, 80,  COLOR_NORMAL, L"Offering ID" );
    m_Font.DrawText( 100, 100, COLOR_NORMAL, L"Display Name" );
    m_Font.DrawText( 100, 120, COLOR_NORMAL, L"Bit Flags" );
    m_Font.DrawText( 100, 140, COLOR_NORMAL, L"Download Date" );
    m_Font.DrawText( 100, 160, COLOR_NORMAL, L"ContentDirectory" );
    m_Font.DrawText( 100, 180, COLOR_NORMAL, L"Install Size" );
    
    // Column 2
    m_Font.DrawText( 280, 80,  COLOR_GREEN, strOfferingID );
    m_Font.DrawText( 280, 100, COLOR_GREEN, strDisplayName );
    m_Font.DrawText( 280, 120, COLOR_GREEN, strBitFlags );
    m_Font.DrawText( 280, 140, COLOR_GREEN, strDate );
    m_Font.DrawText( 280, 160, COLOR_GREEN, strContentDirectory );
    m_Font.DrawText( 280, 180, COLOR_GREEN, strInstallBlocks );
        
    // UI
    m_Font.DrawText( 320, 380, COLOR_NORMAL,
                     L"Press " GLYPH_A_BUTTON L" to remove",XBFONT_CENTER_X );
    
    m_Font.DrawText( 320, 400, COLOR_NORMAL,
                     L"Press " GLYPH_B_BUTTON L" to return to list", XBFONT_CENTER_X );


}




//-----------------------------------------------------------------------------
// Name: RenderInstallContent()
// Desc: Display download/installation progress
//-----------------------------------------------------------------------------
VOID UserInterface::RenderInstallContent( FLOAT fPercentComplete, 
                                          DWORD dwBlocksInstalled, 
                                          DWORD dwBlocksTotal )
{
    m_Font.DrawText( 320, 140, COLOR_NORMAL, L"Installing content", 
                     XBFONT_CENTER_X );
    m_Font.DrawText( 320, 200, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel", 
                     XBFONT_CENTER_X );
    
    // Display visual progress indicator
    D3DXVECTOR4 v4Progress = D3DXVECTOR4( 100, 260, 540, 280 );
    RenderProgress( v4Progress, fPercentComplete );
    
    // Format blocks
    WCHAR strBlocksInstalled[32];
    WCHAR strBlocksTotal[32];
    CXBConfig::FormatInt( dwBlocksInstalled, strBlocksInstalled );
    CXBConfig::FormatInt( dwBlocksTotal, strBlocksTotal );
    
    // Display block progress indicator
    WCHAR strProgress[ 128 ];
    wsprintfW( strProgress, L"%.*s of %.*s blocks", 32, strBlocksInstalled,
                                                    32, strBlocksTotal );
    m_Font.DrawText( 320, 320, COLOR_NORMAL, strProgress, XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderConfirm()
// Desc: Display confirmation UI
//-----------------------------------------------------------------------------
VOID UserInterface::RenderConfirm( const WCHAR* strMessage, DWORD dwCurrItem )
{
    m_Font.DrawText( 320, 140, COLOR_NORMAL, strMessage, XBFONT_CENTER_X );
    
    const WCHAR* const strMenu[] =
    {
        L"Yes",
        L"No",
    };
    
    const FLOAT fYtop = 240.0f;
    const FLOAT fYdelta = 50.0f;
    
    for( DWORD i = 0; i < CONFIRM_MAX; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 280, fYtop + (fYdelta * i), dwColor, strMenu[i] );
    }
    
    // Show selected item with little triangle
    m_Font.DrawText( 280, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
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
// Name: RenderSuccess()
// Desc: Display success message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderMessage( WCHAR* strMessage )
{
    m_Font.DrawText( 320, 200, COLOR_NORMAL, strMessage, XBFONT_CENTER_X );
    m_Font.DrawText( 320, 300, COLOR_NORMAL, L"Press " GLYPH_A_BUTTON L" to continue", 
                     XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderMessage()
// Desc: Displays a message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderMessage( const WCHAR* strMessage,
                                   BOOL bCancelable, BOOL bContinue )
{
    m_Font.DrawText( 320, 140, COLOR_NORMAL, strMessage, 
                     XBFONT_CENTER_X );

    if( bCancelable )
        m_Font.DrawText( 320, 220, COLOR_NORMAL, L"Press " GLYPH_B_BUTTON L" to cancel", 
                         XBFONT_CENTER_X );
    else if( bContinue )
        m_Font.DrawText( 320, 220, COLOR_NORMAL, L"Press " GLYPH_A_BUTTON L" to continue", 
                         XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderProgress()
// Desc: Renders a progress bar at the given rect, fPercent full
//-----------------------------------------------------------------------------
VOID UserInterface::RenderProgress( const D3DXVECTOR4& vecBar,
                                    FLOAT fPercentComplete )
{
    // Quad for filled-in section
    D3DXVECTOR4 pBarVertices[4];
    pBarVertices[0] = D3DXVECTOR4( vecBar.x - 0.5f, vecBar.y - 0.5f, 1.0f, 1.0f );
    pBarVertices[1] = D3DXVECTOR4( vecBar.x + fPercentComplete*(vecBar.z - vecBar.x) - 0.5f, vecBar.y - 0.5f, 1.0f, 1.0f );
    pBarVertices[2] = D3DXVECTOR4( vecBar.x + fPercentComplete*(vecBar.z - vecBar.x) - 0.5f, vecBar.w - 0.5f, 1.0f, 1.0f );
    pBarVertices[3] = D3DXVECTOR4( vecBar.x - 0.5f, vecBar.w - 0.5f, 1.0f, 1.0f );
    
    // Line-strip rectangle for border
    D3DXVECTOR4 pLineVertices[4];
    pLineVertices[0] = D3DXVECTOR4( vecBar.x, vecBar.y, 1.0f, 1.0f );
    pLineVertices[1] = D3DXVECTOR4( vecBar.z, vecBar.y, 1.0f, 1.0f );
    pLineVertices[2] = D3DXVECTOR4( vecBar.z, vecBar.w, 1.0f, 1.0f );
    pLineVertices[3] = D3DXVECTOR4( vecBar.x, vecBar.w, 1.0f, 1.0f );
    
    g_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW );
    g_pd3dDevice->SetTexture( 0, NULL );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    
    // First render the filled-in-section
    g_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, COLOR_PROGRESS );
    g_pd3dDevice->DrawVerticesUP( D3DPT_QUADLIST, 4, pBarVertices, sizeof(D3DXVECTOR4) );
    
    // Then render the linestrip border
    g_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, COLOR_BARBORDER );
    g_pd3dDevice->DrawVerticesUP( D3DPT_LINELOOP, 4, pLineVertices, sizeof(D3DXVECTOR4) );
}




