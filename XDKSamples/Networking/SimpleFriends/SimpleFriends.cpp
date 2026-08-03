//-----------------------------------------------------------------------------
// File: SimpleFriends.cpp
//
// Desc: Illustrates the use of the Xbox Live Friends APIs
//
// Hist: 05.01.03 - New for June release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>                  
#include <xonline.h>              
#include <assert.h> 
#include <stdio.h>

// Xbox Live Friends allow users to maintain a persistent set of contacts
// among all xbox live titles.  This sample shows how to use each of the Friends
// APIs.

// Note that because this is a barebones approach to the API, we will get a 
// potential friend solely from the another account on the harddrive.  To run 
// this sample, you will need something to view the Debug Output channel (such 
// as Visual Studio .NET or xbwatson).  

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
BOOL SignIn( XONLINE_USER*pLogonUsers );
VOID __cdecl Print( const WCHAR*strFormat, ... );
VOID __cdecl Error( const WCHAR*strFormat, ... );
VOID UIMsg( const WCHAR* strText );
VOID BootToDash( DWORD dwReason );

VOID EnumerateFriends( DWORD dwUser, XONLINE_FRIEND *pFriends, DWORD *dwNumFriends, 
                                  XONLINETASK_HANDLE hFriends, XONLINETASK_HANDLE hEnumerate );
VOID FinishEnumeratingFriends( XONLINETASK_HANDLE hFriends, XONLINETASK_HANDLE hEnumerate );

VOID DisplayFriends( CHAR *szGamerTag, XONLINE_FRIEND *pFriends, DWORD dwNumFriends );

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------


// When a title successfully signs in, a task handle is returned that must be
// serviced by the title for the duration of the Xbox Live Session.  The
// global  hLogonTask is used to store this task handle.

XONLINETASK_HANDLE hLogonTask;


//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
VOID __cdecl main()
{
    XONLINE_USER StoredUsers[XONLINE_MAX_STORED_ONLINE_USERS];
    DWORD dwNumStoredUsers;
    XONLINE_USER LogonUsers[XONLINE_MAX_LOGON_USERS]= {0};
    
   // Initialize Input Devices this is required for account enumeration on 
    // Memory Units
    XInitDevices( 0, NULL );
    
    // Before we can enumerate user accounts on any attached Memory Units, we
    // must first allow them sufficient time to mount. 
    while( XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY ){}

    // Before using the XBox Live APIs, a title  must first call XOnlineStartup.     
    // XOnlineStartup will automatically call XNetStartup and WSAStartup with
    // reasonable defaults in order to initialize the Xbox Secure Networking Library and
    // Winsock respectively. If you require special parameters for those functions
    // your title should can call them first before calling
    // XOnlineStartup.
    HRESULT hr = XOnlineStartup( NULL );
    assert( SUCCEEDED( hr ) );
    
    // The XOnlineGetUsers function will enumerate both the hard disk and any     
    // attached memory units looking for user accounts.

    hr = XOnlineGetUsers( StoredUsers, &dwNumStoredUsers );
    assert( SUCCEEDED( hr ) );
    
    // If no accounts were found, a tile must give the player the  option of 
    // going to the online dash to create new account. In addition, it is
    // possible for a player to actually insert/remove an MU while
    // the title account selection UI is active.  A title must
    // call XOnlineGetUsers repeatedly to account for this.
    // For demonstration purposes, we just boot to the account signup section
    // of the online dash if no accounts are found.

    if( dwNumStoredUsers < 2 )
    {
        Print( L"You will need at least 2 accounts to run the SimpleFriends sample.");
        BootToDash( XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP );
    }
    
    LogonUsers[0] = StoredUsers[0];
    LogonUsers[1] = StoredUsers[1];

    // Sign onto the Live Service.  The sample requires no special services
    
    if( SignIn( LogonUsers ) )
    {
        XONLINETASK_HANDLE hFriends, hEnum1, hEnum2;
        XONLINE_FRIEND     User1Friends[ MAX_FRIENDS ];
        XONLINE_FRIEND     User2Friends[ MAX_FRIENDS ];
        DWORD              dwNumUser1Friends;
        DWORD              dwNumUser2Friends;
        DWORD              i;

        // users signed in. First we create the friends task
        
        XOnlineFriendsStartup( NULL, &hFriends );
    
        // Create the enumeration tasks- usually you do this only when displaying
        // the friends page, and close them up afterwords.  Since this sample
        // only deals with friends, we'll open them now, then keep them around
        // until we close down.

        XOnlineFriendsEnumerate( 0, NULL, &hEnum1 );
        XOnlineFriendsEnumerate( 1, NULL, &hEnum2 );                       

        // Enumerate and display friends

        Print( L"---------------------------------------" );
        Print( L"Enumerating Friends");
        EnumerateFriends( 0, User1Friends, &dwNumUser1Friends, hFriends, hEnum1 );              
        EnumerateFriends( 1, User2Friends, &dwNumUser2Friends, hFriends, hEnum2 );

        DisplayFriends( LogonUsers[0].szGamertag, User1Friends, dwNumUser1Friends );
        DisplayFriends( LogonUsers[1].szGamertag, User2Friends, dwNumUser2Friends );
        
        // Add a request to user 2 to be user 1's friend
        // This is a direct API call

        Print( L"---------------------------------------" );
        Print( L"Requesting User 2 to be User 1's friend" );
        
        hr = XOnlineFriendsRequest( 0, LogonUsers[1].xuid );
           
        // you could also request a friend by name, using
        // XOnlineFriendsRequestByName() - this function uses another task handle that you 
        // have to pump until it returns S_OK or an error code      

        if ( hr != S_OK )
        {
            if ( hr == XONLINE_E_NOTIFICATION_LIST_FULL )
            {
                UIMsg( L"Unable to add the specified friend because your friends list is full." );
                Error( L"No more room in friends list" );
            } 
            else if ( hr == XONLINE_E_NOTIFICATION_USER_ALREADY_EXISTS )
            {
                UIMsg( L"That user already exists in your friends list.");
            }
            // XONLINE_E_NOTIFICATION_SELF shouldn't happen here and is provided for illustrative purposes
            else if ( hr == XONLINE_E_NOTIFICATION_SELF )
            {
                UIMsg( L"You can't request yourself as a friend.");
            }                    
            else 
            {                
                UIMsg( L"The server is busy, please try again later.");
                Error( L"Error from XOnlineFriendsRequest.");
            }
                
        }
        else
        {       
            // Now enumerate until we get an update, and display the new lists

            Print( L"Enumerating Friends");
            EnumerateFriends( 0, User1Friends, &dwNumUser1Friends, hFriends, hEnum1 );              
            EnumerateFriends( 1, User2Friends, &dwNumUser2Friends, hFriends, hEnum2 );
    
            DisplayFriends( LogonUsers[0].szGamertag, User1Friends, dwNumUser1Friends );
            DisplayFriends( LogonUsers[1].szGamertag, User2Friends, dwNumUser2Friends );                                
        }

        // Accept the friend request--
        // This is a direct API call
        // I can call it with either
        //      XONLINE_REQUEST_NO - Declines the request. 
        //      XONLINE_REQUEST_YES - Accepts the request. 
        //      XONLINE_REQUEST_BLOCK - Blocks all requests from the user until I initiate one.

        // Search for user 0 (you don't have to do this in a real game- you'd accept based on 
        // the user with the invitation.  Since we know which user invited, we just search for 
        // them directly in the friends list.

        for ( i = 0; i < dwNumUser2Friends; i++ )
        {
            if ( XOnlineAreUsersIdentical( &(User2Friends[i].xuid), &(LogonUsers[0].xuid) ) )
                break;
        }

        if ( i == dwNumUser2Friends )
            Error( L"Couldn't find friend invitation from user 0" );


        Print( L"---------------------------------------" );
        Print( L"Accepting friend request" );
        XOnlineFriendsAnswerRequest( 1, &( User2Friends[ i ] ), XONLINE_REQUEST_YES );

        // Now enumerate until we get an update, and display the new lists

        Print( L"Enumerating Friends");
        EnumerateFriends( 0, User1Friends, &dwNumUser1Friends, hFriends, hEnum1 );              
        EnumerateFriends( 1, User2Friends, &dwNumUser2Friends, hFriends, hEnum2 );

        DisplayFriends( LogonUsers[0].szGamertag, User1Friends, dwNumUser1Friends );
        DisplayFriends( LogonUsers[1].szGamertag, User2Friends, dwNumUser2Friends );                    

        Print( L"---------------------------------------" );
        Print( L"Removing friend" );

        // Search for user 1 (you don't have to do this in a real game- you'd choose the user
        // that was selected for remove in the friends list.
        
        for ( i = 0; i < dwNumUser1Friends; i++ )
        {
            if ( XOnlineAreUsersIdentical( &(User1Friends[i].xuid), &(LogonUsers[1].xuid) ) )
                break;
        }

        if ( i == dwNumUser2Friends )
            Error( L"Couldn't find user 1 in user 0's friend list" );

        XOnlineFriendsRemove( 0, &( User1Friends[ i ] ) );

        // Now enumerate until we get an update, and display the new lists

        Print( L"Enumerating Friends");
        EnumerateFriends( 0, User1Friends, &dwNumUser1Friends, hFriends, hEnum1 );              
        EnumerateFriends( 1, User2Friends, &dwNumUser2Friends, hFriends, hEnum2 );

        DisplayFriends( LogonUsers[0].szGamertag, User1Friends, dwNumUser1Friends );
        DisplayFriends( LogonUsers[1].szGamertag, User2Friends, dwNumUser2Friends );                    
               
        // Game invitations:

        // Here are descriptions of each API, but they aren't used since we're not actually in a session
        // For more information see the full friends sample.
        

        //   XOnlineFriendsGameInvite( 0, SessionID, dwNumUser1Friends, User1Friends );
        //      This would invite all of User 0's friends to the game if we had a session ID.
        //      You can invite single people by just passing in the XONLINE_USER * of the 
        //      target player and a count of 1.

        //  XOnlineFriendsRevokeGameInvite( 0, SessionID, dwNumUser1Friends, User1Friends );
        //      This revokes a previous game invite because the user changed their mind, etc.
        //      This is nice so all of their friends haven't already shut down their games 
        //      only to find the user is no longer playing.  The format is the same as for
        //      XOnlineFriendsGameInvite above.    

        //   XOnlineFriendsAnswerGameInvite() is similar to AnswerRequest above, and is
        //      used to notify the inviting player of your decision.  It also writes out
        //      a structure appropriate for XOnlineFriendsGetAcceptedGameInvite()

        //   XOnlineFriendsJoinGame( 0, &( User1Friends[0] ) )
        //      XOnlineFriendsJoinGame would be used to join a game that is different than the current one:
        //      It will write out a structure appropriate for XOnlineFriendsGetAcceptedGameInvite() and
        //      return.  You'd then put up a UI message along the lines of "Please insert the disc for <titlename>" 
        //      (whatever the title of the accepted game is.  Use XOnlineFriendsGetTitleName() as shown
        //      in DisplayFriends())
          
        //  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        //  XOnlineFriendsGetAcceptedGameInvite() should ALWAYS be called when first initializing an 
        //      online game, before XOnlineLogon or XOnlineFriendsStartup, to determine if a cross-title game
        //      invite has been initiated.  This is demonstrated in the Matchmaking sample

        // A title signs off users by calling XOnlineTaskClose on the
        // task handle returned by XOnlineLogon.  Another situation in which users
        // are signed off is if the Xbox Live Service realizes the task handle 
        // returned by XOnlineLogon is not being serviced by the title (
        // e.g. the user turned the console off).        
        
        Print( L"Signing off..." );

        FinishEnumeratingFriends( hFriends, hEnum1 );
        FinishEnumeratingFriends( hFriends, hEnum2 );
        XOnlineTaskClose( hFriends );
        XOnlineTaskClose( hLogonTask );
    }
    
    // When a title is through with the XBox Live APIs, it can call XOnlineCleanup
    // to perform final cleanup for the online functions.
    XOnlineCleanup();
    
    ::Sleep( 10000 ); // Wait for any debug output to finish
    BootToDash( XLD_LAUNCH_DASHBOARD_MAIN_MENU );
}



//-----------------------------------------------------------------------------
// Name: EnumerateFriends()
// Desc: Enumerate Friends until we get results available
//-----------------------------------------------------------------------------
VOID EnumerateFriends( DWORD dwUser, XONLINE_FRIEND *pFriends, DWORD *dwNumFriends, 
                                  XONLINETASK_HANDLE hFriends, XONLINETASK_HANDLE hEnumerate )
{
    HRESULT hr;

    // After initially retrieving the friends list, continue to pump the handle
    // while the friends list is being displayed to the user. Pumping the handle
    // processes updates to the list. When an updated list is available, 
    // XOnlineTaskContinue returns XONLINETASK_S_RESULTS_AVAIL. When the title 
    // receives this return value, it should call one of the "GetLatest" functions
    
    hr = XONLINETASK_S_RUNNING;
    while ( hr == XONLINETASK_S_RUNNING )
    {
        // have to pump logon and friends while enumerating 
        // In a real game, this would be part of your game loop- you wouldn't block on
        // this.

        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr )) Error( L"Logon task failed with 0x%x", hr );
        hr = XOnlineTaskContinue( hFriends );
        if ( FAILED( hr )) Error( L"Friends task failed with 0x%x", hr );

        hr = XOnlineTaskContinue( hEnumerate );
    }
    
    if ( hr != XONLINETASK_S_RESULTS_AVAIL )
    {        
        UIMsg( L"Error retrieving friends list; try again later" );
        Error( L"Failed enumeration of friends" );
    }

    // Here you can use any of the APIs to get the friends list--
    //    XOnlineFriendsGetLatestByFocus() and XOnlineFriendsGetLatestByRange() will
    //    return partial lists.  For this sample, we're just going to get the whole
    //    list

    *dwNumFriends = XOnlineFriendsGetLatest( dwUser, MAX_FRIENDS, pFriends );
}

//-----------------------------------------------------------------------------
// Name: FinishEnumerateFriends()
// Desc: Close up enumeration task
//-----------------------------------------------------------------------------

VOID FinishEnumeratingFriends( XONLINETASK_HANDLE hFriends, XONLINETASK_HANDLE hEnumerate )
{    
    // when done enumerating, you should call EnumerateFinish, then pump the handle until
    // XONLINETASK_S_SUCCESS is returned

    HRESULT hr;

    XOnlineFriendsEnumerateFinish( hEnumerate );
    
    hr = XONLINETASK_S_RUNNING;
    while ( hr == XONLINETASK_S_RUNNING )
    {
        // have to pump logon and friends while enumerating 
        // In a real game, this would be part of your game loop- you wouldn't block on
        // this.

        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr )) Error( L"Logon task failed with 0x%x", hr );
        hr = XOnlineTaskContinue( hFriends );
        if ( FAILED( hr )) Error( L"Friends task failed with 0x%x", hr );

        hr = XOnlineTaskContinue( hEnumerate );
    }
    
    if ( hr != XONLINETASK_S_SUCCESS )
        Error( L"Error closing down friends task" );

    XOnlineTaskClose( hEnumerate );
}


//-----------------------------------------------------------------------------
// Name: DisplayFriends()
// Desc: Display Friends and status information
//-----------------------------------------------------------------------------
VOID DisplayFriends( CHAR *szGamerTag, XONLINE_FRIEND *pFriends, DWORD dwNumFriends )
{
    DWORD i;
    WCHAR wszOutputData[ 1000 ];
    WCHAR wszPartialData[ 1000 ];   

    Print( L"%S's friends:", szGamerTag );
    
    for ( i = 0; i < dwNumFriends; i++ )
    {   
        swprintf( wszOutputData, L"   %S", pFriends[i].szGamertag );

        if ( pFriends[i].dwFriendState & XONLINE_FRIENDSTATE_FLAG_ONLINE )
        {            
            swprintf( wszPartialData, L" is online" );
            wcscat( wszOutputData, wszPartialData );
        }
        else
        {            
            swprintf( wszPartialData, L" is offline" );
            wcscat( wszOutputData, wszPartialData );
        }

        if ( pFriends[i].dwFriendState & XONLINE_FRIENDSTATE_FLAG_INVITEACCEPTED )
        {            
            swprintf( wszPartialData, L", has accepted %S's invitation", szGamerTag );
            wcscat( wszOutputData, wszPartialData );
        }

        if ( pFriends[i].dwFriendState & XONLINE_FRIENDSTATE_FLAG_INVITEREJECTED )
        {            
            swprintf( wszPartialData, L", has rejected %S's invitation", szGamerTag );
            wcscat( wszOutputData, wszPartialData );
        }    

        if ( pFriends[i].dwFriendState & XONLINE_FRIENDSTATE_FLAG_JOINABLE )
        {            
            swprintf( wszPartialData, L", is joinable");
            wcscat( wszOutputData, wszPartialData );
        }

        if ( pFriends[i].dwFriendState & XONLINE_FRIENDSTATE_FLAG_RECEIVEDREQUEST )
        {            
            swprintf( wszPartialData, L", wants to be %S's friend", szGamerTag );
            wcscat( wszOutputData, wszPartialData );
        }

        if ( pFriends[i].dwFriendState & XONLINE_FRIENDSTATE_FLAG_SENTREQUEST )
        {            
            swprintf( wszPartialData, L", has a pending friend request from %S", szGamerTag );
            wcscat( wszOutputData, wszPartialData );
        }
        
        if ( pFriends[i].dwFriendState & XONLINE_FRIENDSTATE_FLAG_PLAYING )
        {            
            swprintf( wszPartialData, L", is playing in title" );
            wcscat( wszOutputData, wszPartialData );

            // This function will get the english title name from a title ID

            XOnlineFriendsGetTitleName( pFriends[i].dwTitleID, XC_LANGUAGE_ENGLISH, 1000, wszPartialData );
            wcscat( wszOutputData, wszPartialData );
        }

        if ( pFriends[i].dwFriendState & XONLINE_FRIENDSTATE_FLAG_VOICE )
        {            
            swprintf( wszPartialData, L", has voice" );
            wcscat( wszOutputData, wszPartialData );
        }

        if ( pFriends[i].dwFriendState & XONLINE_FRIENDSTATE_FLAG_RECEIVEDINVITE )
        {            
            swprintf( wszPartialData, L", has invited %s to play", szGamerTag );
            wcscat( wszOutputData, wszPartialData );
        }
     
        if ( pFriends[i].dwFriendState & XONLINE_FRIENDSTATE_FLAG_SENTINVITE )
        {            
            swprintf( wszPartialData, L", has been invited by %s to play", szGamerTag );
            wcscat( wszOutputData, wszPartialData );
        }

        Print( wszOutputData );
    }
}



//-----------------------------------------------------------------------------
// Name: Print()
// Desc: Send formatted output to the debug window
//-----------------------------------------------------------------------------
VOID __cdecl Print( const WCHAR* strFormat, ... )
{
    const int MAX_OUTPUT_STR = 512;
    WCHAR strBuffer[MAX_OUTPUT_STR];
    va_list pArglist;
    va_start( pArglist, strFormat );
    INT iChars= wvsprintfW( strBuffer, strFormat, pArglist );
    assert( iChars < MAX_OUTPUT_STR );
    OutputDebugStringW( L"\n*** SimpleFriends: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );
    ( VOID ) iChars;
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
    OutputDebugStringW( L"\n*** SimpleFriends: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );
    ( VOID ) iChars;
    va_end( pArglist );
    ::Sleep( 10000 ); // Wait for output to complete
    BootToDash( XLD_LAUNCH_DASHBOARD_MAIN_MENU );
}




//-----------------------------------------------------------------------------
// Name: UIMsg()
// Desc: Display a recommended user interface message
//       See Xbox_Terminology_List.xls for additional information.
//-----------------------------------------------------------------------------
VOID UIMsg( const WCHAR* strText )
{
    OutputDebugStringW( L"\n*** SimpleFriends: UI Message:\n" );
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
    ZeroMemory( &ld, sizeof( ld ) );
    ld.dwReason = dwReason;
    XLaunchNewImage( NULL, PLAUNCH_DATA( &ld ) );
    // XLaunchNewImage should never return
    assert( FALSE );
}

