//-----------------------------------------------------------------------------
// File: SimpleUnitStats.cpp
//
// Desc: Illustrates the use of the Xbox Live Unit Statistics APIs
//
// Hist: 11.24.03 - New for December release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>                  
#include <xonline.h>              
#include <assert.h> 

// The Xbox Live Statistics service allows a title to store and retrieve 
// statistical information about units playing a game title. The type of
// statistical information stored depends on the nature of the title and is
// defined by the title developer. 
// The statistics are stored on network servers, and are accessible to instances
// of the game title running on any Xbox console on the network. Therefore, the 
// unit statistics are available to a player regardless of the Xbox they are
// playing on. 


//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

//
// A leaderboard (also referred to as a scoreboard) is an ordered, title-specific
// listing of players and their statistics. When a title stores unit 
// statistics with the Statistics Service, the title specifies a leader board
// with which to associate the statistics. Each leader board is identified by a 
// unique non-zero ID.  
// For demonstration purposes a single leaderboard is used.  By default a title
// may use up to 20 leaderboards.  If your title requires addition leaderboards,
// or you wish to support attachments. please contact Xbox Developer Support
//  (xboxds@xbox.com).  

// This sample maintains a single leaderboard with an ID of 'LEADERBOARD_ID'

const DWORD LEADERBOARD_ID = 1;

//
// Each leaderboard entry consists of one or more pieces of statistical 
// information represented by an XONLINE_STAT structure.  These pieces of
// information are identified by an ID.  Some of these attributes are
// common to all titles and are given reserved IDs, while others are
// specific to a title.  These are the IDs used by this sample:
enum
{
    STAT_ID_KILLS =1,          // Number of Kills
    STAT_ID_DEATHS,            // Number of Deaths
    STAT_ID_ASSISTS,           // Number of Assists
    STAT_ID_ACCURACY           // Shooting accuracy
};

// It is important to realize that leaderboards do not have inherent structure
// as, for example, a database table.  It is useful to think for each entry
// of a leaderboard as a property bag.





//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
BOOL SignIn( XONLINE_USER*pLogonUsers );
VOID __cdecl Print( const WCHAR*strFormat, ... );
VOID __cdecl Error( const WCHAR*strFormat, ... );
VOID UIMsg( const WCHAR* strText );
VOID BootToDash( DWORD dwReason );
VOID WriteUnitStatistics();
VOID ReadUnitStatistics();
VOID EnumerateUnitLeaderboard( XUID );




//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------


// When a title successfully signs in, a task handle is returned that must be
// serviced by the title for the duration of the Xbox Live Session.  The
// global  hLogonTask is used to store this task handle.
XONLINETASK_HANDLE hLogonTask;

// Number of players selected for signin
DWORD NumPlayersInUnit;


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

    if( dwNumStoredUsers == 0 )
    {
        Print( L"No user accounts found." );
        BootToDash( XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP );
    }


    // Atleast two users are required to form a unit
    if( dwNumStoredUsers < 2 )
    {
        Error( L"There must be atleast two users to form a unit. Create another account." );
    }

    NumPlayersInUnit = min( XONLINE_STAT_MAX_MEMBERS_IN_UNIT, dwNumStoredUsers );

    for( DWORD i = 0; i < NumPlayersInUnit; ++i )
    {
        Print( L"User %S selected for logon", StoredUsers[i].szGamertag );
        LogonUsers[i] = StoredUsers[i];
    }



    // Sign onto the Live Service.  The sample requires the following
    // services:
    //      XONLINE_STATISTICS_SERVICE  -  For reading/write stats

    if( SignIn( LogonUsers ) )
    {
        srand( GetTickCount() ); // for picking random stat values

        // Write game statistics to a leaderboard
        WriteUnitStatistics();

        // Read game statistics from a leaderboard
        ReadUnitStatistics();

        // Enumerate units with a particular member
        Print( L"Enumerating unit leaderboard entries with %S as a member...",
            LogonUsers[0].szGamertag );
        EnumerateUnitLeaderboard( LogonUsers[0].xuid );


        // A title signs off users by calling XOnlineTaskClose on the
        // task handle returned by XOnlineLogon.  Another situation in which users
        // are signed off is if the Xbox Live Service realizes the task handle 
        // returned by XOnlineLogon is not being serviced by the title (
        // e.g. the user turned the console off).

        Print( L"Signing off..." );
        XOnlineTaskClose( hLogonTask );
    }

    // When a title is through with the XBox Live APIs, it can call XOnlineCleanup
    // to perform final cleanup for the online functions.
    XOnlineCleanup();

    ::Sleep( 10000 ); // Wait for any debug output to finish
    BootToDash( XLD_LAUNCH_DASHBOARD_MAIN_MENU );
}




//-----------------------------------------------------------------------------
// Name: WriteUnitStatistics()
// Desc: Write unit statistics to a leaderboard
//-----------------------------------------------------------------------------
VOID WriteUnitStatistics()
{

    // This sample maintains a single leaderboard with the following
    // attributes:
    //              Kills           The number of kills in a session
    //              Deaths          The number of times the player died
    //              Assists         The number of assists the player provided
    //              Accuracy        Shooting accuracy (0 - 100%)


    // When writing statistics for a unit, 
    // a title supplies a rating to the Live Service, which
    // is then used to rank that gamer against others on the leaderboard.
    // The service, and not the title, assigns the actual rank for
    // the gamer.
    //

    // The XONLINE_STAT structure is used to represent a piece
    // of statistical information associated with a leaderboard
    // entry.  This sample will write five pieces of information
    // for a leaderboard entry.  This information is specified
    // by passing 'UnitAttributes',  an array of XONLINE_STAT structures,
    // to the XOnlineStatWrite function.  This array is indexed by the
    // following enumeration:

    enum
    {
        STAT_KILLS,
        STAT_DEATHS,
        STAT_ASSISTS,
        STAT_ACCURACY,
        STAT_RATING,
        NUM_LEADERBOARD_ATTRIBUTES
    };

    XONLINE_STAT UnitAttributes[ NUM_LEADERBOARD_ATTRIBUTES ];



    // The XOnlineStatWriteEx function is used to write statistical
    // information.  
    //
    // For demonstration purposes, this sample will write statistics for
    // a unit user to a single leaderboard, using the
    // XONLINE_STAT_PROCID_UPDATE_REPLACE procedure to completely replace
    // existing statistics with new values.  For some attributes, such as
    // kills or deaths, titles would actually want to use the
    // XONLINE_STAT_PROCID_UPDATE_INCREMENT procedure instead to add to the
    // existing values (or subtract, if the increment is negative).  Titles
    // may also choose to update players' ratings via the Elo scoring
    // system by using the XONLINE_STAT_PROCID_ELO procedure.  Powerful
    // server-evaluated 'if then' statements are even possible by using
    // XONLINE_STAT_PROCID_CONDITIONAL procedures.  If using conditionals,
    // the procedure(s) that depend on one MUST come after it in the array,
    // since all procedures are processed in order by the server.  The
    // dependent Replace, Increment or Elo procedure would then specify the
    // one-based index into the array of that previous conditional in the
    // dwConditionalIndex field in the appropriate union structure.
    // Procedures that are not dependent upon conditionals should specify 0,
    // like in this sample.
    //
    // The statistics service requires that the users be signed onto the
    // console from which write requests are made.  To meet this
    // requirement, this sample will treat all signed on users as
    // a unit and write stats on their behalf.


    XONLINE_USER      *pUsers = XOnlineGetLogonUsers();


    // The XOnlineStatWritEx function takes an array of XONLINE_STAT_PROC
    // structures.  Each of these entries specifies the unit members, the
    // leaderboard (by ID), and an array on XONLINE_STAT 
    // structures, which contain the information for that entry:
    //
    // +-------------------+      +----------------+----------------+
    // | XONLINE_STAT_PROC |      |  XONLINE_STAT  |  XONLINE_STAT  |
    // |     xuidMembers   |      |      wID       |      wID       |
    // |      pStats ------+----->|      type      |      type      |...
    // |    dwNumStats     |      |   union value  |   union value  |
    // |  dwLeaderBoardID  |      +----------------+----------------+
    // +-------------------+
    //
    // Since this sample only writes statistics for a single unit and 
    // a single leaderboard, there is only one element in the XONLINE_STAT_PROC
    // array 'StatProc'. 

    XONLINE_STAT_PROC StatProc;

    // Compute some random statistics
    LONG lKills       = rand() % 20;
    LONG lDeaths      = rand() % 20;
    LONG lAssists     = rand() % 20;

    // The rating is just a 64-bit number.  The larger the number,
    // the better the rating.  This sample, uses the following
    // formula for calculating the rating.
    LONGLONG llRating = 100*lKills + 10*lAssists - 5*lDeaths;
    DOUBLE dAccuracy  = DOUBLE( rand() % 101 );
    StatProc.wProcedureID = XONLINE_STAT_PROCID_UPDATE_REPLACE_UNIT;

    ZeroMemory(&(StatProc.UpdateUnit.xuidUnitMembers),
        sizeof(StatProc.UpdateUnit.xuidUnitMembers));

    // Fill the xuidUnitMembers with the XUIDs for the members
    // of the unit
    for( DWORD i = 0; i < NumPlayersInUnit; ++i )
    {
        StatProc.UpdateUnit.xuidUnitMembers[i] = pUsers[i].xuid;
    }

    StatProc.UpdateUnit.dwLeaderBoardID = LEADERBOARD_ID;
    StatProc.UpdateUnit.dwConditionalUnitIndex = 0;
    StatProc.UpdateUnit.dwNumStats = NUM_LEADERBOARD_ATTRIBUTES;
    StatProc.UpdateUnit.pStats = UnitAttributes;

    // Populate the UnitAttributes array with the statistical information
    // The XONLINE_STAT structure needs three pieces of information:
    //
    // * The ID of the attribute.  This is a 16 bit unsigned value.
    //   Avoid larger values (0xF000 and above)
    //   since those values are reserved.
    //
    // * The Type of the attribute.  The type can be:
    //     XONLINE_STAT_LONG      a 32-bit integer
    //     XONLINE_STAT_LONGLONG  a 64-bit integer
    //     XONLINE_STAT_DOUBLE    a 64-bit (IEEE) real number
    //
    // * The Data.  The XONLINE_STAT contains an anonymous union
    //

    // Specify the Kills attribute (an integer)
    UnitAttributes[ STAT_KILLS ].wID = STAT_ID_KILLS;
    UnitAttributes[ STAT_KILLS ].type = XONLINE_STAT_LONG;
    UnitAttributes[ STAT_KILLS ].lValue = lKills;

    // Specify the Deaths attribute (an integer)
    UnitAttributes[ STAT_DEATHS ].wID = STAT_ID_DEATHS;
    UnitAttributes[ STAT_DEATHS ].type = XONLINE_STAT_LONG;
    UnitAttributes[ STAT_DEATHS ].lValue = lDeaths;

    // Specify the Assists attribute (an integer)
    UnitAttributes[ STAT_ASSISTS ].wID = STAT_ID_ASSISTS;
    UnitAttributes[ STAT_ASSISTS ].type = XONLINE_STAT_LONG;
    UnitAttributes[ STAT_ASSISTS ].lValue = lAssists;

    // Specify the Accuracy attribute (a real number)
    UnitAttributes[ STAT_ACCURACY ].wID = STAT_ID_ACCURACY;
    UnitAttributes[ STAT_ACCURACY ].type = XONLINE_STAT_DOUBLE;
    UnitAttributes[ STAT_ACCURACY ].dValue = dAccuracy;

    // Finally, specify the rating for entry using the
    // reserved XONLINE_STAT_RATING attribute id.  The rating
    // is a 64-bit integer, so XONLINE_STAT_LONGLONG is specified
    // for the attribute type.
    UnitAttributes[ STAT_RATING ].wID = XONLINE_STAT_RATING;
    UnitAttributes[ STAT_RATING ].type = XONLINE_STAT_LONGLONG;
    UnitAttributes[ STAT_RATING ].llValue = llRating;


    Print( L"Writing Statistics: Kills: %ld Deaths: %ld Assists: %ld Accuracy: %lf",
        UnitAttributes[ STAT_KILLS ].lValue,   
        UnitAttributes[ STAT_DEATHS ].lValue, UnitAttributes[ STAT_ASSISTS ].lValue, 
        UnitAttributes[ STAT_ACCURACY ].dValue
        );

    // Initiate the writing process.  This will return a task handle
    XONLINETASK_HANDLE hWriteTask;

    HRESULT hr = XOnlineStatWriteEx( 1, &StatProc, NULL, &hWriteTask );

    if( FAILED( hr ) ) Error( L"XOnlineStatWriteEx failed with error 0x%x", hr );

    // Service the write task until complete.  The title must also service
    // the logon task as well
    do 
    { 
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) ) Error ( L"Logon Task Failed with 0x%x", hr );
        hr = XOnlineTaskContinue( hWriteTask );
    } while ( hr == XONLINETASK_S_RUNNING );

    XOnlineTaskClose( hWriteTask );

    if( FAILED( hr ) ) Error( L"XOnlineStatWriteEx failed with error 0x%x", hr );

    Print( L"Statistics Written" );
}




//-----------------------------------------------------------------------------
// Name: ReadUnitStatistics()
// Desc: Read unit statistics from a leaderboard
//-----------------------------------------------------------------------------
VOID ReadUnitStatistics()
{
    // This sample maintains a single leaderboard with the following
    // attributes:
    //              Kills           The number of kills in a session
    //              Deaths          The number of times the player died
    //              Assists         The number of assists the player provided
    //              Accuracy        Shooting accuracy (0 - 100%)
    //
    // This sample reads the statistics for a unit consisting of the
    // users who are currently loggon on. The Kills, Deaths, Assists, and Accuracy,
    // will be read for the unit.

    // The first step is to populate an XONLINE_STAT array, 'UnitAttributes',
    // specifying which statistical attributes to return.  The 'UnitAttributes' 
    // array is indexed by the followed enumeration:

    enum
    {
        STAT_KILLS,
        STAT_DEATHS,
        STAT_ASSISTS,
        STAT_ACCURACY,
        NUM_LEADERBOARD_ATTRIBUTES
    };

    XONLINE_STAT      UnitAttributes[ NUM_LEADERBOARD_ATTRIBUTES ] = { 0 };


    // The order of the elements in 'UnitAttributes' is VERY important.
    // The Statistics service returns the information
    // sorted by the attribute ID (wID) member.  This sample populates this
    // array in sorted order initially to avoid this problem.

    // Note that only the ID for each statistic is required for the read
    UnitAttributes[ STAT_KILLS ].wID = STAT_ID_KILLS;

    UnitAttributes[ STAT_DEATHS ].wID = STAT_ID_DEATHS;

    UnitAttributes[ STAT_ASSISTS ].wID = STAT_ID_ASSISTS;

    UnitAttributes[ STAT_ACCURACY ].wID = STAT_ID_ACCURACY;

    // The XOnlineStatUnitRead function takes an array of XONLINE_STAT_SPEC_UNIT
    // structures.  Each of these entries specifies the 
    // leaderboard (by ID), and an array on XONLINE_STAT 
    // structures, which contain the requested information for that entry:
    //
    // +------------------------+      +----------------+----------------+
    // | XONLINE_STAT_SPEC_UNIT |      |  XONLINE_STAT  |  XONLINE_STAT  |
    // |                        |      |      wID       |      wID       |
    // |      pStats -----------+----->|      type      |      type      |...
    // |    dwNumStats          |      |   union value  |   union value  |
    // |  dwLeaderBoardID       |      +----------------+----------------+
    // +------------------------+
    //
    // Since this sample only reads statistics for a single unit and 
    // a single leaderboard, there is only one element in the XONLINE_STAT_SPEC_UNIT
    // array 'StatSpecUnit'. 

    XONLINE_STAT_SPEC_UNIT StatSpecUnit;
    XONLINE_USER      *pUsers = XOnlineGetLogonUsers();

    XUID xuidUnitMembers[XONLINE_STAT_MAX_MEMBERS_IN_UNIT];

    ZeroMemory(xuidUnitMembers, sizeof(xuidUnitMembers) );

 
    // Fill the xuidUnitMembers with the XUIDs for the members
    // of the unit
    for( DWORD iMember = 0; iMember < NumPlayersInUnit; ++iMember )
    {
        xuidUnitMembers[iMember] = pUsers[iMember].xuid;
    }

    StatSpecUnit.dwLeaderBoardID = LEADERBOARD_ID;
    StatSpecUnit.dwNumStats = NUM_LEADERBOARD_ATTRIBUTES;
    StatSpecUnit.pStats = UnitAttributes;

    Print( L"Reading Statistics ..." );

    // Initiate the reading process
    XONLINETASK_HANDLE hReadTask;

    HRESULT hr = XOnlineStatUnitRead( xuidUnitMembers, 1, &StatSpecUnit, NULL, &hReadTask );

    if( FAILED( hr ) ) Error( L"XOnlineStatUnitRead failed with error 0x%x", hr );

    // Service the read task until complete.  The title must also service
    // the logon task as well

    do 
    { 
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) ) Error ( L"Logon Task Failed with 0x%x", hr );
        hr = XOnlineTaskContinue( hReadTask );
    } while ( hr == XONLINETASK_S_RUNNING );

    if ( FAILED( hr ) ) Error ( L"XOnlineStatUnitRead Failed with 0x%x", hr );

 
    // Call XOnlineStatUnitReadGetResult to get the results.  Notice that
    // the StatSpecUnit array is used once again, but this time it will
    // receive the actual data. 

    hr = XOnlineStatUnitReadGetResult( hReadTask, 1, &StatSpecUnit );

    if ( FAILED( hr ) ) Error ( L"XOnlineStatUnitReadGetResult Failed with 0x%x", hr );


    XOnlineTaskClose( hReadTask );

    // If any of the attributes specified were missing from the leaderboard,
    // the 'type' field of the corresponding XONLINE_STAT array entry
    // will be set to XONLINE_STAT_NONE.  Since this sample reads and writes the
    // same information, this should not happen.

    assert( UnitAttributes[ STAT_KILLS ].type != XONLINE_STAT_NONE );
    assert( UnitAttributes[ STAT_DEATHS ].type != XONLINE_STAT_NONE );
    assert( UnitAttributes[ STAT_ASSISTS ].type != XONLINE_STAT_NONE );
    assert( UnitAttributes[ STAT_ACCURACY ].type != XONLINE_STAT_NONE );

    // Finally, display the information. Note that the rank, which is assigned by
    // the service is also displayed.  

    Print( L"Kills: %ld Deaths: %ld Assists: %ld Accuracy: %lf",
        UnitAttributes[ STAT_KILLS ].lValue,   
        UnitAttributes[ STAT_DEATHS ].lValue, UnitAttributes[ STAT_ASSISTS ].lValue, 
        UnitAttributes[ STAT_ACCURACY ].dValue
        );


    Print( L"Statistics Read" );     
}




//-----------------------------------------------------------------------------
// Name: EnumerateUnitLeaderboard()
// Desc: Enumerate units on a leaderboard
//-----------------------------------------------------------------------------
VOID EnumerateUnitLeaderboard( XUID xuidMember )
{

    // The XOnlineStatUnitEnumerate Retrieves a contiguous set
    // of units from a specified leaderboard that a user belongs to.
    // 

    // For enumeration the title specifies the IDs of the attributes that are
    // desired. The order of this array is VERY important.  When the
    // enumeration is completed, the returned are returned
    // sorted by the attribute ID (wID) member.  This
    // means user attributes are returned first, followed
    // by the reserved attributes ids, which are returned in this order:
    // XONLINE_STAT_ATTACHMENT_SIZE. XONLINE_STAT_ATTACHMENT_PATH,
    // XONLINE_STAT_LEADERBOARD_SIZE,
    // XONLINE_STAT_RATING, XONLINE_STAT_RANK.  This sample populates this
    // array in the correct order initially to avoid this problem.

    // IDs for attributes to be returned during enumeration
    const WORD StatsPerUnit[] = 
    { 
            STAT_ID_KILLS,
            STAT_ID_DEATHS,
            STAT_ID_ASSISTS,
            STAT_ID_ACCURACY,
    };

    // The corresponding enumeration used for indexing the 
    // array returned from enumeration (the order must
    // match the entries in StatsPerUnit).

    enum
    {
        STAT_KILLS,
        STAT_DEATHS,
        STAT_ASSISTS,
        STAT_ACCURACY,
    };


    Print( L"Enumerating Unit Leaderboard by rating..." );

    // Specify the desired entries to obtain. 
    const DWORD NUM_ENTRIES   = 10;
    const DWORD NUM_STATS_PER_UNIT = sizeof( StatsPerUnit ) / sizeof( StatsPerUnit[ 0 ] );

    XONLINETASK_HANDLE hEnumTask;




    // 
    // Enumeration results can be ordered by rating (XONLINE_STAT_SORTORDER_RATING),
    // or by last activity time (XONLINE_STAT_SORTORDER_LASTACTIVITY)
    const XONLINE_STAT_SORTORDER SortOrder = XONLINE_STAT_SORTORDER_RATING;

    // Start the enumeration process
    HRESULT hr = XOnlineStatUnitEnumerate( xuidMember, LEADERBOARD_ID, SortOrder, NUM_ENTRIES,
        NUM_STATS_PER_UNIT, StatsPerUnit, NULL, &hEnumTask );


    if( FAILED( hr ) ) 
        Error( L"XOnlineStatUnitEnumerate failed with error 0x%x", hr );

    // Service the enumeration task until complete.  The title must also service
    // the logon task as well

    do 
    { 
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) ) Error ( L"Logon Task Failed with 0x%x", hr );
        hr = XOnlineTaskContinue( hEnumTask );
    } while ( hr == XONLINETASK_S_RUNNING );


    if( FAILED( hr ) ) 
        Error( L"XOnlineStatUnitEnumerate failed with error 0x%x", hr );

  
    XONLINE_STAT_UNIT StatUnits[ NUM_ENTRIES ] = { 0 };

    // The XONLINE_STAT attributes for all users are stored in a one dimensional 
    // array.  If there are N attributes, the first N elements are the 
    // attributes for the first user, the second N elements are the 
    // attributes for the second user.  The attributes for user "i" is at N*i.

    XONLINE_STAT Stats[ NUM_STATS_PER_UNIT * NUM_ENTRIES ];

    DWORD dwNumResults;


    hr = XOnlineStatUnitEnumerateGetResults( 
        hEnumTask, StatUnits, NUM_STATS_PER_UNIT  * NUM_ENTRIES, 
        Stats, &dwNumResults );

    XOnlineTaskClose( hEnumTask );

    if( FAILED( hr ) ) 
        Error( L"XOnlineStatLeaderEnumerateGetResults failed with error 0x%x", hr );

    // Print out results

    // Since the attributes for all users are stored in a flat array (Stats),
    // this sample uses 'pStats' to point to the entries for the current user

    XONLINE_STAT *pStats = &Stats[0];

    for( DWORD i = 0; i < dwNumResults; ++i )
    {
        
        // strMembers is a buffer used to hold a comma separated list of users in
        // a unit
        WCHAR strMembers[ ( 2 + XONLINE_GAMERTAG_SIZE ) * XONLINE_STAT_MAX_MEMBERS_IN_UNIT + 1 ];

        strMembers[0] = '\0';
        for( DWORD iMember = 0; iMember < XONLINE_STAT_MAX_MEMBERS_IN_UNIT; ++iMember )
        {
            if( StatUnits[i].xuidUnitMembers[iMember].qwUserID == 0 ) break;

            if( iMember > 0 )
            { 
                wcscat( strMembers, L", " );
            }

            WCHAR strGamertag[ XONLINE_GAMERTAG_SIZE ];
            wsprintfW( strGamertag, L"%S", StatUnits[i].UnitMemberNames[ iMember ].szGamertag );

            wcscat( strMembers, strGamertag );
        }

        Print( L"Members: %s Kills: %ld Deaths: %ld "
            L"Assists: %d Accuracy: %lf",
            strMembers, 
            pStats[STAT_KILLS].lValue, pStats[STAT_DEATHS].lValue,
            pStats[STAT_ASSISTS].lValue, pStats[STAT_ACCURACY].dValue );

        pStats += NUM_STATS_PER_UNIT; // Skip to the stats for the next user
    }

    Print( L"Enumerated %ld unit(s)", dwNumResults );
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
    OutputDebugStringW( L"\n*** SimpleUnitStats: " );
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
    OutputDebugStringW( L"\n*** SimpleUnitStats: " );
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
    OutputDebugStringW( L"\n*** SimpleUnitStats: UI Message:\n" );
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


