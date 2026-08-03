//-----------------------------------------------------------------------------
// File: SilentAuth.cpp
//
// Desc: Shows Xbox online silent authentication protocols.
//
// Hist: 05.13.03 - Created for June release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <stdio.h>
#include <assert.h>
#include <xonline.h>



//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
BOOL SignIn();
VOID GameLoop();
VOID __cdecl Print( const WCHAR* strFormat, ... );
VOID __cdecl Error( const WCHAR*strFormat, ... );
VOID BootToDash( DWORD dwReason );




//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
XONLINETASK_HANDLE g_hLogonTask = NULL;




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    // Initialize Input Devices this is required for account enumeration on 
    // Memory Units
    XInitDevices( 0, NULL );


    // Wait for any inserted MUs to mount
    while ( XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY ) {}
    
    // Before any of the Xbox online APIs can be used, XOnlineStartup must be 
    // called.  XOnlineStartup automatically calls XNetStartup and 
    // WSAStartup with default parameters in order to initialize the 
    // Xbox Secure Network Libary and the Winsock layer. To specify non-default
    // startup parameters for XNetStartup or WSAStartup, call those functions 
    // prior to calling XOnlineStartup.
    
    HRESULT hr = XOnlineStartup( NULL );
    assert( SUCCEEDED( hr ) );
    (VOID)hr; // avoid compiler warning
    if( SignIn() )
    {
        GameLoop();
        
        if( g_hLogonTask )
        {
            // Finally, signoff by closing the logon task handle.
            Print( L"Signing off" );
            XOnlineTaskClose( g_hLogonTask );    
        }
    }
    
    XOnlineCleanup();    
    BootToDash( XLD_LAUNCH_DASHBOARD_MAIN_MENU );
}




//-----------------------------------------------------------------------------
// Name: SignIn()
// Desc: Demonstrate Xbox online silent authentication
// Authentication takes place in three stages:  First, the Xbox itself it
// authenticated, and then one or more specified users (and possibly guests) 
// accounts.  Up to four players can be signed onto a console. 
// Finally, both the Xbox and the users are authenticated against
// the requested services (e.g. matchmaking).
//-----------------------------------------------------------------------------
BOOL SignIn()
{
    

    // Now, add whatever services are appropriate for your title, but no
    // more. Each service requires additional authentication time
    // and network traffic.  For demonstration purposes, the
    // matchmaking service is specified.  Additional services ids are
    // specified in xonline.h.
    
    const DWORD Services[] = { XONLINE_MATCHMAKING_SERVICE };
    const DWORD dwNumServices = sizeof( Services ) / sizeof( Services[0] );
    
    // Initiate the authentication process.  The silent signon process
    // first authenticates the Xbox.  Next, it selects the most recently used
    // account and authenticates that user.
    // Finally it authenticates against the requested services
    // (validating that both the user *and* the Xbox have access to them).
    // All three stages are handled by the client APIs, though the title
    // is required to check for errors and handle them appropriately.
    HRESULT hr = XOnlineSilentLogon( Services, dwNumServices, NULL, &g_hLogonTask ); 
    
    // Check for errors
    // A title can decide how to respond to errors.  For example, it might
    // decide to continue without signing on the user.
    switch( hr )
    {
    case S_OK:   
        // XOnlineSilentLogon succeeded
        break; 
    case XONLINE_E_SILENT_LOGON_DISABLED:
        Error( L"Silent Logon is not enabled on this console!" );
        break;
    case XONLINE_E_SILENT_LOGON_NO_ACCOUNTS:
        Error( L"No User Accounts Found!" );
        break;
    case XONLINE_E_SILENT_LOGON_PASSCODE_REQUIRED:
        Error( L"Silent Logon cannot proceed: Account requires passcode entry!" );
        break;
    case XONLINE_E_LOGON_NO_NETWORK_CONNECTION:
        // Sign on failed because no network connection was
        // detected.  A title must give the player the
        // option of accessing the network configuration of the online dash
        Print( L"No network connection detected." );
        break;
    default:
        // This should never happen
        assert( FALSE);
        return FALSE;
    }
    
    
    // If sucessful, an asynchronous task handle (XONLINETASK_HANDLE) will
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
    do
    {
        hr = XOnlineTaskContinue( g_hLogonTask );  // Do a small amount of work
    } 
    while ( hr == XONLINETASK_S_RUNNING );  // As long as there is work to do
    
    
    // Check for system authentication errors.
    switch( hr )
    {
    case XONLINE_S_LOGON_CONNECTION_ESTABLISHED:  
        // The Xbox has been validated and there are no system authentication
        // errors
        Print( L"Connection established" );
        break;
    case XONLINE_E_LOGON_CONNECTION_LOST:
        Error( L"Network connection lost" );
        return FALSE;
    case XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE:
        Error( L"Cannot access service" );
        return FALSE;
    case XONLINE_E_LOGON_UPDATE_REQUIRED:
        // XONLINE_E_LOGON_UPDATE_REQUIRED is returned when an 
        // updated version of this title is available on the
        // server.  For silent sign-in, this should be treated as a
        // failure to sign-in the user.
        Error( L"An auto update is required to continue signing in" );
        return FALSE;
    case XONLINE_E_LOGON_INVALID_USER:
        // The user has an unrecognized Gamertag or key.
        // This should be treated as a failure to sign-in.
        Error( L"This account is not current" ); 
        return FALSE;
    case XONLINE_E_LOGON_SERVERS_TOO_BUSY:
        Error( L"The Xbox Live service is very busy" ); 
        return FALSE;
    default:
        // Some other error - title is free to allow access to dash      
        Error( L"Login failed with error 0x%x", hr );
        return FALSE;
    }
    
    // Check for user authentication errors.
    // To check for user authentication errors, we call XOnlineGetLogonUsers.
    // This returns a pointer to an array of XONLINE_USER structures.  The
    // array in controller order and has the 'hr' field of each XONLINE_USER
    // set with a status code indicating whether or not authentication 
    // for that user succeeded.  Since there is only one user, this
    // information is in the first entry.
    PXONLINE_USER Users = XOnlineGetLogonUsers();
    
    assert( Users );
    
    // Check authentication results for this user
    switch( Users[0].hr )
    {
    case S_OK:
        Print( L"%S signed in", Users[0].szGamertag );
        break;
    case XONLINE_S_LOGON_USER_HAS_MESSAGE:
        // Ignore for silent sign-in
        break;
    case XONLINE_E_LOGON_USER_ACCOUNT_REQUIRES_MANAGEMENT:
        // Login failed
        Error( L"This %S account requires management", 
            Users[0].szGamertag );
        return FALSE;
    default:
        // Should never happen     
        assert( FALSE );
        return FALSE;
    }
    
    // Finally check the requested services.  A title
    // may choose to gracefully degrade functionality based
    // on service availability
    for( DWORD i = 0; i < dwNumServices; ++i )
    {
        hr = XOnlineGetServiceInfo( Services[i], NULL );
        
        switch( hr )
        {
        case S_OK:
            Print( L"Service %lu Available", Services[i] );
            break;
        case XONLINE_E_LOGON_SERVICE_NOT_AUTHORIZED:
            // Handle access denial (e.g. Billing service lockout)
            Print( L"Access to service %lu is denied", Services[i] );
            return FALSE;
        case XONLINE_E_LOGON_SERVICE_TEMPORARILY_UNAVAILABLE:
            // A title can decide how to handle the unavailability
            // of a service.  In some cases, the title may decide
            // to temporarily disable a feature (e.g. content download)
            // for the duration of the sesssion, or may decide to not
            // allow gameplay (e.g. if the matchmaking service was
            // unavailable).
            Print( L"Service %lu is unavailable", Services[i] );
            return FALSE;
        default:
            Print( L"Error 0x%x signing onto service %lu", hr, Services[i] );
            return FALSE;
        }            
    }   
    
    // Everything is OK at this point.  
    // Set user online notification state so they are visible to their
    // friends. A real title would also check for the voice peripheral and 
    // specify the XONLINE_FRIENDSTATE_FLAG_VOICE if present.  
    hr = XOnlineNotificationSetState( 0,   // Controller index
        XONLINE_FRIENDSTATE_FLAG_ONLINE, XNKID(), 0, NULL );
            
    assert( SUCCEEDED( hr ) );

    return TRUE;
    
}



//-----------------------------------------------------------------------------
// Name: GameLoop()
// Desc: Demonstrates game loop processing of the logon task handle
//-----------------------------------------------------------------------------
VOID GameLoop()
{
    DWORD dwTickStart = GetTickCount();
    // Duration of game loop in milliseconds
    const DWORD dwDuration = 15000; // 15 seconds

    // Remain signed in for a while
    while( ( GetTickCount() - dwTickStart ) < dwDuration )
    {
        // Continue pumping the logon task handle in your game loop.
        // Failure to pump the task in a timely manner will result in
        // automatic signoff from the system.  The title should always
        // check the return value to make sure the connection is still
        // established
        HRESULT hr = XOnlineTaskContinue( g_hLogonTask );
        
        switch( hr )
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
            Print( L"You were signed out of Xbox Live because another "
                   L"person signed on using your account.\n" );
            return;
        default:
            // Should never happen
            assert( FALSE );
            return;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: Print()
// Desc: Send formatted output to the debug window
//-----------------------------------------------------------------------------
VOID __cdecl Print( const WCHAR* strFormat, ... )
{
    const int MAX_OUTPUT_STR = 512;
    WCHAR strBuffer[ MAX_OUTPUT_STR ];
    va_list pArglist;
    
    va_start( pArglist, strFormat );   
    INT iChars = wvsprintfW( strBuffer, strFormat, pArglist );
    assert( iChars < MAX_OUTPUT_STR );
    OutputDebugStringW( L"\n*** SilentAuth: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );
    (VOID)iChars; // avoid compiler warning
    va_end( pArglist );
}




//-----------------------------------------------------------------------------
// Name: Error()
// Desc: Send formatted output to the debug window and boot back to the dash.
// It is used for reporting fatal errors.
//-----------------------------------------------------------------------------
VOID __cdecl Error( const WCHAR*strFormat, ... )
{
    const int MAX_OUTPUT_STR = 512;
    WCHAR strBuffer[MAX_OUTPUT_STR];
    va_list pArglist;
    va_start( pArglist, strFormat );
    INT iChars= wvsprintfW( strBuffer, strFormat, pArglist );
    assert( iChars < MAX_OUTPUT_STR );
    OutputDebugStringW( L"\n*** SilentAuth: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );
    ( VOID ) iChars;
    va_end( pArglist );
    ::Sleep( 10000 );
    BootToDash( XLD_LAUNCH_DASHBOARD_MAIN_MENU );
}





//-----------------------------------------------------------------------------
// Name: BootToDash()
// Desc: Boot back into either the main or online dashes
//-----------------------------------------------------------------------------
VOID BootToDash( DWORD dwReason )
{
    LD_LAUNCH_DASHBOARD ld;
    ZeroMemory( &ld, sizeof(ld) );
    ld.dwReason = dwReason;
    XLaunchNewImage( NULL, PLAUNCH_DATA( &ld ) );
    // XLaunchNewImage should never return
    assert( FALSE );
}



