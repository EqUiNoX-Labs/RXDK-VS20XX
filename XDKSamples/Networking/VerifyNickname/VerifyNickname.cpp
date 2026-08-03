//-----------------------------------------------------------------------------
// File: VerifyNickname.cpp
//
// Desc: Shows Xbox Nickname Verification.
//
// Hist: 08.14.02 - Created for September Release
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
VOID __cdecl Print( const WCHAR* strFormat, ... );
VOID BootToDash( DWORD dwReason );




//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
XONLINETASK_HANDLE g_hLogonTask;




//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
// This is a the nickname to verify
const WCHAR *g_strNickName = L"happy";



//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    HRESULT hrLogon;

    // Sign in and verify g_strNickName
    if( SignIn() )
    {
        XONLINETASK_HANDLE hVerifyTask;
        
        HRESULT hrVerify = XOnlineVerifyNickname( g_strNickName, NULL, 
                                                  &hVerifyTask );

        if( SUCCEEDED( hrVerify ) )
        {
            Print( L"Verifying nickname...");

            // Pump the verfication task to completion.  Note that
            // we must also continue to pump the main logon task
            // as well.
            do
            {
                hrLogon = XOnlineTaskContinue( g_hLogonTask );
                if( FAILED( hrLogon ) )
                {
                    Print( L"Logon failed with 0x%x", hrLogon );
                    break;
                }
                
                hrVerify = XOnlineTaskContinue( hVerifyTask );

            } while ( hrVerify == XONLINETASK_S_RUNNING );

            if ( SUCCEEDED( hrLogon ) )
            {
                // The result code for the verify task indicates
                // the response from the verification service:
                switch ( hrVerify ) 
                {
                case XONLINETASK_S_SUCCESS:
                    // XONLINETASK_S_SUCCESS indicates that the
                    // nickname has been approved for use.
                    Print( L"Nickname approved");
                    break;
                case XONLINE_E_OFFERING_NAME_TAKEN:
                    // XONLINE_E_OFFERING_NAME_TAKEN indicates that
                    // the nickname cannot be used.
                    Print( L"Nickname cannot be used\n" );
                    break;
                default:
                    Print( L"Nickname verification failed with 0x%x", hrVerify );
                    break;
                }
            }

            XOnlineTaskClose( hVerifyTask );
        }
        
        // Finally, signoff by closing the logon task handle.
        Print( L"Signing off" );
    }

    if( g_hLogonTask )
        XOnlineTaskClose( g_hLogonTask );    
 
    XOnlineCleanup();

    BootToDash( XLD_LAUNCH_DASHBOARD_MAIN_MENU );
}



//-----------------------------------------------------------------------------
// Name: SignIn()
// Desc: Sign in the first user account onto the system.
// Note: In the interest brevity, this sample
//       takes a simplistic approach to authentication.
//       See the SimpleAuth sample for complete example of how to
//       authenticate users.
//-----------------------------------------------------------------------------
BOOL SignIn()
{
    
    // Initialize Input Devices this is required for account enumeration on 
    // Memory Units
    XInitDevices( 0, NULL );

    // Wait for any inserted MUs to mount
    while ( XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY ) {}
    
    // Before any of the Xbox online APIs can be used, XOnlineStartup must be 
    // called. 

    HRESULT hr = XOnlineStartup( NULL );
    assert( SUCCEEDED( hr ) );
    
    // First, obtain a list of user accounts on this Xbox. The XOnlineGetUsers
    // function will enumerate both the hard disk and any attached memory units
    // looking for accounts. 
    XONLINE_USER StoredUsers[ XONLINE_MAX_STORED_ONLINE_USERS ];
    DWORD dwNumStoredUsers;
    
    hr = XOnlineGetUsers( StoredUsers, &dwNumStoredUsers );
    assert( SUCCEEDED( hr ) );
    (VOID)hr; // avoid compiler warning
    
    // If no accounts, then player needs to create an account.
    if( dwNumStoredUsers == 0 )
    {
        Print( L"No user accounts found." );
        return FALSE;
    }
    
    // Sign in the first account on the first controller.
    XONLINE_USER LogonUsers[ XONLINE_MAX_LOGON_USERS ] = { 0 }; // Initially zeroed
    
    LogonUsers[0] = StoredUsers[0]; 
     
    // Initiate the authentication process.  The XOnlineVerifyNickname
    // function requires the nickname verification service, so 
    // we specify a service id of XONLINE_NICKNAME_VERIFICATION_SERVICE.
    const DWORD dwServices[] = { XONLINE_NICKNAME_VERIFICATION_SERVICE };

    hr = XOnlineLogon( LogonUsers, dwServices, 1, 
        NULL, &g_hLogonTask );
    if( hr != S_OK )
    {
        Print( L"XOnlineLogin failed with 0x%x\n", hr );
        return FALSE;
    }

    Print( L"Signing in %S...", LogonUsers[0].szGamertag );

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
    if( hr != XONLINE_S_LOGON_CONNECTION_ESTABLISHED )
    {
        Print( L"Login failed with error 0x%x", hr );
        return FALSE;
    }

    // Check for user authentication errors.
    PXONLINE_USER Users = XOnlineGetLogonUsers();
    
    assert( Users );

    if( FAILED( Users[0].hr ) )
    {
        Print( L"Authentication failed with 0x%x",  Users[0].hr );
        return FALSE;
    }

    
    // Finally check if the verification service was available
    hr = XOnlineGetServiceInfo( XONLINE_NICKNAME_VERIFICATION_SERVICE,
        NULL );

    if( FAILED( hr ) )
    {
        Print( L"Error 0x%x signing onto the verification service" );
        return FALSE;
    }      

    return TRUE;
    
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
    OutputDebugStringW( L"\n*** VerifyNickname: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );
    (VOID)iChars; // avoid compiler warning
    va_end( pArglist );
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



