//-----------------------------------------------------------------------------
// File: FriendsManager.h
//
// Desc: Class and structure definitions for Friends Manager and related
//       objects
//
// Hist: 07.25.02 - Initial creation
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef FRIENDSMANAGER_H
#define FRIENDSMANAGER_H

#include <xtl.h>
#include <xonline.h>
#include <vector>
#include <XBOnline.h>

#if _DEBUG
static const DWORD DBGWARN_SIZE = 200;
#define FM_WARN(x) DbgWarn##x
#else
#define FM_WARN(x)
#endif // _DEBUG




//-----------------------------------------------------------------------------
// Name: class CFriendsManager
// Desc: Provides a layer of abstraction for managing friends, presence, etc.
//-----------------------------------------------------------------------------
class CFriendsManager
{
protected:
    // Friends enumeration state
    enum FRIEND_ENUMERATE_STATE
    {        
        STATE_IDLE,
        STATE_ENUMERATING,
        STATE_CLOSING,
    };

    // Friend list struct
    struct FRIENDLIST
    {
        DWORD              dwState;        // State of this friends list
        XONLINETASK_HANDLE hEnumerate;     // Enumeration task
        BOOL               bAutoStop;      // Automatic stop when done?
        BOOL               bHasChanged;    // Changed since last check?
        DWORD              dwNumFriends;   // Number of friends in list
        XONLINE_FRIEND*    pFriends;       // Array of XONLINE_FRIENDs
    };
    
    // Friends data:
    XONLINETASK_HANDLE m_hFriendsStartup;
    FRIENDLIST         m_aFriendsLists[ XONLINE_MAX_LOGON_USERS ];

    // Saves whether they are a valid user as well as whether they have received a notification
    BYTE               m_byUserFlags[ XONLINE_MAX_LOGON_USERS ];

    // Array of tasks for feedback data. Every time feedback is sent on a
    // particular user, a new task is created which must be pumped to completion.
    typedef std::vector<XONLINETASK_HANDLE> TASKLIST;
    TASKLIST           m_ahFeedback;

    HRESULT PumpFeedback();
    HRESULT PumpFriends();    
    HRESULT StartEnumerateMuteList( DWORD dwUser );

#if _DEBUG
    // Debug-only function for validating internal state
    void DbgValidateState();

    void DbgWarn( WCHAR* format, ... );
#endif // _DEBUG

public:
    CFriendsManager();
    ~CFriendsManager();

    HRESULT Initialize();       // Initialize the Friends Manager object
    HRESULT Shutdown();         // Shut down the Friends Manager object
    HRESULT Process();          // Do any pending work - call every frame
    
    //-------------------------------------------------------------------------
    // Notifications
    //-------------------------------------------------------------------------

    // Returns whether the given player has received a new game invite since
    // the last call of this function.  This _does_ persist between 
    // calls to Process().  The other notifications won't (since we pump the 
    // friends enumeration task which clears them in Process() )
    
    BOOL           HasReceivedNewGameInvite( DWORD dwUser );

    //-------------------------------------------------------------------------
    // Friends
    //-------------------------------------------------------------------------

    // Call StartUpdatingFriends anytime you want to have up-to-date
    // friends information for the user.  Call StopUpdatingFriends when
    // done.  You'll still have friends information even when not enumerating,
    // it just won't be up-to-date
    HRESULT         StartUpdatingFriends( DWORD dwUser );
    HRESULT         StopUpdatingFriends( DWORD dwUser );

    // Add/Remove can be called to add/remove a friend from the user's
    // friends list.  Answer is used to answer a friend request
    HRESULT         AddPlayerToFriendsList( DWORD dwUser, XUID xuidPlayer );
    HRESULT         RemoveFriendFromFriendsList( DWORD dwUser, DWORD dwFriend );
    HRESULT         AnswerFriendRequest( DWORD dwUser, DWORD dwFriend, XONLINE_REQUEST_ANSWER_TYPE answer );

    // Send/Revoke are used to send and revoke game invites to a friend.  NULL
    // can be used to send/revoke to ALL friends.  Answer is used to answer a
    // game invite
    HRESULT         SendGameInvite( DWORD dwUser, XNKID sessionID, DWORD dwFriend );
    HRESULT         RevokeGameInvite( DWORD dwUser, XNKID sessionID, DWORD dwFriend );
    HRESULT         AnswerGameInvite( DWORD dwUser, DWORD dwFriend, XONLINE_GAMEINVITE_ANSWER_TYPE answer );

    // GetAcceptedGameInvite looks for an invitation that was accepted while
    // the player was in another title.  You should automatically log them on
    // and join the session
    HRESULT         GetAcceptedGameInvite( XONLINE_ACCEPTED_GAMEINVITE* pAcceptedGameInvite );

    // JoinCrossTitleGame can be used to initiate a join to a different title.
    // It will write out an accepted invite to the hard drive, and the title
    // should prompt the user to insert the proper disc
    HRESULT         JoinCrossTitleGame( DWORD dwUser, DWORD dwFriend );

    // Attempt to find the player in the user's friends list
    XONLINE_FRIEND* FindPlayerInFriendsList( DWORD dwUser, XUID xuidPlayer );

    // Returns TRUE if the user's friends list has changed since the last time
    // it was called
    BOOL            HasFriendsListChanged( DWORD dwUser );

    // Used for walking the list of friends (for display, etc.)
    DWORD           GetNumFriends( DWORD dwUser );
    XONLINE_FRIEND* GetFriend( DWORD dwUser, DWORD dwIndex );

    // Gets the name of the title the friend is playing.  If not enumerating
    // friends, strTitlename will be set to an empty string.
    HRESULT         GetFriendTitleName( XONLINE_FRIEND* pFriend, WORD wLanguage, 
                                        DWORD cTitleName, WCHAR* strTitlename );

    //-------------------------------------------------------------------------
    // Status
    //-------------------------------------------------------------------------
    
    // Finds state of a friend, and returns which icons to display
    ONLINEICON GetFriendVoiceIcon( DWORD dwUserIndex, DWORD dwFriendIndex );
    ONLINEICON GetFriendOnlineStateIcon( DWORD dwUserIndex, DWORD dwFriendIndex );
};

extern CFriendsManager g_FriendsManager;




#endif // FRIENDSMANAGER_H