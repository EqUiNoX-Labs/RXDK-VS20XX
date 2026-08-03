//-----------------------------------------------------------------------------
// File: XbOnline.cpp
//
// Desc: Shortcut macros and helper functions for the Xbox online samples.
//       Requires linking with XONLINE[D][S].LIB.
//
// Hist: 10.11.01 - New for November XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "XBOnline.h"
#include "XBMemUnit.h"
#include "XBFont.h"
#include "XBNet.h"
#include <cassert>
#include <algorithm>




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD MAX_MEMORY_UNITS  = 2 * XGetPortCount();

// How often (per second) the caret blinks during PIN entry
const FLOAT fCARET_BLINK_RATE = 1.0f;

// During the blink period, the amount the caret is visible. 0.5 equals
// half the time, 0.75 equals 3/4ths of the time, etc.
const FLOAT fCARET_ON_RATIO = 0.75f;




//-----------------------------------------------------------------------------
// Name: XBNet_OnlineInit()
// Desc: Performs all required initialization for XOnline and network APIs
//-----------------------------------------------------------------------------
HRESULT XBNet_OnlineInit( BYTE cfgFlags, PXONLINE_STARTUP_PARAMS pxosp )
{
    XONLINE_STARTUP_PARAMS xosp = { 0 };
    HRESULT hr;

    if( pxosp == NULL )
        pxosp = &xosp;

    if( FAILED( hr = XBNet_Init( cfgFlags ) ) )
        return hr;

    if( FAILED( hr = XOnlineStartup( pxosp ) ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: XBNet_OnlineCleanup()
// Desc: Performs all required cleanup for online APIs
//-----------------------------------------------------------------------------
HRESULT XBNet_OnlineCleanup()
{
    HRESULT hr;

    if( FAILED( hr = XOnlineCleanup() ) )
        return hr;

    if( FAILED( hr = XBNet_Cleanup() ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: XBOnline_GetUserList()
// Desc: Extract all accounts from hard disk and MUs
//-----------------------------------------------------------------------------
HRESULT XBOnline_GetUserList( XONLINE_USER* pUserList, DWORD* pdwNumUsers )
{
    // Wait for any inserted MUs to mount
    while( XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY )
        Sleep(10);

    // Get accounts stored on the hard disk and MUs
    return XOnlineGetUsers( pUserList, pdwNumUsers );
}




//-----------------------------------------------------------------------------
// Name: XBOnline_GetUserList()
// Desc: Extract all accounts from hard disk and MUs
//-----------------------------------------------------------------------------
HRESULT XBOnline_GetUserList( XBUserList& UserList )
{
    HRESULT hr;

    // On input, the list must have room for XONLINE_MAX_STORED_ONLINE_USERS
    // accounts
    XONLINE_USER pUserList[ XONLINE_MAX_STORED_ONLINE_USERS ];

    // Wait for any inserted MUs to mount
    while( XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY );

    // Get accounts stored on the hard disk
    DWORD dwUsers;
    if( FAILED( hr = XOnlineGetUsers( pUserList, &dwUsers ) ) )
        return hr;

    for( DWORD i = 0; i < dwUsers; ++i )
        UserList.push_back( pUserList[i] );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CXBOnlineUI()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBOnlineUI::CXBOnlineUI()
{
    m_strHeader    = L"";
    m_strError[0]  = 0;
}




//-----------------------------------------------------------------------------
// Name: SetErrorStr()
// Desc: Set error string
//-----------------------------------------------------------------------------
VOID CXBOnlineUI::SetErrorStr( const WCHAR* strFormat, va_list pArglist )
{
    INT iChars = wvsprintfW( m_strError, strFormat, pArglist );
    assert( iChars < MAX_ERROR_STR );
    (VOID)iChars; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects
//-----------------------------------------------------------------------------
HRESULT CXBOnlineUI::Initialize()
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

    // Set projection transform
    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 640.0f/480.0f, 0.1f, 100.0f );
    D3DDevice::SetTransform( D3DTS_PROJECTION, &matProj );

    // Set view position
    D3DXMATRIX matView;
    D3DXMatrixTranslation( &matView, 0.0f, 0.0f, 40.0f);
    D3DDevice::SetTransform( D3DTS_VIEW, &matView );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderCreateAccount()
// Desc: Allow player to launch account creation tool
//-----------------------------------------------------------------------------
VOID CXBOnlineUI::RenderCreateAccount( BOOL bHasMachineAccount ) const
{
    RenderHeader();

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
VOID CXBOnlineUI::RenderSelectAccount( DWORD dwCurrItem, 
                                       const XBUserList& UserList ) const
{
    assert( !UserList.empty() );

    RenderHeader();

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
// Name: RenderLoggingOn()
// Desc: Display "logging on" animation
//-----------------------------------------------------------------------------
VOID CXBOnlineUI::RenderLoggingOn( const XONLINE_USER* pUsers ) const
{
    RenderHeader();
    
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
// Name: RenderError()
// Desc: Display error (or any other) message
//-----------------------------------------------------------------------------
VOID CXBOnlineUI::RenderError( BOOL bBootToDash ) const
{
    RenderHeader();
    m_Font.DrawText( 320, 200, COLOR_NORMAL, m_strError, XBFONT_CENTER_X );
    m_Font.DrawText( 320, 300, COLOR_NORMAL, L"Press " GLYPH_A_BUTTON L" to continue", 
                     XBFONT_CENTER_X );
    
    if( bBootToDash )
        m_Font.DrawText( 320, 360, COLOR_NORMAL, 
                         L"Press " GLYPH_X_BUTTON L" to boot to the Xbox Dashboard",
                         XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderHeader()
// Desc: Display standard text
//-----------------------------------------------------------------------------
VOID CXBOnlineUI::RenderHeader() const
{
    m_Font.DrawText( 64, 50, COLOR_NORMAL, m_strHeader );
}




//-----------------------------------------------------------------------------
// Name: RenderOnlineNotificationIcon()
// Desc: Render an online notification icon
//-----------------------------------------------------------------------------
VOID CXBOnlineUI::RenderOnlineNotificationIcon( FLOAT fLeft, FLOAT fTop, 
                                                ONLINEICON Icon ) const
{
    XBOnline_RenderOnlineNotificationIcon( &m_OnlineIconsFont, 
                                           fLeft, fTop, Icon );
}




//-----------------------------------------------------------------------------
// Name: XBOnline_RenderOnlineNotificationIcon()
// Desc: Render an online notification icon
//-----------------------------------------------------------------------------
VOID XBOnline_RenderOnlineNotificationIcon( CXBFont* pOnlineIconsFont, 
                                            FLOAT fLeft, FLOAT fTop, 
                                            ONLINEICON Icon )
{
    WCHAR strIcon[2] = { Icon, 0 };
    pOnlineIconsFont->DrawText( fLeft, fTop, 0xffffffff, strIcon );
}



//-----------------------------------------------------------------------------
// Name: XBOnline_GetOnlineHResultString()
// Desc: Display and online error HRESULT
//-----------------------------------------------------------------------------
const WCHAR* XBOnline_GetOnlineHResultString( HRESULT hr )
{
    switch( hr )
    {
        case XONLINE_E_OVERFLOW:
            return L"XONLINE_E_OVERFLOW";
        case XONLINE_E_NO_SESSION:
            return L"XONLINE_E_NO_SESSION";
        case XONLINE_E_USER_NOT_LOGGED_ON:
            return L"XONLINE_E_USER_NOT_LOGGED_ON";
        case XONLINE_E_NO_GUEST_ACCESS:
            return L"XONLINE_E_NO_GUEST_ACCESS";
        case XONLINE_E_NOT_INITIALIZED:
            return L"XONLINE_E_NOT_INITIALIZED";
        case XONLINE_E_NO_USER:
            return L"XONLINE_E_NO_USER";
        case XONLINE_E_INTERNAL_ERROR:
            return L"XONLINE_E_INTERNAL_ERROR";
        case XONLINE_E_OUT_OF_MEMORY:
            return L"XONLINE_E_OUT_OF_MEMORY";
        case XONLINE_E_TASK_BUSY:
            return L"XONLINE_E_TASK_BUSY";
        case XONLINE_E_SERVER_ERROR:
            return L"XONLINE_E_SERVER_ERROR";
        case XONLINE_E_IO_ERROR:
            return L"XONLINE_E_IO_ERROR";
        case XONLINE_E_BAD_CONTENT_TYPE:
            return L"XONLINE_E_BAD_CONTENT_TYPE";
        case XONLINE_E_USER_NOT_PRESENT:
            return L"XONLINE_E_USER_NOT_PRESENT";
        case XONLINE_E_PROTOCOL_MISMATCH:
            return L"XONLINE_E_PROTOCOL_MISMATCH";
        case XONLINE_E_LOGON_NO_NETWORK_CONNECTION:
            return L"XONLINE_E_LOGON_NO_NETWORK_CONNECTION";
        case XONLINE_S_LOGON_CONNECTION_ESTABLISHED:
            return L"XONLINE_S_LOGON_CONNECTION_ESTABLISHED";
        case XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE:
            return L"XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE";
        case XONLINE_E_LOGON_UPDATE_REQUIRED:
            return L"XONLINE_E_LOGON_UPDATE_REQUIRED";
        case XONLINE_E_LOGON_SERVERS_TOO_BUSY:
            return L"XONLINE_E_LOGON_SERVERS_TOO_BUSY";
        case XONLINE_E_LOGON_CONNECTION_LOST:
            return L"XONLINE_E_LOGON_CONNECTION_LOST";
        case XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON:
            return L"XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON";
        case XONLINE_E_LOGON_CHANGE_USER_FAILED:
            return L"XONLINE_E_LOGON_CHANGE_USER_FAILED";
        case XONLINE_E_LOGON_SERVICE_NOT_REQUESTED:
            return L"XONLINE_E_LOGON_SERVICE_NOT_REQUESTED";
        case XONLINE_E_LOGON_SERVICE_NOT_AUTHORIZED:
            return L"XONLINE_E_LOGON_SERVICE_NOT_AUTHORIZED";
        case XONLINE_E_LOGON_SERVICE_TEMPORARILY_UNAVAILABLE:
            return L"XONLINE_E_LOGON_SERVICE_TEMPORARILY_UNAVAILABLE";
        case XONLINE_S_LOGON_USER_HAS_MESSAGE:
            return L"XONLINE_S_LOGON_USER_HAS_MESSAGE";
        case XONLINE_E_LOGON_USER_ACCOUNT_REQUIRES_MANAGEMENT:
            return L"XONLINE_E_LOGON_USER_ACCOUNT_REQUIRES_MANAGEMENT";
        case XONLINE_E_LOGON_MU_NOT_MOUNTED:
            return L"XONLINE_E_LOGON_MU_NOT_MOUNTED";
        case XONLINE_E_LOGON_MU_IO_ERROR:
            return L"XONLINE_E_LOGON_MU_IO_ERROR";
        case XONLINE_E_LOGON_NOT_LOGGED_ON:
            return L"XONLINE_E_LOGON_NOT_LOGGED_ON";
        case XONLINE_E_NOTIFICATION_BAD_CONTENT_TYPE:
            return L"XONLINE_E_NOTIFICATION_BAD_CONTENT_TYPE";
        case XONLINE_E_NOTIFICATION_REQUEST_TOO_SMALL:
            return L"XONLINE_E_NOTIFICATION_REQUEST_TOO_SMALL";
        case XONLINE_E_NOTIFICATION_INVALID_MESSAGE_TYPE:
            return L"XONLINE_E_NOTIFICATION_INVALID_MESSAGE_TYPE";
        case XONLINE_E_NOTIFICATION_NO_ADDRESS:
            return L"XONLINE_E_NOTIFICATION_NO_ADDRESS";
        case XONLINE_E_NOTIFICATION_INVALID_PUID:
            return L"XONLINE_E_NOTIFICATION_INVALID_PUID";
        case XONLINE_E_NOTIFICATION_NO_CONNECTION:
            return L"XONLINE_E_NOTIFICATION_NO_CONNECTION";
        case XONLINE_E_NOTIFICATION_SEND_FAILED:
            return L"XONLINE_E_NOTIFICATION_SEND_FAILED";
        case XONLINE_E_NOTIFICATION_RECV_FAILED:
            return L"XONLINE_E_NOTIFICATION_RECV_FAILED";
        case XONLINE_E_NOTIFICATION_MESSAGE_TRUNCATED:
            return L"XONLINE_E_NOTIFICATION_MESSAGE_TRUNCATED";
        case XONLINE_E_NOTIFICATION_SERVER_BUSY:
            return L"XONLINE_E_NOTIFICATION_SERVER_BUSY";
        case XONLINE_E_NOTIFICATION_LIST_FULL:
            return L"XONLINE_E_NOTIFICATION_LIST_FULL";
        case XONLINE_E_NOTIFICATION_BLOCKED:
            return L"XONLINE_E_NOTIFICATION_BLOCKED";
        case XONLINE_E_NOTIFICATION_FRIEND_PENDING:
            return L"XONLINE_E_NOTIFICATION_FRIEND_PENDING";
        case XONLINE_E_NOTIFICATION_FLUSH_TICKETS:
            return L"XONLINE_E_NOTIFICATION_FLUSH_TICKETS";
        case XONLINE_E_NOTIFICATION_TOO_MANY_REQUESTS:
            return L"XONLINE_E_NOTIFICATION_TOO_MANY_REQUESTS";
        case XONLINE_E_MATCH_INVALID_SESSION_ID:
            return L"XONLINE_E_MATCH_INVALID_SESSION_ID";
        case XONLINE_E_MATCH_INVALID_TITLE_ID:
            return L"XONLINE_E_MATCH_INVALID_TITLE_ID";
        case XONLINE_E_MATCH_INVALID_DATA_TYPE:
            return L"XONLINE_E_MATCH_INVALID_DATA_TYPE";
        case XONLINE_E_MATCH_REQUEST_TOO_SMALL:
            return L"XONLINE_E_MATCH_REQUEST_TOO_SMALL";
        case XONLINE_E_MATCH_REQUEST_TRUNCATED:
            return L"XONLINE_E_MATCH_REQUEST_TRUNCATED";
        case XONLINE_E_MATCH_INVALID_SEARCH_REQ:
            return L"XONLINE_E_MATCH_INVALID_SEARCH_REQ";
        case XONLINE_E_MATCH_INVALID_OFFSET:
            return L"XONLINE_E_MATCH_INVALID_OFFSET";
        case XONLINE_E_MATCH_INVALID_ATTR_TYPE:
            return L"XONLINE_E_MATCH_INVALID_ATTR_TYPE";
        case XONLINE_E_MATCH_INVALID_VERSION:
            return L"XONLINE_E_MATCH_INVALID_VERSION";
        case XONLINE_E_MATCH_OVERFLOW:
            return L"XONLINE_E_MATCH_OVERFLOW";
        case XONLINE_E_MATCH_INVALID_RESULT_COL:
            return L"XONLINE_E_MATCH_INVALID_RESULT_COL";
        case XONLINE_E_MATCH_INVALID_STRING:
            return L"XONLINE_E_MATCH_INVALID_STRING";
        case XONLINE_E_MATCH_STRING_TOO_LONG:
            return L"XONLINE_E_MATCH_STRING_TOO_LONG";
        case XONLINE_E_MATCH_BLOB_TOO_LONG:
            return L"XONLINE_E_MATCH_BLOB_TOO_LONG";
        case XONLINE_E_MATCH_INVALID_ATTRIBUTE_ID:
            return L"XONLINE_E_MATCH_INVALID_ATTRIBUTE_ID";
        case XONLINE_E_MATCH_SESSION_ALREADY_EXISTS:
            return L"XONLINE_E_MATCH_SESSION_ALREADY_EXISTS";
        case XONLINE_E_MATCH_CRITICAL_DB_ERR:
            return L"XONLINE_E_MATCH_CRITICAL_DB_ERR";
        case XONLINE_E_MATCH_NOT_ENOUGH_COLUMNS:
            return L"XONLINE_E_MATCH_NOT_ENOUGH_COLUMNS";
        case XONLINE_E_MATCH_PERMISSION_DENIED:
            return L"XONLINE_E_MATCH_PERMISSION_DENIED";
        case XONLINE_E_MATCH_INVALID_PART_SCHEME:
            return L"XONLINE_E_MATCH_INVALID_PART_SCHEME";
        case XONLINE_E_MATCH_INVALID_PARAM:
            return L"XONLINE_E_MATCH_INVALID_PARAM";
        case XONLINE_E_MATCH_DATA_TYPE_MISMATCH:
            return L"XONLINE_E_MATCH_DATA_TYPE_MISMATCH";
        case XONLINE_E_MATCH_SERVER_ERROR:
            return L"XONLINE_E_MATCH_SERVER_ERROR";
        case XONLINE_E_MATCH_NO_USERS:
            return L"XONLINE_E_MATCH_NO_USERS";
        case XONLINE_E_MATCH_INVALID_BLOB:
            return L"XONLINE_E_MATCH_INVALID_BLOB";
        case XONLINE_E_OFFERING_BAD_REQUEST:
            return L"XONLINE_E_OFFERING_BAD_REQUEST";
        case XONLINE_E_OFFERING_INVALID_USER:
            return L"XONLINE_E_OFFERING_INVALID_USER";
        case XONLINE_E_OFFERING_INVALID_OFFER_ID:
            return L"XONLINE_E_OFFERING_INVALID_OFFER_ID";
        case XONLINE_E_OFFERING_INELIGIBLE_FOR_OFFER:
            return L"XONLINE_E_OFFERING_INELIGIBLE_FOR_OFFER";
        case XONLINE_E_OFFERING_OFFER_EXPIRED:
            return L"XONLINE_E_OFFERING_OFFER_EXPIRED";
        case XONLINE_E_OFFERING_SERVICE_UNREACHABLE:
            return L"XONLINE_E_OFFERING_SERVICE_UNREACHABLE";
        case XONLINE_E_OFFERING_PURCHASE_BLOCKED:
            return L"XONLINE_E_OFFERING_PURCHASE_BLOCKED";
        case XONLINE_E_OFFERING_PURCHASE_DENIED:
            return L"XONLINE_E_OFFERING_PURCHASE_DENIED";
        case XONLINE_E_OFFERING_BILLING_SERVER_ERROR:
            return L"XONLINE_E_OFFERING_BILLING_SERVER_ERROR";
        case XONLINE_E_OFFERING_OFFER_NOT_CANCELABLE:
            return L"XONLINE_E_OFFERING_OFFER_NOT_CANCELABLE";
        case XONLINE_E_OFFERING_NOTHING_TO_CANCEL:
            return L"XONLINE_E_OFFERING_NOTHING_TO_CANCEL";
        case XONLINE_E_OFFERING_ALREADY_OWN_MAX:
            return L"XONLINE_E_OFFERING_ALREADY_OWN_MAX";
        case XONLINE_E_OFFERING_NO_CHARGE:
            return L"XONLINE_E_OFFERING_NO_CHARGE";
        case XONLINE_E_OFFERING_PERMISSION_DENIED:
            return L"XONLINE_E_OFFERING_PERMISSION_DENIED";
        case XONLINE_E_OFFERING_NAME_TAKEN:
            return L"XONLINE_E_OFFERING_NAME_TAKEN";
        case XONLINE_E_UODB_KEY_ALREADY_EXISTS:
            return L"XONLINE_E_UODB_KEY_ALREADY_EXISTS";
        case XONLINE_E_MSGSVR_INVALID_REQUEST:
            return L"XONLINE_E_MSGSVR_INVALID_REQUEST";
        case XONLINE_E_FEEDBACK_NULL_TARGET:
            return L"XONLINE_E_FEEDBACK_NULL_TARGET";
        case XONLINE_E_FEEDBACK_BAD_TYPE:
            return L"XONLINE_E_FEEDBACK_BAD_TYPE";
        case XONLINE_E_FEEDBACK_CANNOT_LOG:
            return L"XONLINE_E_FEEDBACK_CANNOT_LOG";
        case XONLINE_E_STAT_BAD_REQUEST:
            return L"XONLINE_E_STAT_BAD_REQUEST";
        case XONLINE_E_STAT_INVALID_TITLE_OR_LEADERBOARD:
            return L"XONLINE_E_STAT_INVALID_TITLE_OR_LEADERBOARD";
        case XONLINE_E_STAT_USER_NOT_FOUND:
            return L"XONLINE_E_STAT_USER_NOT_FOUND";
        case XONLINE_E_STAT_SET_FAILED_0:
            return L"XONLINE_E_STAT_SET_FAILED_0";
        case XONLINETASK_S_SUCCESS:
            return L"XONLINETASK_S_SUCCESS";
        case S_OK:
            return L"S_OK";
    }

    return NULL;
}



