//-----------------------------------------------------------------------------
// File: SimpleStats.cpp
//
// Desc: Illustrates the use of the Xbox Live Statistics APIs
//
// Hist: 03.15.03 - New for April release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>                  
#include <xonline.h>              
#include <assert.h> 

// The Xbox Live Statistics service allows a title to store and retrieve 
// statistical information about players of that game title. The type of
// statistical information stored depends on the nature of the title and is
// defined by the title developer. 
// The statistics are stored on network servers, and are accessible to instances
// of the game title running on any Xbox console on the network. Therefore, the 
// player's statistics remain with the player regardless of the Xbox they are
// playing on. Statistics collected by the title over a series of game sessions
// can be used to track the player's performance on the title over time. 
// For example, the title might track the player's top five scores and the dates
// when they were obtained, or the player's 10-day, 30-day, and 90-day moving 
// averages. The Statistics Service is also able to provide ranking information
// about players of the title. That is, the service can compare a given player's 
// performance to that of other players. Unlike traditional rankings in which a
// game title can only compare players that have played on the same console, 
// the Statistics Service is able to rank players who have played the title 
// on any network-enabled Xbox console. 
// With the April 2003 release of the XDK, titles may also associate attachments
// with statistics.  An attachment is a package of data defined by a title.
// For example, in a racing game, this might be a replay video of fastest
// laptime ever.


//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Location used to build an attachment for upload
const CHAR SRC_ATTACHMENT_ROOT[] = "z:\\attachment\\";

// The name of the attachment file
const CHAR ATTACHMENT_NAME[]     = "data.bin";

//
// A leaderboard (also referred to as a scoreboard) is an ordered, title-specific
// listing of players and their statistics. When a title stores a player's 
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
    STAT_ID_KILLS,             // Number of Kills
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
VOID WriteStatistics();
VOID ReadStatistics();
VOID EnumerateLeaderboard( XUID *  );
VOID ResetStatistics();
VOID UploadStatsAttachment( XONLINETASK_HANDLE hWriteTask );
VOID GenerateStatsAttachmentDirectory();
VOID DownloadStatsAttachment( const WCHAR *strRemotePath );




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
    
    LogonUsers[0] = StoredUsers[0];

    // Sign onto the Live Service.  The sample requires the following
    // services:
    //      XONLINE_STATISTICS_SERVICE  -  For reading/write stats
    //      XONLINE_SIGNATURE_SERVICE   -  For stats with attachments
    //      XONLINE_STORAGE_SERVICE     -  For stats with attachments

    if( SignIn( LogonUsers ) )
    {
        srand( GetTickCount() ); // for picking random stat values

        // Write game statistics to a leaderboard
        WriteStatistics();

        // Read game statistics from a leaderboard
        ReadStatistics();

        // Block for 1 second to avoid violating minimum operation interval
        // limits between calls of similar types (stat retrieval via
        // XOnlineStatRead above and XOnlineStatLeaderEnumerate below).
        // Titles are only allowed to retrieve and/or store statistics once
	// at the beginning of a game session, once at the end of a game
        // session, and no more than every 5 minutes during a game session.
        // Outside of a game session, titles still must not write statistics
        // more often than every 5 minutes, though they may read or enumerate
        // once per second if the user is browsing a leaderboard, for example,
        // but note that at all times titles must only invoke stats in
        // response to user interaction.
        // All statistics operations allow titles to work with multiple sets
        // of data at once, so titles should batch multiple requests into a
        // single call if they occur closely spaced.  Another technique for
        // reducing calls is to wait until the user settles on a leaderboard
        // page before making the enumeration request.
        // For demonstration purposes, we simply sleep for 1 second between
        // these two operations.
        Print( L"Blocking to ensure minimum interval between similar operations has elapsed" );
        ::Sleep( 1000 );

        // Enumerate users on a leaderboard by rank
        EnumerateLeaderboard( NULL );

        // Block for 1 second to avoid violating minimum operation interval
        // limits between similar operations.
        Print( L"Blocking to ensure minimum interval between similar operations has elapsed" );
        ::Sleep( 1000 );

        // Enumerate users on a leaderboard using a user pivot
        EnumerateLeaderboard( &LogonUsers[0].xuid );

        // Remove statistics for a user on a leaderboard
        ResetStatistics();
 
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
// Name: WriteStatistics()
// Desc: Write statistics to a leaderboard
//-----------------------------------------------------------------------------
VOID WriteStatistics()
{

    // This sample maintains a single leaderboard with the following
    // attributes:
    //              Kills           The number of kills in a session
    //              Deaths          The number of times the player died
    //              Assists         The number of assists the player provided
    //              Accuracy        Shooting accuracy (0 - 100%)
    //              Nickname        The nickname used for the session
    // This sample also demonstrates how to send upload stat attachments.
    // If a user written to a leadboard has a good enough rating, an
    // attachment for that user may be uploaded.  This allows, for example,
    // a racing title to upload playback data so other gamers can view the
    // race in question.

    // Compute some random statistics
    LONG lKills       = rand() % 20;
    LONG lDeaths      = rand() % 20;
    LONG lAssists     = rand() % 20;
    DOUBLE dAccuracy  = DOUBLE( rand() % 101 );

    // A leaderboard is a ranking of users based
    // on a *rating*.  When writing statistics for a gamer, 
    // a title supplies a rating to the Live Service, which
    // is then used to rank that gamer against others on the leaderboard.
    // The service, and not the title, assigns the actual rank for
    // the gamer.
    //
    // The rating is just a 64-bit number.  The larger the number,
    // the better the rating.  This sample, uses the following
    // formula for calculating the rating.
    LONGLONG llRating = 100*lKills + 10*lAssists - 5*lDeaths;

    // The XONLINE_STAT structure is used to represent a piece
    // of statistical information associated with a leaderboard
    // entry.  This sample will write six pieces of information
    // for a leaderboard entry.  This information is specified
    // by passing 'Attributes',  an array of XONLINE_STAT structures,
    // to the XOnlineStatWrite function.  This array is indexed by the
    // following enumeration:

    enum
    {
        STAT_KILLS,
        STAT_DEATHS,
        STAT_ASSISTS,
        STAT_ACCURACY,
        STAT_NICKNAME,
        STAT_RATING,
        NUM_LEADERBOARD_ATTRIBUTES
    };

    XONLINE_STAT      Attributes[ NUM_LEADERBOARD_ATTRIBUTES ];



    // Populate the Attributes array with the statistical information
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
    //     XONLINE_STAT_LPCWSTR   a string (Nicknames and stat attachments only)
    //
    // * The Data.  The XONLINE_STAT contains an anonymous union
    //
 
    // Specify the Kills attribute (an integer)
    Attributes[ STAT_KILLS ].wID = STAT_ID_KILLS;
    Attributes[ STAT_KILLS ].type = XONLINE_STAT_LONG;
    Attributes[ STAT_KILLS ].lValue = lKills;

    // Specify the Deaths attribute (an integer)
    Attributes[ STAT_DEATHS ].wID = STAT_ID_DEATHS;
    Attributes[ STAT_DEATHS ].type = XONLINE_STAT_LONG;
    Attributes[ STAT_DEATHS ].lValue = lDeaths;

    // Specify the Assists attribute (an integer)
    Attributes[ STAT_ASSISTS ].wID = STAT_ID_ASSISTS;
    Attributes[ STAT_ASSISTS ].type = XONLINE_STAT_LONG;
    Attributes[ STAT_ASSISTS ].lValue = lAssists;

    // Specify the Accuracy attribute (a real number)
    Attributes[ STAT_ACCURACY ].wID = STAT_ID_ACCURACY;
    Attributes[ STAT_ACCURACY ].type = XONLINE_STAT_DOUBLE;
    Attributes[ STAT_ACCURACY ].dValue = dAccuracy;

    // Specify the nickname attribute (a string).  The string
    // data type is supported only for use with Nicknames and stat attachment
    // paths.  The Nickname attribute is specified using the XONLINE_STAT_NICKNAME
    // reserved ID.
    const WCHAR strNickname[] = L"danimal";

    Attributes[ STAT_NICKNAME ].wID = XONLINE_STAT_NICKNAME;
    Attributes[ STAT_NICKNAME ].type = XONLINE_STAT_LPCWSTR;
    Attributes[ STAT_NICKNAME ].lpString = strNickname;      

    // Finally, specify the rating for entry using the
    // reserved XONLINE_STAT_RATING attribute id.  The rating
    // is a 64-bit integer, so XONLINE_STAT_LONGLONG is specified
    // for the attribute type.
    Attributes[ STAT_RATING ].wID = XONLINE_STAT_RATING;
    Attributes[ STAT_RATING ].type = XONLINE_STAT_LONGLONG;
    Attributes[ STAT_RATING ].llValue = llRating;


    // The XOnlineStatWrite function is used to write statistical
    // information.  This function is quite flexible and can write data
    // for multiple users and leaderboards in a single call.
    // For demonstration purposes, this sample will write statistics for
    // a single user to a single leaderboard.  The statistics service
    // requires that the users be signed onto the console from which write
    // requests are made.  To meet this requirement, this sample will write
    // the stats on behalf of the user signed in on the first controller.

    XONLINE_USER      *pUsers = XOnlineGetLogonUsers();


    // The XOnlineStatWrite function takes an array of XONLINE_STAT_SPEC
    // structures.  Each of these entries specifies a user (by XUID), a
    // leaderboard (by ID), and an array on XONLINE_STAT 
    // structures, which contain the information for that entry:
    //
    // +-------------------+      +----------------+----------------+
    // | XONLINE_STAT_SPEC |      |  XONLINE_STAT  |  XONLINE_STAT  |
    // |     xuidUser 1    |      |      wID       |      wID       |
    // |      pStats ------+----->|      type      |      type      |...
    // |    dwNumStats     |      |   union value  |   union value  |
    // |  dwLeaderBoardID  |      +----------------+----------------+
    // +-------------------+      +----------------+----------------+
    // | XONLINE_STAT_SPEC |      |  XONLINE_STAT  |  XONLINE_STAT  |
    // |     xuidUser 2    |      |      wID       |      wID       |
    // |      pStats ------+----->|      type      |      type      |...
    // |    dwNumStats     |      |   union value  |   union value  |
    // |  dwLeaderBoardID  |      +----------------+----------------+
    // +-------------------+
    //
    // Since this sample only writes statistics for a single user and 
    // a single leaderboard, there is only one element in the XONLINE_STAT_SPEC
    // array 'Spec'. If a title supports stats with attachments, then only
    // a single user and leaderboard may be specified in a call to XOnlineStatWrite.

    XONLINE_STAT_SPEC Spec;   // Single leaderboard and user
    const DWORD NUM_STAT_SPECS = 1;

    Spec.xuidUser = pUsers[0].xuid;                  // Specify user by XUID
    Spec.dwLeaderBoardID = LEADERBOARD_ID;           // Specify the leaderboard
    Spec.pStats = Attributes;                        // Point to array populated earlier
    Spec.dwNumStats = NUM_LEADERBOARD_ATTRIBUTES;    // Specify size of XONLINE_STAT array

    Print( L"Writing Statistics for %S...", pUsers[0].szGamertag );
    Print( L"Nickname: %s Kills: %ld Deaths: %ld Assists: %ld Accuracy: %lf",
            strNickname, lKills, lDeaths, lAssists, dAccuracy
            );

    // Initiate the writing process.  This will return a task handle
    XONLINETASK_HANDLE hWriteTask;

    HRESULT hr = XOnlineStatWrite( NUM_STAT_SPECS, &Spec, NULL, &hWriteTask );

    if( FAILED( hr ) ) Error( L"XOnlineStatWrite failed with error 0x%x", hr );

    // Service the write task until complete.  The title must also service
    // the logon task as well
    do 
    { 
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) ) Error ( L"Logon Task Failed with 0x%x", hr );
        hr = XOnlineTaskContinue( hWriteTask );
    } while ( hr == XONLINETASK_S_RUNNING );

     // If the final result code was XONLINE_S_STAT_CAN_UPLOAD_ATTACHMENT,
    // the title can optionally upload an attachment for the user.
    // This might be a video of the associated game session, etc.
    if( hr == XONLINE_S_STAT_CAN_UPLOAD_ATTACHMENT )
    {
        Print( L"User %S qualifies for an attachment upload", pUsers[0].szGamertag );
        UploadStatsAttachment( hWriteTask ); // Upload the attachment
    }

    XOnlineTaskClose( hWriteTask );

    if( FAILED( hr ) ) Error( L"XOnlineStatWrite failed with error 0x%x", hr );

    Print( L"Statistics Written" );
}




//-----------------------------------------------------------------------------
// Name: UploadStatsAttachment()
// Desc: Upload an attachment to the stats service
//-----------------------------------------------------------------------------
VOID UploadStatsAttachment( XONLINETASK_HANDLE hWriteTask )
{
    HANDLE hRemoteReference;

    // Get the final result of the write operation and
    // a remote file reference, which will be returned
    // if the user has earned a good enough rating to
    // quality for an stats attachment upload

    PXONLINE_STAT_ATTACHMENT_REFERENCE pAttachmentReferences;
    DWORD dwNumAttachmentReferences;
 
    HRESULT hr = hr = XOnlineStatWriteGetResult( hWriteTask, &hRemoteReference,
             &pAttachmentReferences, &dwNumAttachmentReferences );

    if( FAILED( hr ) ) Error( L"XOnlineStatWriteGetResult failed with 0x%x", hr );
   
    // Since more than one user/leaderboard combination can be specified when
    // calling XOnlineStatWrite, the pAttachmentReferences parameter will
    // point to an array (of size dwNumAttachmentReferences) 
    // XONLINE_STAT_ATTACHMENT_REFERENCE structures which indicate
    // which users qualified for the upload

    assert( dwNumAttachmentReferences == 1 ); // Only stats for one user was written

    // The dwLeaderboardIndex member of the XONLINE_STAT_ATTACHMENT_REFERENCE
    // structure indicates the new position of the user on the leaderboard.
    // The qwUserPuid indicates the user id (which can be compared to the
    // qwUserID member of a XUID).

    Print( L"User 0x%I64x Rank is: %lu", pAttachmentReferences->qwUserPuid,
        pAttachmentReferences->dwLeaderboardIndex );

    // The XOnlineStorageUpload function is used to upload the attachment
    // to the service.  The attachment data is specified as a directory whose
    // contents will be uploaded
    
    GenerateStatsAttachmentDirectory();  // Generate a random attachment

    XONLINETASK_HANDLE hUploadTask;

    // Initiate the upload process
    hr = XOnlineStorageUpload( hRemoteReference, SRC_ATTACHMENT_ROOT,
                               0, NULL, &hUploadTask );

    if( FAILED( hr ) ) Error( L"XOnlineStorageUpload failed with 0x%x", hr );

    // Service the upload task until complete.  The title must also service
    // the logon task as well

    do 
    { 
        // A title may obtain, for display purposes, the progress of
        // the upload by calling XOnlineStorageGetProgress

        DWORD dwPercent;
        hr = XOnlineStorageGetProgress( hUploadTask , &dwPercent, NULL, NULL );
        if( FAILED( hr ) ) 
            Error( L"XOnlineStorageGetProgress failed with 0x%x", hr );

        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) ) Error ( L"Logon Task Failed with 0x%x", hr );

        hr = XOnlineTaskContinue( hUploadTask );
    } while ( hr == XONLINETASK_S_RUNNING );

    XOnlineTaskClose( hUploadTask );

    if ( FAILED( hr ) ) Error ( L"Upload Task Failed with 0x%x", hr );

    Print( L"Attachment uploaded" );
}




//-----------------------------------------------------------------------------
// Name: ReadStatistics()
// Desc: Read statistics from a leaderboard
//-----------------------------------------------------------------------------
VOID ReadStatistics()
{
    // This sample maintains a single leaderboard with the following
    // attributes:
    //              Kills           The number of kills in a session
    //              Deaths          The number of times the player died
    //              Assists         The number of assists the player provided
    //              Accuracy        Shooting accuracy (0 - 100%)
    //              Nickname        The nickname used for the session
    //
    // This sample reads the statistics for the user signed onto the
    // first controller.  The Kills, Deaths, Assists, Accuracy, Nickname,
    // and Rank (assigned by the live service) will be read for the user.
    // This sample also demonstrates reading stats with attachments.

    // The first step is to populate an XONLINE_STAT array, 'Attributes',
    // specifying which statistical attributes to return.  The 'Attributes' 
    // array is indexed by the followed enumeration:

    enum
    {
        STAT_KILLS,
        STAT_DEATHS,
        STAT_ASSISTS,
        STAT_ACCURACY,
        STAT_NICKNAME,
        STAT_RANK,
        STAT_ATTACHMENT_SIZE,
        STAT_ATTACHMENT,
        NUM_LEADERBOARD_ATTRIBUTES
    };

    XONLINE_STAT      Attributes[ NUM_LEADERBOARD_ATTRIBUTES ] = { 0 };


    // The order of the elements in 'Attributes' is VERY important.
    // The Statistics service returns the information
    // sorted by the attribute ID (wID) member.  This
    // means user attributes are returned first, followed
    // by the reserved attributes ids, which are returned in this order:
    // XONLINE_STAT_ATTACHMENT_SIZE. XONLINE_STAT_ATTACHMENT_PATH,
    // XONLINE_STAT_LEADERBOARD_SIZE, XONLINE_STAT_NICKNAME,
    // XONLINE_STAT_RATING, XONLINE_STAT_RANK.  This sample populates this
    // array in sorted order initially to avoid this problem.

    // Note that only the ID for each statistic is required for the read
    Attributes[ STAT_KILLS ].wID = STAT_ID_KILLS;

    Attributes[ STAT_DEATHS ].wID = STAT_ID_DEATHS;

    Attributes[ STAT_ASSISTS ].wID = STAT_ID_ASSISTS;

    Attributes[ STAT_ACCURACY ].wID = STAT_ID_ACCURACY;

    // The reserved ID XONLINE_STAT_NICKNAME is used to specify 
    // that the nickname should be returned
    Attributes[ STAT_NICKNAME ].wID = XONLINE_STAT_NICKNAME;

    // The reserved ID XONLINE_STAT_RANK is used to specify 
    // that the rank should be returned. 

    Attributes[ STAT_RANK ].wID = XONLINE_STAT_RANK;

    // The reserved ID XONLINE_STAT_ATTACHMENT_PATH is used to
    // specify that the server-side attachment path should
    // be returned.  

    Attributes[ STAT_ATTACHMENT ].wID = XONLINE_STAT_ATTACHMENT_PATH;
    
    // The reserved ID XONLINE_STAT_ATTACHMENT_SIZE is used to
    // specify that the size of any server size attachment should
    // be returned.  This is the amount of storage space required for
    // the attachment download.

    Attributes[ STAT_ATTACHMENT_SIZE ].wID = XONLINE_STAT_ATTACHMENT_SIZE;
   
    // The XOnlineStatRead function takes an array of XONLINE_STAT_SPEC
    // structures.  Each of these entries specifies a user (by XUID), a
    // leaderboard (by ID), and an array on XONLINE_STAT 
    // structures, which contain the requested information for that entry:
    //
    // +-------------------+      +----------------+----------------+
    // | XONLINE_STAT_SPEC |      |  XONLINE_STAT  |  XONLINE_STAT  |
    // |     xuidUser 1    |      |      wID       |      wID       |
    // |      pStats ------+----->|      type      |      type      |...
    // |    dwNumStats     |      |   union value  |   union value  |
    // |  dwLeaderBoardID  |      +----------------+----------------+
    // +-------------------+      +----------------+----------------+
    // | XONLINE_STAT_SPEC |      |  XONLINE_STAT  |  XONLINE_STAT  |
    // |     xuidUser 2    |      |      wID       |      wID       |
    // |      pStats ------+----->|      type      |      type      |...
    // |    dwNumStats     |      |   union value  |   union value  |
    // |  dwLeaderBoardID  |      +----------------+----------------+
    // +-------------------+
    //
    // Since this sample only reads statistics for a single user and 
    // a single leaderboard, there is only one element in the XONLINE_STAT_SPEC
    // array 'Spec'. 

    XONLINE_STAT_SPEC Spec;
    XONLINE_USER      *pUsers = XOnlineGetLogonUsers();
    
    const DWORD NUM_STAT_SPECS = 1;

    Spec.xuidUser = pUsers[0].xuid;                  // Specify user by XUID
    Spec.dwLeaderBoardID = LEADERBOARD_ID;           // Specify the leaderboard
    Spec.pStats = Attributes;                        // Point to array populated earlier
    Spec.dwNumStats = NUM_LEADERBOARD_ATTRIBUTES;    // Specify size of XONLINE_STAT array

    Print( L"Reading Statistics..." );

    // Initiate the reading process
    XONLINETASK_HANDLE hReadTask;
    
    HRESULT hr = XOnlineStatRead( NUM_STAT_SPECS, &Spec, NULL, &hReadTask );

    if( FAILED( hr ) ) Error( L"XOnlineStatRead failed with error 0x%x", hr );

    // Service the read task until complete.  The title must also service
    // the logon task as well

    do 
    { 
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) ) Error ( L"Logon Task Failed with 0x%x", hr );
        hr = XOnlineTaskContinue( hReadTask );
    } while ( hr == XONLINETASK_S_RUNNING );

    if ( FAILED( hr ) ) Error ( L"XOnlineStatRead Failed with 0x%x", hr );

    // After the read task is finished, obtain the results using
    // XOnlineStatReadGetResults.  Since the nickname and stats attachment path
    // were requested, the title must specify space to receive it. 
    // The size of the extra storage required for nicknames is:
    // (XONLINE_STAT_MAX_NICKNAME_LENGTH + 1 ) multiplied by the
    // number of users requested multiplied by the size of a WCHAR. 
    // The size of the extra storage required for the stat attachment path
    // is XONLINESTORAGE_MAX_PATH bytes.
    // Since there is just one user, a buffer 'ExtraStorage' to used 
    // to hold both the nickname and stat attachment path, 
    // and the size of it is passed to XOnlineStatReadGetResult

    BYTE ExtraStorage[ XONLINESTORAGE_MAX_PATH + 
                       sizeof(WCHAR) * (XONLINE_STAT_MAX_NICKNAME_LENGTH + 1) ];

    // Call XOnlineStatReadGetResult to get the results.  Notice that
    // the Spec array is used once again, but this time it will
    // receive the actual data.  The size of, and the actual pointer to, the
    // storage used for the nickname and stat attachment path are
    // passed in as well.

    hr = XOnlineStatReadGetResult( hReadTask, NUM_STAT_SPECS, &Spec, 
        sizeof( ExtraStorage ), ExtraStorage );

    if ( FAILED( hr ) ) Error ( L"XOnlineStatReadGetResult Failed with 0x%x", hr );


    XOnlineTaskClose( hReadTask );

    // If any of the attributes specified were missing from the leaderboard,
    // the 'type' field of the corresponding XONLINE_STAT array entry
    // will be set to XONLINE_STAT_NONE.  Since this sample reads and writes the
    // same information, this should not happen.

    assert( Attributes[ STAT_KILLS ].type != XONLINE_STAT_NONE );
    assert( Attributes[ STAT_DEATHS ].type != XONLINE_STAT_NONE );
    assert( Attributes[ STAT_ASSISTS ].type != XONLINE_STAT_NONE );
    assert( Attributes[ STAT_ACCURACY ].type != XONLINE_STAT_NONE );
    assert( Attributes[ STAT_NICKNAME ].type != XONLINE_STAT_NONE );
    assert( Attributes[ STAT_RANK ].type != XONLINE_STAT_NONE );

    // Finally, display the information. Note that the rank, which is assigned by
    // the service is also displayed.  Also, notice that for the nickname, the
    // character pointer member (lpString) of the XONLINE_STAT structure is used.

    Print( L"Rank: %d Nickname: %s Kills: %ld Deaths: %ld Assists: %ld Accuracy: %lf",
            Attributes[ STAT_RANK ].lValue,    Attributes[ STAT_NICKNAME ].lpString,
            Attributes[ STAT_KILLS ].lValue,   Attributes[ STAT_DEATHS ].lValue,
            Attributes[ STAT_ASSISTS ].lValue, Attributes[ STAT_ACCURACY ].dValue
            );

    // Next, check if there was an attachment for this user. If there
    // is no attachment, then the type of the attachment attribute is
    // set to XONLINE_STAT_NONE, otherwise it will be set to 
    // XONLINE_STAT_LPCWSTR.

    if( Attributes[ STAT_ATTACHMENT ].type != XONLINE_STAT_NONE )
    {
        assert( Attributes[ STAT_ATTACHMENT ].type == XONLINE_STAT_LPCWSTR );
        assert( Attributes[ STAT_ATTACHMENT_SIZE ].type == XONLINE_STAT_LONG );

        Print( L"An attachment of %lu bytes is available.", 
            Attributes[ STAT_ATTACHMENT_SIZE ].lValue );
        // Download and display the attachment
        DownloadStatsAttachment( Attributes[ STAT_ATTACHMENT ].lpString );
    }

    Print( L"Statistics Read" );     
}




//-----------------------------------------------------------------------------
// Name: EnumerateLeaderboard()
// Desc: Enumerate users on a leaderboard
//-----------------------------------------------------------------------------
VOID EnumerateLeaderboard( XUID *pxuidPivot )
{

    // The XOnlineStatLeaderEnumerate function supports two methods of enumerating
    // a leaderboard.  The first method is enumerating some number of entries
    // starting with a particular rank (e.g. 10 entries starting with rank 5).
    // This is useful for displaying the top players, or scrolling through 
    // leaderboards.  The second method is to enumerate entries about a particular
    // player (known as the pivot).  The pivot is a identified by the XUID of a player
    // on the leaderboard.  If the pxuidPivot parameter is NULL, the former technique
    // is used, and if not, the latter technique is used.
    // 

    // For enumeration the title specifies the IDs of the attributes that are
    // desired. The order of this array is VERY important.  When the
    // enumeration is completed, the returned are returned
    // sorted by the attribute ID (wID) member.  This
    // means user attributes are returned first, followed
    // by the reserved attributes ids, which are returned in this order:
    // XONLINE_STAT_ATTACHMENT_SIZE. XONLINE_STAT_ATTACHMENT_PATH,
    // XONLINE_STAT_LEADERBOARD_SIZE, XONLINE_STAT_NICKNAME,
    // XONLINE_STAT_RATING, XONLINE_STAT_RANK.  This sample populates this
    // array in the correct order initially to avoid this problem.

    // IDs for attributes to be returned during enumeration
    const WORD StatsPerUser[] = 
    { 
            STAT_ID_KILLS,
            STAT_ID_DEATHS,
            STAT_ID_ASSISTS,
            STAT_ID_ACCURACY,
            XONLINE_STAT_NICKNAME,
            XONLINE_STAT_RANK
    };
    
    // The corresponding enumeration used for indexing the 
    // array returned from enumeration (the order must
    // match the entries in StatsPerUser).

    enum
    {
            STAT_KILLS,
            STAT_DEATHS,
            STAT_ASSISTS,
            STAT_ACCURACY,
            STAT_NICKNAME,
            STAT_RANK
    };


    if( pxuidPivot )
        Print( L"Enumerating leaderboard entries around a user..." );
    else
        Print( L"Enumerating Leaderboard by rank..." );

    // When enumerating by rank, start with the first entry (top player)
    const DWORD STARTING_RANK = 1;

    // Specify the desired entries to obtain. When enumerating by rank,
    // this is the maximum number of entries retrieved.  When enumerating using
    // a pivot, approximately NUM_ENTRIES/2 entries above and below the pivot
    // will be returned.

    const DWORD NUM_ENTRIES   = 10;
    const DWORD NUM_STATS_PER_USER = sizeof( StatsPerUser ) / sizeof( StatsPerUser[ 0 ] );

    XONLINETASK_HANDLE hEnumTask;

    // Start the enumeration process
    HRESULT hr = XOnlineStatLeaderEnumerate( pxuidPivot,  
                            STARTING_RANK,   // Ignored if pxuid is not NULL 
                            NUM_ENTRIES,
                            LEADERBOARD_ID,
                            NUM_STATS_PER_USER, StatsPerUser, NULL,
                            &hEnumTask );

    if( FAILED( hr ) ) 
        Error( L"XOnlineStatLeaderEnumerate failed with error 0x%x", hr );

    // Service the enumeration task until complete.  The title must also service
    // the logon task as well

    do 
    { 
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) ) Error ( L"Logon Task Failed with 0x%x", hr );
        hr = XOnlineTaskContinue( hEnumTask );
    } while ( hr == XONLINETASK_S_RUNNING );
    

    // If enumerating using a user pivot, and it wasn't found
    // in the leaderboard, XONLINE_E_STAT_USER_NOT_FOUND is returned

    if( hr == XONLINE_E_STAT_USER_NOT_FOUND )
    {
        Print( L"User pivot not found in leaderboard" );
        XOnlineTaskClose( hEnumTask );
        return;
    }

    if( FAILED( hr ) ) 
        Error( L"XOnlineStatLeaderEnumerate failed with error 0x%x", hr );

    // Obtain the results of the enumeration.   The xuid and gamertag for
    // each entry is stored in a XONELINE_STAT_USER structure,

    XONLINE_STAT_USER StatUsers[ NUM_ENTRIES ];

    // The XONLINE_STAT attributes for all users are stored in a one dimensional 
    // array.  If there are N attributes, the first N elements are the 
    // attributes for the first user, the second N elements are the 
    // attributes for the second user.  The attributes for user "i" is at N*i.

    XONLINE_STAT Stats[ NUM_STATS_PER_USER * NUM_ENTRIES ];

    // Since the Nickname attribute was requested, this sample supplies
    // a buffer for it.  Individual Nickname attributes will point to areas inside
    // this buffer.

    WCHAR ExtraStorage[ NUM_ENTRIES * (XONLINE_STAT_MAX_NICKNAME_LENGTH + 1)];

    DWORD dwLeaderboardSize;
    DWORD dwNumResults;

    hr = XOnlineStatLeaderEnumerateGetResults( 
        hEnumTask, NUM_ENTRIES, StatUsers, NUM_STATS_PER_USER  * NUM_ENTRIES, 
        Stats, &dwLeaderboardSize, &dwNumResults, sizeof( ExtraStorage ), 
        (BYTE *) ExtraStorage );

    XOnlineTaskClose( hEnumTask );

    if( FAILED( hr ) ) 
        Error( L"XOnlineStatLeaderEnumerateGetResults failed with error 0x%x", hr );

    // Print out results

    // Since the attributes for all users are stored in a flat array (Stats),
    // this sample uses 'pStats' to point to the entries for the current user

    XONLINE_STAT *pStats = &Stats[0];

    for( DWORD i = 0; i < dwNumResults; ++i )
    {
        Print( L"Rank: %ld User: %S Kills: %ld Deaths: %ld "
               L"Assists: %d Accuracy: %lf Nickname: %s",
            pStats[STAT_RANK].lValue, StatUsers[i].szGamertag, 
            pStats[STAT_KILLS].lValue, pStats[STAT_DEATHS].lValue,
            pStats[STAT_ASSISTS].lValue, pStats[STAT_ACCURACY].dValue, 
            pStats[STAT_NICKNAME].lpString );

        pStats += NUM_STATS_PER_USER; // Skip to the stats for the next user
    }

    Print( L"Enumerated %ld user(s) out of %ld entries in the leaderboard", 
        dwNumResults, dwLeaderboardSize );
}




//-----------------------------------------------------------------------------
// Name: ResetStatistics()
// Desc: Remove statistics for a user on a leaderboard
//-----------------------------------------------------------------------------
VOID ResetStatistics()
{

    XONLINE_USER      *pUsers = XOnlineGetLogonUsers();
    XONLINETASK_HANDLE hResetTask;
    

    // The XOnlineStatReset function is used to remove statistics for
    // a user from a leaderboard.  The actual user is required to be
    // signed on before the statistics can be reset.

    // For testing on PartnerNet only, if the title specifies a value of zero
    // for the qwUserID member of the xuid parameter, the Statistics service
    // resets all user data on the leader board specified by the dwLeaderBoardId
    // parameter. If both the qwUserID member and the dwLeaderBoardId parameter
    // are zero, the Statistics service resets all user data on all leader boards
    // for that title.

    Print( L"Resetting statistics for %S...", pUsers[0].szGamertag );

    // Initiate the reset process
    HRESULT hr = XOnlineStatReset( pUsers[0].xuid, LEADERBOARD_ID,
                                   NULL, &hResetTask );
    
    if( FAILED( hr ) ) 
        Error( L"XOnlineStatReset failed with error 0x%x", hr );

    // Service the read task until complete.  The title must also service
    // the logon task as well

    do 
    { 
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) ) Error ( L"Logon Task Failed with 0x%x", hr );
        hr = XOnlineTaskContinue( hResetTask );
    } while ( hr == XONLINETASK_S_RUNNING );
    
    XOnlineTaskClose( hResetTask );

    if( FAILED( hr ) ) 
        Error( L"XOnlineStatReset failed with error 0x%x", hr );

    Print( L"Statistics reset for %S", pUsers[0].szGamertag );

}



//-----------------------------------------------------------------------------
// Name: GenerateStatsAttachmentDirectory()
// Desc: Generate a stats attachment directory for upload
//-----------------------------------------------------------------------------
VOID GenerateStatsAttachmentDirectory()
{
    // Generate the attachment: a file containing random DWORD
    BOOL bResult = CreateDirectory( SRC_ATTACHMENT_ROOT, NULL );

    if( !bResult ) Error( L"Unable to create attachment directory" );

    CHAR strAttachment[MAX_PATH];
    strcpy( strAttachment, SRC_ATTACHMENT_ROOT );
    strcat( strAttachment, ATTACHMENT_NAME );

    HANDLE hFile = ::CreateFile(  strAttachment,
                        GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

    if( hFile == INVALID_HANDLE_VALUE )
        Error( L"Unable to create attachment data" );

    DWORD dwData = rand();

    DWORD dwNumBytesWritten;

    bResult = WriteFile( hFile, &dwData, sizeof( dwData ), 
                                &dwNumBytesWritten, NULL );

    if( !bResult || dwNumBytesWritten != sizeof( dwData ) )
        Error( L"File write error creating attachment data" );

    CloseHandle( hFile );
    
    Print( L"Created attachment (data = %lu)", dwData );
}




//-----------------------------------------------------------------------------
// Name: DownloadStatsAttachment()
// Desc: Download a stats attachment
//-----------------------------------------------------------------------------
VOID DownloadStatsAttachment( const WCHAR *strRemotePath )
{
    XONLINETASK_HANDLE hDownloadTask; 
    
    // Initiate the download task
    HRESULT hr = XOnlineStorageDownload( XONLINESTORAGE_FACILITY_STATS, 0, 
            strRemotePath, NULL, 0, NULL, &hDownloadTask );

    if ( FAILED( hr ) ) Error( L"XOnlineStorageDownload failed with 0x%x", hr );

    // Service the download task until complete.  The title must also service
    // the logon task as well

    do 
    { 
        // A title may obtain, for display purposes, the progress of
        // the download by calling XOnlineStorageGetProgress

        DWORD dwPercent;
        hr = XOnlineStorageGetProgress( hDownloadTask , &dwPercent, NULL, NULL );
        if( FAILED( hr ) ) 
            Error( L"XOnlineStorageGetProgress failed with 0x%x", hr );

        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) ) Error ( L"Logon Task Failed with 0x%x", hr );

        hr = XOnlineTaskContinue( hDownloadTask );
    } while ( hr == XONLINETASK_S_RUNNING );

    XOnlineTaskClose( hDownloadTask );

    if ( FAILED( hr ) ) Error ( L"Download Task Failed with 0x%x", hr );

    // The XOnlineStorageGetInstallLocation function is used to 
    // obtain the location of the downloaded attachment. 
    // NOTE: Before a shipping title can access this data, it must verify the
    // content using the forthcoming (in June) verification functions.

    CHAR strInstallLocation[MAX_PATH];
    DWORD cbLocation = sizeof( strInstallLocation );

    hr = XOnlineStorageGetInstallLocation( XONLINESTORAGE_FACILITY_STATS, 
                strRemotePath, strInstallLocation, &cbLocation );
    if (FAILED(hr)) Error( L"XOnlineStorageGetInstallLocation failed with %x", 
                             hr );

    Print( L"Attachment downloaded to %S", strInstallLocation );

    // Read the attachment data, which is just a DWORD
    strcat( strInstallLocation, ATTACHMENT_NAME );

    HANDLE hFile = ::CreateFile(  strInstallLocation,
                        GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

    if( hFile == INVALID_HANDLE_VALUE )
        Error( L"Unable to open attachment file" );

    DWORD dwNumBytesRead;
    DWORD dwData;

    BOOL  bResult = ReadFile( hFile, &dwData, sizeof( dwData ), 
                                &dwNumBytesRead, NULL );

    if( !bResult || dwNumBytesRead != sizeof( dwData ) )
        Error( L"Error reading attachment data" );

    CloseHandle( hFile );

    Print( L"Read Attachment (data = %lu)", dwData );

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
    OutputDebugStringW( L"\n*** SimpleStats: " );
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
    OutputDebugStringW( L"\n*** SimpleStats: " );
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
    OutputDebugStringW( L"\n*** SimpleStats: UI Message:\n" );
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


