//-----------------------------------------------------------------------------
// File: UserInterface.cpp
//
// Desc: QualityOfService rendering functions
//
// Hist: 05.24.02 - New for June release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "UserInterface.h"
#include "XBApp.h"
#include <cassert>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_1, L"Display\nhelp" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_1, L"Select menu\nitem" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_1, L"Cancel" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_1, L"Menu navigation" },
};

#define NUM_HELP_CALLOUTS 4




//-----------------------------------------------------------------------------
// Name: UserInterface()
// Desc: Constructor
//-----------------------------------------------------------------------------
UserInterface::UserInterface()
{
    m_UI.SetHeader( L"Quality of Service" );
    m_strSessionName = NULL;
}




//-----------------------------------------------------------------------------
// Name: SetErrorStr()
// Desc: Set error string
//-----------------------------------------------------------------------------
VOID __cdecl UserInterface::SetErrorStr( const WCHAR* strFormat, ... )
{
    va_list pArgList;
    va_start( pArgList, strFormat );

    m_UI.SetErrorStr( strFormat, pArgList );

    va_end( pArgList );
}




//-----------------------------------------------------------------------------
// Name: SetSessionName()
// Desc: Set session name
//-----------------------------------------------------------------------------
VOID UserInterface::SetSessionName( const WCHAR* strSessionName )
{
    m_strSessionName = strSessionName;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects
//-----------------------------------------------------------------------------
HRESULT UserInterface::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return E_FAIL;

    // Create the help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return E_FAIL;

    return m_UI.Initialize();
}




//-----------------------------------------------------------------------------
// Name: RenderMode()
// Desc: Display SysLink/Online selection
//-----------------------------------------------------------------------------
VOID UserInterface::RenderMode( DWORD dwCurrItem )
{
    m_UI.RenderHeader();
    m_Font.DrawText( 300, 50, COLOR_NORMAL, m_strSessionName );

    m_Font.DrawText( 320, 140, COLOR_NORMAL, L"Choose Network Mode",
                     XBFONT_CENTER_X );

    const WCHAR* const strMatch[] =
    {
        L"System Link",
        L"Online"
    };

    const FLOAT fYtop = 200.0f;
    const FLOAT fYdelta = 50.0f;

    for( DWORD i = 0; i < MENU_MODE_MAX; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 260, fYtop + (fYdelta * i), dwColor, strMatch[i] );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 260, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );

    m_Font.DrawText( 320, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help", XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderListenParams()
// Desc: Display QoS listener parameters
//-----------------------------------------------------------------------------
VOID UserInterface::RenderListenParams( DWORD dwCurrItem, BOOL bListen, 
                                        DWORD dwSamples, DWORD dwListenBandwidth, 
                                        DWORD dwLookupBandwidth, BOOL bIsOnline )
{
    m_UI.RenderHeader();
    m_Font.DrawText( 300, 50, COLOR_NORMAL, m_strSessionName );

    WCHAR strTitle[ 64 ];
    wsprintfW( strTitle, L"QoS Parameters (%s)", bIsOnline ? L"Online" : L"System Link" );
    m_Font.DrawText( 320, 100, COLOR_NORMAL, strTitle, XBFONT_CENTER_X );

    const WCHAR* const strSetting[] =
    {
        L"Start QoS Lookups",
        L"Listen for Probes:",
        L"Probe Samples:",
        L"Listen Bandwidth:",
        L"Lookup Bandwidth:"
    };

    FLOAT fYtop = 160.0f;
    const FLOAT fYdelta = 40.0f;

    for( DWORD i = 0; i < MENU_LISTEN_PARAMS_MAX; ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 140, fYtop + (fYdelta * i), dwColor, strSetting[i] );

        FLOAT fCol1 = 350.0f;
        FLOAT fCol2 = 460.0f;
        FLOAT fWid  = 100.0f; // slider width

        switch( i )
        {
        case MENU_SET_LISTEN_STATE:
            m_Font.DrawText( fCol1, fYtop + (fYdelta * i), dwColor,
                             bListen ? L"On" : L"Off" );
            break;
        case MENU_NUM_SAMPLES:
        {
            if( dwSamples == 0 )
                m_Font.DrawText( fCol1, fYtop + (fYdelta * i), dwColor, 
                                 L"0 (Test connectivity only)" );
            else
            {
                RenderSliderBar( i, fCol1, fWid, fYtop, fYdelta, (FLOAT)dwSamples / 
                                                                 (FLOAT)MAX_SAMPLES );
                WCHAR strSamples[ 64 ];
                wsprintfW( strSamples, L"%lu", dwSamples );
                m_Font.DrawText( fCol2, fYtop + (fYdelta * i), dwColor, strSamples );
            }
            break;
        }
        case MENU_LISTEN_BANDWIDTH:
        {
            RenderSliderBar( i, fCol1, fWid, fYtop, fYdelta, (FLOAT)dwListenBandwidth / 
                                                             (FLOAT)LISTEN_BANDWIDTH_MAX );
            WCHAR strListenBandwidth[ 64 ];
            wsprintfW( strListenBandwidth, L"%lu Kb/s", dwListenBandwidth / 1000 );
            m_Font.DrawText( fCol2, fYtop + (fYdelta * i), dwColor, strListenBandwidth );
            break;
        }
        case MENU_LOOKUP_BANDWIDTH:
        {
            RenderSliderBar( i, fCol1, fWid, fYtop, fYdelta, (FLOAT)dwLookupBandwidth / 
                                                             (FLOAT)LOOKUP_BANDWIDTH_MAX );
            WCHAR strLookupBandwidth[ 64 ];
            wsprintfW( strLookupBandwidth, L"%lu Kb/s", dwLookupBandwidth / 1000 );
            m_Font.DrawText( fCol2, fYtop + (fYdelta * i), dwColor, strLookupBandwidth );
            break;
        }
        default:
            break;
        }
    }

    // Show selected item with little triangle
    m_Font.DrawText( 140, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );

    m_Font.DrawText( 320, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help", XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderSliderBar()
// Desc: Display slider bar
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSliderBar( DWORD i, FLOAT fX, FLOAT fWidth, FLOAT fYtop, 
                                     FLOAT fYdelta, FLOAT fPercent )
{
    struct BACKGROUNDVERTEX
    { 
        D3DXVECTOR4 p;
        D3DCOLOR color;
    };
    BACKGROUNDVERTEX v[4];

    FLOAT x1 = fX;
    FLOAT x2 = x1 + (100.0f * fPercent);
    FLOAT y1 = fYtop + (fYdelta * i);
    FLOAT y2 = y1 + 20.0f;
    v[0].p = D3DXVECTOR4( x1 - 0.5f, y1 - 0.5f, 1.0f, 1.0f );  v[0].color = 0xffffffff;
    v[1].p = D3DXVECTOR4( x2 - 0.5f, y1 - 0.5f, 1.0f, 1.0f );  v[1].color = 0xffffffff;
    v[2].p = D3DXVECTOR4( x1 - 0.5f, y2 - 0.5f, 1.0f, 1.0f );  v[2].color = 0xff00ff00;
    v[3].p = D3DXVECTOR4( x2 - 0.5f, y2 - 0.5f, 1.0f, 1.0f );  v[3].color = 0xff00ff00;

    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    g_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );
    g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]) );
}




//-----------------------------------------------------------------------------
// Name: RenderSysLinkSearch()
// Desc: Display System Link session search screen
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSysLinkSearch()
{
    m_UI.RenderHeader();
    m_Font.DrawText( 300, 50, COLOR_NORMAL, m_strSessionName );
    m_Font.DrawText( 320, 240, COLOR_NORMAL, 
                     L"Searching for System Link sessions",
                     XBFONT_CENTER_X | XBFONT_CENTER_Y );
}




//-----------------------------------------------------------------------------
// Name: RenderSessionList()
// Desc: Display probes
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSessionList( DWORD dwCurrItem, 
                                       const XNADDR& xnTitleAddr,
                                       const SessionList& sessionList,
                                       const XNQOS* pxnQos,
                                       const XNQOS* pxnServiceQos,
                                       DWORD dwSamples, BOOL bIsOnline )
{
    m_UI.RenderHeader();
    m_Font.DrawText( 300, 50, COLOR_NORMAL, m_strSessionName );

    WCHAR strTitle[ 64 ];
    wsprintfW( strTitle, L"QoS Results (%s)", bIsOnline ? L"Online" : L"System Link" );
    m_Font.DrawText( 320, 100, COLOR_NORMAL, strTitle, XBFONT_CENTER_X );

    FLOAT fYtop = 160.0f;
    const FLOAT fYdelta = 35.0f;

    DWORD dwItems = pxnQos->cxnqos;
    if( pxnServiceQos && bIsOnline )
        dwItems += pxnServiceQos->cxnqos;

    for( DWORD i = 0; i < dwItems; ++i )
    {
        const XNQOSINFO* pQosInfo = ( i < pxnQos->cxnqos ) ?
                                    pxnQos->axnqosinfo + i :
                                    pxnServiceQos->axnqosinfo;

        // State
        BOOL bComplete  = ( pQosInfo->bFlags & XNET_XNQOSINFO_COMPLETE ) != 0;
        BOOL bContacted = ( pQosInfo->bFlags & XNET_XNQOSINFO_TARGET_CONTACTED ) != 0;
        BOOL bDisabled  = ( pQosInfo->bFlags & XNET_XNQOSINFO_TARGET_DISABLED ) != 0;
        BOOL bHaveData  = ( pQosInfo->bFlags & XNET_XNQOSINFO_DATA_RECEIVED ) != 0;

        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;

        // Xbox console XNADDR
        XNADDR xnAddr;
        ZeroMemory( &xnAddr, sizeof(xnAddr) );
        if( i < sessionList.size() )
            xnAddr = sessionList.GetXnAddr( i );

        // Name of the target
        WCHAR strSessionName[ 64 ];
        if( !bHaveData )
        {
            // Xbox console sessions
            if( i < sessionList.size() )
            {
                // Use the XNADDR.ina of the Xbox console
                SOCKADDR_IN addr;
                addr.sin_family = AF_INET;
                addr.sin_addr   = xnAddr.ina;
                addr.sin_port   = htons( 0 );
                wsprintfW( strSessionName, L"%d.%d.%d.%d", 
                                           addr.sin_addr.S_un.S_un_b.s_b1,
                                           addr.sin_addr.S_un.S_un_b.s_b2,
                                           addr.sin_addr.S_un.S_un_b.s_b3,
                                           addr.sin_addr.S_un.S_un_b.s_b4 );
            }
            
            // XNetQosLookup Security Gateway(s)
            else if( i < pxnQos->cxnqos )
            {
                lstrcpyW( strSessionName, L"SG (legacy)" );
            }
            
            // XNetQosServiceLookup Security Gateway(s)
            else
            {
                lstrcpyW( strSessionName, L"SG (preferred)" );
            }
        }   
        else
        {
            // The only data used by this sample is the session "name"
            assert( pQosInfo->cbData != 0 );
            assert( pQosInfo->pbData != NULL );
            CopyMemory( strSessionName, pQosInfo->pbData, pQosInfo->cbData );
            
            // Identify ourself in the list
            if( memcmp( &xnAddr.ina, &xnTitleAddr.ina, sizeof(xnAddr.ina) ) == 0 &&
                memcmp( &xnAddr.abEnet, &xnTitleAddr.abEnet, sizeof( xnAddr.abEnet) ) == 0 )
                lstrcatW( strSessionName, L" (self)" );
        }
        m_Font.DrawText( 110, fYtop + (fYdelta * i), dwColor, strSessionName );

        // State of the target
        WCHAR strState[ 48 ];
        if( bComplete )
            lstrcpyW( strState, L"Complete" );
        else if( bContacted )
            lstrcpyW( strState, L"Contacted" );
        else
            lstrcpyW( strState, L"In Progress" );

        m_Font.DrawText( 270, fYtop + (fYdelta * i), dwColor, strState );

        // Avg round-trip time
        if( bComplete )
        {
            if( dwSamples == 0 )
            {
                m_Font.DrawText( 390, fYtop + (fYdelta * i), dwColor, L"Connected" );
            }
            else
            {
                WCHAR strRTT[ 32 ];
                if( bDisabled )
                    lstrcpyW( strRTT, L"Not listening" );
                else if( pQosInfo->cProbesRecv )
                    wsprintfW( strRTT, L"%hu ms", pQosInfo->wRttMedInMsecs );
                else
                    lstrcpyW( strRTT, L"Not available" );
                m_Font.DrawText( 390, fYtop + (fYdelta * i), dwColor, strRTT );
            }
        }
        else
        {
            // Progress of the probe
            RenderSliderBar( i, 390, 70, fYtop, fYdelta, (FLOAT)pQosInfo->cProbesRecv /
                                                         (FLOAT)dwSamples );
        }
    }

    // Show selected item with little triangle
    m_Font.DrawText( 110, fYtop + (fYdelta * dwCurrItem ), 0xff00ff00,
                     GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    m_Font.DrawText( 320, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help", XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderProbeData()
// Desc: Display detailed probe data
//-----------------------------------------------------------------------------
VOID UserInterface::RenderProbeData( DWORD dwCurrItem, const WCHAR* strSession,
                                     const XNADDR& xnTitleAddr,
                                     const SessionList& sessionList, 
                                     const XNQOS* pxnQos, 
                                     const XNQOS* pxnServiceQos, BOOL bIsOnline )
{
    const XNQOSINFO* pQosInfo = ( dwCurrItem < pxnQos->cxnqos ) ? 
                                  pxnQos->axnqosinfo + dwCurrItem :
                                  pxnServiceQos->axnqosinfo;

    BOOL bContacted = ( pQosInfo->bFlags & XNET_XNQOSINFO_TARGET_CONTACTED ) != 0;
    BOOL bDisabled  = ( pQosInfo->bFlags & XNET_XNQOSINFO_TARGET_DISABLED ) != 0;
    BOOL bHaveData  = ( pQosInfo->bFlags & XNET_XNQOSINFO_DATA_RECEIVED ) != 0;

    // Xbox console XNADDR
    XNADDR xnAddr;
    ZeroMemory( &xnAddr, sizeof(xnAddr) );
    if( dwCurrItem < sessionList.size() )
        xnAddr = sessionList.GetXnAddr( dwCurrItem );

    // Name of the target
    WCHAR strSessionName[ 64 ];
    if( !bHaveData )
    {
        if( dwCurrItem < sessionList.size() )
        {
            // Use the XNADDR.ina of the Xbox console
            SOCKADDR_IN addr;
            addr.sin_family = AF_INET;
            addr.sin_addr   = xnAddr.ina;
            addr.sin_port   = htons( 0 );
            wsprintfW( strSessionName, L"%d.%d.%d.%d", 
                                        addr.sin_addr.S_un.S_un_b.s_b1,
                                        addr.sin_addr.S_un.S_un_b.s_b2,
                                        addr.sin_addr.S_un.S_un_b.s_b3,
                                        addr.sin_addr.S_un.S_un_b.s_b4 );
        }
        else
        {
            // We have a Security Gateway
            lstrcpyW( strSessionName, L"Security Gateway" ); 
        }
    }   
    else
    {
        // The only data used by this sample is the session "name"
        assert( pQosInfo->cbData != 0 );
        assert( pQosInfo->pbData != NULL );
        CopyMemory( strSessionName, pQosInfo->pbData, pQosInfo->cbData );

        // Identify ourself in the list
        if( memcmp( &xnAddr.ina, &xnTitleAddr.ina, sizeof(xnAddr.ina) ) == 0 &&
            memcmp( &xnAddr.abEnet, &xnTitleAddr.abEnet, sizeof( xnAddr.abEnet) ) == 0 )
            lstrcatW( strSessionName, L" (self)" );
    }

    // State of the target
    WCHAR strState[ 32 ];
    if( bDisabled )
        lstrcpyW( strState, L"Not Listening" );
    else
        lstrcpyW( strState, bContacted ? L"Contacted" : L"Not Contacted" );

    // Probes transmitted
    WCHAR strProbesSent[ 32 ];
    wsprintfW( strProbesSent, L"%hu", pQosInfo->cProbesXmit );

    // Probes received
    WCHAR strProbesRecv[ 32 ];
    wsprintfW( strProbesRecv, L"%hu", pQosInfo->cProbesRecv );

    // Min round-trip time
    WCHAR strMinRoundTrip[ 32 ];
    if( pQosInfo->cProbesRecv )
        wsprintfW( strMinRoundTrip, L"%hu ms", pQosInfo->wRttMinInMsecs );
    else
        lstrcpyW( strMinRoundTrip, L"Not available" );

    // Median round-trip time
    WCHAR strMedRoundTrip[ 32 ];
    if( pQosInfo->cProbesRecv )
        wsprintfW( strMedRoundTrip, L"%hu ms", pQosInfo->wRttMedInMsecs );
    else
        lstrcpyW( strMedRoundTrip, L"Not available" );
        
    // NOTE: Resulting bandwidth values are only rough estimates, and are not
    // always accurate indicators of true bandwidth conditions. They are only
    // useful in comparing relative bandwidths of various consoles, and should
    // not be diplayed or used to set player maximums or other similar game 
    // restrictions.

    // Avg upstream bandwidth
    WCHAR strAvgUpstream[ 32 ];
    if( !bContacted || bDisabled )
        lstrcpyW( strAvgUpstream, L"Not available" );
    else
        wsprintfW( strAvgUpstream, L"%.0f Kbits/s (est.)",
                   (DOUBLE)pQosInfo->dwUpBitsPerSec / 1000.0 );

    // Avg downstream bandwidth
    WCHAR strAvgDownstream[ 32 ];
    if( !bContacted || bDisabled )
        lstrcpyW( strAvgDownstream, L"Not available" );
    else
        wsprintfW( strAvgDownstream, L"%.0f Kbits/s (est.)", 
                   (DOUBLE)pQosInfo->dwDnBitsPerSec / 1000.0 );

    // Draw
    m_UI.RenderHeader();
    m_Font.DrawText( 300, 50, COLOR_NORMAL, m_strSessionName );

    WCHAR strTitle[ 64 ];
    wsprintfW( strTitle, L"QoS Detailed Results (%s)", bIsOnline ? L"Online" : L"System Link" );
    m_Font.DrawText( 320, 100, COLOR_NORMAL, strTitle, XBFONT_CENTER_X );

    // First column
    m_Font.DrawText( 120, 160, COLOR_NORMAL, L"Session Name:" );
    m_Font.DrawText( 120, 190, COLOR_NORMAL, L"State:" );
    m_Font.DrawText( 120, 220, COLOR_NORMAL, L"Probes Sent:" );
    m_Font.DrawText( 120, 250, COLOR_NORMAL, L"Probes Received:" );
    m_Font.DrawText( 120, 280, COLOR_NORMAL, L"Min Round Trip:" );
    m_Font.DrawText( 120, 310, COLOR_NORMAL, L"Median Round Trip:" );
    m_Font.DrawText( 120, 340, COLOR_NORMAL, L"Avg Upstream:" );
    m_Font.DrawText( 120, 370, COLOR_NORMAL, L"Avg Downstream:" );

    // Second column
    m_Font.DrawText( 320, 160, COLOR_HIGHLIGHT, strSessionName );
    m_Font.DrawText( 320, 190, COLOR_HIGHLIGHT, strState );
    m_Font.DrawText( 320, 220, COLOR_HIGHLIGHT, strProbesSent );
    m_Font.DrawText( 320, 250, COLOR_HIGHLIGHT, strProbesRecv );
    m_Font.DrawText( 320, 280, COLOR_HIGHLIGHT, strMinRoundTrip );
    m_Font.DrawText( 320, 310, COLOR_HIGHLIGHT, strMedRoundTrip );
    m_Font.DrawText( 320, 340, COLOR_HIGHLIGHT, strAvgUpstream );
    m_Font.DrawText( 320, 370, COLOR_HIGHLIGHT, strAvgDownstream );

    m_Font.DrawText( 320, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help", XBFONT_CENTER_X );
}



//-----------------------------------------------------------------------------
// Name: RenderCreateAccount()
// Desc: Allow player to launch account creation tool
//-----------------------------------------------------------------------------
VOID UserInterface::RenderCreateAccount( BOOL bHasMachineAccount )
{
    m_UI.RenderCreateAccount( bHasMachineAccount );
    m_Font.DrawText( 320, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help", XBFONT_CENTER_X );

}




//-----------------------------------------------------------------------------
// Name: RenderSelectAccount()
// Desc: Display list of accounts
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSelectAccount( DWORD dwCurrItem, 
                                         XBUserList& UserList )
{
    m_UI.RenderSelectAccount( dwCurrItem, UserList );
    m_Font.DrawText( 320, 410, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help", XBFONT_CENTER_X );

}




//-----------------------------------------------------------------------------
// Name: RenderSigningIn()
// Desc: Display login message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderSigningIn()
{
    m_UI.RenderLoggingOn();
}




//-----------------------------------------------------------------------------
// Name: RenderOnlineCreate()
// Desc: Creating online session
//-----------------------------------------------------------------------------
VOID UserInterface::RenderOnlineCreate()
{
    m_UI.RenderHeader();
    m_Font.DrawText( 300, 50, COLOR_NORMAL, m_strSessionName );
    m_Font.DrawText( 320, 240, COLOR_NORMAL, 
                     L"Creating session on Matchmaking service",
                     XBFONT_CENTER_X | XBFONT_CENTER_Y );
}




//-----------------------------------------------------------------------------
// Name: RenderOnlineSearch()
// Desc: Asking matchmaking for session list
//-----------------------------------------------------------------------------
VOID UserInterface::RenderOnlineSearch()
{
    m_UI.RenderHeader();
    m_Font.DrawText( 300, 50, COLOR_NORMAL, m_strSessionName );
    m_Font.DrawText( 320, 240, COLOR_NORMAL, 
                     L"Searching for Online sessions",
                     XBFONT_CENTER_X | XBFONT_CENTER_Y );
}




//-----------------------------------------------------------------------------
// Name: RenderError()
// Desc: Display error message
//-----------------------------------------------------------------------------
VOID UserInterface::RenderError()
{
    m_UI.RenderError();
}




//-----------------------------------------------------------------------------
// Name: RenderHelp()
// Desc: Display help
//-----------------------------------------------------------------------------
VOID UserInterface::RenderHelp() 
{
    m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
}

