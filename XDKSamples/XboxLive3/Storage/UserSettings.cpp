//-------------------------------------------------------------------------------------
// File: UserSettings.cpp
//
// Desc: Holds the implementation for a user settings object.
//       This object is used by the Storage sample to
//       demomstrate saving and retreiving settings via
//       Xbox Live.
//
// Hist: 08.10.04 - New for Sept release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include <assert.h>
#include "UserSettings.h"


////////////////////////////////////
// CUserSettings Member Functions //
////////////////////////////////////

// User creatable settings object
// Made a class to help support ease
// of settings uploading

//-------------------------------------------------------------------------------------
// Name: SetToDefaults()
// Desc: Sets user settings to defaults
//-------------------------------------------------------------------------------------
VOID CUserSettings::SetToDefaults()
{
    // set all settings to default values
    m_wStickInverted = DEFAULT_STICK_INVERTED_SETTING;
    m_wHudVisible    = DEFAULT_HUD_VISIBILITY_SETTING;
    m_wSoundVolume   = DEFAULT_SOUND_VOLUME_SETTING;
    
    // zero the string buffers before copying the defaults values in
    ZeroMemory( m_strNickname , sizeof(WCHAR) * MAX_NICKNAME_SETTING_SIZE );
    lstrcpynW( m_strNickname , DEFAULT_NICKNAME_SETTING , 
               MAX_NICKNAME_SETTING_SIZE );
    
    ZeroMemory( m_strSettingsName , sizeof(WCHAR) * MAX_SETTINGS_NAME_SIZE );
    lstrcpynW( m_strSettingsName , DEFAULT_SETTINGS_NAME_SETTING , 
               MAX_SETTINGS_NAME_SIZE );
}

//-------------------------------------------------------------------------------------
// Name: CUserSettings()
// Desc: Creates an inert and safe object.
//-------------------------------------------------------------------------------------
CUserSettings::CUserSettings() : m_bDirty( FALSE )
{
    SetToDefaults();
}

//-------------------------------------------------------------------------------------
// Name: Copy Operator
// Desc: Copies the source user content into the destination
//-------------------------------------------------------------------------------------
CUserSettings& CUserSettings::operator=( const CUserSettings& src )
{
    // we don't want to overwrite ourselves
    if( this != &src )
    {
        // set all settings to source's values
        m_bDirty            = src.m_bDirty;

        m_wStickInverted    = src.m_wStickInverted;
        m_wHudVisible       = src.m_wHudVisible;
        m_wSoundVolume      = src.m_wSoundVolume;
        
        // zero the string buffers before copying the source's values in
        ZeroMemory( m_strNickname , sizeof(WCHAR) * MAX_NICKNAME_SETTING_SIZE );
        lstrcpynW( m_strNickname , src.m_strNickname , 
                   MAX_NICKNAME_SETTING_SIZE );
        
        ZeroMemory( m_strSettingsName , sizeof(WCHAR) * MAX_SETTINGS_NAME_SIZE );
        lstrcpynW( m_strSettingsName , src.m_strSettingsName , 
                   MAX_SETTINGS_NAME_SIZE );   
    }

    return *this;
}

//-------------------------------------------------------------------------------------
// Name: OffsetValue
// Desc: offsets the value of the given setting index.  If this setting is a string,
//       it merely regenerates.  This offset is expected to be either 1 or -1
//-------------------------------------------------------------------------------------
VOID CUserSettings::OffsetValue( WORD wSettingIndex , INT iOffset )
{
    // if there's an non-zero offset to any value, these settings are now dirty because
    // one setting will change
    if ( iOffset )
    {
        SetDirty( TRUE );

	    INT iValue;
	
	    // define constant for size of volume range
	    const INT VOLUME_SETTING_RANGE = MAX_SOUND_VOLUME_SETTING - 
                                         MIN_SOUND_VOLUME_SETTING + 1;
	
	    // change enumeration settings by increment of decrement the value, with 
        // wrapping change any string settings by regenerating the string randomly
	    switch( wSettingIndex )
	    {
	    case USER_SETTING_INDEX_STICK_INVERTED:
	        iValue = (INT)m_wStickInverted + iOffset;
	        m_wStickInverted = (WORD)( ( iValue + NUM_STICK_INVERTED_SETTINGS ) 
	                                     % NUM_STICK_INVERTED_SETTINGS );
	        break;
	
	    case USER_SETTING_INDEX_HUD_VISIBILITY:
	        iValue = (INT)m_wHudVisible + iOffset;
	        m_wHudVisible = (WORD)( ( iValue + NUM_HUD_VISIBILITY_SETTINGS ) 
	                                     % NUM_HUD_VISIBILITY_SETTINGS );
	       break;
	
	    case USER_SETTING_INDEX_SOUND_VOLUME:
	        iValue = (INT)m_wSoundVolume + iOffset;
	        m_wSoundVolume = (WORD)( ( ( iValue + VOLUME_SETTING_RANGE ) 
	                                     % VOLUME_SETTING_RANGE ) 
	                                     + MIN_SOUND_VOLUME_SETTING );
	        break;
	
	    case USER_SETTING_INDEX_NICKNAME:
	        GenerateNewNickName();
	        break;
	
	    case USER_SETTING_INDEX_SETTINGS_NAME:
	        GenerateNewSettingsName();
	        break;
	
	    }
    }
}

//-------------------------------------------------------------------------------------
// Name: PutWideValueImage
// Desc: writes wide char value to buffer, given the setting requested.
//       It's recommended to make sure the wide string buffer has at least
//       MAX_SETTINGS_IMAGE_VALUE_SIZE characters
//-------------------------------------------------------------------------------------
VOID    CUserSettings::PutWideValueImage( WCHAR* pStrSettingBuffer , 
                                          WORD wSettingIndex )
{
    // copies into provided wide string buffer the string of the value of the
    // requested setting
    switch( wSettingIndex )
    {
    case USER_SETTING_INDEX_STICK_INVERTED:
        lstrcpynW( pStrSettingBuffer , 
                   STR_STICK_INVERTED[ m_wStickInverted ] , 
                   MAX_SETTINGS_IMAGE_VALUE_SIZE );
        break;

    case USER_SETTING_INDEX_HUD_VISIBILITY:
        lstrcpynW( pStrSettingBuffer , 
                   STR_HUD_VISIBILITY[ m_wHudVisible ] , 
                   MAX_SETTINGS_IMAGE_VALUE_SIZE );
       break;

    case USER_SETTING_INDEX_SOUND_VOLUME:
        _itow( m_wSoundVolume , pStrSettingBuffer , 10 );
        break;

    case USER_SETTING_INDEX_NICKNAME:
        lstrcpynW( pStrSettingBuffer , 
                   m_strNickname , 
                   MAX_SETTINGS_IMAGE_VALUE_SIZE );
        break;

    case USER_SETTING_INDEX_SETTINGS_NAME:
         lstrcpynW( pStrSettingBuffer , 
                    m_strSettingsName , 
                    MAX_SETTINGS_IMAGE_VALUE_SIZE );
         break;

    }
}

//-------------------------------------------------------------------------------------
// Name: GenerateNewSettingsName
// Desc: Generates a new settings name, which is partially a randomly generated
//       name and partially a date stamp
//-------------------------------------------------------------------------------------
VOID CUserSettings::GenerateNewSettingsName()
{
    // generate prefix randomly
    WCHAR       strPrefix[ MAX_SETTINGS_NAME_PREFIX + 1 ];
    XBRandName_GetRandomName( strPrefix , (DWORD)MAX_SETTINGS_NAME_PREFIX );

    // write prefix into settings name string
    lstrcpynW( m_strSettingsName , strPrefix , (DWORD)MAX_SETTINGS_NAME_PREFIX );
    
    // append underscore
    DWORD        dwPrefixLen = (DWORD)wcslen( strPrefix );
    m_strSettingsName[ dwPrefixLen ] = L'_';

    // get date
    SYSTEMTIME  sysTime;
    GetSystemTime( &sysTime );
    
    // get year string
    const WORD YEAR_STR_SIZE = 5;
    WCHAR       strYear[ YEAR_STR_SIZE ];
    _itow( sysTime.wYear, strYear , YEAR_STR_SIZE );

    DWORD        dwStrIndex = dwPrefixLen + 1;
    // write two digit month then a slash
    m_strSettingsName[ dwStrIndex++ ] = L'0' + (WCHAR)( sysTime.wMonth / 10 );
    m_strSettingsName[ dwStrIndex++ ] = L'0' + (WCHAR)( sysTime.wMonth % 10 );
    m_strSettingsName[ dwStrIndex++ ] = L'/';
    // write two digit day then a slash
    m_strSettingsName[ dwStrIndex++ ] = L'0' + (WCHAR)( sysTime.wDay / 10 );
    m_strSettingsName[ dwStrIndex++ ] = L'0' + (WCHAR)( sysTime.wDay % 10 );
    m_strSettingsName[ dwStrIndex++ ] = L'/';
    // write the last two digits of the year, then terminate string
    m_strSettingsName[ dwStrIndex++ ] = strYear[ YEAR_STR_SIZE - 2 ];
    m_strSettingsName[ dwStrIndex++ ] = strYear[ YEAR_STR_SIZE - 1 ];
    m_strSettingsName[ dwStrIndex ] = (WCHAR)0;

}

//-------------------------------------------------------------------------------------
// Name: EnterUpload
// Desc: Initializes upload for any current user settings to the server
//       returns TRUE if succeeds
//-------------------------------------------------------------------------------------
BOOL CUserSettings::EnterUpload( CXBOnlineTask& taskSettings , 
                                 ULONGLONG qwSettingsUser , 
                                 DWORD dwControllerUserPort )
{
    // the user content must not have been tampered with
    assert( ! IsDirty() );

    // The data will be uploaded per user per title
    DWORD dwFacility = XONLINESTORAGE_FACILITY_PER_USER_TITLE;

    // Step 1
    //
    // Create a name for the file to be placed
    // on the server
  
    DWORD       dwPathLength          = MAX_SERVER_PATH_SIZE;

    WCHAR       wszStorageServerPath[MAX_SERVER_PATH_SIZE];

    HRESULT hrCreatePath = XOnlineStorageCreateServerPath(
                                dwFacility,             // The type of storage to use
                                qwSettingsUser,         // qwID of the user
                                0,                      // qwID of the team (not used)
                                STR_SETTINGS_FILE_NAME, // Name of the file
                                wszStorageServerPath,   // Output path on the server
                                &dwPathLength           // Length of server path string
                           );

    if( FAILED( hrCreatePath ) )
    {
        return FALSE;
    }

    // Step 2
    //
    // Start the upload from memory to the server

    FILETIME      ftServerExpirationDate;
    
    // Never expire this data
    ZeroMemory( &ftServerExpirationDate, sizeof( ftServerExpirationDate ) );

    HRESULT hrUpload = XOnlineStorageUploadFromMemory(
                            dwFacility,              // Which type of storage
                            dwControllerUserPort,    // Controller port used
                            wszStorageServerPath,    // Name of the destination file
                            ftServerExpirationDate,  // Expiration date of the data
                            (BYTE*)this,             // Pointer to data to upload
                            sizeof( *this ),         // Amount of data to upload
                            0,                       // Upload flags must be set to 0
                            NULL,                    // Work event
                            &taskSettings            // Task for this work 
                        );

    return ( SUCCEEDED( hrUpload ) );
}

//-------------------------------------------------------------------------------------
// Name: ExitDownloadSettings
// Desc: Deinitializes download of user settings into the user settings object, via
//       a get-results call (also confirms the data received matches what is expected)
//       Returns TRUE if succeeds
//-------------------------------------------------------------------------------------
BOOL CUserSettings::ExitUpload( CXBOnlineTask& taskSettings )
{
    // Finished!
    taskSettings.Close();

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: EnterDownload
// Desc: Initializes download of any stored user settings into the user settings object
//       returns HRESULT of the download call
//-------------------------------------------------------------------------------------
HRESULT CUserSettings::EnterDownload( CXBOnlineTask& taskSettings , 
                                    ULONGLONG qwSettingsUser , 
                                    DWORD dwControllerUserPort ,
                                    BYTE* pReceiveBuffer , 
                                    DWORD dwReceiveBufferSize )
{
    // The data can be uploaded to a team
    // or each user has individual space PER title
    DWORD dwFacility = XONLINESTORAGE_FACILITY_PER_USER_TITLE;

    // Step 1
    //
    // Find where the files is on  the server
 
    DWORD       dwPathLength          = MAX_SERVER_PATH_SIZE;

    WCHAR       wszStorageServerPath[MAX_SERVER_PATH_SIZE];

    HRESULT hrCreatePath = XOnlineStorageCreateServerPath(
                                dwFacility,             // The type of storage to use
                                qwSettingsUser,         // qwID of the user
                                0,                      // qwID of the team (not used)
                                STR_SETTINGS_FILE_NAME, // Name of the file
                                wszStorageServerPath,   // Output path on the server
                                &dwPathLength           // Length of server path string
                            );

    if( FAILED( hrCreatePath ) )
    {
        return hrCreatePath;
    }

    // Step 2
    //
    // Start the download

    ZeroMemory( pReceiveBuffer , dwReceiveBufferSize );

    HRESULT hrDownload = XOnlineStorageDownloadToMemory(
                            dwFacility,           // The type of storage
                            dwControllerUserPort, // The port of the user
                            wszStorageServerPath, // The full name of data
                            pReceiveBuffer,       // Where to place
                            dwReceiveBufferSize,  // size of data buffer
                            0,                    // d/l flags must be 0
                            NULL,                 // Work event
                            &taskSettings         // Task for the work
                         );

    if( FAILED( hrDownload ) )
    {
        taskSettings.Close();
    }

    return hrDownload;
}

//-------------------------------------------------------------------------------------
// Name: ExitDownload
// Desc: Deinitializes download of user settings into the user settings object, via
//       a get-results call (also confirms the data received matches what is expected)
//       Returns TRUE if succeeds
//-------------------------------------------------------------------------------------
BOOL CUserSettings::ExitDownload( CXBOnlineTask& taskSettings , 
                                  ULONGLONG qwSettingsUser , 
                                  BYTE* pReceiveBuffer )
{
    // Step 1
    // get results

    DWORD     dwSizeDownloaded    = 0;
    DWORD     dwSpaceLeftOnServer = 0;
    ULONGLONG qwOwnerID           = 0;
    BYTE*     pDataLocation       = NULL;
    FILETIME  ftCreationDate;
    
    ZeroMemory( &ftCreationDate, sizeof( ftCreationDate ) );

    HRESULT hrDownload = XOnlineStorageDownloadToMemoryGetResults(
            (XONLINETASK_HANDLE)(taskSettings), // Task used to start download
            &pDataLocation,                     // Where the data was placed
            &dwSizeDownloaded,                  // Amount downloaded
            &dwSpaceLeftOnServer,               // Space left on server
            &qwOwnerID,                         // ID of the data's owner
            &ftCreationDate                     // Date this data was created on
            );

    if ( FAILED( hrDownload ) )
    {
        return FALSE;
    }

    // Step 2
    // make sure downloaded data checks out

    assert( dwSizeDownloaded == sizeof( *this ) );
    assert( SUCCEEDED( hrDownload ) );
    assert( qwOwnerID == qwSettingsUser );
    assert( pDataLocation == pReceiveBuffer );

    // Step 3
    //
    // Finished!
    // Close the task and assign the new settings
    taskSettings.Close();

    // copy results buffer into temporary settings variable
    memcpy( this , pReceiveBuffer , sizeof( *this ) );

    return TRUE;
}
