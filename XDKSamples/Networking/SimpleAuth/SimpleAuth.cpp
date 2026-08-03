//-----------------------------------------------------------------------------
// File: SimpleAuth.cpp
//
// Desc: Shows Xbox online authentication protocols.
//
// Hist: 11.01.01 - Created for December release
//       04.22.02 - New for May release
//       07.16.02 - Updated for Aug release
//       03.11.03 - Updated for April release
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
VOID ChangeLogonUsers();
VOID __cdecl Print( const WCHAR* strFormat, ... );
VOID UIMsg( const WCHAR * strText );
VOID BootToDash( DWORD dwReason );




//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
XONLINETASK_HANDLE g_hLogonTask;




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
        
        // Demonstrate how to use XOnlineChangeLogonUsers for changing 
        // the users signed on to the system without tearing down the
        // existing session
        ChangeLogonUsers();

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
// Desc: Demonstrate Xbox online authentication
// Authentication takes place in three stages:  First, the Xbox itself it
// authenticated, and then one or more specified users (and possibly guests) 
// accounts.  Up to four players can be signed onto a console. 
// Finally, both the Xbox and the users are authenticated against
// the requested services (e.g. matchmaking).
//-----------------------------------------------------------------------------
BOOL SignIn()
{
    
    // First, obtain a list of user accounts on this Xbox. The XOnlineGetUsers
    // function will enumerate both the hard disk and any attached memory units
    // looking for accounts. 
    XONLINE_USER StoredUsers[ XONLINE_MAX_STORED_ONLINE_USERS ];
    DWORD dwNumStoredUsers;
    
    HRESULT hr = XOnlineGetUsers( StoredUsers, &dwNumStoredUsers );
    assert( SUCCEEDED( hr ) );
    (VOID)hr; // avoid compiler warning
    
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
    if( StoredUsers[0].dwUserOptions & XONLINE_USER_OPTION_REQUIRE_PASSCODE )
    {
#ifdef XBOX_SAMPLE
        Print(L"%S has a passcode", StoredUsers[0].szGamertag );
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
    XONLINE_USER LogonUsers[ XONLINE_MAX_LOGON_USERS ] = { 0 }; // Initially zeroed
    
    // Sign in the first account on the first controller.
    LogonUsers[0] = StoredUsers[0]; 
    // Next, add a guest player on controller 2
    // A guest account is specified by copying the sponsor account
    // into the controller array and then setting one of the guest bits
    // in the dwUserFlags of the XUID.
    LogonUsers[1] = StoredUsers[0];    // First, copy the sponsor account info
    
    // Next, set one of the guest bits of the XUID by calling
    // XOnlineSetUserGuestNumber.
    // The second parameter indicates the guest number for the sponsor.  It can
    // be 1, 2, or 3, since there can be up to three guest accounts.  It
    // actually doesn't matter which of these values we use, only that no two 
    // players use the same guest number for the same sponsor.
    XOnlineSetUserGuestNumber( LogonUsers[1].xuid.dwUserFlags, 1 );
    
    // Any unused users must remain zeroed out.
    // Here, LogonUsers[2] and LogonUsers[3] are still zeroed-out indicating
    // that no players will be signed in on those controllers.
    
    
    // Now, add whatever services are appropriate for your title, but no
    // more. Each service requires additional authentication time
    // and network traffic.  For demonstration purposes, the
    // matchmaking service is specified.  Additional services ids are
    // specified in xonline.h.
    
    const DWORD Services[] = { XONLINE_MATCHMAKING_SERVICE };
    const DWORD dwNumServices = sizeof( Services ) / sizeof( Services[0] );
    
    // Initiate the authentication process.  The signon process
    // first authenticates the Xbox.  Next, it authenticates each
    // user, and finally authenticates against the requested services
    // (validating that both the users *and* the Xbox have access to them).
    // All three stages are handled by the client APIs, though the title
    // is required to check for errors and handle them appropriately.
    hr = XOnlineLogon( LogonUsers, Services, dwNumServices, NULL, &g_hLogonTask );
    
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
#pragma message("TCR: Title must give the option of accessing the " \
                "network configuration of the online dash.")
#endif
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
               L"Press A to start the troubleshooter or B to cancel.");
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
                       L"Press A to read it now, or B to read later.");
                // The title must allow the option of booting into
                // the account management section of the online dash in order
                // to view the messages
                BootToDash( XLD_LAUNCH_DASHBOARD_ACCOUNT_MANAGEMENT );
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
            
            // Check for user permissions
            // Guests may not use voice and may not purchase
            if( !bGuest )
            {
                if( XOnlineIsUserVoiceAllowed( dwUserFlags ) )
                        Print( L"    %S is allowed to use voice", 
                            Users[i].szGamertag );
                
                if(XOnlineIsUserPurchaseAllowed( dwUserFlags ) )
                        Print( L"    %S is allowed to purchase", 
                            Users[i].szGamertag );

                if( !XOnlineIsUserNicknameAllowed( dwUserFlags ) )
                    Print( L"    %S is nickname banned", 
                        Users[i].szGamertag );
            }
            else
            {
                if( !XOnlineIsUserNicknameAllowed( dwUserFlags ) )
                    Print( L"    Guest %d of %S is nickname banned", 
                        XOnlineUserGuestNumber( dwUserFlags ),
                        Users[i].szGamertag );
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




//-----------------------------------------------------------------------------
// Name: ChangeLogonUsers()
// Desc: Demonstrate the use of the XOnlineChangeLogonUsers function for 
//       changing the set of users currently signed on without tearning down
//       the original session
//-----------------------------------------------------------------------------
VOID ChangeLogonUsers()
{
    HRESULT hr;
    XONLINE_USER NewUsers[ XONLINE_MAX_STORED_ONLINE_USERS ];
    XONLINETASK_HANDLE hChangeLogonTask;

    Print( L"Signing off guest account..." );

    memcpy( NewUsers, XOnlineGetLogonUsers(), sizeof( NewUsers ) );

    // Remove the user at controller 1 (the guest account)
    ZeroMemory( &NewUsers[1], sizeof( XONLINE_USER ) );

    // Initiate the process of modifying the signed on users
    hr = XOnlineChangeLogonUsers( NewUsers, NULL, &hChangeLogonTask );

    // Check for errors
    switch( hr )
    {
    case S_OK:   
        // XOnlineChangeLogonUsers succeeded
        break;
    case XONLINE_E_NO_GUEST_ACCESS:
        // Host sponsor for guest is not present
        // Not possible in this sample since we
        // are removing a guest account
        assert( FALSE);
        return;
    case XONLINE_E_OUT_OF_MEMORY:
        // Not enough memory to create the change task
        // The original users are still signed on
        Print( L"Out of memory" );
        return;
    case XONLINE_E_TASK_BUSY:
        // Another logon task is currently being performed, 
        // or the previous logon task has not been closed.
        // Not possible for this sample
        assert( FALSE);
        return;
    default:
        // This should never happen
        assert( FALSE);
        return;
    }

    BOOL bUserChangeCommitted = FALSE;

    // Pump the change task until complete.  Titles are also
    // required to pump the main logon task as well.
    do
    {
        hr = XOnlineTaskContinue( g_hLogonTask ); 
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
            UIMsg( L"You were signed out of Xbox Live because another\n"
                   L"person signed on using your account.\n"
                   L"Press A to continue." );
            return;
        default:
            // Should never happen
            assert( FALSE );
            return;
        }

        // Process the change task
        hr = XOnlineTaskContinue( hChangeLogonTask );

        // The process of changing the signed on users can be
        // a two phase process (depending on the nature of the
        // change).  If the change task returns XONLINE_S_LOGON_COMMIT_USER_CHANGE
        // then the second phase of the change has started.  From this
        // point on, a failure will require the that the existing logon session
        // be terminated.  
        if( hr == XONLINE_S_LOGON_COMMIT_USER_CHANGE )
        {
            bUserChangeCommitted = TRUE;
        }

    } while( hr == XONLINETASK_S_RUNNING || 
             hr == XONLINE_S_LOGON_COMMIT_USER_CHANGE );

    XOnlineTaskClose( hChangeLogonTask );

    // Check for errors
    switch( hr )
    {
    case XONLINE_S_LOGON_USER_CHANGE_COMPLETE:
        Print( L"Guest account successfully signed off." );
        break;
    case XONLINE_E_LOGON_CHANGE_USER_FAILED:
        // If this error occurs before the change task has reached the
        // XONLINE_S_LOGON_COMMIT_USER_CHANGE phase, then existing users
        // can continue to play. Otherwise, the existing logon session
        // must be terminated.
        if( bUserChangeCommitted )
        {
            Print( L"Failed to sign off guest account. " 
                   L"Existing logon session must be terminated. " );
            XOnlineTaskClose( g_hLogonTask );  
            g_hLogonTask = NULL;
        }
        else
        {
            Print( L"Failed to sign off guest account. " 
                   L"Existing logon session is still valid. " );
        }
        break;
    case XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE:
        // One or more problems were encountered trying to log onto the
        // online service. If this error occurs before the change task
        // has reached the XONLINE_S_LOGON_COMMIT_USER_CHANGE phase,
        // then existing users can continue to play. Otherwise, the existing
        // logon session must be terminated and the user should be given the
        // option to reboot into the network troubleshooting portion of the 
        // Xbox Dashboard.
        if( bUserChangeCommitted )
        {
            Print( L"Failed to sign off guest account. " 
                   L"Unable to access the service.\n" 
                   L"Existing logon session must be terminated. " );
            XOnlineTaskClose( g_hLogonTask );  
            g_hLogonTask = NULL;
            UIMsg( L"Your Xbox console cannot connect to Xbox Live.\n"
                   L"Press A to start the troubleshooter or B to cancel.");
            // A title should allow the player the *option* of booting
            // into the network configuration section of the online dash
            // Boot into the network configuration section of the online dash
            BootToDash( XLD_LAUNCH_DASHBOARD_NETWORK_CONFIGURATION );
        }
        else
        {
            Print( L"Failed to sign off guest account. " 
                   L"Unable to access the service.\n" 
                   L"Existing logon session is still valid." );
        }
        break;
    case XONLINE_E_LOGON_SERVERS_TOO_BUSY:
        Print( L"Failed to sign off guest account. " 
               L"Existing logon session must be terminated." );
        XOnlineTaskClose( g_hLogonTask );  
        g_hLogonTask = NULL;
        UIMsg( L"The Xbox Live service is very busy.\n" 
               L"Press A to try again or press B to cancel." );
        break;
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
        break;
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
    OutputDebugStringW( L"\n*** SimpleAuth: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );
    (VOID)iChars; // avoid compiler warning
    va_end( pArglist );
}




//-----------------------------------------------------------------------------
// Name: UIMsg()
// Desc: Display a recommended user interface message
//       See Xbox_Terminology_List.xls for additional information.
//-----------------------------------------------------------------------------
VOID UIMsg( const WCHAR* strText )
{
    OutputDebugStringW( L"\n*** SimpleAuth: UI Message:\n" );
    OutputDebugStringW( strText );
    OutputDebugStringW( L"\n" );
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



