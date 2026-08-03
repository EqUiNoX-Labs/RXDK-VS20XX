//-------------------------------------------------------------------------------------
// File: UserSettings.h
//
// Desc: Holds the definition for a user settings object.
//       This object is used by the Storage sample to
//       demomstrate saving and retreiving settings via
//       Xbox Live.
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef USERSETTINGS_H
#define USERSETTINGS_H

#include <xtl.h>
#include <xonline.h>
#include "common.h"
#include "XBRandName.h"
#include "XBOnlineTask.h"

// constants for use in User Settings
enum
{
    USER_SETTING_INDEX_STICK_INVERTED,
    USER_SETTING_INDEX_HUD_VISIBILITY,
    USER_SETTING_INDEX_SOUND_VOLUME,
    USER_SETTING_INDEX_NICKNAME,
    USER_SETTING_INDEX_SETTINGS_NAME,
    NUM_USER_SETTING_INDICES
};

const WCHAR* const USER_SETTING_STR_INDEX[ NUM_USER_SETTING_INDICES ] =
{
    L"Inverted stick?",
    L"HUD visible?",
    L"Sound volume",
    L"Nickname",
    L"Settings Name"
};

// enumeration of stick inverted setting
enum
{
    STICK_INVERTED_NO,
    STICK_INVERTED_YES,

    NUM_STICK_INVERTED_SETTINGS
};

const WCHAR* const STR_STICK_INVERTED[NUM_STICK_INVERTED_SETTINGS] =
{
    L"NO",
    L"YES"
};

// default stick inversion setting
const WORD DEFAULT_STICK_INVERTED_SETTING = STICK_INVERTED_NO;

// enumeration of hud visibility setting
enum
{
    HUD_VISIBILITY_NO,
    HUD_VISIBILITY_YES,

    NUM_HUD_VISIBILITY_SETTINGS
};

const WCHAR* const STR_HUD_VISIBILITY[NUM_HUD_VISIBILITY_SETTINGS] =
{
    L"NO",
    L"YES"
};

// Progress activity
enum
{
    PROGRESS_ACTIVITY_NONE = -1,
    PROGRESS_ACTIVITY_DOWNLOAD,
    PROGRESS_ACTIVITY_UPLOAD,
    NUM_PROGRESS_ACTIVITIES
};

// default hud visibility setting
const WORD DEFAULT_HUD_VISIBILITY_SETTING = HUD_VISIBILITY_NO;

// min and max volumes for sound
const WORD  MIN_SOUND_VOLUME_SETTING = 0;
const WORD  MAX_SOUND_VOLUME_SETTING = 10;
// default sound volume
const WORD  DEFAULT_SOUND_VOLUME_SETTING = 5;

// max nickname size
const WORD  MAX_NICKNAME_SETTING_SIZE = 16;
// default nickname
const WCHAR DEFAULT_NICKNAME_SETTING[] = L"Nickname";

// max settings name size
const WORD  MAX_SETTINGS_NAME_PREFIX = 16;
const WORD  MAX_SETTINGS_NAME_DATE   = 8;
const WORD  MAX_SETTINGS_NAME_SIZE   = MAX_SETTINGS_NAME_PREFIX +
                                       MAX_SETTINGS_NAME_DATE + 1;
// default settings name
const WCHAR DEFAULT_SETTINGS_NAME_SETTING[] = L"Settings";

// size of setting value string size
const WORD  MAX_SETTINGS_IMAGE_VALUE_SIZE = MAX_SETTINGS_NAME_SIZE;

// name of file used to store the settings per user per title
const WCHAR STR_SETTINGS_FILE_NAME[] = L"settings.dat";


// User creatable settings object
// Made a class to help support ease
// of settings uploading
class CUserSettings
{

protected:

    // yes/no value for stick inversion
    WORD    m_wStickInverted;

    // yes/no for hud visibility
    WORD    m_wHudVisible;

    // 0-10 value for sound volume
    WORD    m_wSoundVolume;

    // nickname setting for user
    WCHAR   m_strNickname[ MAX_NICKNAME_SETTING_SIZE ];

    // name of this setting for user
    WCHAR   m_strSettingsName[ MAX_SETTINGS_NAME_SIZE ];

    // true if contents have changed since last initialization/download
    BOOL    m_bDirty;

public:

    // Get/Set methods
    WORD    GetStickInverted() const
        { return m_wStickInverted; }
    VOID    SetStickInverted( const WORD inverted )
        { m_wStickInverted = inverted; }

    WORD    GetHudVisible() const
        { return m_wHudVisible; }
    VOID    SetHudVisible( const WORD visible )
        { m_wHudVisible = visible; }

    WORD    GetSoundVolume() const
        { return m_wSoundVolume; }
    VOID    SetSoundVolume( const WORD volume )
        { m_wSoundVolume = volume; }

    WCHAR*  GetNickname() const
        { return (WCHAR*)m_strNickname; }
    VOID    GenerateNewNickName()
        { XBRandName_GetRandomName( m_strNickname ,
            (DWORD)MAX_NICKNAME_SETTING_SIZE ); }

    WCHAR*  GetSettingsName() const
        { return (WCHAR*)m_strSettingsName; }
    VOID    GenerateNewSettingsName();

    // Dirtiness methods
    BOOL    IsDirty() const
        { return m_bDirty; }
    VOID    SetDirty( const BOOL bDirty )
        { m_bDirty = bDirty; }

    // defaults
    VOID    SetToDefaults();

    // increment/decrement value
    // increments or decrements values, and will wrap the value if necessary
    // if the "value" is a string, the increment or decrement will have the
    // same effect.. the string will regenerate.
    VOID    OffsetValue( WORD wSettingIndex , INT iOffset );
    VOID    IncrementValue( WORD wSettingIndex )
            { OffsetValue( wSettingIndex ,  1 ); }
    VOID    DecrementValue( WORD wSettingIndex )
            { OffsetValue( wSettingIndex , -1 ); }

    // write wide char value to buffer, given the setting requested
    // It's recommended to make sure the wide string buffer has at least
    // MAX_SETTINGS_IMAGE_VALUE_SIZE characters
    VOID    PutWideValueImage( WCHAR* pStrSettingBuffer , WORD wSettingIndex );

    // *** upload and download to storage methods
    BOOL    EnterUpload( CXBOnlineTask& taskSettings ,   // upload task
                         ULONGLONG qwSettingsUser ,      // user id
                         DWORD dwControllerUserPort );   // controller index
    HRESULT UpdateUpload( CXBOnlineTask& taskSettings )
                            { return taskSettings.Continue(); }
    BOOL    ExitUpload( CXBOnlineTask& taskSettings );   // upload task

    HRESULT EnterDownload( CXBOnlineTask& taskSettings , // download task
                           ULONGLONG qwSettingsUser ,    // user id
                           DWORD dwControllerUserPort ,  // controller index
                           BYTE* pReceiveBuffer ,        // download buffer
                           DWORD dwReceiveBufferSize );  // download buffer size
    HRESULT UpdateDownload( CXBOnlineTask& taskSettings )
                            { return taskSettings.Continue(); }
    BOOL    ExitDownload( CXBOnlineTask& taskSettings ,  // download task
                          ULONGLONG qwSettingsUser ,     // user id
                                                         // (for verification)
                          BYTE* pReceiveBuffer );        // download buffer
                                                         // (for verification)


    // assignment
    CUserSettings& operator=( const CUserSettings& );

    // constructor/destructor
    CUserSettings();
    ~CUserSettings() {}
};

// Size of buffer to download settings into
const DWORD SETTINGS_DL_BUFFER_SIZE =
            max( MIN_XONLINE_DOWNLOAD_BUFFER_SIZE , sizeof( CUserSettings ) );

#endif // USERSETTINGS_H