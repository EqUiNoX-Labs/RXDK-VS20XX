//-----------------------------------------------------------------------------
// File: Common.h
//
// Desc: Friends global header
//
// Hist: 10.20.01 - New for Nov release
//       02.15.02 - Updated for Mar release
//       03.14.02 - Updated for April release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef FRIENDS_COMMON_H
#define FRIENDS_COMMON_H

#include "xtl.h"
#include "xonline.h"

#pragma warning( disable: 4786 )
#include <vector>




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

const DWORD DEFAULT_USER = 0;

const DWORD MAX_FRIENDS_DISPLAYED = 5;
const DWORD MAX_ACCOUNTS_DISPLAYED = 7;
const DWORD MAX_POTENTIALS_DISPLAYED = 5;

enum ACTIONS
{
    // Action menu
    ACTION_INVITE = 0,
    ACTION_REVOKE,
    ACTION_GAME_INVITE_ACCEPT,
    ACTION_GAME_INVITE_DECLINE,
    ACTION_GAME_INVITE_REMOVE,
    ACTION_FRIEND_REQUEST_ACCEPT,
    ACTION_FRIEND_REQUEST_DECLINE,
    ACTION_FRIEND_REQUEST_BLOCK,
    ACTION_REMOVE,
    ACTION_JOIN_GAME,    
    ACTION_MAX,
};

enum
{
    // Confirm removal menu
    CONFIRM_REMOVE_YES = 0,
    CONFIRM_REMOVE_NO,
    CONFIRM_REMOVE_MAX,
};


enum
{
    // Feedback menu
    FEEDBACK_FIRST          = XONLINE_FEEDBACK_NEG_NICKNAME,
    FEEDBACK_LAST           = XONLINE_FEEDBACK_POS_SESSION,
    FEEDBACK_MAX            = NUM_XONLINE_FEEDBACK_TYPES
};

//-----------------------------------------------------------------------------
// Typedefs
//-----------------------------------------------------------------------------
typedef std::vector< ACTIONS > ActionList;




#endif // FRIENDS_COMMON_H
