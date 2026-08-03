//-----------------------------------------------------------------------------
// File: SingleElimination.cpp
//
// Desc: This implements an implementation of a single elimination tournament
// with up to eight players. The simulation runs entirely on one machine, which
// makes it easier to run it, and avoids some of the distracting complications
// which occur with a multi-box sample.
//
// This sample does not demonstrate reminders, cancelling tournaments, or
// efficient querying of large tournaments.
//
// Hist: 10.3.03 - Created
//       12.1.03 - Updated for December XDK and to use XLast generated code
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <xgraphics.h>
#include <xonline.h>
#include <XBOnline.h>
#include <XBOnlineTask.h>

#include "comps.h"



//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD SECONDS_PER_MINUTE = 60;
const DWORD SECONDS_PER_HOUR = 60 * SECONDS_PER_MINUTE;
const DWORD SECONDS_PER_DAY = SECONDS_PER_HOUR * 24;

// When creating a real tournament you would want to have a reasonable time
// period for people to join the tournament. However, for testing purposes
// it is useful to have a short period, because you can't start playing
// tournament rounds until registration has closed.
// Short tournament delays mean you have to register players very quickly,
// especially if the server's clock is slightly ahead of the clients (it
// can be five minutes ahead or behind).
// SECONDS_BEFORE_TOURNAMENT_CLOSE is the delay in seconds from opening
// registration to closing it. If it is too small then even the automated
// registration of users may fail, especially if you sit too long in the
// debugger. With the TimeWarp functionality built in to the sample it is
// reasonable to have long delays between tournament open and close, and
// long delays between rounds.
const DWORD SECONDS_BEFORE_TOURNAMENT_CLOSE = SECONDS_PER_HOUR * 12;

// Have the rounds happen daily. This is specified using minutes because
// this makes it easier to change to shorter frequencies.
const DWORD SECONDS_BETWEEN_ROUNDS = SECONDS_BEFORE_TOURNAMENT_CLOSE * 2;

// Specify how long the rounds should be. Make sure the round length is
// shorter than the time between rounds.
const DWORD ROUND_LENGTH_IN_SECONDS = SECONDS_BETWEEN_ROUNDS / 2;

// Normally the reminder would be set to an hour or so. Five minutes is
// the minimum. Make sure it is shorter than the time between rounds.
const DWORD REMINDER_ADVANCE_MINUTES = 5 * SECONDS_PER_MINUTE;

// How many seconds to warp into the future when time-warping.
const DWORD TIME_WARP_SECONDS = ROUND_LENGTH_IN_SECONDS * 2;



// The data set IDs or templates refer to the different databases that are
// being referenced by different competitions APIs. When using XLast generated
// code the only time we need to use a dataset ID directly is for time
// warping.
const DWORD COMPETITIONS_DATASET = 1;

// The competitions sample supports a maximum of eight players in a competition.
// You can set this value to a larger number, but the tournament display code
// will need to be enhanced.
const DWORD MAX_ENTRANTS = 8;

// The maximum number of rounds that can be played in a particular
// tournament.
const DWORD MAX_ROUND_EVENTS = MAX_ENTRANTS - 1;

// Enums to define which screen we are currently on.
enum WHICH_SCREEN
{
    SCREEN_INTRO,               // Main screen, with options to create or search for tournaments
    SCREEN_CREATETOURNAMENT,    // Mode for creating a tournament - nothing is actually displayed
    SCREEN_DISPLAYTOURNAMENT,   // Screen for displaying the structure of a tournament

    SCREEN_SELECT,              // Generic select screen - used to choose an item from g_SelectList
    SCREEN_STARTSEARCH,         // 'Screen' that is never displayed - just used to trigger searching

    SCREEN_ERROR,               // Generic error screen
};

// Have a 2D array of text for a generic selection system. MAX_SELECT_ITEMS
// be at least 16 because it used to select gamer tags.
const DWORD MAX_SELECT_ITEMS = 50;
const DWORD MAX_SELECT_TEXT = 1000;
// Maximum number of items to display in the select screen before scrolling.
const DWORD MAX_VISIBLE_SELECT_ITEMS = 12;
// The search results array must be the same length as the select text
// array, since we need to display all the competitions we found in
// the select list.
const DWORD MAX_SEARCH_RESULTS = MAX_SELECT_ITEMS;




//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
XONLINETASK_HANDLE hLogonTask;

CSEEventsTopologyQueryResult g_Topology[ MAX_ROUND_EVENTS ];
// How many rounds of data did we get about the competition topology.
DWORD g_TopologyCount;

// Helper object for doing queries
CSEEntrantsMyCompetitionsQuery g_CompQuery;

// Results from queries
CSEEntrantsMyCompetitionsQueryResult g_SearchResults[ MAX_SEARCH_RESULTS ];

// Variables for controlling the select screen
// Text to be displayed on the select screen
CHAR g_SelectList[ MAX_SELECT_ITEMS ][ MAX_SELECT_TEXT ];
// How many items are in the list?
DWORD g_SelectSize = 0;
// Which is the currently selected item in the list? This is used when selecting
// users or tournaments.
DWORD g_SelectEntry = 0;
// Header text for the select screen
const WCHAR* g_SelectScreenHeader;
// Which screen should the select screen go to after the user chooses an item?
WHICH_SCREEN g_SelectNextScreen;

// This class manages the sample single elimination tournament we
// created in XLAST.
CSECompetition g_MyCompetition;


//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
BOOL SignIn( XONLINE_USER *pLogonUsers, const DWORD *pdwServiceIDs, DWORD dwServices );
FILETIME AddSeconds( FILETIME StartTime, __int64 DeltaSeconds );




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont         m_Font;             // Font object

    ULONGLONG       m_qwCompetitionID;  // ID of the currently selected competition.

    XBUserList      m_UserList;         // List of Live accounts found on the machine

    // Start a particular menu screen.
    VOID StartScreen( WHICH_SCREEN whichScreen );

    WHICH_SCREEN    m_WhichScreen;      // Which screen are we on - menu state machine

    const WCHAR*    m_ScreenHeader;     // Text for the top of the screen
    const WCHAR*    m_ScreenFooter;     // Text for the bottom of the screen

    DWORD           m_SelectedUser;     // The index for the selected user, when doing searches.

    WCHAR           m_strMessage[1024]; // Space for error message text.

    VOID            CreateTournament(); // Create a tournament for the selected user
    VOID            TournamentSearch(); // Search for tournaments the selected user is signed up for
    VOID            DisplayTournamentInit(); // Get topology information for display
    VOID            PlayRound();        // Simulate playing a tournament round
    // Join the logged on user to the tournament, then log off.
    HRESULT         JoinCompetitionAndLogoff( DWORD portNumber );

    // Logon the specified user(s)
    BOOL            Logon( XONLINE_USER* pUser0, XONLINE_USER* pUser1, XONLINE_USER* pUser2, XONLINE_USER* pUser3 );
    // Log everybody off
    VOID            Logoff();
    // Pump the logon task and the specified task, and optionally return an HRESULT
    BOOL            WaitForTaskToComplete( CXBOnlineTask& Task, HRESULT* pHR = 0 );
    // Pump the logon task and the competition manager, and optionally return an HRESULT
    BOOL            WaitForTaskToComplete( CSECompetition& SEComp, HRESULT *pHR = 0 );
    // Pump the logon task and a query, and optionally return an HRESULT
    BOOL            WaitForTaskToComplete( ICompetitionQuery* pQuery, HRESULT *pHR = 0 );

    // Warp through time - move the selected competition back in time - effectively moving
    // the players forward through time.
    VOID            TimeWarp();

    // Get the local XONLINE_USER struct for the specified player
    // id. This function doesn't handle remote users.
    XONLINE_USER    GetUserStruct( ULONGLONG playerID );
public:
    // Log everybody off, go to the error screen, and displayed a formatted error message
    VOID            ReportMessageAndLogoff( const WCHAR* format, ... );

    virtual HRESULT Initialize();
    virtual HRESULT Render();
    virtual HRESULT FrameMove();

    CXBoxSample();
};

//-----------------------------------------------------------------------------
// Name: xbApp()
// Desc: App class - globally declared to make it available to Print and UiMsg
//-----------------------------------------------------------------------------
static CXBoxSample xbApp;




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    if( FAILED( xbApp.Create() ) )
        return;
    xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample() 
            :CXBApplication()
{
    m_strMessage[0] = 0;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Initialize the online library
    HRESULT hr;

    hr = XOnlineStartup( NULL );
    if( FAILED( hr ) )
    {
        XBUtil_DebugPrint( "XOnlineStartup failed (error 0x%x)", hr );
    }

    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    hr = XBOnline_GetUserList( m_UserList );

    if( FAILED( hr ) )
    {
        ReportMessageAndLogoff( L"XBOnline_GetUserList failed (error 0x%x)", hr );
    }
    else if( m_UserList.size() == 0 )
    {
        ReportMessageAndLogoff( L"Error - no live accounts found." );
    }
    else
        StartScreen( SCREEN_INTRO );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetUserStruct()
// Desc: This function returns an XONLINE_USER struct for the specified player
// ID, to faciliate logging them in. Note that this function
// currently only searches the list of gamertags on the local machine, which is
// sufficient for this sample but might be inadequate in a real competition.
//-----------------------------------------------------------------------------
XONLINE_USER CXBoxSample::GetUserStruct( ULONGLONG playerID )
{
    for( DWORD i = 0; i < m_UserList.size(); ++i )
    {
        if( m_UserList[i].xuid.qwUserID == playerID )
            return m_UserList[i];
    }

    // If a user has been deleted from the machine while they are signed up
    // for a tournament then this function will not be able to find their
    // user struct.
    // The sample will not be able to play matches for deleted users.
    XONLINE_USER unknown = { 0 };
    return unknown;
}




//-----------------------------------------------------------------------------
// Name: StartScreen()
// Desc: Do whatever initialization is required for the screen we are about to
// start.
//-----------------------------------------------------------------------------
VOID CXBoxSample::StartScreen( WHICH_SCREEN whichScreen )
{
    // Record the screen that is now active.
    m_WhichScreen = whichScreen;
    // Default to no title.
    m_ScreenHeader = L"";
    m_ScreenFooter = L"";

    // Run any necessary startup code for this screen.
    switch ( m_WhichScreen )
    {
    case SCREEN_INTRO:
        m_ScreenHeader = L"Press " GLYPH_A_BUTTON L" to create a tournament.\n"
                         L"Press " GLYPH_Y_BUTTON L" to search for tournaments.";
        m_strMessage[0] = 0;
        break;

    case SCREEN_CREATETOURNAMENT:
        CreateTournament();
        break;

    // Dummy screen just to trigger tournament searching on the selected user.
    case SCREEN_STARTSEARCH:
        TournamentSearch();
        break;

    case SCREEN_SELECT:
        // Start with item zero selected. Then display the list of items
        // and let the user select from it.
        g_SelectEntry = 0;
        m_ScreenHeader = g_SelectScreenHeader;
        g_SelectScreenHeader = L"";
        break;

    case SCREEN_ERROR:
        // Generic error display screen
        m_ScreenHeader = L"Press " GLYPH_A_BUTTON L" to continue.";
        break;

    case SCREEN_DISPLAYTOURNAMENT:
        DisplayTournamentInit();
        break;

    default:
        assert( 0 );
        break;
    }
}




//-----------------------------------------------------------------------------
// Name: DisplayTournamentInit()
// Desc: Initialize data for displaying the tournament topology.
//-----------------------------------------------------------------------------
VOID CXBoxSample::DisplayTournamentInit()
{
    // Zero the count of topology entries.
    g_TopologyCount = 0;

    // Grab the competition ID from the selected entry in the search results.
    m_qwCompetitionID = g_SearchResults[ g_SelectEntry ].att_comp_id;
    if( g_SearchResults[ g_SelectEntry ].att_comp_status == XONLINE_COMP_STATUS_PRE_INIT )
    {
        // Registration hasn't closed yet, so we can't display the tournament.
        // However we can give the user the option to advance time so that
        // registration has closed. This is a handy debug feature.
        m_ScreenHeader = L"Competition brackets are not yet created.\n"
                         L"Press " GLYPH_X_BUTTON L" to advance time, press " GLYPH_B_BUTTON L" to cancel.";
        return;
    }

    // Sign in the same user that was used for the tournament search, since we
    // know that they are in this tournament.
    BOOL Result = Logon( &m_UserList[ m_SelectedUser ], 0, 0, 0 );
    if ( !Result )
    {
        // Don't show any error message here - Logon will report the errors.
        return;
    }

    // Get the topology results, one page at a time.
    // Normally a progress bar or other display would be shown at this point.
    // In many cases a single page of results will be sufficient for topologies.
    for( DWORD page = 0; /**/; ++page )
    {
        // The round IDs specify what range of rounds we will query on.
        // Any rounds whose IDs are >= qwStartingEventTopologyID and <= qwEndingEventTopologyID
        // are returned. The rounds and matches are zero based.
        // Normally you would not query the entire competition as that would be too
        // much data.
        const DWORD startRound = 0;
        const DWORD endRound = 3;   // Maximum four rounds for the sample.
        const DWORD startMatch = 0;
        const DWORD endMatch = 3;   // Maximum four matches per round for the sample.
        CSEEventsTopologyQuery query;
        HRESULT hr = query.Query( m_qwCompetitionID, TOPOLOGY_ID( startRound, startMatch ),
                TOPOLOGY_ID( endRound, endMatch ), page );
        const WCHAR* errorMessage = L"Error getting competition topology - 0x%08lX\n"
                                    L"Competition not started, or other error.";
        if( FAILED( hr ) )
        {
            ReportMessageAndLogoff( errorMessage, hr );
            return;
        }

        if( !WaitForTaskToComplete( &query ) )
            return;

        // Make sure we are getting reasonable results.
        assert( query.dwItemsReturned <= query.dwTotalItemsInResult );

        // Copy the results of this particular query into our own buffer
        for( UINT i = 0; i < query.dwItemsReturned; ++i )
        {
            memcpy( g_Topology + g_TopologyCount, &query.Results[i], sizeof( CSEEventsTopologyQueryResult ) );
            ++g_TopologyCount;
        }

        // If we have all the results there are, we might as well stop querying.
        if( g_TopologyCount == query.dwTotalItemsInResult )
            break;
    }
    // The sample logs off after doing a topology query. A real game would not do this.
    Logoff();

    m_ScreenHeader = L"Press " GLYPH_A_BUTTON L" to play the next round, " GLYPH_B_BUTTON L" to cancel,\n"
                     L"or press " GLYPH_X_BUTTON L" to advance time.";
}




//-----------------------------------------------------------------------------
// Name: Logon()
// Desc: Log on one to four users. This function loops until the logon has
// succeeded or failed.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::Logon( XONLINE_USER* pUser0, XONLINE_USER* pUser1, XONLINE_USER* pUser2, XONLINE_USER* pUser3 )
{
    assert( hLogonTask == 0 );
    // We have to log on at least one user.
    assert( pUser0 || pUser1 || pUser2 || pUser3 );

    // Initialize the user list to zero.
    XONLINE_USER pUserList[ XGetPortCount() ] = { 0 };

    if( pUser0 )
        pUserList[ 0 ] = *pUser0;
    if( pUser1 )
        pUserList[ 1 ] = *pUser1;
    if( pUser2 )
        pUserList[ 2 ] = *pUser2;
    if( pUser3 )
        pUserList[ 3 ] = *pUser3;

    const DWORD NUM_SERVICES = 4;
    DWORD m_pServices[NUM_SERVICES] =
    {
        XONLINE_MATCHMAKING_SERVICE,
        XONLINE_FEEDBACK_SERVICE,
        XONLINE_ARBITRATION_SERVICE,
        XONLINE_QUERY_SERVICE
    };

    BOOL Result = SignIn( pUserList, m_pServices, NUM_SERVICES );

    return Result;
}




//-----------------------------------------------------------------------------
// Name: Logoff()
// Desc: Logoff all users. The competitions sample frequently logs off players
// because it is simulating a multi-user competition on one box. A real
// game supporting competitions would leave the user logged on.
//-----------------------------------------------------------------------------
VOID CXBoxSample::Logoff()
{
    if( !hLogonTask )
        return;

    XOnlineTaskClose( hLogonTask );
    hLogonTask = 0;
}




//-----------------------------------------------------------------------------
// Name: ReportMessageAndLogoff()
// Desc: Print the specified string and data to a buffer and go to the error
// screen. If the user is logged on, log them off.
//-----------------------------------------------------------------------------
VOID CXBoxSample::ReportMessageAndLogoff( const WCHAR* format, ... )
{
    va_list arglist;
    va_start( arglist, format );
    const DWORD MessageSize = sizeof( m_strMessage ) / sizeof( m_strMessage[0] );
    _vsnwprintf( m_strMessage, MessageSize, format, arglist );
    va_end( arglist );

    m_strMessage[ MessageSize - 1 ] = '\0';
    OutputDebugStringW( m_strMessage );
    OutputDebugStringW( L"\n" );

    // If we are logged on, log off.
    Logoff();

    StartScreen( SCREEN_ERROR );
}




//-----------------------------------------------------------------------------
// Name: CreateTournament()
// Desc: Create a tournament for the selected user and register all users
// on the box - up to eight - for the competition.
//-----------------------------------------------------------------------------
VOID CXBoxSample::CreateTournament()
{
    DWORD portNumber = 0;

    // Simple way to show that something is happening.
    m_ScreenHeader = L"Creating competition. Please wait...";
    Render();

    // Create a tournament using the account of the selected user, as stored
    // in g_SelectEntry.
    assert( g_SelectEntry >= 0 && g_SelectEntry < m_UserList.size() );
    BOOL Result = Logon( &m_UserList[ g_SelectEntry ], 0, 0, 0 );
    if ( !Result )
    {
        // Don't show any error message here - Logon will report the errors.
        return;
    }

    // Create a tournament.
    XONLINE_COMP_SINGLE_ELIMINATION_ATTRIBUTES compAttributes = {0};
    compAttributes.dwPrivateSlots = 0;
    compAttributes.dwPublicSlots = MAX_ENTRANTS - compAttributes.dwPrivateSlots;
    // The competition will be cancelled if the minimum number of players don't
    // sign up before registration closes.
    compAttributes.dwMinimumPlayers = 2;

    FILETIME    SystemTime;
    GetSystemTimeAsFileTime( &SystemTime );
    // Registration opens today.
    compAttributes.ftRegistrationOpen = SystemTime;

    // Set the registration to close in 24 hours.
    compAttributes.ftRegistrationClose = AddSeconds( compAttributes.ftRegistrationOpen,
            SECONDS_BEFORE_TOURNAMENT_CLOSE );

    // Make sure that competition start isn't earlier than registration close.
    // I set it to five seconds later - you would normally have a longer delay.
    const DWORD MINSTARTDELAYSECONDS = 5;
    compAttributes.ftCompetitionStart = AddSeconds( compAttributes.ftRegistrationClose,
            MINSTARTDELAYSECONDS );  

    compAttributes.dwMatchReminderAdvanceMinutes = REMINDER_ADVANCE_MINUTES;

    // The time from registration close to tournament start
    // needs to be longer than the reminder time.
    // There must be sufficient time between compStart and roundOneStart to fire a reminder.
    // The round one start time must be greater than the competition start time plus
    // the reminder time - I add one extra second to ensure it is greater.
    compAttributes.ftRoundOneStart = AddSeconds( compAttributes.ftCompetitionStart,
            ( REMINDER_ADVANCE_MINUTES + 1 ) * SECONDS_PER_MINUTE );

    // The first round must end before the second round starts.
    compAttributes.ftRoundOneEnd = AddSeconds( compAttributes.ftRoundOneStart,
            ROUND_LENGTH_IN_SECONDS );

    // This code would set up daily rounds.
    // This says that we have a new round every day.
    //compAttributes.Interval = XONLINE_COMP_INTERVAL_DAILY;
    // When the Interval type is daily we set a day mask rather than a count.
    //compAttributes.UnitOrMask.DayMask = XONLINE_COMP_DAY_MASK_ALL;

    // Specify how frequently the rounds should happen.
    compAttributes.Interval = XONLINE_COMP_INTERVAL_MINUTE;
    compAttributes.UnitOrMask.dwUnitsOfTime = SECONDS_BETWEEN_ROUNDS / SECONDS_PER_MINUTE;

    compAttributes.fTeamCompetition = FALSE;
    // Team size is irrelevant since this isn't a team competition.
    //compAttributes.dwTeamSize = ;

    // This competition expects 3 additional arguments ...
    // This is specific to the way this competition is set up, other competitions may not
    // need attributes at all

    // With the generated code from XLAST, all we need to do
    // is pass the extra arguments into the competition-specific
    // Create function.
    WCHAR wszName[] = L"Single Elimination sample tournament";
    const BYTE rgbBlobValue[7] = { 0x1, 0x2, 0x3, 0x4, 0x3, 0x2, 0x1 };
    const DWORD mapID = 1234;

    CBlob tempBlob( sizeof( rgbBlobValue ), (PVOID)rgbBlobValue );
    HRESULT hr = g_MyCompetition.Create(
            portNumber,
            &compAttributes,
            NULL, NULL, NULL, // display name, desc, url
            mapID,
            wszName,
            tempBlob
            );

    // When the xonline task for creating this competition
    // is complete, g_MyCompetition will hold the new competition
    // ID for us.
    if( !WaitForTaskToComplete( g_MyCompetition, &hr ) )
    {
        // The quota for competitions will probably be about three active competitions
        // per user, although it may be higher on the test network. Competitions that
        // are recently cancelled will count against this quota.
        // Give a verbose error message for this particular error.
        if( hr == XONLINE_E_QUERY_QUOTA_FULL )
            ReportMessageAndLogoff( L"Competitions quota exceeded for this user.\n"
                            L"Select a different user and try again." );
        return;
    }

    // Since not everything uses g_MyCompetition, save the competition ID elsewhere
    m_qwCompetitionID = g_MyCompetition.m_CreateResults.qwCompetitionID;

    // Have the creator join the competition they just created.
    hr = JoinCompetitionAndLogoff( portNumber );
    // If the creator can't join his own competition, let's just give up.
    // Should perhaps cancel the competition.
    if( FAILED( hr ) )
        return;


    // Now register some users for this competition. We already have one.
    DWORD EntrantCount = 1;

    for( DWORD i = 0; i < m_UserList.size() && EntrantCount < MAX_ENTRANTS; ++i )
    {
        // Log on the next user and have them join the competition - being careful
        // to skip the creator, who has already joined.
        if( i != g_SelectEntry && Logon( &m_UserList[i], 0, 0, 0 ) )
        {
            // Join the logged on user to the active competition.

            hr = JoinCompetitionAndLogoff( portNumber );
            // If registration is closed then it's not a fatal error for the
            // competition - it just means that not everyone was registered.
            // This error will happen if the registration process takes too
            // long and the registration time is short.
            if( hr == XONLINE_E_COMP_REGISTRATION_CLOSED )
                break;

            if( FAILED( hr ) )
            {
                // For other fatal errors just exit - JoinCompetitionAndLogoff()
                // will have displayed an adequate error message.
                return;
            }
            else
            {
                ++EntrantCount;
            }
        }
    }

    // Let the user know what was created.
    ReportMessageAndLogoff( L"%d users added to competition 0x%016I64x.\n"
                 L"Competition will start once the %d second start\n"
                 L"delay has elapsed.",
                 EntrantCount, m_qwCompetitionID, SECONDS_BEFORE_TOURNAMENT_CLOSE );
}




//-----------------------------------------------------------------------------
// Name: JoinCompetitionAndLogoff()
// Desc: Join the logged on user to the current competition. Return whatever
// HRESULT comes back from the service.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::JoinCompetitionAndLogoff( DWORD portNumber )
{
    CXBOnlineTask manageTask;

    // You can specify extra data such as their seed - typically a rating from
    // a leaderboard
    // XONLINE_COMP_ATTR_ENTRANT_SEED

    // Add the new user to the competition. This is asynchronous and you would
    // normally want to do other things while waiting for the task to complete.

    // The competition manager has a thin wrapper for XOnlineCompetitionManageEntrant
    // that makes it slightly easier for us - we don't need to worry about the
    // competition ID because that was saved when we created the competition
    // Note that we could also have set the competition ID manually.  For instance,
    // we could have searched for the currently available competitions, selected one,
    // and then joined it by setting g_MyCompetition.m_CreateResults.qwCompetitionID
    HRESULT hr = g_MyCompetition.ManageEntrant( XONLINE_COMP_ACTION_JOIN, portNumber,
                    0, NULL );

    if( FAILED( hr ) )
    {
        ReportMessageAndLogoff( L"XOnlineCompetitionManageEntrant failed (error 0x%x)", hr );
        return hr;
    }

    // If this fails with 0x80156206 (XONLINE_E_COMP_CANCELLED) it may mean that the
    // competition was closed before enough competitors signed up, and was
    // therefore automatically cancelled.
    // If this fails with 0x80156203 (XONLINE_E_COMP_REGISTRATION_CLOSED) it means that the
    // registration closed, presumably because the registration open time had expired.
    if( !WaitForTaskToComplete( g_MyCompetition, &hr ) )
        return hr;

    Logoff();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: TournamentSearch()
// Desc: Search for all tournaments associated with the selected user.
// The list of tournaments is then displayed on the
// select screen for the user to choose from.
//-----------------------------------------------------------------------------
VOID CXBoxSample::TournamentSearch()
{
    assert( m_UserList.size() > 0 );
    // Logon the selected user
    BOOL Result = Logon( &m_UserList[ g_SelectEntry ], 0, 0, 0 );
    if ( !Result )
    {
        // Don't show any error message here - Logon will report the errors.
        return;
    }

    // Copy the selected user index so we can use that when querying the tournament topology.
    m_SelectedUser = g_SelectEntry;

    // For sample purposes we always query on controller zero.
    DWORD dwUserIndex = 0;

    // Do a search query to find out what competitions this user is signed up for.
    // How many items are in the list to select from?
    // We start at zero.
    g_SelectSize = 0;

    // Errors on the first search pass will cause the user to be logged out, in
    // which case we need to stop searching - but may still want to display
    // whatever results we already obtained.
    for( DWORD SearchPass = 0; SearchPass < 2 && hLogonTask; ++SearchPass )
    {
        // How many results on this pass?
        DWORD TotalThisPass = 0;
        for( DWORD page = 0; g_SelectSize < MAX_SEARCH_RESULTS; ++page )
        {
            // g_CompQuery is an instance of the class that performs
            // this query. All we have to do is reset it, then perform the
            // query. We call StopTask here because g_CompQuery may
            // be in the STATE_DONE state, which means a previous query
            // completed successfully. We immediately use the results of the
            // previous query in this sample, so just reset the query class
            // instance and continue...
            HRESULT hr = g_CompQuery.Query(
                    XOnlineGetLogonUsers()[dwUserIndex].xuid.qwUserID,
                    // First search for active competitions, then search for
                    // competitions that are not yet ready for playing
                    ( SearchPass == 0 ) ? XONLINE_COMP_STATUS_ACTIVE : XONLINE_COMP_STATUS_PRE_INIT,
                    page
                    );
            if( FAILED( hr ) )
            {
                ReportMessageAndLogoff( L"Error 0x%08lx searching for competitions.", hr );
                return;
            }

            // When the query task is completed, the query class instance
            // will reap the query results and store it in its local buffer
            if( !WaitForTaskToComplete( &g_CompQuery ) )
            {
                // Error message will be created by WaitForTaskToComplete
                return;
            }

            // We don't have to call XOnlineCompetitionSearchGetResults - the query
            // class did that for us.  We will use some information it picked up
            // from doing so, however.
            DWORD   cReturned = g_CompQuery.dwItemsReturned;
            DWORD   cTotal = g_CompQuery.dwTotalItemsInResult;

            // Make sure we are getting reasonable results.
            assert( cReturned <= cTotal );

            // Make a list of strings to choose from.
            for( DWORD i = 0; i < cReturned && g_SelectSize < MAX_SEARCH_RESULTS; ++i )
            {
                const CHAR* Status = "";

                // The XLAST generated query code saved the results in a local buffer
                // All we have to do is iterate over the results member of the query class instance
                if( g_CompQuery.Results[ i ].att_comp_status == XONLINE_COMP_STATUS_ACTIVE )
                    Status = "active";
                else
                {
                    assert( g_CompQuery.Results[ i ].att_comp_status == XONLINE_COMP_STATUS_PRE_INIT );
                    Status = "pre-init";
                }
                sprintf( g_SelectList[ g_SelectSize ], "Competition ID 0x%016I64x, status: %s\n",
                            g_CompQuery.Results[ i ].att_comp_id,
                            Status );
                // Print information about the competition to the debug output
                XBUtil_DebugPrint( g_SelectList[ g_SelectSize ] );

                // Copy the new entries into our own search results buffer
                // We do this because g_CompQuery only retrieves one
                // page of results at a time whereas we want information
                // about all the results in memory
                memcpy( &g_SearchResults[ g_SelectSize ], &g_CompQuery.Results[ i ],
                        sizeof( CSEEntrantsMyCompetitionsQueryResult ) );
                ++g_SelectSize;
            }

            TotalThisPass += cReturned;
            // No need to do an additional query - we have all of the results.
            if( TotalThisPass == cTotal )
                break;
        }
    }

    Logoff();

    // Either report that no competitions were found, or let the user select one to display.
    if( g_SelectSize == 0 )
        ReportMessageAndLogoff( L"No competitions found." );
    else
    {
        g_SelectNextScreen = SCREEN_DISPLAYTOURNAMENT;
        g_SelectScreenHeader = L"Press " GLYPH_A_BUTTON L" to select a tournament.\n"
                               L"Press " GLYPH_B_BUTTON L" to cancel.";
        StartScreen( SCREEN_SELECT );
    }
}




//-----------------------------------------------------------------------------
// Name: TimeWarp()
// Desc: Warp through time - tell the server to adjust the competition times back so that it
// is time for the next round. This is equivalent to moving the user into the future.
// This can be done to allow testing of competitions that would otherwise take days,
// and avoids the timing problems of having very short rounds.
// This function works on the currently selected competition.
//-----------------------------------------------------------------------------
VOID CXBoxSample::TimeWarp()
{
    // Sign in the same user that was used for the tournament search, since we
    // know that they are in this tournament.
    // In a real application you would display all tournaments applicable to the
    // logged in user and that would be appropriate.
    BOOL Result = Logon( &m_UserList[ m_SelectedUser ], 0, 0, 0 );
    if ( !Result )
    {
        // Don't show any error message here - Logon will report the errors.
        return;
    }

    DWORD portNumber = 0;
    const DWORD cNumAttributes = 1;
    XONLINE_ATTRIBUTE Attributes[ cNumAttributes ] =
    {
        { XONLINE_COMP_ATTR_DEBUG_ADVANCE_TIME, TRUE, TIME_WARP_SECONDS }
    };

    CXBOnlineTask timeWarpTask;

    HRESULT hr = XOnlineQuerySelect( portNumber, 0, COMPETITIONS_DATASET,
            m_qwCompetitionID, XONLINE_COMP_ACTION_DEBUG_ADVANCE_TIME,
            cNumAttributes, Attributes, NULL, &timeWarpTask );

    if( FAILED( hr ) )
    {
        ReportMessageAndLogoff( L"XOnlineQuerySelect failed (error 0x%x)", hr );
        return;
    }

    // Wait for server tasks to complete - failures will be reported.
    if( !WaitForTaskToComplete( timeWarpTask ) )
        return;

    ReportMessageAndLogoff( L"Time has been advanced %d seconds. Please wait\n"
                            L"about 30 seconds for timed server tasks to complete.",
                            TIME_WARP_SECONDS );
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // This InputEvent should generally be restricted to only coming from logged
    // in controllers.
    XBGAMEPAD_EVENT InputEvent = m_DefaultGamepad.Event;

    switch( m_WhichScreen )
    {
    case SCREEN_SELECT:
        // Let the user select from the item in the list - either a user or
        // a tournament.
        if( InputEvent == XBGAMEPAD_A )
        {
            StartScreen( g_SelectNextScreen );
        }

        if( InputEvent == XBGAMEPAD_B )
        {
            StartScreen( SCREEN_INTRO );
        }

        if( InputEvent == XBGAMEPAD_DPAD_UP )
        {
            if( g_SelectEntry == 0 )
                g_SelectEntry = g_SelectSize - 1;
            else
                --g_SelectEntry;
        }

        if( InputEvent == XBGAMEPAD_DPAD_DOWN )
        {
            if( g_SelectEntry == g_SelectSize - 1 )
                g_SelectEntry = 0;
            else
                ++g_SelectEntry;
        }
        break;

    case SCREEN_INTRO:
        // Select a user, for creating a tournament or searching for tournaments
        if( InputEvent == XBGAMEPAD_A || InputEvent == XBGAMEPAD_Y )
        {
            // Make a list of strings to choose from.
            for( g_SelectSize = 0; g_SelectSize < m_UserList.size(); ++g_SelectSize )
            {
                sprintf( g_SelectList[ g_SelectSize ], "%s", m_UserList[ g_SelectSize ].szGamertag );
            }

            if( InputEvent == XBGAMEPAD_A )
            {
                g_SelectNextScreen = SCREEN_CREATETOURNAMENT;
                g_SelectScreenHeader = L"Press " GLYPH_A_BUTTON L" to select user to create tournament.\n"
                                    L"Press " GLYPH_B_BUTTON L" to cancel.";

                if( g_SelectSize == 0 )
                    ReportMessageAndLogoff( L"No users available to create a tournament." );
                else if( g_SelectSize == 1 )
                    ReportMessageAndLogoff( L"Only one user found.\nTwo users required to populate a tournament." );
                else
                    StartScreen( SCREEN_SELECT );
            }
            else
            {
                g_SelectNextScreen = SCREEN_STARTSEARCH;
                g_SelectScreenHeader = L"Press " GLYPH_A_BUTTON L" to select user to search for tournaments.\n"
                                    L"Press " GLYPH_B_BUTTON L" to cancel.";

                if( g_SelectSize == 0 )
                    ReportMessageAndLogoff( L"No users available to search for tournaments." );
                else
                    StartScreen( SCREEN_SELECT );
            }
        }
    break;

    case SCREEN_ERROR:
        if( InputEvent == XBGAMEPAD_A )
            StartScreen( SCREEN_INTRO );
        break;

    case SCREEN_DISPLAYTOURNAMENT:
        if( InputEvent == XBGAMEPAD_B )
        {
            StartScreen( SCREEN_INTRO );
        }

        if( InputEvent == XBGAMEPAD_A )
        {
            PlayRound();
        }

        if( InputEvent == XBGAMEPAD_X )
        {
            TimeWarp();
        }
        break;

    default:
        assert( 0 );
        break;
    }

    // Maintain our connection once we've logged on
    if( hLogonTask )
    {
        HRESULT hr = XOnlineTaskContinue( hLogonTask );

        if( FAILED( hr ) )
        {
            if( hr == XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON )
                ReportMessageAndLogoff( L"You have been signed out because your\n"
                                      L"account signed in on another Xbox" );
            else
                ReportMessageAndLogoff( L"Connection was lost. Must relogin" );

            return S_OK;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: PlayRound()
// Desc: Play one round of the tournament, choosing a random winner, and using
// the arbitration service to submit the results.
//-----------------------------------------------------------------------------
VOID CXBoxSample::PlayRound()
{
    // Play a round of the tournament.
    Logoff();

    // Normally a user would find out about matches that they need to play via
    // reminders. In this artificial scenario we are getting the topology and
    // then logging in the users who need to play the next round.
    DWORD eventIndex = 0;
    while( eventIndex < g_TopologyCount )
    {
        // Look for a round that hasn't been played.
        if( g_Topology[ eventIndex ].winner == 0 )
            break;
        ++eventIndex;
    }

    ULONGLONG eventID = g_Topology[ eventIndex ].bi_entity_id;
    if( g_Topology[ eventIndex ].player1 == 0 ||
            g_Topology[ eventIndex ].player1 == 0 )
    {
        ReportMessageAndLogoff( L"No rounds ready to play" );
        return;
    }

    XONLINE_USER players[2];
    players[0] = GetUserStruct( g_Topology[ eventIndex ].player1 );
    players[1] = GetUserStruct( g_Topology[ eventIndex ].player2 );
    if( players[0].xuid.qwUserID == 0 || players[1].xuid.qwUserID == 0 )
    {
        ReportMessageAndLogoff( L"User(s) have been deleted - match cannot be played." );
        return;
    }

    if( !Logon( &players[0], &players[1], 0, 0 ) )
    {
        XBUtil_DebugPrint( "Logon failed!\n" );
        return;
    }

    // If we use the g_MyCompetition object to manager our competition then
    // we have to initialize it with the competition ID.
    g_MyCompetition.m_CreateResults.qwCompetitionID = m_qwCompetitionID;

    XONLINE_ARB_ID  arbid = { 0 };
    // Normally the users who were going to play a match would find each
    // other with a matchmaking query. They could then use
    // session ID to create the arbitration ID.
    memcpy( &arbid.SessionID, "FAKESESS", sizeof( arbid.SessionID ) );

    // Set the maximum round length. The game is responsible for extending this if necessary.
    const DWORD MAX_ROUND_SECONDS = 5 * SECONDS_PER_MINUTE;
    const DWORD flags = XONLINE_ARB_REGISTER_FLAG_USER_COMPETITION;

    // Now we register for the competition. Each player must do this with
    // the same arbitration id.
    for( DWORD portNumber = 0; portNumber < 2; ++portNumber )
    {
        // Again, g_MyCompetition provides a thin wrapper to XOnlineCompetitionCheckin
        HRESULT hr = g_MyCompetition.Checkin( portNumber, eventID );
        if( FAILED( hr ) )
        {
            ReportMessageAndLogoff( L"XOnlineCompetitionCheckin failed (error 0x%x)", hr );
            return;
        }

        // There is no information to reap from Checkin, except
        // a success/failure code
        if( !WaitForTaskToComplete( g_MyCompetition ) )
            return;

        // Then the host creates an arbitration round id.
        if( portNumber == 0 )
        {
            // Only the host creates the round id
            HRESULT hr = XOnlineArbitrationCreateRoundID( &arbid.qwRoundID );
            if( FAILED( hr ) )
                return;
            // Now this arbitration ID has to be shared with the other person who is
            // playing this round. Since we are playing the rounds on one box this
            // is very easy.
        }

        // The XLAST generated code provides a simple wrapper
        // to XOnlineSessionRegister[GetResults].
        // Because the competition manager has no internal buffer for
        // reaping the results of XOnlineSessionRegister, you must
        // call the SessionRegisterGetResults() member of the manager yourself
        // to reap the results and close the xonline task
        hr = g_MyCompetition.SessionRegister( &arbid, MAX_ROUND_SECONDS, flags );
        if( FAILED( hr ) )
        {
            ReportMessageAndLogoff( L"XOnlineCompetitionCheckin failed (error 0x%x)", hr );
            return;
        }
        if( !WaitForTaskToComplete( g_MyCompetition ) )
        {
            return;
        }

        // Here, however, we don't care about the results of the call
        // We know the task ran to completion and succeeded, so just stop the task
        g_MyCompetition.StopTask();
    }


    // Register for arbitration also. This should be done once per box. All logged on users on that
    // machine will be registered for arbitration.
    CXBOnlineTask ArbitrationHandle;
    HRESULT hr = XOnlineArbitrationRegister( &arbid, MAX_ROUND_SECONDS, flags,
                0, &ArbitrationHandle );
    if( FAILED( hr ) )
        return;

    if( !WaitForTaskToComplete( ArbitrationHandle ) )
    {
        return;
    }

    const DWORD REGISTRANTSBUFFERSIZE = 2;
    // Allocate a buffer for the registrants and zero it.
    XONLINE_ARB_REGISTRANT RegistrantsBuffer[ REGISTRANTSBUFFERSIZE ] = { 0 };

    // You can find out how many machines have registered so far with the specified arbitration ID.
    // In this scenario it will be one. In the general arbitration case you would iterate
    // through the results in the RegistrantsBuffer, adding up the users on each machine.
    DWORD NumRegisteredBoxes = 0;
    hr = XOnlineArbitrationRegisterGetResults( ArbitrationHandle,
            REGISTRANTSBUFFERSIZE, RegistrantsBuffer, &NumRegisteredBoxes );
    if( FAILED( hr ) )
        return;

    // Now we build the results package.
    BOOL PlayerOneWins = rand() % 2;
    const DWORD NUM_RESULT_ATTRIBUTES = 3;
    XONLINE_ATTRIBUTE   rgAttributes[NUM_RESULT_ATTRIBUTES];
    // The attributes must be in this order - entityid, winner, then loser.
    rgAttributes[0].dwAttributeID = XONLINE_COMP_ATTR_EVENT_ENTITY_ID;
    rgAttributes[0].info.integer.qwValue = g_Topology[ eventIndex ].bi_entity_id;
    rgAttributes[1].dwAttributeID = XONLINE_COMP_ATTR_EVENT_WINNER;
    rgAttributes[1].info.integer.qwValue = PlayerOneWins ?
            g_Topology[ eventIndex ].player1 : g_Topology[ eventIndex ].player2;
    rgAttributes[2].dwAttributeID = XONLINE_COMP_ATTR_EVENT_LOSER;
    rgAttributes[2].info.integer.qwValue = PlayerOneWins ?
            g_Topology[ eventIndex ].player2 : g_Topology[ eventIndex ].player1;

    // Now we submit our results.
    DWORD ArbitrationReportFlags = 0;

    // The competition manager provides a thin wrapper for submitting results
    hr = g_MyCompetition.SubmitResults( &arbid, ArbitrationReportFlags, NULL, 0,
            NULL, NUM_RESULT_ATTRIBUTES, rgAttributes );
    if( FAILED( hr ) )
        return;

    if( !WaitForTaskToComplete( g_MyCompetition ) )
        return;

    // We're done so we log off - a real game wouldn't log off at this point.
    Logoff();

    ReportMessageAndLogoff( L"%s won the round.", PlayerOneWins ?
                g_Topology[ eventIndex ].p1_gamertag :
                g_Topology[ eventIndex ].p2_gamertag );
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background and clear the zbuffer
    RenderGradientBackground( 0xff404040, 0xff404080 );

    // Show title and other information
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 48, 36, 0xffffffff, L"SingleElimination" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );

    // Draw the generic header and footer text.
    m_Font.DrawText( 40, 72, 0xffffffff, m_ScreenHeader, 0 );
    m_Font.DrawText( 40, 380, 0xffffffff, m_ScreenFooter, 0 );

    // Display any error or informative messages there may be.
    m_Font.DrawText( 320, 380, 0xffffffff, m_strMessage, XBFONT_CENTER_X );

    switch( m_WhichScreen )
    {
        case SCREEN_DISPLAYTOURNAMENT:
        {
            // Render the topology of the tournament from the data obtained previously.
            m_Font.SetScaleFactors( .9f, .9f );
            FLOAT hOffset = 40;
            FLOAT hDelta = 200;
            FLOAT vDelta = 81;
            FLOAT vStart = 120;
            FLOAT vOffset = vStart + vDelta / 2;
            ULONGLONG currentRound = g_Topology[0].round;
            WCHAR    buffer[2000];
            for( DWORD i = 0; i < g_TopologyCount; ++i )
            {
                // Make sure we have reasonable information - we don't want
                // a player playing themself or beating themself.
                // Make sure the players is not playing themself.
                assert( g_Topology[i].player1 == 0 ||
                        g_Topology[i].player1 != g_Topology[i].player2 );
                // Make sure the loser is not also the winner.
                assert( g_Topology[i].loser == 0 ||
                        g_Topology[i].loser != g_Topology[i].winner );
                // The loser, if set, should be player 1 or player 2.
                assert( g_Topology[i].loser == 0 ||
                        g_Topology[i].loser == g_Topology[i].player1 ||
                        g_Topology[i].loser == g_Topology[i].player2 );
                // The winner, if set, should be player 1 or player 2.
                assert( g_Topology[i].winner == 0 ||
                        g_Topology[i].winner == g_Topology[i].player1 ||
                        g_Topology[i].winner == g_Topology[i].player2 );

                if( g_Topology[i].round != currentRound )
                {
                    // Skip to the next round - move horizontally and reset
                    // the vertical position.
                    currentRound = g_Topology[i].round;
                    vDelta *= 2;
                    hOffset += hDelta;
                    vOffset = vStart + vDelta / 2; 
                }

                if( g_Topology[i].player1 && g_Topology[i].player2 == 0 &&
                                g_Topology[i].winner )
                {
                    // If a player is playing nobody and they won, that's a bye.
                    swprintf( buffer, L"%s\ngets a bye", g_Topology[i].p1_gamertag );
                }
                else if( g_Topology[i].player2 && g_Topology[i].player1 == 0 &&
                                g_Topology[i].winner )
                {
                    // If a player is playing nobody and they won, that's a bye.
                    // Note that when there is a bye the non-zero player could be player 1
                    // or player 2.
                    swprintf( buffer, L"%s\ngets a bye", g_Topology[i].p2_gamertag );
                }
                else if( g_Topology[i].winner )
                {
                    // Otherwise, if we have a winner then this round has been played.
                    // Display the winner and loser.
                    if( g_Topology[i].player1 == g_Topology[i].winner )
                        swprintf( buffer, L"%s\nbeat\n%s", g_Topology[i].p1_gamertag,
                                    g_Topology[i].p2_gamertag );
                    else
                        swprintf( buffer, L"%s\nbeat\n%s", g_Topology[i].p2_gamertag,
                                    g_Topology[i].p1_gamertag );
                }
                else
                {
                    // Otherwise we have an unplayed match - one or more users may not yet be
                    // determined.
                    swprintf( buffer, L"%s\nv.s.\n%s", g_Topology[i].p1_gamertag,
                                g_Topology[i].p2_gamertag );
                }

                m_Font.DrawText( hOffset, vOffset, 0xffffffff, buffer, XBFONT_CENTER_Y );
                vOffset += vDelta;
            }

            DWORD lastMatch = g_TopologyCount - 1;
            if( g_TopologyCount > 0 && g_Topology[lastMatch].winner )
            {
                WCHAR* winnerName = g_Topology[lastMatch].p1_gamertag;
                if( g_Topology[lastMatch].winner == g_Topology[lastMatch].player2 )
                    winnerName = g_Topology[lastMatch].p2_gamertag;

                swprintf( buffer, L"%s won!", winnerName );
                m_Font.DrawText( hOffset, 360, 0xffffffff, buffer, XBFONT_CENTER_Y );
            }

            m_Font.SetScaleFactors( 1.0f, 1.0f );
            break;
        }

        case SCREEN_SELECT:
        {
            // Render the list of items to select from.
            DWORD ListOffset = 0;
            // If the selected item is far enough down, adjust the list
            // position to make it visible.
            if( g_SelectEntry > MAX_VISIBLE_SELECT_ITEMS - 1 )
                ListOffset = g_SelectEntry + 1 - MAX_VISIBLE_SELECT_ITEMS;
            FLOAT hOffset = 40;
            FLOAT vOffset = 135;
            FLOAT vDelta = 25;
            for( DWORD i = 0; i < g_SelectSize && i < MAX_VISIBLE_SELECT_ITEMS; ++i )
            {
                WCHAR buffer[1000];
                swprintf( buffer, L"%S", g_SelectList[ i + ListOffset ] );
                // Highlight the selected item in brighter text
                if( i + ListOffset == g_SelectEntry )
                    m_Font.DrawText( hOffset, vOffset, 0xffffffff, buffer );
                else
                    m_Font.DrawText( hOffset, vOffset, 0x9f9f9f9f, buffer );
                vOffset += vDelta;
            }
            break;
        }
    }

    m_Font.End();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: WaitForTaskToComplete
// Desc: Helper function for waiting for a task to complete.
//       In a real game, this would be part of your game loop
//       Returns TRUE for success, FALSE for failure.
//       Optionally returns the results code in pHR
//-----------------------------------------------------------------------------
BOOL CXBoxSample::WaitForTaskToComplete( CXBOnlineTask& Task, HRESULT* pHR )
{
    HRESULT hr = XONLINETASK_S_RUNNING;

    while ( hr == XONLINETASK_S_RUNNING )
    {
        // have to pump logon and the task
        // In a real game, this would be part of your game loop- you
        // wouldn't block on this.

        assert( hLogonTask );
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) )
        {
            ReportMessageAndLogoff( L"Logon task failed with 0x%x", hr );
            if( pHR )
                *pHR = hr;
            return FALSE;
        }
        hr = Task.Continue();
        if ( FAILED( hr ) )
        {
            ReportMessageAndLogoff( L"Competitions task failed with 0x%x", hr );        
            if( pHR )
                *pHR = hr;
            return FALSE;
        }
        
        // put in a delay of about 1 frame so as not to spam with MessageSendGetProgress
        Sleep( 15 );
    }
    
    if ( hr != XONLINETASK_S_SUCCESS )
    {
        ReportMessageAndLogoff( L"Task failed with 0x%x", hr );       
        if( pHR )
            *pHR = hr;
        return FALSE;
    }

    if( pHR )
        *pHR = S_OK;
    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: WaitForTaskToComplete
// Desc: Helper function for waiting for a task to complete.
//       This function waits on a CSECompetition object.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::WaitForTaskToComplete( CSECompetition& SEComp, HRESULT *pHR )
{
    HRESULT hr = XONLINETASK_S_RUNNING;

    while( hr == XONLINETASK_S_RUNNING )
    {
        // have to pump logon and the task
        // In a real game, this would be part of your game loop- you
        // wouldn't block on this.

        assert( hLogonTask );
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) )
        {
            ReportMessageAndLogoff( L"Logon task failed with 0x%x", hr );
            if( pHR )
                *pHR = hr;
            return FALSE;
        }

        hr = SEComp.Process();
        if ( FAILED( hr ) )
        {
            ReportMessageAndLogoff( L"Competitions task failed with 0x%x", hr );        
            if( pHR )
                *pHR = hr;
            return FALSE;
        }
        
        // put in a delay of about 1 frame so as not to spam with MessageSendGetProgress
        Sleep( 15 );
    }
    
    if( pHR )
        *pHR = S_OK;
    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: WaitForTaskToComplete
// Desc: Helper function for waiting for a task to complete.
//       This function waits on a ICompetitionQuery object.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::WaitForTaskToComplete( ICompetitionQuery* pQuery, HRESULT *pHR )
{
    HRESULT hr = XONLINETASK_S_RUNNING;

    while( hr == XONLINETASK_S_RUNNING )
    {
        // have to pump logon and the task
        // In a real game, this would be part of your game loop- you
        // wouldn't block on this.

        assert( hLogonTask );
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) )
        {
            ReportMessageAndLogoff( L"Logon task failed with 0x%x", hr );
            if( pHR )
                *pHR = hr;
            return FALSE;
        }

        hr = pQuery->Process();
        if ( FAILED( hr ) )
        {
            ReportMessageAndLogoff( L"Competitions task failed with 0x%x", hr );        
            if( pHR )
                *pHR = hr;
            return FALSE;
        }
        
        // put in a delay of about 1 frame so as not to spam with MessageSendGetProgress
        Sleep( 15 );
    }
    
    if( pHR )
        *pHR = S_OK;
    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: Print()
// Desc: Send formatted output to the debug window
//-----------------------------------------------------------------------------
VOID __cdecl Print( const WCHAR* strFormat, ... )
{
    const int MAX_OUTPUT_STR = 512;
    WCHAR strBuffer[MAX_OUTPUT_STR];

    va_list arglist;
    va_start( arglist, strFormat );
    _vsnwprintf( strBuffer, MAX_OUTPUT_STR, strFormat, arglist );
    va_end( arglist );
    strBuffer[ MAX_OUTPUT_STR - 1 ] = '\0';

    OutputDebugStringW( L"\n*** SingleElimination: " );
    OutputDebugStringW( strBuffer ); 
    OutputDebugStringW( L"\n" );
}




//-----------------------------------------------------------------------------
// Name: Error()
// Desc: Send formatted output to the debug window and to the screen.
// It is used for reporting fatal errors.
//-----------------------------------------------------------------------------
VOID __cdecl Error( const WCHAR* strFormat, ... )
{
    const int MAX_OUTPUT_STR = 512;
    WCHAR strBuffer[MAX_OUTPUT_STR];

    va_list arglist;
    va_start( arglist, strFormat );
    _vsnwprintf( strBuffer, MAX_OUTPUT_STR, strFormat, arglist );
    va_end( arglist );
    strBuffer[ MAX_OUTPUT_STR - 1 ] = '\0';

    OutputDebugStringW( L"\n*** SingleElimination: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n" );

    xbApp.ReportMessageAndLogoff( strBuffer );
}




//-----------------------------------------------------------------------------
// Name: UIMsg()
// Desc: Display a recommended user interface message
//       See Xbox_Terminology_List.xls for additional information.
//-----------------------------------------------------------------------------
VOID UIMsg( const WCHAR* strText )
{
    OutputDebugStringW( L"\n*** SingleElimination: UI Message:\n" );
    OutputDebugStringW( strText );
    OutputDebugStringW( L"\n" );
    xbApp.ReportMessageAndLogoff( strText );
}




//-----------------------------------------------------------------------------
// Name: AddSeconds()
// Desc: Add the specified number of seconds to the FILETIME passed in, and return
// the result.
//-----------------------------------------------------------------------------
FILETIME AddSeconds( FILETIME StartTime, __int64 DeltaSeconds )
{
    // FILETIME structures count 100-nanosecond intervals, therefore there
    // are 10,000,000 ticks per second in a FILETIME structure.
    const int TICKSPERSECOND = 10000000;

    // Copy StartTime to a ULARGE_INTEGER
    ULARGE_INTEGER data;
    assert( sizeof( data ) == sizeof( StartTime ) );
    memcpy( &data, &StartTime, sizeof( data ) );

    // Do the math
    data.QuadPart += DeltaSeconds * TICKSPERSECOND;

    FILETIME Result;
    memcpy( &Result, &data, sizeof( data ) );
    return Result;
}
