//-----------------------------------------------------------------------------
// File: SimpleTeams.cpp
//
// Desc: Illustrates the use of the Xbox Live Teams APIs
//
// Hist: 09.25.03 - New for November release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>                  
#include <xonline.h>              
#include <assert.h> 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//-----------------------------------------------------------------------------
// Defines and constants
//-----------------------------------------------------------------------------

const DWORD SIMPLETEAMS_BUFFER_SIZE = 100;

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
BOOL SignIn( XONLINE_USER*pLogonUsers, const DWORD *pdwServiceIDs, DWORD dwNumServices );
VOID __cdecl Print( const WCHAR*strFormat, ... );
VOID __cdecl Error( const WCHAR*strFormat, ... );
VOID UIMsg( const WCHAR* strText );
VOID BootToDash( DWORD dwReason );

VOID DisplayTeamsForPlayer( DWORD dwUser );
VOID WaitForTaskToComplete( XONLINETASK_HANDLE hTask );
VOID WaitForABit( DWORD dwNumMilliseconds );



//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------


// When a title successfully signs in, a task handle is returned that must be
// serviced by the title for the duration of the Xbox Live Session.  The
// global  hLogonTask is used to store this task handle.

XONLINETASK_HANDLE hLogonTask;
XONLINE_USER LogonUsers[XONLINE_MAX_LOGON_USERS]= {0};

const DWORD USER_0 = 0;
const DWORD USER_1 = 1;




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------

VOID __cdecl main()
{
    XONLINE_USER StoredUsers[XONLINE_MAX_STORED_ONLINE_USERS];
    DWORD dwNumStoredUsers;
    
    // random seed for team name
    srand( time( 0 ) );
   
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
        Print( L"You will need at least 2 accounts to run the SimpleTeams sample.");
        BootToDash( XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP );
    }
    
    // For SimpleTeams, we just take the first two players on the xbox

    LogonUsers[ USER_0 ] = StoredUsers[ USER_0 ];
    LogonUsers[ USER_1 ] = StoredUsers[ USER_1 ];

    // Sign onto the Live Service.  The sample requires no special services
    
    DWORD dwNumServices = 1;

    const DWORD pdwServices[] = { XONLINE_TEAM_SERVICE };

    if( SignIn( LogonUsers, pdwServices, dwNumServices ) )
    {   
        XONLINE_TEAM_PROPERTIES         TeamProps;
        XONLINE_TEAM_MEMBER_PROPERTIES  TeamMemberProps;
        XONLINE_TEAM                    Team;
        XONLINETASK_HANDLE              hTask;

        DisplayTeamsForPlayer( USER_0 );

        //----------------
        // Create a team
        //-----------------
        Print( L"Creating team" );

        ZeroMemory( &TeamProps, sizeof( TeamProps ) );
        ZeroMemory( &TeamMemberProps, sizeof( TeamMemberProps ) );
            
        // Copy in the strings for creating a team
        // create a random team name, since several people will be running this
        swprintf( TeamProps.wszTeamName, L"CoolTeam%d", rand() );
        wcscpy( TeamProps.wszDescription, L"Coolest Team of any sample!" );
        wcscpy( TeamProps.wszMotto, L"You are hot, we are cool!" );
        wcscpy( TeamProps.wszURL, L"http:\\\\www.xbox.com" );
        
        // Can have extra, title-specific data - none for this sample
        TeamProps.TeamDataSize = 0;

        // The server will override this and give the owner all privileges
        TeamMemberProps.dwPrivileges = 0;
        
        // Can have extra data on a per team member basis - none for this sample
        TeamMemberProps.TeamMemberDataSize = 0;
        
        // create the team
        if ( FAILED( XOnlineTeamCreate( 0, &TeamProps, &TeamMemberProps, 100, NULL, &hTask ) ) )
            Error( L"Error creating team" );

        // wait for the task to complete- in a real game this would be part of the game loop
        WaitForTaskToComplete( hTask );
        
        // get the team result
        if ( FAILED( XOnlineTeamCreateGetResults( hTask, &Team ) ) )
            Error( L"Error getting team create results" );

        XOnlineTaskClose( hTask );

        // display the teams
        DisplayTeamsForPlayer( USER_0 );

        //----------------
        // Set properties on a team
        //-----------------

        Print( L"Changing Team Properties.." );
        wcscpy( TeamProps.wszMotto, L"Now with more coolness!" );        

        // Call XOnlineTeamSetProperties with the new motto
        if ( FAILED( XOnlineTeamSetProperties( USER_0, Team.xuidTeam, &TeamProps, NULL, &hTask ) ) )
            Error( L"Error setting team properties" );

        WaitForTaskToComplete( hTask );
        XOnlineTaskClose( hTask );

        // Display the new teams
        DisplayTeamsForPlayer( USER_0 );
        DisplayTeamsForPlayer( USER_1 );

        //------------------
        // Recruit a member to a team
        //------------------
        
        // To recruit a member, you need to create a XONLINE_TEAM_MEMBER_PROPERTIES structure
        // for them.  This includes title specific data and a privilege level.
        // The privilege level is only enforced by the title
        ZeroMemory( &TeamMemberProps, sizeof( TeamMemberProps ) );
        TeamMemberProps.dwPrivileges = XONLINE_TEAM_RECRUIT_MEMBERS;
        TeamMemberProps.TeamMemberDataSize = 0;
        
        Print( L" Recruiting User 1 to the team ");

        // XOnlineTeamMemberRecruit sends a message to the recruited player inviting them to join the team
        // you can get a message handle back here if you want to add custom message properties
        XOnlineTeamMemberRecruit( USER_0, Team.xuidTeam, LogonUsers[ USER_1 ].xuid, &TeamMemberProps, NULL, NULL, &hTask );
        WaitForTaskToComplete( hTask );
        XOnlineTaskClose( hTask );
      
        // because we are demonstrating teams and not messaging, we won't loop here until user 1 gets a message.
        // instead, we'll just wait a little while and then accept the invite.  Normally you would process the message,
        // let user 1 read it, and accept or decline to join the team.  See the SimpleMessaging sample for an example
        // of how to process messages
        Print( L" Waiting for 10 seconds - in a game we'd get a team recruitment message and show it to User 1" );
        WaitForABit( 10000 );

        //------------------
        // Answering a recruit message
        //------------------

        Print( L" Joining the team as User 1" );

        // Answer the recruitment message (that we just sent from User 0)
        XOnlineTeamMemberAnswerRecruit( USER_1, Team.xuidTeam, XONLINE_PEER_ANSWER_YES, NULL, &hTask );
        WaitForTaskToComplete( hTask );
        XOnlineTaskClose( hTask );
        
        DisplayTeamsForPlayer( USER_1 );

        //------------------
        // Display team status 
        //------------------

        DWORD               dwNumTeamMembers;
        XONLINETASK_HANDLE  hPresenceTask, hTeamTask;
        XUID                xTeamMembers[ XONLINE_MAX_TEAM_MEMBER_COUNT ];        
        XONLINE_PRESENCE    PresenceInfo[ XONLINE_MAX_TEAM_MEMBER_COUNT ];
        XONLINE_TEAM_MEMBER TeamMemberInfo;
        WCHAR               wszStatus[ SIMPLETEAMS_BUFFER_SIZE ];
        WCHAR               wszPrivilege[ SIMPLETEAMS_BUFFER_SIZE];
        WCHAR               wszTitle[ SIMPLETEAMS_BUFFER_SIZE];
        DWORD i;

        Print( L" Enumerating team members" );
        
        // Start enumerating team members
        XOnlineTeamMembersEnumerate( USER_0, Team.xuidTeam, 0, NULL, &hTeamTask );
        WaitForTaskToComplete( hTeamTask );
        
        // Get list of xuids
        XOnlineTeamMembersEnumerateGetResults( hTeamTask, &dwNumTeamMembers, xTeamMembers );

        Print( L" Getting presence info" );
        
        // initialize presence
        hr = XOnlinePresenceInit( USER_0, NULL, &hPresenceTask );
        if( FAILED( hr ) )
            Error( L"Error initializing presence, hr = 0x%x", hr );
        
        // you can create several groups- we're adding everyone to group '0'
        hr = XOnlinePresenceAdd( hPresenceTask, 0, dwNumTeamMembers, xTeamMembers ); 
        if( FAILED( hr ) )
            Error( L"Could not add team members to presence query, hr = 0x%x", hr );
       
        // submit task 
        hr = XOnlinePresenceSubmit( hPresenceTask );
        if ( FAILED( hr ) )
            Error( L"Error getting presence submit, hr = 0x%x", hr );
        WaitForTaskToComplete( hPresenceTask );

        // get the latest presence info, now that we have results
        hr = XOnlinePresenceGetLatest( hPresenceTask, 0, dwNumTeamMembers, PresenceInfo );
        if ( FAILED( hr ) )
            Error( L"Error getting presence info" );
        
        // loop through the team members, and display data on each one
        for( i = 0; i < dwNumTeamMembers; i++ )
        {
            // Get the details for this teammember (name, privileges, etc)
            hr = XOnlineTeamMemberGetDetails( hTeamTask, xTeamMembers[ i ], &TeamMemberInfo );
            if ( FAILED( hr ) )
                Error( L"Error getting details on team members" );
        
            // create a string representing their privilege level
            if ( TeamMemberInfo.TeamMemberProperties.dwPrivileges & XONLINE_TEAM_DELETE_MEMBER )
                wcscpy( wszPrivilege, L"Owner" );
            else if ( TeamMemberInfo.TeamMemberProperties.dwPrivileges & XONLINE_TEAM_RECRUIT_MEMBERS )
                wcscpy( wszPrivilege, L"Recruit" );
            else 
                wcscpy( wszPrivilege, L"Peon" );

            // Create a string representing their logon status - this may be offline
            // because we haven't allowed enough time for our presence to fully propogate in this sample
            
            if ( PresenceInfo[ i ].dwUserState & XONLINE_PRESENCE_FLAG_PLAYING )
                wcscpy( wszStatus, L"Playing" );
            else if ( PresenceInfo[ i ].dwUserState & XONLINE_PRESENCE_FLAG_ONLINE )
                wcscpy( wszStatus, L"Online" );            
            else 
                wcscpy( wszStatus, L"Offline" );

            // get the title name
            XOnlinePresenceGetTitleName( hPresenceTask, PresenceInfo[ i ].dwTitleID, XC_LANGUAGE_ENGLISH, SIMPLETEAMS_BUFFER_SIZE, wszTitle );            

            // display teammember info 
            if ( PresenceInfo[ i ].dwUserState & XONLINE_PRESENCE_FLAG_ONLINE ) 
            {    
                Print( L" %S: %s, %s in %s", TeamMemberInfo.szGamertag, wszPrivilege, wszStatus, wszTitle );                        
            }
            else
            {
                Print( L" %S: %s, %s", TeamMemberInfo.szGamertag, wszPrivilege, wszStatus );                        
            }
        }

        XOnlineTaskClose( hTeamTask );
        XOnlineTaskClose( hPresenceTask );

        //------------------
        // Remove a user
        //------------------

        // We're going to remove user 1 from the team now                

        Print( L" Removing user 1 from the team" );        
        hr = XOnlineTeamMemberRemove( USER_0, Team.xuidTeam, LogonUsers[ USER_1 ].xuid, NULL, &hTask );        
        if( FAILED( hr ) )
            Error( L"Could not remove a member from the team- hr = 0x%x", hr );
        WaitForTaskToComplete( hTask );
        XOnlineTaskClose( hTask );

        DisplayTeamsForPlayer( USER_1 );

        //------------------
        // Delete a team
        //------------------

        // delete the team        

        Print( L"Deleting Team" );
        hr =  XOnlineTeamDelete( USER_0, Team.xuidTeam, NULL, &hTask );
        if( FAILED( hr ) )
            Error( L"Could not delete the team- hr = 0x%x", hr );
        WaitForTaskToComplete( hTask );    
        XOnlineTaskClose( hTask );

        DisplayTeamsForPlayer( USER_0 );


        // close logon task

        XOnlineTaskClose( hLogonTask );
    }
    
    // When a title is through with the XBox Live APIs, it can call XOnlineCleanup
    // to perform final cleanup for the online functions.
    XOnlineCleanup();
    
    ::Sleep( 10000 ); // Wait for any debug output to finish
    BootToDash( XLD_LAUNCH_DASHBOARD_MAIN_MENU );
}




//-----------------------------------------------------------------------------
// Name: DisplayTeamsForPlayer()
// Desc: Display information on teams a player is part of 
//-----------------------------------------------------------------------------
VOID DisplayTeamsForPlayer( DWORD dwUser )
{      
    XUID                xuidTeams[ XONLINE_MAX_TEAM_COUNT ];    
    XONLINE_TEAM        Team;    
    DWORD               dwTeamCount, i;
    XONLINETASK_HANDLE  hTask;
    

    Print( L"User %d teams:", dwUser );

    // enumerate user's teams
    if ( FAILED( XOnlineTeamEnumerateByUserXUID( dwUser, LogonUsers[ dwUser ].xuid, NULL, &hTask ) ) )
    {        
        Error( L"Could not enumerate teams" );
    }

    // wait for the enumeration to complete
    WaitForTaskToComplete( hTask );

    dwTeamCount = XONLINE_MAX_TEAM_COUNT;

    // get results of the enumeration
    if ( FAILED( XOnlineTeamEnumerateGetResults( hTask, &dwTeamCount, xuidTeams ) ) )
    {
        XOnlineTaskClose( hTask );
        Error( L"Could not get enumeration results" );
    }
    
    Print( L" on %d Teams", dwTeamCount );
    for( i = 0; i < dwTeamCount; i++ )
    {
        XOnlineTeamGetDetails( hTask, xuidTeams[ i ], &Team );        
               
        Print( L"  Team %s: \"%s\"", Team.TeamProperties.wszTeamName, Team.TeamProperties.wszMotto );
    }

    XOnlineTaskClose( hTask );
    
}




//-----------------------------------------------------------------------------
// Name: WaitForTaskToComplete
// Desc: Helper function for waiting for a Teams task to complete
//       In a real game, this would be part of your game loop
//-----------------------------------------------------------------------------
VOID WaitForTaskToComplete( XONLINETASK_HANDLE hTask )
{
    HRESULT hr = XONLINETASK_S_RUNNING;

    while ( hr == XONLINETASK_S_RUNNING )
    {
        // have to pump logon and the task
        // In a real game, this would be part of your game loop- you wouldn't block on
        // this.
        
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr )) 
            Error( L"Logon task failed with 0x%x", hr );
        
        hr = XOnlineTaskContinue( hTask );        
        if ( FAILED( hr )) 
            Error( L"Teams task failed with 0x%x", hr );        
        
        // put in a delay of about 1 frame so as not to spam with MessageSendGetProgress
        Sleep( 15 );
    }
    
    if (FAILED(hr))
            Error( L"Task failed with 0x%x", hr );       
}




//-----------------------------------------------------------------------------
// Name: WaitForABit
// Desc: Helper function for waiting while pumping the task handle
//-----------------------------------------------------------------------------
VOID WaitForABit( DWORD dwNumMilliseconds )
{
    HRESULT hr = XONLINETASK_S_RUNNING;
    
    DWORD time = timeGetTime();

    Print( L"Pumping online task handle for %d milliseconds", dwNumMilliseconds );

    while ( timeGetTime() - time < dwNumMilliseconds  )
    {
        // have to pump logon and the task
        // In a real game, this would be part of your game loop- you wouldn't block on
        // this.
        
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr )) Error( L"Logon task failed with 0x%x", hr );
       
        // put in a delay of about 1 frame - not because it is necessary, but 
        // because it mimics actual conditions
        Sleep( 15 );
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
    OutputDebugStringW( L"\n*** SimpleTeams: " );
    OutputDebugStringW( strBuffer ); 
    OutputDebugStringW( L"\n" );
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
    OutputDebugStringW( L"\n*** SimpleTeams: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n" );
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
    OutputDebugStringW( L"\n*** SimpleTeams: UI Message:\n" );
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

