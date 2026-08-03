//-------------------------------------------------------------------------------------
// File: TitleServerClient.cpp
//
// Desc: This module exhibits basic features that show how to connect to a dedicated 
//       Title Server
//
// Hist: 06.16.03 - Created for August release
//
// Xbox Advanced Technology Group : Xbox Live
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------
#include <xtl.h>
#include <stdio.h>
#include <assert.h>
#include <xonline.h>

//-------------------------------------------------------------------------------------
// Structures
//-------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------
// Name: QueryResult
// Desc: a structure that stores the results of the records of the title server upon 
//       lookup. (This structure's packing alignment is one byte.) 
//-------------------------------------------------------------------------------------
#pragma pack(push, 1)
struct QueryResult 
{
    ULONGLONG qwEntityID;          // title server ID
    WORD      wTitleServerAddress; // (unused)
    TSADDR    TitleServerAddress;  // title server address
    WORD      wSessionID;          // (unused)
    XNKID     SessionID;           // session ID
    WORD      wKeyExchangeKey;     // (unused)
    XNKEY     KeyExchangeKey;      // key exchange key
};
#pragma pack(pop)

//-------------------------------------------------------------------------------------
// Prototypes
//-------------------------------------------------------------------------------------
BOOL SignIn();
BOOL PrepareConnection();
VOID GameLoop();
VOID __cdecl Print( const WCHAR* strFormat, ... );
VOID __cdecl Error( const WCHAR* strFormat, ... );
VOID UIMsg( const WCHAR * strText );
VOID BootToDash( DWORD dwReason );


// DEFINE USE_LOCAL_MACHINE to connect to a title server running on a local
// PC
// #define USE_LOCAL_MACHINE TRUE

//-------------------------------------------------------------------------------------
// Constants
//-------------------------------------------------------------------------------------

// Connecting to a title server is straight-forward.  The first step is to include the 
// game server service ID during authentication.  The game server service ID is 
// assigned by the Xbox Live operations center and is typically the same as the ID 
// assigned to the title.
//
// The LSP security gateway must be configured with the service ID:
//
// Service
// {
//     Id   0xFFFF0021      ; Kerberos service ID
//     Name TITLE_SERVICE   ; an arbitrary name used to refer to the
//                          ;  service in a Server config block.
// }

const DWORD TITLE_SERVER_SERVICE_ID = 0xFFED8810;
//const DWORD TITLE_SERVER_SERVICE_ID = 101;

// The Title Server port will be mapped by the LSP security gateway to a specific 
// back-end server's port and IP. That port will be sepcified in the sgconfig.ini file 
// like this:
//
// Server
// {
//     Id 100                       ; The port the title uses to connect to this server
//     Service TITLE_SERVICE        ; The Kerberos service required 
//                                  ; to access this server
//     Address
//     {
//         InterfaceId 1            ; specifies a Datacenter NIC to use
//         Ip          10.10.0.2    ; IP of the back-end server
//         Port        8001         ; Port of the back end server
//     }
// }

const WORD  TITLE_SERVER_PORT       = 100;

// The LSP security gateway must be configured with the title IDs of all titles that 
// will make use of the title servers behind it. This tells the SG to advertise its 
// address in the Query service.
//
// Title
// {
//     TitleId 0xFFFF0021           ; A title which will use this site
// }
//
// Title
// {
//     TitleId 0xFFFF0022           ; Multiple Title sections may be specified.
//                                  ; (N.B. multi-title configurations have not
//                                  ;  been tested in the preview release)
// }

// The title server site ID is a number that identifies the Kerberos site that contains 
// the title servers. This ID is assigned by the Xbox Live operations center and is 
// typically the same as the ID assigned to the title.
//
// The LSP security gateway must be configured with the site ID 
// in the LspProxy section:
//
// LspProxy
// {
//     Ip     10.10.0.100   ; IP address of the LSP proxy
//     Port   8000          ; TCP port of the LSP proxy
//     SiteId 0xFFFF0021    ; Kerberos Site ID
// }

//-------------------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------------------
XONLINETASK_HANDLE g_hLogonTask;         // Online logon task handle
SOCKET             g_TitleServerSocket;  // Title Server traffic socket
SOCKADDR_IN        g_TitleServerAddrIn;  // Title Server endpoint address

// Once logged on, request a set of TSADDR records from the Query service.
// Select one of the results from the query randomly.

QueryResult        g_aQueryResults[10];  // TitleServer lookup records

// Title Server lookup is similar to MatchMaking lookups. The query results include
// a TSADDR (instead of an XNADDR), a SessionID and a KeyExchangeKey. The Key must be 
// registered with XNetRegsiterKey.  
// The TSADDR can be converted to an IN_ADDR with XNetTsAddrToInAddr.

XNKEY              g_KeyExchangeKey;     // Key for KeyEx with LSP server
XNKID              g_SessionID;          // SessionID for KeyEx with LSP server
TSADDR             g_TitleServerAddress; // TitleServer address

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Initialize Input Devices this is required for account enumeration on 
    // Memory Units
    // Note: using zero and null, respectively, as parameters allows this call to 
    //       initialize the maximum number of devices available
    XInitDevices( 0, NULL );

    // Wait for any inserted MUs to mount or other enumeration processes to finish
    while ( XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY ) {}
    
    // Before any of the Xbox online APIs can be used, XOnlineStartup must be 
    // called.  XOnlineStartup automatically calls XNetStartup and 
    // WSAStartup with default parameters in order to initialize the 
    // Xbox Secure Network Libary and the Winsock layer. To specify non-default
    // startup parameters for XNetStartup or WSAStartup, call those functions 
    // prior to calling XOnlineStartup.
    
#ifdef USE_LOCAL_MACHINE
    // In order to connect to the remote machine, we must bypass
    // security and send data in the clear.  This requires us
    // to explicitly call XNetStartup with the XNET_STARTUP_BYPASS_SECURITY
    // flag.  You will also need to link with the non-secure online library
    // (xonlined.lib)
    XNetStartupParams startupParams;
    ZeroMemory( &startupParams, sizeof(startupParams) );
    startupParams.cfgSizeOfStruct = sizeof(startupParams);
    startupParams.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
    INT iXNetStartupResult = XNetStartup( &startupParams );
    if( iXNetStartupResult != NO_ERROR )
	{
        // Xbox will reboot to the dashboard if XNetStartup returns any error
        Error( L"XNetStarup failed with 0x%x", iXNetStartupResult );
	}

    // The major and minor version number of Winsock that will be used.
    // Example: Winsock 2.1 means WINSOCK_VERS_MAJOR will be 2, 
    // and WINSOCK_VERS_MINOR will be 1.
    //
    // For this sample, we will be using Winsock 2.2
    const INT WINSOCK_VERS_MAJOR = 2;
    const INT WINSOCK_VERS_MINOR = 2;

    // Standard WinSock startup. The Xbox allows all versions of Winsock
    // up through 2.2 (i.e. 1.0, 1.1, 2.0, 2.1, and 2.2), although it 
    // technically supports only and exactly what is specified in the 
    // Xbox network documentation, not necessarily the full Winsock 
    // functional specification.
    WSADATA wsaData;
    INT     iWSAStartupResult = 
            WSAStartup( MAKEWORD(WINSOCK_VERS_MAJOR,WINSOCK_VERS_MINOR), &wsaData );

    if( iWSAStartupResult != NO_ERROR )
    {
        // Xbox will reboot to the dashboard if WSAStartup returns any error
        Error( L"WSAStartup failed with 0x%x", iWSAStartupResult );
    }

#endif // USE_LOCAL_MACHINE

    HRESULT hrOnlineStartup = XOnlineStartup( NULL );
    assert( SUCCEEDED( hrOnlineStartup ) );
    (VOID)hrOnlineStartup; // avoid compiler warning

    if( SignIn() )
    {

        if( PrepareConnection() )
        {
            GameLoop();

            // Clean up address and key info
            INT iCloseResult = closesocket( g_TitleServerSocket );
            if ( iCloseResult == SOCKET_ERROR )
            {
                Error( L"Closing socket failed with 0x%x", WSAGetLastError() );
            }

#ifndef USE_LOCAL_MACHINE

           // frees association to title server endpoint socket
            INT iUnregisterInAddrResult = 
                XNetUnregisterInAddr( g_TitleServerAddrIn.sin_addr );
            if ( iUnregisterInAddrResult != 0 )
            {
                Error( L"Unregistering title server endpoint socket failed with 0x%x", 
                       iUnregisterInAddrResult );
            }


            // unregisters session key information
            INT iUnregisterKeyResult = XNetUnregisterKey( &g_SessionID );
            if ( iUnregisterKeyResult != 0 )
            {
                Error( L"Unregistering session ID failed with 0x%x", 
                       iUnregisterKeyResult );
            }

#endif // USE_LOCAL_MACHINE

        }

        if( g_hLogonTask )
        {
            // Finally, signoff by closing the logon task handle.
            Print( L"Signing off" );
            HRESULT hrTaskClose = XOnlineTaskClose( g_hLogonTask );
            if ( hrTaskClose != S_OK )
            {
                Error( L"Closing logon task failed" );
            }
        }
    }

    
    // cleanup routines for Xbox Live functions
    HRESULT hrCleanup = XOnlineCleanup();
    if ( hrCleanup != S_OK )
    {
        Error( L"Online cleanup failed" );
    }

#ifdef USE_LOCAL_MACHINE

    // quits Xbox Winsock layer
    INT iWSACleanupResult = WSACleanup();
    if ( iWSACleanupResult == SOCKET_ERROR )
    {
        Error( L"Online cleanup failed with 0x%x", WSAGetLastError() );
    }
#endif // USE_LOCAL_MACHINE

    // reboots
    BootToDash( XLD_LAUNCH_DASHBOARD_MAIN_MENU );
}


//-------------------------------------------------------------------------------------
// Name: SignIn()
// Desc: Demonstrate Xbox online authentication
// Authentication takes place in three stages:  First, the Xbox itself it
// authenticated, and then one or more specified users (and possibly guests) 
// accounts.  Up to four players can be signed onto a console. 
// Finally, both the Xbox and the users are authenticated against
// the requested services (e.g. matchmaking).
//-------------------------------------------------------------------------------------
BOOL SignIn()
{
    
    // First, obtain a list of user accounts on this Xbox. The XOnlineGetUsers
    // function will enumerate both the hard disk and any attached memory units
    // looking for accounts. 
    XONLINE_USER aStoredUsers[ XONLINE_MAX_STORED_ONLINE_USERS ];
    DWORD        dwNumStoredUsers;
    
    HRESULT hrGetUsers = XOnlineGetUsers( aStoredUsers, &dwNumStoredUsers );
    assert( SUCCEEDED( hrGetUsers ) );
    (VOID)hrGetUsers; // avoid compiler warning
    
    // If no accounts, then player needs to create an account.
    if( dwNumStoredUsers == 0 )
    {
        Print( L"No user accounts found." );
        // Titles must give the player the *option* of going to
        // the online dash to create new account. In addition, it is
        // possible for a player to actually insert/remove an MU while
        // the title account selection UI is active.  A title must
        // call XOnlineGetUsers repeatedly to account for this.
        // For demonstration purposes, we boot to the account signup section
        // of the online dash
        BootToDash( XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP );
        return FALSE;
    }
    
    // This sample shows how to authenticate a single user and a guest. 
    
    // Before signing on, the title must check accounts for
    // passcodes. If present, the players must be prompted for them.
    // Passcodes are for *client-side* authentication only -- the
    // Xbox online service does not use them for authentication. For
    // demonstration purposes, we just make note of any passcode, and continue.
    // (The 'passcode' field of the XONLINE_USER structure contains the actual
    // passcode).
    if( aStoredUsers[0].dwUserOptions & XONLINE_USER_OPTION_REQUIRE_PASSCODE )
    {
#ifdef XBOX_SAMPLE
        Print(L"%S has a passcode", aStoredUsers[0].szGamertag );
#else
#pragma message("TCR: Title UI must prompt for a passcode and " \
                "verify it before signing on.")
#endif
    }
    
    // Next, actually sign on.  This is accomplished using
    // XOnlineLogon. XOnlineLogon requires a list of exactly 4 XONLINE_USER 
    // accounts (1 per controller) to login in a single call.  The list must be 
    // a one-to-one match of controller to player in order for the online 
    // system to recognize which player is using which controller. Any unused 
    // entries must be zeroed out.  This sample shows how to authenticate a 
    // single user and a guest. For brevity, we select the first account and  
    // the first controller. We then add a guest account, using the second
    // controller. 
    XONLINE_USER aLogonUsers[ XONLINE_MAX_LOGON_USERS ] = { 0 }; // Initially zeroed
    
    // Sign in the first account on the first controller.
    aLogonUsers[0] = aStoredUsers[0]; 
    
    // Any unused users must remain zeroed out.
    // Here, aLogonUsers[1], aLogonUsers[2], aLogonUsers[3] are still
    // zeroed-out indicating that no players will be signed in on those controllers.
      
    // Now, add whatever services are appropriate for your title, but no
    // more. Each service requires additional authentication time
    // and network traffic.  The service IDs for the title server and
    // the matchmaking service are specified.
    
    const DWORD X_SERVICES_ARRAY[] = 
    { 
        XONLINE_QUERY_SERVICE, 
#ifndef USE_LOCAL_MACHINE
        TITLE_SERVER_SERVICE_ID
#endif // USE LOCAL MACHINE
    };
    const DWORD NUM_X_SERVICES = sizeof( X_SERVICES_ARRAY ) / 
                                 sizeof( X_SERVICES_ARRAY[0] );
    
    // Initiate the authentication process.  The signon process
    // first authenticates the Xbox.  Next, it authenticates each
    // user, and finally authenticates against the requested services
    // (validating that both the users *and* the Xbox have access to them).
    // All three stages are handled by the client APIs, though the title
    // is required to check for errors and handle them appropriately.
    HRESULT hrLogon = XOnlineLogon( aLogonUsers, X_SERVICES_ARRAY, NUM_X_SERVICES, 
                                    NULL, &g_hLogonTask );
    
    // Check for errors
    switch( hrLogon )
    {
    case S_OK:   
        // XOnlineLogon succeeded
        break; 
    case XONLINE_E_LOGON_NO_NETWORK_CONNECTION:
        // Sign on failed because no network connection was
        // detected.  A title must give the player the
        // option of accessing the network configuration of the online dash
        Print( L"No network connection detected." );
#ifdef XBOX_SAMPLE
        BootToDash( XLD_LAUNCH_DASHBOARD_NETWORK_CONFIGURATION );
#else
#pragma message("TCR: Title must give the option of accessing the " \
                "network configuration of the online dash.")
#endif
        break;
    default:
        // This should never happen
        assert( FALSE );
        return FALSE;
    }
      
    // If successful, an asynchronous task handle (XONLINETASK_HANDLE) will
    // be returned.  As with the other Xbox online APIs that return
    // task handles, XOnlineTaskContinue() is used to perform a "unit" of
    // work.  The HRESULT returned by calling XOnlineTaskContinue will
    // indicate if additional work is required (XONLINETASK_S_RUNNING) or if
    // the task has failed.  Some of the result codes returned will depend on 
    // the actual type of task being pumped.  However, the SUCCEEDED and
    // FAILED macros can be used for error handling purposes.

    Print( L"Signing in..." );

    // Go into a loop, calling XOnlineTaskContinue on the logon task
    // until the task completes.  This can take up to a minute or more
    // depending on network conditions.  If successful, 
    // XONLINE_S_LOGON_CONNECTION_ESTABLISHED will be returned.
    // In a real title, this would appear inside your game loop. 
	HRESULT hrTask;
    do
    {
        hrTask = XOnlineTaskContinue( g_hLogonTask );  // Do a small amount of work
    } 
    while ( hrTask == XONLINETASK_S_RUNNING );  // As long as there is work to do
    
    // Titles must check for, and handle, authentication errors in the
    // following order:
    // 1. System authentication errors.
    // 2. User authentication errors.
    // 3. Service authentication errors.
    // It important to check for, and handle user errors before service
    // errors.  Consider the case where there is an account maintenance issue 
    // for a user, AND a requested service is unavailable.   A title must
    // allow the user to deal with the account issues before service issues
    // (especially since an account issue could be the cause of
    // the service issue).
    
    // 1. Check for system authentication errors.
    switch( hrTask )
    {
    case XONLINE_S_LOGON_CONNECTION_ESTABLISHED:  
        // The Xbox has been validated and there are no system authentication
        // errors
        Print( L"Connection established" );
        break;
    case XONLINE_E_LOGON_CONNECTION_LOST:
        Print( L"Network connection lost" );
        // A title should allow the player the *option* of booting
        // into the network configuration section of the online dash
        // Boot into the network configuration section of the online dash
        BootToDash( XLD_LAUNCH_DASHBOARD_NETWORK_CONFIGURATION );
        return FALSE;
    case XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE:
        UIMsg( L"Your Xbox console cannot connect to Xbox Live.\n"
               L"Press A to start the troubleshooter or B to cancel." );
        // A title should allow the player the *option* of booting
        // into the network configuration section of the online dash
        // Boot into the network configuration section of the online dash
        BootToDash( XLD_LAUNCH_DASHBOARD_NETWORK_CONFIGURATION );
        return FALSE;
    case XONLINE_E_LOGON_UPDATE_REQUIRED:
        // XONLINE_E_LOGON_UPDATE_REQUIRED is returned when an 
        // updated version of this title is available on the
        // server and gameplay must not continue until that title
        // is updated. The title must allow the user the option of
        // updating the title before continuing. If the user decides to
        // update immediately, the title is required to call XOnlineTitleUpdate
        // so that the update is downloaded to the hard drive. The
        // XOnlineTitleUpdate function will boot into an "updater 
        // application", which performs the actual update.  Once
        // complete, the updated title will be executed.  Please
        // note the autoupdate is intended as a means of addressing
        // catastrophic defects or security holes in a shipping title, 
        // and is NOT a general purpose update mechanism for adding 
        // new features.
        UIMsg( L"A required update is available for the XBox Live Service.\n"
               L"Press A to update or B to cancel.  You cannot connect\n"
               L"to Xbox Live until the update is installed." );
        XOnlineTitleUpdate( 0 ); 
        assert( FALSE ); // Should not reach here
        return FALSE;
    case XONLINE_E_LOGON_INVALID_USER:
        // One or more users has an unrecognized Gamertag or key.
        // A title should allow the player the *option* of booting
        // into the account management section of the dash.
        Print( L"This account is not current. "
               L"Press A to update the account in Account Recovery or B to cancel." ); 
        // Boot into the account management section of the online dash
        BootToDash( XLD_LAUNCH_DASHBOARD_ACCOUNT_MANAGEMENT );
        assert( FALSE ); // Should not reach here
        return FALSE;
    case XONLINE_E_LOGON_SERVERS_TOO_BUSY:
        UIMsg( L"The Xbox Live service is very busy.\n" 
               L"Press A to try again or press B to cancel." );
        return FALSE;
    default:
        // Some other error - title is free to allow access to dash      
        Print( L"Login failed with error 0x%x", hrTask );
        return FALSE;
    }
    
    // 2. Check for user authentication errors.
    // To check for user authentication errors, we call XOnlineGetLogonUsers.
    // This returns a pointer to an array of XONLINE_USER structures.  This
    // array is similar the User array we populated and passed into
    // XOnlineLogon, but it has the 'hr' field of each XONLINE_USER
    // set with a status code indicating whether or not authentication 
    // for that user succeeded.
    PXONLINE_USER aUsers = XOnlineGetLogonUsers();
    
	// Ensure that we were able to retrieve our array of users
    assert( aUsers != NULL );
    
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        if( aUsers[i].xuid.qwUserID != 0 ) // A valid user
        {            
            // Check authentication results for this user
            switch( aUsers[i].hr )
            {
            case S_OK:
                Print( L"%S signed in", aUsers[i].szGamertag );
                break;
            case XONLINE_S_LOGON_USER_HAS_MESSAGE:
                // Logon succeeded, and user has messages
                Print( L"%S signed in, and has messages", 
                    aUsers[i].szGamertag );
                UIMsg( L"You have a new Xbox Live message.\n"
                       L"Press A to read it now, or B to read later." );
                // The title must allow the option of booting into
                // the account management section of the online dash in order
                // to view the messages
                BootToDash( XLD_LAUNCH_DASHBOARD_ACCOUNT_MANAGEMENT );
                break;
            case XONLINE_E_LOGON_USER_ACCOUNT_REQUIRES_MANAGEMENT:
                // Login failed
                Print( L"This %S account requires management", 
                    aUsers[i].szGamertag );
                UIMsg( L"You have an important message from Xbox Live.\n"
                       L"Press A to read the message." );
                // The title must allow the option of booting into
                // the account management section of the online dash
                BootToDash( XLD_LAUNCH_DASHBOARD_ACCOUNT_MANAGEMENT );
                return FALSE;
            default:
                // Should never happen     
                assert( FALSE );
                return FALSE;
            }            
        }
        
    }
    
    // 3. Finally check the requested services
    for( DWORD i = 0; i < NUM_X_SERVICES; ++i )
    {
        HRESULT hrGetServiceInfo = XOnlineGetServiceInfo( X_SERVICES_ARRAY[i], NULL );
        
        switch( hrGetServiceInfo )
        {
        case S_OK:
            Print( L"Service %lu Available", X_SERVICES_ARRAY[i] );
            break;
        case XONLINE_E_LOGON_SERVICE_NOT_AUTHORIZED:
            // Handle access denial (e.g. Billing service lockout)
            Print( L"Access to service 0x%x is denied", X_SERVICES_ARRAY[i] );
            return FALSE;
        case XONLINE_E_LOGON_SERVICE_TEMPORARILY_UNAVAILABLE:
            // A title can decide how to handle the unavailability
            // of a service.  In some cases, the title may decide
            // to temporarily disable a feature (e.g. content download)
            // for the duration of the sesssion, or may decide to not
            // allow gameplay (e.g. if the matchmaking service was
            // unavailable).
            Print( L"Service %lu is unavailable", X_SERVICES_ARRAY[i] );
            return FALSE;
        default:
            Print( L"Error 0x%x signing onto service %lu", hrGetServiceInfo, 
                   X_SERVICES_ARRAY[i] );
            return FALSE;
        }            
    }   
    
    // Everything is OK at this point.  For each user (except guests)
    // set their online notification state so they are visible to their
    // friends. A real title would also check for the voice peripheral and 
    // specify the XONLINE_FRIENDSTATE_FLAG_VOICE if present.  
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        if( aUsers[i].xuid.qwUserID != 0 && 
            !XOnlineIsUserGuest( aUsers[i].xuid.dwUserFlags ) )
        {

           // attempt to set user's online notification state...
           HRESULT hrSetState = XOnlineNotificationSetState( 
                    i,                               // Controller Index
                    XONLINE_FRIENDSTATE_FLAG_ONLINE, // state flags
                                                     // (just stating user 
                                                     // is online here)
                    XNKID(),                         // this user's current session
                    0,                               // buffer size of title-specific
                                                     // info buffer (not being 
                                                     // used here)
                    NULL                             // buffer for title-specific info
                                                     // (not being used here)
                    );
            
			// Ensure the set state operation succeeded
            assert( SUCCEEDED( hrSetState ) );
        }
    }

    return TRUE;
    
}


//-------------------------------------------------------------------------------------
// Name: PrepareConnection()
// Desc: Demonstrates establishing a connection to the title server
//-------------------------------------------------------------------------------------
BOOL PrepareConnection()
{
	// attempt to create a socket that supports datagrams under the UDP protocol
    g_TitleServerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if( g_TitleServerSocket == INVALID_SOCKET )
    {
        Print( L"Could not create a UDP datagram socket." );
        return FALSE;
    }

    SOCKADDR_IN addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;

    // A title should use port 1000 for primary traffic
    // Port 1000 gives 0 extra port overhead on the wire
    // Ports 1001-1255 give 2 bytes overhead on the wire
    // All other ports give 4 bytes overhead on the wire

	// we'll be using port 1000
	const DWORD TITLE_SOCKET_PORT = 1000;

	// converts port number to TCP/IP byte order (big endian)
    addr.sin_port        = htons( (u_short)TITLE_SOCKET_PORT ); 

	// associates local address to title socket
    int iResultBind = bind( g_TitleServerSocket, (struct sockaddr *) &addr, 
                            sizeof(addr) );
    if( iResultBind == SOCKET_ERROR )
    {
        Print( L"Could not bind local address to title server socket." );
        return FALSE;
    }

	// enable socket to be non-blocking (a non-zero in/out parameter must be provided
	// to allow this)
	const DWORD NON_BLOCKING_SOCKET = 1;
    DWORD dwNonBlocking = NON_BLOCKING_SOCKET;
    INT iResultNonBlock = ioctlsocket(g_TitleServerSocket, FIONBIO, &dwNonBlocking );
    if( iResultNonBlock == SOCKET_ERROR )
    {
        Print( L"Could not enable title server socket to be non-blocking." );
        return FALSE;
    }

#ifdef USE_LOCAL_MACHINE
    // For testing and development set STR_HOST_ADDRESS and PORT to the IP address 
    // and port number of the development machine that is running the title server
    //
    // *** IMPORTANT NOTE ***
    // If you have USE_LOCAL_MACHINE defined as TRUE, and your development machine 
    // gets reassigned to different IP addresses often, you must update 
    // STR_HOST_ADDRESS to reflect your latest IP address every time!
    // **********************

    const char* STR_HOST_ADDRESS = "131.107.62.238";
    const INT   HOST_PORT        = 8001; // use 8001 for local machine

    g_TitleServerAddrIn.sin_family = AF_INET;

	// converts port number to TCP/IP byte order (big endian)
    g_TitleServerAddrIn.sin_port = htons( (u_short)HOST_PORT );

 	// converts string-formatted IP address to DWORD format
    g_TitleServerAddrIn.sin_addr.s_addr = inet_addr( STR_HOST_ADDRESS );

    assert( g_TitleServerAddrIn.sin_addr.s_addr != INADDR_NONE );

#else // USE_LOCAL_MACHINE is not defined

	// we'll be making a query for datasets that match four different attribute
	// sets
    const DWORD NUM_ATTRIBUTE_SPECS = 4;
	enum // the number of types should match the number of attribute specs, above
	{
        ATTSPEC_ENTITY_ID,
        ATTSPEC_TS_ADDR,
        ATTSPEC_SESSION_ID,
        ATTSPEC_KEY_EXCHANGE_KEY
	};

    XONLINE_ATTRIBUTE_SPEC aQueryAttrs[NUM_ATTRIBUTE_SPECS];
    XONLINETASK_HANDLE     hTaskQuery;

    aQueryAttrs[ATTSPEC_ENTITY_ID].dwType          = X_ATTRIBUTE_DATATYPE_ENTITY_ID;
    aQueryAttrs[ATTSPEC_ENTITY_ID].dwLength        = 0;
    aQueryAttrs[ATTSPEC_TS_ADDR].dwType            = XONLINE_LSP_ATTR_TSADDR;
    aQueryAttrs[ATTSPEC_TS_ADDR].dwLength          = sizeof(g_TitleServerAddress);
    aQueryAttrs[ATTSPEC_SESSION_ID].dwType         = XONLINE_LSP_ATTR_XNKID;
    aQueryAttrs[ATTSPEC_SESSION_ID].dwLength       = sizeof(g_SessionID);
    aQueryAttrs[ATTSPEC_KEY_EXCHANGE_KEY].dwType   = XONLINE_LSP_ATTR_KEK;
    aQueryAttrs[ATTSPEC_KEY_EXCHANGE_KEY].dwLength = sizeof(g_KeyExchangeKey);

	const DWORD QUERY_SEARCH_PROCEDURE_INDEX = 1;
	const DWORD RESULT_PAGE_INDEX = 0;
	const DWORD NUM_RESULTS_PER_PAGE = 10; // 255 is the maximum size

    // attempt to query matching attributes
    HRESULT hrQuerySearch = XOnlineQuerySearch( 
            XONLINE_LSP_DEFAULT_DATASET_ID, // ID for default LSP dataset
            QUERY_SEARCH_PROCEDURE_INDEX,   // search procedure index
            RESULT_PAGE_INDEX,              // current page number
            NUM_RESULTS_PER_PAGE,           // number of results per page
            NUM_ATTRIBUTE_SPECS,            // number of competition attributes
                                            //     in the attribute specs array
            aQueryAttrs,                    // competition attribute specs array
            0,                              // number of competition attributes
                                            //     in the attributes array 
                                            //     (not used here)
            NULL,                           // competition attributes array 
                                            //     (not used here)
            NULL,                           // handle to event related to this task 
                                            //     (not used)
            &hTaskQuery                     // address of query task
            );


    if ( FAILED( hrQuerySearch ) )
    {
        Print( L"Failed in attempting to query dataset 0x%x" , XONLINE_LSP_DEFAULT_DATASET_ID );
        return FALSE;
    }

    // Go into a loop, calling XOnlineTaskContinue on the query task
    // until the task completes.  
    // In a real title, this would appear inside your game loop.
	HRESULT hrTask;
    do
    {
        hrTask = XOnlineTaskContinue( hTaskQuery );  // Do a small amount of work
    } 
    while ( hrTask == XONLINETASK_S_RUNNING );  // As long as there is work to do
    
    if ( FAILED( hrTask ) )
    {
        Print( L"Failed in completing query task with error 0x%x" , hrTask );
        return FALSE;
    }

    // Parse the results
    DWORD cTotalResults;
    DWORD cResults;
    DWORD cbResults = sizeof(g_aQueryResults);

	// get the results from the previous XOnlineQuerySearch call
    HRESULT hrGetResults = XOnlineQuerySearchGetResults( hTaskQuery, &cTotalResults, 
                                                         &cResults, &cbResults, 
                                                         (PBYTE)g_aQueryResults );

    if ( FAILED( hrGetResults ) )
    {
        Print( L"Failed in acquiring query results with error 0x%x" , hrGetResults );
        return FALSE;
    }

	// closes task and its resources
    XOnlineTaskClose( hTaskQuery );
    hTaskQuery = NULL;

    // we can't 
    if (cResults == 0)
    {
        Print( L"No results were returned from query search." );
        return FALSE;
    }

    // output the query results
    for (DWORD i = 0; i < cResults; ++i)
    {
        DWORD ipa = g_aQueryResults[i].TitleServerAddress.inaOnline.s_addr;
        Print( L"%d: %I64x %I64x %d.%d.%d.%d", i, g_aQueryResults[i].qwEntityID, 
              *(ULONGLONG *)&g_aQueryResults[i].SessionID, (ipa) & 0xFF, 
			  (ipa >> 8) & 0xFF, (ipa >> 16) & 0xFF, (ipa >> 24) & 0xFF );
    }

    // Choose a TSADDR. Note that we're taking a shortcut here by choosing
    // an address from the first page of results. In a real title you
    // would want to choose from the full set to ensure good load balancing.

    DWORD iRandom;
    XNetRandom((BYTE *)&iRandom, sizeof(iRandom));
    iRandom %= min(NUM_RESULTS_PER_PAGE, cResults);

    memcpy( &g_TitleServerAddress, &g_aQueryResults[iRandom].TitleServerAddress, 
		   sizeof( g_TitleServerAddress ) );
    memcpy( &g_SessionID,          &g_aQueryResults[iRandom].SessionID, 
		   sizeof( g_SessionID) );
    memcpy( &g_KeyExchangeKey,     &g_aQueryResults[iRandom].KeyExchangeKey, 
		   sizeof(g_KeyExchangeKey) );

    // Register the key with XNet
    if ( XNetRegisterKey( &g_SessionID, &g_KeyExchangeKey ) != 0 )
    {
        Print( L"Could not register XNet key for session." );
        return FALSE;
    }

    // Convert title server IP address to a local secure title address
    if ( XNetTsAddrToInAddr( &g_TitleServerAddress, TITLE_SERVER_SERVICE_ID, 
		                     &g_SessionID, &g_TitleServerAddrIn.sin_addr ) != 0 )
    {
        Print( L"Could not create secure address from title server IP address." );
        return FALSE;
    }

    g_TitleServerAddrIn.sin_family = AF_INET;
    g_TitleServerAddrIn.sin_port = htons( TITLE_SERVER_PORT );

#endif // USE_LOCAL_MACHINE

    return TRUE;
}


//-------------------------------------------------------------------------------------
// Name: GameLoop()
// Desc: Demonstrates game loop processing of the logon task handle
//-------------------------------------------------------------------------------------
VOID GameLoop()
{
    DWORD dwTickStart = GetTickCount();

    // Duration of game loop in milliseconds
    const DWORD dwDuration = 30000; // 30 seconds
    const DWORD dwPacketSendInterval = 1500; // 1.5 seconds

    SOCKADDR_IN RemoteInAddr;   
    DWORD dwLastSendTick = dwTickStart;

	const INT STR_MESSAGE_MAX_LEN = 256;
    CHAR strMessage[ STR_MESSAGE_MAX_LEN ];
    INT iMessageLen;
    INT iSendResult;
    INT iReceiveResult;
    INT iPacket = 1; // we'll index packet names in a 1-based fashion

    // Remain signed in for a while
    while( ( GetTickCount() - dwTickStart ) < dwDuration )
    {

        // On the send interval, create and send a UDP packet to
        // the title server
        if( ( GetTickCount() - dwLastSendTick ) >=  dwPacketSendInterval )
        {
            dwLastSendTick = GetTickCount();

            // Prepare some data...
            wsprintfA( strMessage, "Packet %d", iPacket++ );
            Print( L"Sending %S...", strMessage );
            iMessageLen = strlen( strMessage );

			// send that data via the title server socket
            iSendResult = sendto( g_TitleServerSocket, strMessage, iMessageLen, 0, 
                                  (struct sockaddr *) &g_TitleServerAddrIn, 
                                  sizeof(g_TitleServerAddrIn) );
            if( iSendResult == SOCKET_ERROR )
            {
                Print( L"sendto() failed with 0x%x", WSAGetLastError() );
            }
        }

        // Check if there is a packet from the title server
        int iRemoteAddrLen = sizeof(RemoteInAddr);
        iMessageLen = sizeof( strMessage );

		// attempt to receive a packet 
        iReceiveResult = recvfrom( g_TitleServerSocket, strMessage, iMessageLen, 0, 
                                   (struct sockaddr *) &RemoteInAddr, 
								   &iRemoteAddrLen );

		// if the receive call succeeded and bytes were received,
		// report the packet received.
		// NOTE: since packets are not guaranteed to get responses, 
		//       there is no need to error out if there are no
		//       responses.  However, this is a good place to test
		//       if a server, local or not, that CAN send packets
		//       is responding with packets or not.
        if( (iReceiveResult != SOCKET_ERROR) && (iReceiveResult > 0) )
        {
            iMessageLen = iReceiveResult;      // Length of the data received
            strMessage[iMessageLen] = '\0';    // Terminate the data
            Print( L"Packet received: %S", strMessage );
        }

        // Continue pumping the logon task handle in your game loop.
        // Failure to pump the task in a timely manner will result in
        // automatic signoff from the system.  The title should always
        // check the return value to make sure the connection is still
        // established
        HRESULT hrTask = XOnlineTaskContinue( g_hLogonTask );
        
        switch( hrTask )
        {
        case XONLINE_S_LOGON_CONNECTION_ESTABLISHED:  
             // Still signed on
            break;  
        case XONLINE_E_LOGON_CONNECTION_LOST:
            // The connection was lost.
            Print( L"Connection was lost." );
            return;
        case XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON:
            // A title is required to check for the case where a player
            // has been signed out because that same account has been
            // signed on another Xbox.  This is indicated by the
            // XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON error code.
            UIMsg( L"You were signed out of Xbox Live because another\n"
                   L"person signed on using your account.\n"
                   L"Press A to continue." );
            return;
        default:
            // Should never happen
            assert( FALSE );
            return;
        }
    }
}


//-------------------------------------------------------------------------------------
// Name: Print()
// Desc: Send formatted output to the debug window
//-------------------------------------------------------------------------------------
VOID __cdecl Print( const WCHAR* strFormat, ... )
{
    // prepare message buffer
    const INT MAX_OUTPUT_STR = 512;
    WCHAR strBuffer[ MAX_OUTPUT_STR ];
    va_list pArglist;
    
    // output message
    va_start( pArglist, strFormat );   
    INT iChars = wvsprintfW( strBuffer, strFormat, pArglist );
    assert( iChars < MAX_OUTPUT_STR );
    OutputDebugStringW( L"\n*** TitleServerClient: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );
    (VOID)iChars; // avoid compiler warning
    va_end( pArglist );
}


//-------------------------------------------------------------------------------------
// Name: Error()
// Desc: Send formatted output to the debug window and boot back to the dash.
// It is used for reporting fatal errors.
//-------------------------------------------------------------------------------------
VOID __cdecl Error( const WCHAR*strFormat, ... )
{
    // prepare message buffer
    const int MAX_OUTPUT_STR = 512;
    WCHAR strBuffer[MAX_OUTPUT_STR];
    va_list pArglist;

    // output message
    va_start( pArglist, strFormat );
    INT iChars= wvsprintfW( strBuffer, strFormat, pArglist );
    assert( iChars < MAX_OUTPUT_STR );
    OutputDebugStringW( L"\n*** TitleServerClient: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );
    (VOID)iChars;
    va_end( pArglist );

    // sleep for a little but to allow output to complete, 
    // then reboot to the dashboard
    ::Sleep( 10000 );
    BootToDash( XLD_LAUNCH_DASHBOARD_MAIN_MENU );
}


//-------------------------------------------------------------------------------------
// Name: UIMsg()
// Desc: Display a recommended user interface message
//       See Xbox_Terminology_List.xls for additional information.
//-------------------------------------------------------------------------------------
VOID UIMsg( const WCHAR* strText )
{
    OutputDebugStringW( L"\n*** TitleServerClient: UI Message:\n" );
    OutputDebugStringW( strText );
    OutputDebugStringW( L"\n" );
}


//-------------------------------------------------------------------------------------
// Name: BootToDash()
// Desc: Boot back into either the main or online dashes
//-------------------------------------------------------------------------------------
VOID BootToDash( DWORD dwReason )
{
    LD_LAUNCH_DASHBOARD ld;
    ZeroMemory( &ld, sizeof(ld) );
    ld.dwReason = dwReason;
    XLaunchNewImage( NULL, PLAUNCH_DATA( &ld ) );

    // XLaunchNewImage should never return
    assert( FALSE );
}


