//-----------------------------------------------------------------------------
// File: ContentDownload.h
//
// Desc: Shows Xbox online content enumeration, download, installation
//       and removal.
//
// Hist: 09.10.01 - New for Nov release
//       04.05.02 - Updated for May release; Added billable content and content
//                  details.  Updated for the new HD/DVD content enumeration API
//       06.05.02 - Updated for June release; Updated billing stuctures and
//                  added removal of "bad" content
//       08.01.02 - Updated for Sept release; Added content downloader launch 
//       04.28.03 - Updated for June release; Decreased download time by
//                                            pumping download task handle
//                                            multiple times per frame
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef CONTENTDOWNLOAD_H
#define CONTENTDOWNLOAD_H

#include "Common.h"
#include "XBApp.h"
#include "XBFont.h"
#include "XBHelp.h"
#include "XBNet.h"
#include "XBOnlineTask.h"
#include "XBStopwatch.h"
#include "UserInterface.h"




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Number of services to authenticate
const DWORD NUM_SERVICES = 1;




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    enum State
    {
        STATE_CREATE_ACCOUNT,           // Create user account
        STATE_SELECT_ACCOUNT,           // Select user account
        STATE_LOGGING_ON,               // Perform authentication
        STATE_SELECT_DEVICE,            // Allow player to select device
        STATE_CHECK_FOR_NEW_CONT,       // Check for new content
        STATE_ENUM_CONTENT,             // Get list of content on device
        STATE_SELECT_CONTENT,           // Allow player to select content
        STATE_GET_DETAILS,              // Get the content details 
        STATE_CONTENT_DETAILS,          // Display content detailed info
        STATE_CONFIRM_PURCHASE,         // Confirm purchase of content
        STATE_PURCHASE,                 // Purchase content
        STATE_CONFIRM_CANCEL_SUB,       // confirm cancelation of subscription
        STATE_CANCEL_SUB,               // cancel content
        STATE_INSTALL_CONTENT,          // Download/install content
        STATE_VERIFY_CONTENT,           // Verify content
        STATE_BAD_CONTENT,              // Could not verify content
        STATE_CONFIRM_REMOVE,           // Verify content removal
        STATE_CONFIRM_ABORT,            // Confirm abort of content
        STATE_CONFIRM_PARTIAL_REMOVE,   // Confirm removal of partial content
        STATE_CONTENT_METADATA,         // show content meta data
        STATE_REMOVE,                   // remove content
        STATE_ERROR,                    // Error
        STATE_SUCCESS                   // Success
    };

    enum Event
    {
        EV_BUTTON_A,
        EV_BUTTON_B,
        EV_BUTTON_X,
        EV_BUTTON_Y,
        EV_BUTTON_BACK,
        EV_BUTTON_WHITE,
        EV_BUTTON_START,
        EV_UP,
        EV_DOWN,
        EV_NULL
    };

    enum EnumType
    {
        HARD_DISK,
        ONLINE,
        ONLINE_DOWNLOADER
    };

    UserInterface       m_UI;                // UI object
    State               m_State;             // current state
    State               m_NextState;         // return to this state
    EnumType            m_EnumType;          // enumeration type
    DWORD               m_dwCurrItem;        // current selected menu item
    DWORD               m_dwTopItem;         // tracks the index of the top
                                             //  item
    XBUserList          m_UserList;          // list of available accounts
    DWORD               m_dwCurrUser;        // index of curr user in
                                             //   m_UserList
    DWORD               m_dwUserIndex;       // which controller
    WCHAR               m_strUser[ XONLINE_GAMERTAG_SIZE ]; // current user
                                                            //   name
    DWORD               m_pServices[ NUM_SERVICES ];        // List of
                                                            //   desired
                                                            //   services
    CXBNetLink          m_NetLink;           // Network link checking
    XONLINEOFFERING_ENUM_PARAMS m_EnumParams; // enumerate params
    BOOL                m_bIsLoggedOn;       // TRUE if authenticated
    ContentList         m_ContentList;       // list of content
    DWORD               m_dwCurrContent;     // selected content
    CXBOnlineTask       m_hOnlineTask;       // online task for pumping
    CXBOnlineTask       m_hContentTask;      // content task handle
    BYTE*               m_pEnumBuffer;       // enumeration buffer
    BYTE*               m_pDetailsBuffer;
    FLOAT               m_fPercentComplete;  // for progress bars
    DWORD               m_dwBlocksInstalled; // blocks installed
    DWORD               m_dwBlocksTotal;     // total blocks to install

    BOOL                m_bHelp;             // display help

    CXBFont             m_Font;              // Game font
    CXBHelp             m_Help;

    WCHAR               m_strSuccess[512];
    WCHAR               m_strError[512];
public:

    CXBoxSample();
    virtual HRESULT Initialize();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();

    BOOL                BillingEnabled();    // can the current user
                                             // purchase content
                                             // or change subscription
                                             // status

private:

    Event GetEvent() const;

    VOID UpdateStateCreateAccount( Event );
    VOID UpdateStateSelectAccount( Event );
    VOID UpdateStateLoggingOn( Event );
    VOID UpdateStateSelectDevice( Event );
    VOID UpdateStateCheckForNewContent( Event );
    VOID UpdateStateConfirm( Event );
    VOID UpdateStateSelectContent( Event );
    VOID UpdateStateContentMetadata( Event );
    VOID UpdateStateRemoveContent( Event );
    VOID UpdateStateEnumContent( Event );
    VOID UpdateStateGetDetails( Event );
    VOID UpdateStateContentDetails( Event );
    VOID UpdateStatePurchase( Event );
    VOID UpdateStateCancelSub( Event );
    VOID UpdateStateInstallContent( Event );
    VOID UpdateStateVerifyContent( Event );
    VOID UpdateStateBadContent( Event );
    VOID UpdateStateContinue( Event );
    VOID BeginLogin();
    VOID BeginCheckForNewContent();
    VOID BeginEnum();
    VOID BeginGetDetails();
    VOID BeginPurchase();
    VOID BeginInstall();
    VOID BeginCancelSub();
    VOID __cdecl SetErrorString( const WCHAR*, ... );
    VOID __cdecl SetErrorString( HRESULT hr, const WCHAR*, ... );
    VOID GetLocalContent( const CHAR* strRootPath );
    BOOL VerifyContent( const CHAR* strContentDirctory );
    VOID RemoveContent();
    VOID SetPlayerState( DWORD );
    VOID Reset( );

};

#endif // CONTENTDOWNLOAD_H
