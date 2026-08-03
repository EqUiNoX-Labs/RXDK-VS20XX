//-----------------------------------------------------------------------------
// File: Text.cpp
//
// Desc: All text in single place to simplify localization
//
// Hist: 04.16.01 - Added for May XDK release 
//       09.24.02 - Modified to use file-based localized strings
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef TECHCERTGAME_TEXT_H
#define TECHCERTGAME_TEXT_H
#include <xtl.h>




//-----------------------------------------------------------------------------
// String constants. Note: the order of these strings constants MUST MATCH the
// strings stored in the localized text files (d:\media\strings\*.txt)
//-----------------------------------------------------------------------------
enum
{
    TEXT_GAME_NAME_FORMAT,
    TEXT_SUNDAY,
    TEXT_MONDAY,
    TEXT_TUESDAY,
    TEXT_WEDNESDAY,
    TEXT_THURSDAY,
    TEXT_FRIDAY,
    TEXT_SATURDAY,
    TEXT_RECONNECT_CNTRLR,
    TEXT_CONFIRM_QUIT,
    TEXT_DEMO,
    TEXT_RETURN_TO_MENU,
    TEXT_GAME_NAME,
    TEXT_LOADINGX,
    TEXT_FREE,
    TEXT_MENU_START,
    TEXT_MENU_LOAD,
    TEXT_MENU_OPTIONS,
    TEXT_MENU_RESUME,
    TEXT_MENU_SAVE,
    TEXT_MENU_QUIT,
    TEXT_MENU_VIBRATION,
    TEXT_MENU_TITLESAFE_L1,
    TEXT_MENU_TITLESAFE_L2,
    TEXT_MENU_SAVE_OPTIONS,
    TEXT_ON,
    TEXT_OFF,
    TEXT_MENU_MUSIC_VOLUME,
    TEXT_MENU_EFFECT_VOLUME,
    TEXT_MENU_SOUNDTRACK,
    TEXT_MS_XBOX,
    TEXT_INTRO,
    TEXT_PRESS_START,
    TEXT_SAVE_FAILED,
    TEXT_GAME_SAVED,
    TEXT_LOAD_FAILED,
    TEXT_GAME_LOADED,
    TEXT_NO_ROOM_MU,
    TEXT_NO_ROOM_HD,
    TEXT_NO_ROOM_MU_PLZ_FREE,
    TEXT_NO_ROOM_HD_PLZ_FREE,
    TEXT_NO_SAVES,
    TEXT_LOADING,
    TEXT_SAVING_MU,
    TEXT_SAVING,
    TEXT_DO_NOT_REMOVE_MU,
    TEXT_DO_NOT_POWEROFF,
    TEXT_SAVE_GAME,
    TEXT_LOAD_GAME,
    TEXT_ILLUS_GRAPHICS,
    TEXT_FORMAT_DEVICE,
    TEXT_CHOOSE_LOAD,
    TEXT_CHOOSE_SAVE,
    TEXT_TODAY,
    TEXT_NOW,
    TEXT_FORMAT_GAME,
    TEXT_EMPTY_SPACE,
    TEXT_A_SELECT,
    TEXT_B_BACK,
    TEXT_Y_DELETE,
    TEXT_OVERWRITE,
    TEXT_YES,
    TEXT_NO,
    TEXT_DELETE,
    TEXT_LOADING_GAME_LIST,
    TEXT_MAX_SAVED_GAMES,
    TEXT_MU_UNUSABLE,
    TEXT_MU_FULL,
    TEXT_UNUSABLE_MU_NAME,
    TEXT_FULL_MU_NAME,
    TEXT_MU_REMOVED,
    TEXT_ACTION_SAVE,
    TEXT_ACTION_LOAD,
    TEXT_MAX_BLOCKS,
    TEXT_XHD,
    TEXT_MENU_EXIT,

    TEXT_NUMSTRINGS
};




//-----------------------------------------------------------------------------
// Array of localized strings. Access the strings with the provided macro.
//-----------------------------------------------------------------------------
extern WCHAR* g_StringArray[TEXT_NUMSTRINGS];

#define STRING(w) g_StringArray[TEXT_##w]




//-----------------------------------------------------------------------------
// Name: LoadStrings()
// Desc: Loads localized strings for the app
//-----------------------------------------------------------------------------
HRESULT LoadStrings( DWORD dwLanguage );




#endif // TECHCERTGAME_TEXT_H
