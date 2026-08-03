//-----------------------------------------------------------------------------
// File: QualityOfService.h
//
// Desc: Illustrates Quality of Service calls on Xbox.
//       Allows the user to do QoS probes to other System Link consoles,
//       Online consoles, and Online Security Gateways.
//
// Hist: 05.24.02 - New for June release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef QUALITYOFSERVICE_H
#define QUALITYOFSERVICE_H

#include "Common.h"
#include "UserInterface.h"
#include "XBApp.h"
#include "XBNet.h"
#include "XBOnlineTask.h"
#include "XBRandName.h"



//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Number of services to authenticate
const DWORD NUM_SERVICES = 1;
const DWORD NONCE_BYTES  = 8;      // Larger means less chance of random matches




//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct MsgFindSession;
struct MsgSessionFound;




//-----------------------------------------------------------------------------
// Message payloads
//-----------------------------------------------------------------------------
// Pack to minimize network traffic
#pragma pack( push )
#pragma pack( 1 )

//-----------------------------------------------------------------------------
// Local Nonce struct used by SysLink broadcast messages
//-----------------------------------------------------------------------------
struct Nonce
{
    // Used for client verification. The larger the number of NONCE_BYTES,
    // the less likely there is to be an accidental match between client & host
    BYTE byRandom[ NONCE_BYTES ]; 
};

#pragma pack( pop )




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    enum State
    {
        STATE_SELECT_MODE,          // Select SysLink or Online mode
        STATE_SYSLINK_SEARCH,       // Search for System Link sessions
        STATE_CREATE_ACCOUNT,       // Create online account
        STATE_SELECT_ACCOUNT,       // Select online account
        STATE_SIGNING_IN,           // Sign in to online service
        STATE_ONLINE_CREATE,        // Create online session
        STATE_ONLINE_SEARCH,        // Search for online sessions
        STATE_SELECT_LISTEN_PARAMS, // Set listen parameters
        STATE_SESSION_LIST,         // Show current SysLink or Online sessions
        STATE_SHOW_PROBE_DATA,      // Show detailed probe data for a session
        STATE_ERROR,                // Error screen
        STATE_HELP                  // Help
    };

    enum Event
    {
        EV_BUTTON_A,
        EV_BUTTON_B,
        EV_BUTTON_BACK,
        EV_BUTTON_WHITE,
        EV_UP,
        EV_DOWN,
        EV_LEFT,
        EV_RIGHT,
        EV_NULL
    };

    UserInterface   m_UI;                 // UI object
    State           m_State;              // current state
    State           m_NextState;          // return to this state
    DWORD           m_dwCurrItem;         // current selected menu item
    XBUserList      m_UserList;           // available accounts
    DWORD           m_dwCurrUser;         // index of curr user in m_UserList
    DWORD           m_dwUserIndex;        // which controller
    CXBNetLink      m_NetLink;            // network link checking
    CXBOnlineTask   m_hOnlineTask;        // main online task
    CXBOnlineTask   m_hMatchCreateTask;   // matchmaking create task
    CXBOnlineTask   m_hMatchSearchTask;   // matchmaking search task
    CXBSocket       m_BroadSock;          // Broadcast socket for SysLink broadcast msgs
    Nonce           m_Nonce;              // for SysLink message verification
    BOOL            m_bIsSessionRegistered; // TRUE if key is registered
    BOOL            m_bListen;            // TRUE if listening enabled
    BOOL            m_bIsSignedIn;        // TRUE if signed on to Xbox Live
    XNADDR          m_xnTitleAddress;     // The XNADDR of this console
    XNKID           m_xnSessionID;        // ID for the current session
    XNKEY           m_xnKeyExchangeKey;   // Key Exchange Key for the current session
    DWORD           m_dwListenBandwidth;  // QoS maximum listen bandwidth (bps)
    DWORD           m_dwLookupBandwidth;  // QoS maximum lookup bandwidth (bps)
    DWORD           m_dwSamples;          // QoS number of samples sent per probe
    WCHAR           m_strSessionName[ MAX_SESSION_NAME ];
    DWORD           m_pServices[ NUM_SERVICES ]; // List of desired services
    SessionList     m_Sessions;           // session to probe
    CXBStopWatch    m_SysLinkSearchTimer; // wait for session search to complete
    XNQOS*          m_pxnQos;             // Probe results
    XNQOS*          m_pxnServiceQos;      // Live service probe results

public:

    virtual HRESULT Initialize();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();

    CXBoxSample();

private:

    VOID GetOnlineUsers();
    Event GetEvent() const;

    VOID UpdateStateSelectMode( Event );
    VOID UpdateStateSysLinkSearch( Event );
    VOID UpdateStateCreateAccount( Event );
    VOID UpdateStateSelectAccount( Event );
    VOID UpdateStateSigningIn( Event );
    VOID UpdateStateOnlineCreate( Event );
    VOID UpdateStateOnlineSearch( Event );
    VOID UpdateStateSelectListenParams( Event );
    VOID UpdateStateSessionList( Event );
    VOID UpdateStateShowProbeData( Event );
    VOID UpdateStateError( Event );
    VOID UpdateStateHelp( Event );

    VOID BeginSignIn();
    VOID BeginOnlineSession();
    VOID BeginOnlineSearch();

    VOID SetPlayerState( DWORD );
    VOID SetListenState( BOOL );
    VOID SetListenBitsPerSec( DWORD );
    VOID SetListenData( const VOID*, DWORD );
    VOID SendFindSession();
    VOID SendSessionFound( const Nonce& );
    BOOL ProcessBroadcastMessage();
    VOID ProcessFindSession( const MsgFindSession& );
    VOID ProcessSessionFound( const MsgSessionFound& );
    VOID BeginQosLookup();
    VOID Reset();
    VOID BeginOnlineListen();
    VOID EndOnlineListen();
    VOID BeginSysLinkListen();
    VOID EndSysLinkListen();
};




#endif // QUALITYOFSERVICE_H
