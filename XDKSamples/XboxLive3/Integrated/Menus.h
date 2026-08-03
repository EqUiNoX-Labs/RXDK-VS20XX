//-------------------------------------------------------------------------------------
// File: Menus.h
//
// Desc: Holds menu utilitys and constant text used in the sample for menu choices.
//       Provides menu selection and rendering helpers.
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef MENUS_H
#define MENUS_H

#include <xtl.h>
#include "Common.h"
#include "XBFont.h"


// Screen position constants

const FLOAT POS_SCREEN_TITLE_Y       = 80.0f;
const FLOAT POS_VERSUS_Y             = 200.0f;
const FLOAT WIDTH_VERSUS_X           = 30.0f;
const FLOAT POS_GAME_SCORE_PADDING   = 30.0f;
const FLOAT POS_MESSAGE_Y            = 240.0f;
const FLOAT POS_GAME_SETUP_Y         = 200.0f;
const FLOAT POS_MENU_START_Y         = 100.0f;
const FLOAT POS_HEADER_Y             = 40.0f;
const FLOAT POS_HEADER_LEFT          = 40.0f;
const FLOAT POS_HEADER_RIGHT         = ( SCREEN_SIZE_X - POS_HEADER_LEFT );
const FLOAT POS_FOOTER_Y             = 420.0f;
const FLOAT POS_FOOTER_LEFT          = 40.0f;
const FLOAT POS_FOOTER_RIGHT         = ( SCREEN_SIZE_X - POS_FOOTER_LEFT );
const FLOAT DEFAULT_TEXT_PADDING     = 30.0f;
const FLOAT TEXT_PADDING_INVITE_INFO = 20.0f;


// Constants for the leaderboard

const FLOAT LEADERBOARD_TEXT_PADDING = ( DEFAULT_TEXT_PADDING * 0.8f );
const FLOAT POS_LEADER_HEADER_Y      = POS_MENU_START_Y + LEADERBOARD_TEXT_PADDING;
const FLOAT POS_TEAM_X               = 100.0f;
const FLOAT POS_KILLS_X              = 250.0f;
const FLOAT POS_DEATHS_X             = 300.0f;
const FLOAT POS_ASSISTS_X            = 375.0f;
const FLOAT POS_RATING_X             = 475.0f;


// Constants for the account selection screen

const FLOAT POS_ACCOUNT_LIST_START   = POS_SCREEN_TITLE_Y +
                                       ( DEFAULT_TEXT_PADDING * 2.0f );


// Constants for the Team Roster screen

const INT   NUM_ENTRIES_PER_SCREEN   = 8; // Only display 8 team members at once
const INT   NUM_TEAMS_PER_SCREEN     = 7;


// Events IDs used to trigger
// transitions in the state machine
// These refer to actions from
// the game controller:
enum Event
{
    EV_BUTTON_A,                // "A" button pressed
    EV_BUTTON_B,                // "B" button pressed
    EV_BUTTON_X,                // "X" button pressed
    EV_BUTTON_Y,                // "Y" button pressed
    EV_BUTTON_WHITE,            // "WHITE" button pressed
    EV_BUTTON_BLACK,            // "BLACK" button pressed
    EV_BUTTON_START,            // "START" button pressed
    EV_BUTTON_BACK,             // "BACK" button pressed
    EV_UP,                      // DPAD UP pressed
    EV_DOWN,                    // DPAD DOWN pressed
    EV_LEFT,                    // DPAD LEFT pressed
    EV_RIGHT,                   // DPAD RIGHT pressed
    EV_NULL                     // No Event / Idle event
};


// Bit flags used the the RenderFooter function
// to determine which parts to display/render

enum EFooterFlags
{
    FOOTER_RENDER_NONE   = 0,  // Render nothing / reset
    FOOTER_RENDER_SELECT = 1,  // Render "Select" in the BR corner
    FOOTER_RENDER_CANCEL = 2   // Render "Cancel" in the BL corner
};


////////////////
// Game Menus //
////////////////

// GAME SETUP menu
const WCHAR* const MENU_MAIN[]   =
{
    L"Create Match",
    L"QuickMatch",
    L"Inbox",
    L"User Settings",
    L"Team Leaderboard",
    L"Teams"
};
enum
{
    MENU_MAIN_CREATE_MATCH = 0,
    MENU_MAIN_QUICKMATCH,
    MENU_MAIN_INBOX,
    MENU_MAIN_USER_SETTINGS,
    MENU_MAIN_TEAMS_LEADERBOARD,
    MENU_MAIN_TEAMS,
    NUM_ITEMS_MAIN_MENU
};


// TEAMS menu
const WCHAR* const MENU_TEAMS[]        =
{
    L"Recent Players",
    L"View My Teams",
    L"Create A Team",
};
enum
{
    MENU_TEAMS_RECENT_PLAYERS = 0,
    MENU_TEAMS_VIEW_MY_TEAMS,
    MENU_TEAMS_CREATE_TEAM,
    NUM_ITEMS_TEAMS_MENU
};

// Menu Invite
const WCHAR* const MENU_INVITE[]       =
{
    L"Accept Invite",
    L"Decline Invite",
    L"Never Accept Invite From This Player"
};

enum
{
    MENU_INVITE_ACCEPT = 0,
    MENU_INVITE_DECLINE,
    MENU_INVITE_NEVER,
    NUM_ITEMS_INVITE_MENU
};

// Voice Mail Menu
const WCHAR* const MENU_VOICE_MAIL[]    =
{
    L"Record Voice Mail",
    L"Play Voice Mail",
    L"Clear Voice Mail",
    L"Send Voice Mail To Own Team",
    L"Send Voice Mail To Another Team"
};

enum
{
    MENU_VOICE_MAIL_RECORD = 0,
    MENU_VOICE_MAIL_PLAY,
    MENU_VOICE_MAIL_CLEAR,
    MENU_VOICE_MAIL_SEND_TO_OWN_TEAM,
    MENU_VOICE_MAIL_SEND_TO_ANOTHER_TEAM,
    NUM_ITEMS_VOICE_MAIL_MENU
};

// Menu Team operations
const WCHAR* const MENU_TEAM_OPS[] =
{
    L"View Roster",
    L"Edit Icon",
    L"View Joined Competitions",
    L"Search For Competitions",
    L"Create Competition",
    L"Change Name & Motto",
    L"Add to statistics",
    L"Delete Team"
};

enum
{
    MENU_TEAM_OPS_VIEW_ROSTER = 0,
    MENU_TEAM_OPS_EDIT_ICON,
    MENU_TEAM_OPS_SEARCH_JOINED_TOURNEYS,
    MENU_TEAM_OPS_SEARCH_AVAILABLE_TOURNEYS,
    MENU_TEAM_OPS_CREATE_TOURNEY,
    MENU_TEAM_OPS_CHANGE_NAME,
    MENU_TEAM_OPS_CHANGE_STATS,
    MENU_TEAM_OPS_DELETE,
    NUM_ITEMS_TEAM_OPS_MENU
};


// TEAM MEMBER OPERATIONS menu
const WCHAR* const MENU_MEMBER_OPS[] =
{
    L"Set To Owner Permissions",
    L"Set To Recruiter Permissions",
    L"Set To Peon Permissions",
    L"Kick Off the Team"
};
enum
{
    MENU_MEMBER_OPS_SET_OWNER = 0,
    MENU_MEMBER_OPS_SET_RECRUITER,
    MENU_MEMBER_OPS_SET_PEON,
    MENU_MEMBER_OPS_KICKOFF,
    NUM_ITEMS_MEMBER_OPS_MENU  = 4
};

const WCHAR* const PRECONCEIVED_TEAM_MESSAGES[]   =
{
    L"\"OK, is everyone ready?\"",
    L"\"TEH CONK3R1NG W1LL COMM3Nc3\"",
    L"\"Huzzah!\"",
    L"\"Should we invite more people?\"",
    L"\"Fancy a wallop?\"",
    L"\"Q: Are We Not Men? A: We Are Devs!\"",
    L"\"OK, We're not just playing games (oh wait, we are)\"",
    L"\"r3ady 2 g3t Pwn3d?????\"",
    L"\"It was my band's fault! Sorry!!\""
};

const INT NUM_PRECONCEIVED_TEAM_MESSAGES = 9;

DWORD GetMenuPosition( DWORD dwCurMenuPosition,
                       Event event,
                       DWORD& dwMenuRenderStart,
                       DWORD dwNumMenuItems,
                       DWORD dwNumItemsPerScreen );

INT GetMenuPosition( INT iCurMenuPosition,
                     INT iNumMenuItems,
                     Event event,
                     BOOL bMenuWrap = TRUE );

Event GetEvent();
Event GetEvent( const INT iController );

const INT MAX_NUM_MENU_ITEMS   = 25;
const INT MAX_MENU_STRING_SIZE = 32;
typedef WCHAR MENU_LIST[MAX_NUM_MENU_ITEMS][MAX_MENU_STRING_SIZE];

VOID RenderFooter( CXBFont& font, WORD flags );

VOID RenderMenu( CXBFont& font,
                 const WCHAR* strMenuName,
                 const WCHAR** rwMenuText,
                 const WORD wNumMenuItems,
                 const INT iCurMenuItem );

VOID RenderScrollingMenu( CXBFont& font,
                          const WCHAR* wszMenuName,
                          DWORD dwRenderStart,
                          DWORD dwItemSelected,
                          DWORD dwNumItems,
                          const WCHAR* wszLeftHeader,
                          MENU_LIST rwLeftList,
                          const WCHAR* wszRightHeader,
                          MENU_LIST rwRightList,
                          BOOL bZeroIndexed = FALSE );

#endif // MENUS_H