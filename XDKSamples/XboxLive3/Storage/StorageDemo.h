//-------------------------------------------------------------------------------------
// File: StorageDemo.h
//
// Desc: Demonstrates Xbox Live storage. Storage is done on a "per team per title"
//       basis. Storage is also shown on a "per user per title" basis.  And also
//       on a "publisher/global" basis.  Storage is used to allow for the creation and 
//       usage of buddy icons and team logos that will be persistent across Xbox Live.
//
//       Security best practices are also shown for dealing with distributed content.
//       The Xbox Live signature service is demonstrated for this purpose.
//
// Hist: 08.10.04 - New for Sept release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef STORAGE_DEMO_H
#define STORAGE_DEMO_H

#include <vector>
#include "XBApp.h"
#include "XBNet.h"
#include "XBOnline.h"
#include "XBOnlineTask.h"
#include "XBRandName.h"

#include "Common.h"
#include "UserContent.h"
#include "UserSettings.h"


//-------------------------------------------------------------------------------------
// Constants
//-------------------------------------------------------------------------------------

static const D3DCOLOR COLOR_POINTER  = COLOR_GREEN;

// UI constants

const INT   MAX_USERS                = 4; // Maximum user that can logon at once
const INT   NUM_ENTRIES_PER_SCREEN   = 7; // Maximum number of items to show at once


// Screen size constants

const FLOAT SCREEN_SIZE_X            = 640.0f;
const FLOAT SCREEN_SIZE_Y            = 480.0f;
const FLOAT SCREEN_CENTER_X          = ( SCREEN_SIZE_X * 0.5f );
const FLOAT SCREEN_CENTER_Y          = ( SCREEN_SIZE_Y * 0.5f );


// UI position constants

const FLOAT POS_SCREEN_TITLE_Y       = 80.0f;
const FLOAT POS_MESSAGE_Y            = 240.0f;
const FLOAT POS_HEADER_Y             = 40.0f;
const FLOAT POS_HEADER_LEFT          = 40.0f;
const FLOAT POS_HEADER_RIGHT         = ( SCREEN_SIZE_X - POS_HEADER_LEFT );
const FLOAT POS_FOOTER_Y             = 420.0f;
const FLOAT POS_FOOTER_LEFT          = 40.0f;
const FLOAT POS_FOOTER_RIGHT         = ( SCREEN_SIZE_X - POS_FOOTER_LEFT );
const FLOAT DEFAULT_TEXT_PADDING     = 30.0f;
const FLOAT POS_ACCOUNT_LIST_START   = POS_SCREEN_TITLE_Y + 
                                       ( DEFAULT_TEXT_PADDING * 2.0f );

const INT MAX_MESSAGE_LENGTH         = 64; // Maximum length of a UI message string
const INT MAX_SIZE_STATE_STACK       = 8; // The maximum size of the state stack


// Used for determining if a menu wraps around or not
enum
{
    MENU_WRAP_OFF,
    MENU_WRAP_ON
};

// Error returns for signing in
enum
{
    E_NETWORK_ERROR = 1,
    E_ACCOUNT_ERROR,

    NUM_LOGIN_ERRORS
};


const INT    MAX_SAVES              = 25; // Max number of saves to enumerate
const INT    MAX_NETWORK_RESULTS    = 10; // Max number of network files to enumerate


// File name constants used for content
// downloading and uploading

WCHAR*       TEAM_LOGO_FILENAME     = L"logo";    // Name of team logo file on network
const WCHAR* INSTALL_LOCATION_FILE  = L"install"; // Name of file containing the save name

const CHAR* const FILENAME_SAVEMETA = "savemeta.xbx";
const CHAR* const FILENAME_INSTALL  = "install";


////////////////
// Game Menus //
////////////////

// GAME SETUP menu
const WCHAR* const MENU_MAIN[] =
{
    L"Content Management",
    L"User Settings",
    L"My Teams",
    L"Recent Players"
};
enum
{
    MENU_MAIN_CONTENT_MANAGEMENT = 0,
    MENU_MAIN_USER_SETTINGS,
    MENU_MAIN_TEAMS,
    MENU_MAIN_RECENT_PLAYERS,
    NUM_ITEMS_MAIN_MENU
};

// CONTENT MANAGEMENT menu
const WCHAR* const MENU_CONTENT_MANAGEMENT[] =
{
    L"Create New Content",
    L"List Local Content",
};
enum
{
    MENU_CONTENT_MANAGEMENT_CREATE_CONTENT = 0,
    MENU_CONTENT_MANAGEMENT_LIST_LOCAL_CONTENT,
    NUM_ITEMS_CONTENT_MANAGEMENT_MENU
};

// LOCAL CONTENT OPTIONS menu
const WCHAR* const MENU_LOCAL_CONTENT_OPTIONS[] =
{
    L"Edit Content",
    L"Upload as my shared content",
    L"View Content",
    L"Delete Content",
};
enum
{
    MENU_LOCAL_CONTENT_OPTIONS_EDIT = 0,
    MENU_LOCAL_CONTENT_UPLOAD,
    MENU_LOCAL_CONTENT_OPTIONS_VIEW,
    MENU_LOCAL_CONTENT_OPTIONS_DELETE,
    NUM_ITEMS_LOCAL_CONTENT_OPTIONS_MENU
};


//-------------------------------------------------------------------------------------
// Classes
//-------------------------------------------------------------------------------------

// Progress activity
enum
{
    PROGRESS_ACTIVITY_NONE = -1,
    PROGRESS_ACTIVITY_DOWNLOAD,
    PROGRESS_ACTIVITY_UPLOAD,
    NUM_PROGRESS_ACTIVITIES
};


// Sample Application Class
// Demonstrates Team management and statistics
//
class CXBoxSample : public CXBApplication
{
public:

    // Events IDs used to trigger
    // transitions in the state machine
    // These refer to actions from
    // the game controller:
    enum Event
    {
        EV_BUTTON_A,                // "A" button pressed
        EV_BUTTON_B,                // "B" button pressed
        EV_BUTTON_X,                // "X" button pressed
        EV_BUTTON_Y,                // "Y" button pressed
        EV_BUTTON_WHITE,            // "WHITE" button pressed
        EV_BUTTON_BLACK,            // "BLACK" button pressed
        EV_BUTTON_START,            // "START" button pressed
        EV_BUTTON_BACK,             // "BACK" button pressed
        EV_UP,                      // DPAD UP pressed
        EV_DOWN,                    // DPAD DOWN pressed
        EV_LEFT,                    // DPAD LEFT pressed
        EV_RIGHT,                   // DPAD RIGHT pressed
        EV_NULL                     // No Event / Idle event
    };

    // UI States:
    enum EUIStates
    {
        STATE_SELECT_ACCOUNT,        // Allows the player select the Live account they 
                                     // wish to logon with
        STATE_LOGIN,                 // Attempts to log a player onto the Xbox Live 
                                     // service
        STATE_LOGIN_FAILED,          // Screen that tells the user they were unable to 
                                     // logon to Live
        STATE_NETWORK_ERROR,         // Screen that tells the user they have experienced 
                                     // network problems
        STATE_MAIN,                  // Menu that allows the player to create or join a 
                                     // match.
        STATE_CONTENT_MANAGEMENT,    // Upload, edit, create
        STATE_LIST_SAVED_CONTENT,    // Shows the use all content saved on the HD
        STATE_LOCAL_CONTENT_OPTIONS, // Allows the user to perform operations on data
        STATE_RECENT_PLAYERS,        // Allows content actions for recent players of user
        STATE_VIEW_MY_TEAMS,         // Allows the user to view a list of teams they are a member of
        STATE_VIEW_TEAM_ROSTER,      // Allows the player to view a team roster and edit their buddy icon
        STATE_VIEW_TEAMMATE_ICON,    // Allows the player to view the icon of a teammate
        STATE_SETTINGS_EDIT,         // Allows the user to edit settings
        STATE_CONTENT_EDIT,          // Allows the user to edit content
        STATE_MESSAGE_WINDOW,        // Window to display a message
        NUM_STATES
    };

    // Bit flags used the the RenderFooter function
    // to determine which parts to display/render
    enum EFooterFlags
    {
        FOOTER_RENDER_NONE   = 0,  // Render nothing / reset
        FOOTER_RENDER_SELECT = 1,  // Render "Select" in the BR corner
        FOOTER_RENDER_CANCEL = 2   // Render "Cancel" in the BL corner
    };

protected:

    BOOL                    m_bIsSigningIn;
    BOOL                    m_bUserSignedIn;

    INT                     m_dwControllingUserPort;


    // STATE SPECIFIC VARIABLES

    // Message of the day
    WCHAR                   m_wszMessageOfTheDay[MAX_MESSAGE_LENGTH];
    CUserContent            m_motdIcon;
    LPDIRECT3DTEXTURE8      m_pMOTDTexture;

    // State Select Account
    DWORD                   m_dwAccountRenderStart;
    WORD                    m_wCurUserIndex;

    // State Login
    INT                     m_iSignInResult;

    // State Main
    BOOL                    m_bFirstInit;

    // State ListSavedContent
    XGAME_FIND_DATA         m_savedContentData[MAX_SAVES];
    DWORD                   m_dwNumContentSaves;
    DWORD                   m_dwSaveSelected;
    DWORD                   m_dwSaveRenderStart;
    EUIStates               m_eContentAction;

    // State Recent Players
    DWORD                   m_dwPlayerSelected;
    DWORD                   m_dwPlayerRenderStart;

    // State ViewMyTeams
    XUID                    m_rwTeamXUIDS[XONLINE_MAX_TEAM_COUNT];
    DWORD                   m_dwTeamCount;
    INT                     m_iTeamSelected;
    XONLINE_TEAM            m_rwTeamInfo[XONLINE_MAX_TEAM_COUNT];
    XONLINE_TEAM_PROPERTIES m_createdTeamProps;
    LPDIRECT3DTEXTURE8*     m_ppTeamLogoTextures;
    DWORD                   m_dwTeamLogoToDL;

    // State EditSettings
    CXBOnlineTask           m_hUserSettingsTask;
    BYTE                    m_pSettingsReceiveBuffer[SETTINGS_DL_BUFFER_SIZE];
    CUserSettings           m_userSettings;
    CUserSettings           m_editableUserSettings;
    WCHAR*                  m_wszFilename;

    // State ViewTeamRoster
    CXBOnlineTask            m_phTeamRosterTask;
    DWORD                    m_dwTeamMemberCount;
    DWORD                    m_dwRosterRenderStart;
    DWORD                    m_dwTeamMemberSelected;
    XUID                     m_rwTeamMembers[XONLINE_MAX_TEAM_MEMBER_COUNT];
    DWORD                    m_dwTeamMemberTextureToDL;
    LPDIRECT3DTEXTURE8*      m_ppTeammateTextures;
    LPDIRECT3DVERTEXBUFFER8  m_pLogoVerts;

    // State EditContent
    CUserContent            m_userContent;
    INT                     m_iTurtleX;
    INT                     m_iTurtleY;
    CXBStopWatch            m_turtleFlashTimer;
    D3DCOLOR                m_dwTurtleColor;
    LPDIRECT3DTEXTURE8      m_lpPreviewTexture;
    DWORD                   m_rwButtonColorMap[6];
    WCHAR                   m_swzBaseFilename[MAX_GAMENAME];
    BOOL                    m_bUploadInsteadOfSave;
    BOOL                    m_bTeamLogo;


    // Progress bar states
    
    DWORD                   m_dwProgressActivity;
    BOOL                    m_bProgressSucceeded;
    DWORD                   m_dwProgressPercentage;
    WCHAR                   m_wszProgressMessageFormat[MAX_MESSAGE_LENGTH];
    WCHAR                   m_wszProgressMessage[MAX_MESSAGE_LENGTH];

    // State GameMessage
    WCHAR                   m_szGameMessage[MAX_MESSAGE_LENGTH];

    // *** UI specific variables ***
    // Font object used to render the UI's text
    CXBFont                 m_font;

    // The current colors to draw
    // the background, text and highlight with
    D3DCOLOR                m_dwBGColor;
    D3DCOLOR                m_dwTextColor;
    D3DCOLOR                m_dwHighlightColor;

    // The current state of the UI
    EUIStates               m_state;
    EUIStates               m_stateStack[MAX_SIZE_STATE_STACK];
    WORD                    m_wStateStackSize;

    // Index of the item currently selected in the UI menu
    INT                     m_iItemSelected;

    // Task to logon the user and to send Keep-Alives to Xbox Live
    CXBOnlineTask           m_hLogonTask;

    // User data for all Xbox Live accounts held on the Hard Drive and memory units
    XONLINE_USER            m_rwStoredUsers[ XONLINE_MAX_STORED_ONLINE_USERS ];          
    // The number of users stored in m_rwStoredUsers
    DWORD                   m_dwNumStoredUsers;

    // Network session helper functions
    INT     StartSignIn();
    INT     ContinueSignIn();
    INT     FinishSignIn();

    INT     GetFileIndex( const WCHAR* wszFilename,
                          PXONLINESTORAGE_FILE_INFO*& rwEnumResults,
                          DWORD& dwNumResults );
    HRESULT DownloadGlobal( const DWORD dwControllingUserPort,
                            const WCHAR* wszStorageServerPath,
                            PBYTE rwBuffer,
                            DWORD& dwBufferSize );
    BOOL    EnumerateGlobalStorage( PXONLINESTORAGE_FILE_INFO*& rwEnumResults,
                                    DWORD& dwNumResults );
    BOOL    UploadSave( INT iSaveGameSlot );
    BOOL    VerifyFile( HANDLE hSignature,
                        const CHAR *szPath, const CHAR *szFilename,
                        PBYTE pData, DWORD &dwBufferSize );
    BOOL    DownloadSave( ULONGLONG qwUserID, CHAR* szLocation );
    BOOL    ViewUserSave( ULONGLONG qwUserID, CUserContent& userContent );

    // Icon/sprite helper functions
    LPDIRECT3DVERTEXBUFFER8 CreateFace( FLOAT fX, FLOAT fY );
    VOID                    SetFacePos( LPDIRECT3DVERTEXBUFFER8 pVerts,
                                        FLOAT fX, FLOAT fY );
    VOID                    TranslateFace( LPDIRECT3DVERTEXBUFFER8 pVerts,
                                           FLOAT fX, FLOAT fY );
    VOID                    RenderSprite ( LPDIRECT3DVERTEXBUFFER8 pVerts,
                                           LPDIRECT3DTEXTURE8 pTexture );
                                     
    // Teams helper functions
    BOOL    GetTeamList();
    BOOL    GetTeamRoster();

    // Message of the day (used in main menu)
    BOOL    GetMessageOfTheDay( PXONLINESTORAGE_FILE_INFO* rwEnumResults,
                                DWORD dwNumResults );
    BOOL    GetUIColors( PXONLINESTORAGE_FILE_INFO* rwEnumResults,
                         DWORD dwNumResults );
    BOOL    GetMessageIcon( PXONLINESTORAGE_FILE_INFO* rwEnumResults,
                            DWORD dwNumResults );

    // State handling functions
    // Functions that handle the
    // entrance into, updating of,
    // rendering of, and exiting
    // of the UI/Game states

    Event   GetEvent( INT iController ) const;
    VOID    PushState( EUIStates newState );
    VOID    PopState( BOOL bReinit = FALSE );
    VOID    ClearStack() { m_wStateStackSize = 1; m_state = m_stateStack[0];}
    VOID    PushMessageWindow( const CHAR* strTextMessage );

    VOID    RenderMenu( const WCHAR* strMenuName,
                        const WCHAR** rwMenuText,
                        const WORD wNumMenuItems,
                        const INT iCurMenuItem );

    INT     GetMenuPosition( INT iCurMenuPosition, INT iNumMenuItems, Event , 
                             INT iMenuWrap = MENU_WRAP_ON );

    VOID    RenderControllingUser();

    // Progress methods
    // initializes progress task and states thereof; and creates a progress 
    // message showing how much progress has occurred in an upload/download task 
    // (note: this should be in a printf style format to allow display of integer 
    // percentage number i.e. "Upload content progress so far: %u")
    // also, message string should be no longer than MAX_MESSAGE_LENGTH - 1
    VOID    SetProgressTask( DWORD dwProgressActivity , const CHAR* szMessageFormat );
    // attempts to get progress for current progress task.  If attempt fails,
    // FALSE is returned, else TRUE
    BOOL    UpdateProgressForTask( CXBOnlineTask& task );
    // displays a screen with progress message
    VOID    RenderProgressWindow();
    // TRUE if progress has completed (100%) otherwise false
    BOOL    ProgressCompleted()  { return (BOOL)( m_dwProgressPercentage == 100 ); }
    // deinitializes progress task and states thereof
    VOID    ClearProgressTask();

    // State SelectAccount
    VOID    EnterStateSelectAccount();
    VOID    UpdateStateSelectAccount( INT iUser, Event event );
    VOID    RenderStateSelectAccount();
    VOID    ExitStateSelectAccount() {}

    // State LoginPassword
    VOID    EnterStateLogin();
    VOID    UpdateStateLogin( Event );
    VOID    RenderStateLogin();
    VOID    ExitStateLogin() {}

    // State LoginFailed
    VOID    EnterStateLoginFailed() {}
    VOID    UpdateStateLoginFailed( Event );
    VOID    RenderStateLoginFailed();
    VOID    ExitStateLoginFailed() {}

    // State NetworkError
    VOID    EnterStateNetworkError() {}
    VOID    UpdateStateNetworkError( Event );
    VOID    RenderStateNetworkError();
    VOID    ExitStateNetworkError();

    // State Main
    VOID    EnterStateMain();
    VOID    UpdateStateMain( Event event );
    VOID    RenderStateMain();
    VOID    ExitStateMain() {}

    // State RecentPlayers
    VOID    EnterStateRecentPlayers();
    VOID    UpdateStateRecentPlayers( Event event );
    VOID    RenderStateRecentPlayers();
    VOID    ExitStateRecentPlayers() {}

    // State ViewMyTeams
    VOID    EnterStateViewMyTeams();
    VOID    UpdateStateViewMyTeams( Event event );
    VOID    RenderStateViewMyTeams();
    VOID    ExitStateViewMyTeams() {}

    // State ViewTeamRoster
    VOID    EnterStateViewTeamRoster() { m_dwRosterRenderStart = 0; m_dwTeamMemberSelected = 0;}
    VOID    UpdateStateViewTeamRoster( Event event );
    VOID    RenderStateViewTeamRoster();
    VOID    ExitStateViewTeamRoster() {}

    // State ViewTeammateIcon
    VOID    EnterStateViewTeammateIcon() {}
    VOID    UpdateStateViewTeammateIcon( Event event );
    VOID    RenderStateViewTeammateIcon();
    VOID    ExitStateViewTeammateIcon() {}

    // State ContentManagement
    VOID    EnterStateContentManagement();
    VOID    UpdateStateContentManagement( Event event );
    VOID    RenderStateContentManagement();
    VOID    ExitStateContentManagement() {}

    // State ListSavedContent
    VOID    EnterStateListSavedContent();
    VOID    UpdateStateListSavedContent( Event event );
    VOID    RenderStateListSavedContent();
    VOID    ExitStateListSavedContent() {}

    // State LocalContentOptions
    VOID    EnterStateLocalContentOptions();
    VOID    UpdateStateLocalContentOptions( Event event );
    VOID    RenderStateLocalContentOptions();
    VOID    ExitStateLocalContentOptions() {}

    // State SettingsEdit
    VOID    EnterStateSettingsEdit();
    VOID    UpdateStateSettingsEdit( Event );
    VOID    RenderStateSettingsEdit();
    VOID    ExitStateSettingsEdit() {}

    // State ContentEdit
    VOID    EnterStateContentEdit();
    VOID    UpdateStateContentEdit( Event event );
    VOID    RenderStateContentEdit();
    VOID    ExitStateContentEdit() {}

    // State MessageWindow
    VOID    EnterStateMessageWindow() {}
    VOID    UpdateStateMessageWindow( Event event );
    VOID    RenderStateMessageWindow();
    VOID    ExitStateMessageWindow() {}

    // Extra rendering functions
    VOID    RenderHeader();
    VOID    RenderFooter( WORD flags );

public:


    // Overloaded functions defined by the application
    // class to execute game logic and rendering
    virtual HRESULT         Render();
    virtual HRESULT         Initialize();
    virtual HRESULT         FrameMove();

    ~CXBoxSample();
};

#endif // STORAGE_DEMO_H