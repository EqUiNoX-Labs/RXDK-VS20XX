//-----------------------------------------------------------------------------
// File: SignIn.cpp
//
// Desc: User Authentication
// Hist: 03.15.03 - New for April release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <assert.h>
#include <xonline.h>



//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
VOID __cdecl Print( const WCHAR* strFormat, ... );
VOID UIMsg( const WCHAR * strText );
VOID BootToDash( DWORD dwReason );




//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern XONLINETASK_HANDLE hLogonTask;

//-----------------------------------------------------------------------------
// Name: SignIn()
// Desc: Demonstrate Xbox online authentication
// Authentication takes place in three stages:  First, the Xbox itself it
// authenticated, and then one or more specified users (and possibly guests)  
// accounts.  Up to four players can be signed onto a console.  
// Finally, both the Xbox and the users are authenticated against
// the requested services (e.g. matchmaking).
//-----------------------------------------------------------------------------
BOOL SignIn( XONLINE_USER *pLogonUsers )
{
    const DWORD Services[] = { 
        XONLINE_STATISTICS_SERVICE
    };
    const DWORD dwNumServices = sizeof( Services ) / sizeof( Services[0] );
     
    // Initiate the authentication process.  The signon process
    // first authenticates the Xbox.  Next, it authenticates each
    // user, and finally authenticates against the requested services
    // (validating that both the users *and* the Xbox have access to them).
    // All three stages are handled by the client APIs, though the title
    // is required to check for errors and handle them appropriately.
    HRESULT hr = XOnlineLogon( pLogonUsers, Services, dwNumServices, NULL, &hLogonTask );
     
    // Check for errors
    switch( hr )
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
#pragma message( "TCR: Title must give the option of accessing the " \
                "network configuration of the online dash." )
#endif
        break;
    default:
        // This should never happen
        assert( FALSE );
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
        hr = XOnlineTaskContinue( hLogonTask );  // Do a small amount of work
    }  
    while ( hr == XONLINETASK_S_RUNNING );  // As long as there is work to do
     
    // Titles must check for, and handle, authentication errors in the
    // following order:
    // 1. System authentication errors.
    // 2. User authentication errors.
    // 3. Service authentication errors.
    // It important to check for, and handle user errors before service
    // errors.  Consider the case where there is an account maintenace issue  
    // for a user, AND a requested service is unavailable.   A title must
    // allow the user to deal with the account issues before service issues
    // (especially since an account issue could be the cause of
    // the service issue).
     
    // 1. Check for system authentication errors.
    switch( hr )
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
        // into the account management section of the online dash
        Print( L"This account is not current. Press A to update the account in Account Recovery or B to cancel." );  
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
        Print( L"Login failed with error 0x%x", hr );
        return FALSE;
    }
     
    // 2. Check for user authentication errors.
    // To check for user authentication errors, we call XOnlineGetLogonUsers.
    // This returns a pointer to an array of XONLINE_USER structures.  This
    // array is similar the User array we populated and passed into
    // XOnlineLogon, but it has the 'hr' field of each XONLINE_USER
    // set with a status code indicating whether or not authentication  
    // for that user succeeded.
    PXONLINE_USER Users = XOnlineGetLogonUsers();
     
    assert( Users );
     
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        if( Users[i].xuid.qwUserID != 0 ) // A valid user
        {
            DWORD dwUserFlags = Users[i].xuid.dwUserFlags;
             
            BOOL bGuest = XOnlineIsUserGuest( dwUserFlags );

            // Check authentication results for this user
            switch( Users[i].hr )
            {
            case S_OK:
                if( bGuest )
                    Print( L"Guest %d of %S signed in",  
                        XOnlineUserGuestNumber( dwUserFlags ),
                        Users[i].szGamertag );
                else
                    Print( L"%S signed in", Users[i].szGamertag );
                break;
            case XONLINE_S_LOGON_USER_HAS_MESSAGE:
                // Logon succeeded, and user has messages
                Print( L"%S signed in, and has messages",  
                    Users[i].szGamertag );
                UIMsg( L"You have a new Xbox Live message.\n"
                       L"Press A to read it now, or B to read later." );
                // The title must allow the option of booting into
                // the account management section of the online dash in order
                // to view the messages
                // BootToDash( XLD_LAUNCH_DASHBOARD_ACCOUNT_MANAGEMENT );
                break;
            case XONLINE_E_LOGON_USER_ACCOUNT_REQUIRES_MANAGEMENT:
                // Login failed
                Print( L"This %S account requires management",  
                    Users[i].szGamertag );
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
     
   // Everything is OK at this point.  For each user (except guests)
    // set their online notification state so they are visible to their
    // friends. A real title would also check for the voice peripheral and 
    // specify the XONLINE_FRIENDSTATE_FLAG_VOICE if present.  
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        if( Users[i].xuid.qwUserID != 0 && 
            !XOnlineIsUserGuest( Users[i].xuid.dwUserFlags ) )
        {
            hr = XOnlineNotificationSetState( i,   // Controller index
                XONLINE_FRIENDSTATE_FLAG_ONLINE, XNKID(), 0, NULL );
            
            assert( SUCCEEDED( hr ) );
        }
    }

     
    return TRUE;
     
}

