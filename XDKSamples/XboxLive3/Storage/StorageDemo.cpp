//-------------------------------------------------------------------------------------
// File: StorageDemo.cpp
//
// Desc: Demonstrates Xbox Live storage. Storage is done on a "per team per title"
//       basis. Storage is also shown on a "per user per title" basis.  And also
//       on a "publisher/global" basis.  Storage is used to allow for the creation and 
//       usage of buddy icons and team logos that will be persistent across Xbox Live.
//
//       Security best practices are also shown for dealing with distributed content.
//       The Xbox Live signature service is demonstrated for this purpose.
//
// Hist: 08.10.04 - New for Sept release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include <algorithm>
#include <xtl.h>
#include <xonline.h>
#include "XBFont.h"
#include "XBMemUnit.h"
#include "XBVoice.h"
#include "StorageDemo.h"
#include "Common.h"


//////////////////////////////////
// CXBoxSample Member Functions //
//////////////////////////////////

//-------------------------------------------------------------------------------------
// Name: StartSignIn()
// Desc: Attempts to sign in the player at the given index to the XLogonUsers
//       array. Returns an error stating the type of problem (network
//       or account) encountered if the signon process fails.
//       Once StartSignIn() has been called, the task must be finished
//       by ContinueSignIn() and FinishSignIn()
//-------------------------------------------------------------------------------------
INT CXBoxSample::StartSignIn()
{
    // NOTE:
    // Before signing on, the title must check accounts for
    // passcodes. If present, the players must be prompted for them.
    // Passcodes are for *client-side* authentication only -- the
    // Xbox online service does not use them for authentication. For
    // demonstration purposes, we just make note of any passcode, and continue.
    // (The 'passcode' field of the XONLINE_USER structure contains the actual
    // passcode).
    #pragma message("TCR: Title UI must prompt for a passcode and verify it before signing on.")

    // Initiate the authentication process.  The signon process
    // first authenticates the Xbox.  Next, it authenticates each
    // user, and finally authenticates against the requested services
    // (validating that both the users *and* the Xbox have access to them).
    // All three stages are handled by the client APIs, though the title
    // is required to check for errors and handle them appropriately.
    XONLINE_USER rwLogonUsers[ XONLINE_MAX_LOGON_USERS ] = { 0 }; // Initially zeroed

    // establish an array of one account to log on
    rwLogonUsers[m_dwControllingUserPort] = m_rwStoredUsers[ m_wCurUserIndex ];

    HRESULT hrLogon = XOnlineLogon(
                            rwLogonUsers, // Array of users to login
                            SERVICES,     // The array of services we want
                            NUM_SERVICES, // The number of services we want
                            NULL,         // Event
                            &m_hLogonTask // The task to assign
                       );

    // Check for errors
    switch( hrLogon )
    {
    case S_OK:   
        // XOnlineLogon succeeded
        break;

    case XONLINE_E_LOGON_NO_NETWORK_CONNECTION:
        // Sign on failed because no network connection was
        // detected.  A title must give the player the
        // option of accessing the network configuration of the online dash
    default:
        return E_NETWORK_ERROR;
    }
    
    return S_OK;
}
    
//-------------------------------------------------------------------------------------
// Name: ContinueSignIn()
// Desc: Attempts to sign in the player to Xbox Live
//       Returns an error stating the type of problem (network or account)
//       encountered if the signon process fails.
//       Once a user has signed on they may create or join a game session.
//-------------------------------------------------------------------------------------
INT CXBoxSample::ContinueSignIn()
{
    // If sucessful, an asynchronous task handle (XONLINETASK_HANDLE) will
    // be returned.  As with the other Xbox online APIs that return
    // task handles, XOnlineTaskContinue() is used to perform a "unit" of
    // work.  The HRESULT returned by calling XOnlineTaskContinue will
    // indicate if additional work is required (XONLINETASK_S_RUNNING) or if
    // the task has failed.  Some of the result codes returned will depend on 
    // the actual type of task being pumped.  However, the SUCCEEDED and
    // FAILED macros can be used for error handling purposes.

    // Go into a loop, calling XOnlineTaskContinue on the logon task
    // until the task completes.  This can take up to a minute or more
    // depending on network conditions.  If successful, 
    // XONLINE_S_LOGON_CONNECTION_ESTABLISHED will be returned.
    // In a real title, this would appear inside your game loop.  

    HRESULT hrLogon = m_hLogonTask.Continue();

    // Check to make sure the logon is proceeding
    // without error, and act on any errors found

    if ( hrLogon == XONLINETASK_S_RUNNING )
        return S_OK;
    
    // Check the results to see if we were successful
    // or if the login failed
    switch( hrLogon )
    {
    case XONLINE_S_LOGON_CONNECTION_ESTABLISHED:  
        // The Xbox has been validated and there are
        // no system authentication errors
        return XONLINE_S_LOGON_CONNECTION_ESTABLISHED;
        break;

    case XONLINE_E_LOGON_CONNECTION_LOST:
    case XONLINE_E_LOGON_SERVERS_TOO_BUSY:
        // Some other error - title is free to allow access to dash
        m_hLogonTask.Close();

        m_bUserSignedIn  = FALSE;

        return E_NETWORK_ERROR;

    case XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE:
    case XONLINE_E_LOGON_INVALID_USER:
    default:
        // Some other error - title is free to allow access to dash
        m_hLogonTask.Close();

        m_bUserSignedIn  = FALSE;
 
        return E_ACCOUNT_ERROR;
    }
}

//-------------------------------------------------------------------------------------
// Name: FinishSignIn()
// Desc: Attempts to finish the SignIn task.
//       Returns an error stating the type of problem (network or account)
//       encountered if the signon process fails.
//       Checks to make sure that the services we need for the game are
//       available and annouces the player's presence onto the network.
//       Once a user has signed on they may create or join a game session.
//-------------------------------------------------------------------------------------
INT CXBoxSample::FinishSignIn()
{
    // Titles must check for, and handle, authentication errors in the
    // following order:
    // 1. System authentication errors (Done by StartSignIn)
    // 2. User authentication errors.
    // 3. Service authentication errors.
    // It important to check for, and handle user errors before service
    // errors.  Consider the case where there is an account maintenance issue 
    // for a user, AND a requested service is unavailable.   A title must
    // allow the user to deal with the account issues before service issues
    // (especially since an account issue could be the cause of
    // the service issue).
    
    // 2. Check for user authentication errors.
    // To check for user authentication errors, we call XOnlineGetLogonUsers.
    // This returns a pointer to an array of XONLINE_USER structures.  This
    // array is similar the User array we populated and passed into
    // XOnlineLogon, but it has the 'hr' field of each XONLINE_USER
    // set with a status code indicating whether or not authentication 
    // for that user succeeded.
    const PXONLINE_USER rwUsers = XOnlineGetLogonUsers();
    
    assert( rwUsers );
    
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        if( rwUsers[i].xuid.qwUserID != 0 ) // A valid user
        {
            // Check authentication results for this user
            switch( rwUsers[i].hr )
            {
            case S_OK:
                break;

            case XONLINE_E_LOGON_USER_ACCOUNT_REQUIRES_MANAGEMENT:
            default:
                m_hLogonTask.Close();

                m_bUserSignedIn = FALSE;
                return E_ACCOUNT_ERROR;
            }            
        }
    }
    
    // 3. Finally check the requested services
    for( DWORD i = 0; i < NUM_SERVICES; ++i )
    {
        HRESULT hrServiceInfo = XOnlineGetServiceInfo( SERVICES[i], NULL );
        
        switch( hrServiceInfo )
        {
        case S_OK:
            break;
        case XONLINE_E_LOGON_SERVICE_NOT_AUTHORIZED:
        case XONLINE_E_LOGON_SERVICE_TEMPORARILY_UNAVAILABLE:
        default:
            m_hLogonTask.Close();

            m_bUserSignedIn = FALSE;
            return E_ACCOUNT_ERROR;
        }            
    }   
    
    // Everything is OK at this point.  For each user (except guests)
    // set their online notification state so they are visible to their
    // friends. A real title would also check for the voice peripheral and 
    // specify the XONLINE_FRIENDSTATE_FLAG_VOICE if present.  
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        if( rwUsers[i].xuid.qwUserID != 0 && 
            !XOnlineIsUserGuest( rwUsers[i].xuid.dwUserFlags ) )
        {
            HRESULT hrNotification = 
                    XOnlineNotificationSetState( i,   // Controller index
                                                 XONLINE_FRIENDSTATE_FLAG_ONLINE,
                                                 XNKID(),
                                                 0,
                                                 NULL );
            
            if ( FAILED( hrNotification ) )
            {
                m_hLogonTask.Close();

                m_bUserSignedIn = FALSE;
                return E_ACCOUNT_ERROR;
            }
        }
    }

    m_bUserSignedIn = TRUE;
    return (INT)S_OK;
}


//-------------------------------------------------------------------------------------
// Name: GetFileIndex()
// Desc: Returns the index of the file needed in regards to the enumeration results.
//       If the filename is not found, then -1 is returned.
//-------------------------------------------------------------------------------------
INT CXBoxSample::GetFileIndex( const WCHAR* wszFilename,
                               PXONLINESTORAGE_FILE_INFO*& rwEnumResults,
                               DWORD& dwNumResults )
{
    INT iFilenameLength = wcslen( wszFilename );

    for( INT i = 0; i < (INT)dwNumResults; ++i )
    {
        INT iSize   = wcslen( rwEnumResults[i]->wszPathName );
        INT iOffset = iFilenameLength;
        LPCWSTR pString = rwEnumResults[i]->wszPathName + ( iSize - iOffset );

        assert( ( iSize - iOffset ) > 0 );
        
        if ( !_wcsnicmp( wszFilename, pString, iFilenameLength ) )
        {
            return i;
        }
    }

    return -1;
}

//-------------------------------------------------------------------------------------
// Name: DownloadGlobal()
// Desc: Downloads data from the publisher global region into the given buffer.
//       Takes the name of the file, the buffer to write to and the buffer size.
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::DownloadGlobal( const DWORD dwControllingUserPort,
                                     const WCHAR* wszStorageServerPath,
                                     PBYTE rwBuffer,
                                     DWORD& dwBufferSize )
{
    assert( wszStorageServerPath );

    DWORD dwFacility = XONLINESTORAGE_FACILITY_PER_TITLE;


    // Step 1
    //
    // Start the download into memory

    CXBOnlineTask hDownloadTask;

    HRESULT hrDownload = XOnlineStorageDownloadToMemory(
                            dwFacility,            // The type of storage
                            dwControllingUserPort, // The port of the user
                            wszStorageServerPath,  // The full name of the data
                            rwBuffer,              // Where to place
                            dwBufferSize,          // size of data buffer
                            0,                     // Download flags, must be 0
                            NULL,                  // Work event
                            &hDownloadTask         // Task for the work
                         );


    // Step 2
    //
    // Pump the task until finished

    if( !WaitForTaskToComplete( hDownloadTask, &hrDownload ) )
    {
        hDownloadTask.Close();

        return hrDownload;
    }


    // Step 3
    //
    // Now that the download is complete
    // go ahead and read the results

    DWORD     dwSizeDownloaded    = 0;
    DWORD     dwSpaceLeftOnServer = 0;
    ULONGLONG qwOwnerID           = 0;
    PBYTE     pDataLocation      = NULL;
    FILETIME  ftCreationDate;
    
    ZeroMemory( &ftCreationDate, sizeof( ftCreationDate ) );

    hrDownload = XOnlineStorageDownloadToMemoryGetResults(
                    hDownloadTask,        // Task used to start download
                    &pDataLocation,       // Where the data was placed,
                    &dwSizeDownloaded,    // Amount downloaded
                    &dwSpaceLeftOnServer, // Space left on server (bytes)
                    &qwOwnerID,           // ID of the data's owner
                    &ftCreationDate       // Date this data was created on
                );

    assert( dwSizeDownloaded > 0 );
    assert( SUCCEEDED( hrDownload ) );
    assert( pDataLocation == rwBuffer );

    
    // Step 4
    //
    // Finished!
    // Close the task and assign the new content

    hDownloadTask.Close();

    return hrDownload;
}

//-------------------------------------------------------------------------------------
// Name: EnumerateGlobalStorage
// Desc: Enumerates the list of files in global publisher storage
//       Returns the number of results as an output parameter
//       and places the results in the given array.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnumerateGlobalStorage( PXONLINESTORAGE_FILE_INFO*& rwEnumResults,
                                          DWORD& dwNumResults )
{
    dwNumResults  = 0;
    rwEnumResults = NULL;


    // Step 1
    //
    // Build network server path to the publisher supplied data

    DWORD       dwFacility   = XONLINESTORAGE_FACILITY_PER_TITLE;
    ULONGLONG   qwUserID     = m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID;
    DWORD       dwPathLength = MAX_SERVER_PATH_SIZE;

    WCHAR       wszStorageServerPath[MAX_SERVER_PATH_SIZE];

    HRESULT hrCreatePath = XOnlineStorageCreateServerPath(
                                dwFacility,           // The type of storage to use
                                qwUserID,             // qwID of the user
                                qwUserID,             // wqID of the team
                                L"*",                 // Name of the file
                                wszStorageServerPath, // Output path on the server
                                &dwPathLength         // Length of server path string
                           );

    if( FAILED( hrCreatePath ) )
        return FALSE;


    // Step 2
    //
    // Start the enumeration

    CXBOnlineTask hEnumerateTask;

    HRESULT hrEnumerate = XOnlineStorageEnumerate(
                                dwFacility,              // Type of storage to list
                                m_dwControllingUserPort, // Controller port
                                wszStorageServerPath,    // Files to look for
                                0,                       // starting index
                                MAX_NETWORK_RESULTS,     // Max results to return
                                NULL,                    // Work event
                                &hEnumerateTask          // Task to assign
                            );

    assert( SUCCEEDED( hrEnumerate ) );


    // Step 3
    //
    // Pump the task until it is finished

    if( !WaitForTaskToComplete( hEnumerateTask, &hrEnumerate) )
        return FALSE;


    // Step 4
    //
    // Get the results

    DWORD dwTotalResults    = 0;

    hrEnumerate = XOnlineStorageEnumerateGetResults(
                        hEnumerateTask, // Task used to start the enumeration
                        &dwTotalResults,
                        &dwNumResults,
                        &rwEnumResults
                    );


    // Step 5
    //
    // Close the task and return our success

    hEnumerateTask.Close();

    return SUCCEEDED( hrEnumerate );
}

//-------------------------------------------------------------------------------------
// Name: UploadSave
// Desc: Uploads the saved game in the given slot. Verifies the data is legitimate
//       before uploading
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::UploadSave( INT iSaveGameSlot )
{
    assert( iSaveGameSlot >= 0 );
    assert( iSaveGameSlot < (INT)m_dwNumContentSaves );


    // Print the user's name to a string
    ULONGLONG qwUserID = m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID;
    UINT iHi = (UINT)( qwUserID >> 32 );
    UINT iLo = (UINT)qwUserID;
    WCHAR wszFilename[MAX_GAMENAME] = { 0 };
    _snwprintf( wszFilename, MAX_GAMENAME, L"%08x%08x\0",
                iHi, iLo );


    // Step 1
    //
    // Verify that the content is valid
    CUserContent uploadContent;

    if( !uploadContent.Load( m_bUserSignedIn,
                            m_savedContentData[iSaveGameSlot].szSaveGameDirectory,
                            m_savedContentData[iSaveGameSlot].szSaveGameName ) )
    {
        return FALSE;
    }

    if( uploadContent.IsDead() || uploadContent.IsDirty() )
    {
        return FALSE;
    }


    // Step 2
    //
    // Write out a file that contains the name of the save file

    // Convert the wide name to a normal string
    CHAR szFilename[MAX_GAMENAME] = { 0 };

    _snprintf( szFilename, MAX_GAMENAME , "%s%S",
                m_savedContentData[iSaveGameSlot].szSaveGameDirectory,
                INSTALL_LOCATION_FILE );

    HANDLE hSave = CreateFile(
                        szFilename,            // file name
                        GENERIC_WRITE,         // access mode
                        0,                     // share mode - No sharing until
                                               // handle is closed
                        NULL,                  // Security abilities -
                                               // Reserved, use NULL
                        OPEN_ALWAYS,           // Create new file or overwrite existing file
                        FILE_ATTRIBUTE_NORMAL, // file attributes
                        NULL                   // handle to template file,
                                               // reserved must use NULL
                    );

    DWORD   dwNumBytesWritten = 0;
    DWORD   dwNumBytesToWrite = wcslen( m_savedContentData[iSaveGameSlot].szSaveGameName );
    BOOL    bWrite            = TRUE;

    CHAR szGameName[64] = { 0 };
    _snprintf( szGameName, 64, "%S\0", m_savedContentData[iSaveGameSlot].szSaveGameName );
    
    // Write the name of the save
    bWrite = WriteFile( hSave,              // Handle of the file
                        szGameName,         // Data buffer to write
                        dwNumBytesToWrite,  // Number of bytes to write
                        &dwNumBytesWritten, // Ouput: Number bytes written
                        NULL );             // Overlapping write event

    assert( bWrite );
    assert( dwNumBytesWritten == dwNumBytesToWrite );

    CloseHandle( hSave );


    // Step 3
    //
    // Create a string that is the storage path
    // The storage path is the location of the
    // file on the server once it is finished
    // uploading

    ULONGLONG qwTeamID     = 0; // This is per user per title, so no team id
    DWORD     dwPathLength = MAX_SERVER_PATH_SIZE;
    DWORD     dwFacility   = XONLINESTORAGE_FACILITY_PER_USER_TITLE;
    WCHAR     wszStorageServerPath[MAX_SERVER_PATH_SIZE] = { 0 };


    HRESULT hrCreatePath = XOnlineStorageCreateServerPath(
                                dwFacility,           // The type of storage to use
                                qwUserID,             // qwID of the user
                                qwTeamID,             // qwID of the team (not used)
                                wszFilename,          // Name of the file
                                wszStorageServerPath, // Output path on the server
                                &dwPathLength         // Length of server path string
                        );

    if( FAILED( hrCreatePath ) )
        return FALSE;


    // Step 4
    //
    // Now that we know the save is safe, we can upload the directory
    // that it is in. XOnlineStorageUploadByServerPath takes
    // a directory on the local Xbox and uploads all files in
    // that directory. The "zip" is saved as the given file name
    //
    // Note:
    // There is no way to enumerate the names of files stored
    // using the PER_USER and PER_TEAM facilities. Thus
    // you must know the name of the file you want to download

    const CHAR*   pDirectoryName = m_savedContentData[iSaveGameSlot].szSaveGameDirectory;
    CXBOnlineTask hUploadTask;
    FILETIME      ftServerExpirationDate;
    
    // Never expire this data
    ZeroMemory( &ftServerExpirationDate, sizeof( ftServerExpirationDate ) );

    HRESULT hrUpload = XOnlineStorageUploadByServerPath(
                            dwFacility,              // Facility to store in
                            m_dwControllingUserPort, // Controller port
                            wszStorageServerPath,    // Name to save as
                            ftServerExpirationDate,  // Expiration date
                            pDirectoryName,          // Directory to package and upload
                            0,                       // Upload flags must be zero
                            NULL,                    // Work event
                            &hUploadTask             // Task to assign
                        );

    assert( SUCCEEDED( hrUpload ) );


    // Step 5
    //
    // Pump the task until finished

    WaitForTaskToComplete( hUploadTask, &hrUpload );
    
    hUploadTask.Close();


    // Step 6
    //
    // Delete our temp install location file
    // and return our success
    DeleteFile( szFilename );

    return( SUCCEEDED( hrUpload ) );
}

//-------------------------------------------------------------------------------------
// Name: VerifyFile()
// Desc: Takes a handle to an open content signature file,
//       the directory of the file we want to verify and the
//       name of the file we want to verify.
//       Calculates a new signature and compares it to the signature that the data
//       should have. Returns TRUE if the file has a valid signature.
//       Writes the file contents and size of the contents to the output parameter.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::VerifyFile( HANDLE hSignature,
                              const CHAR *szPath, const CHAR *szFilename,
                              PBYTE pData, DWORD &dwBufferSize )
{
    // Step 1
    //
    // Get the signature for the index that we were given

    PBYTE pSignature = NULL;
    DWORD dwSigSize  = 0;

    BOOL bLocate = XLocateSignatureByName( hSignature,   // Handle to sig
                                           szFilename,   // Filename to find sig for
                                           0,            // Offset - should be zero
                                           0,            // Data size - should be zero
                                           &pSignature,  // Pointer to signature
                                           &dwSigSize ); // Size of signature

    assert( bLocate );
    assert( dwSigSize > 0 );
    assert( pSignature );

    if( !bLocate )
        return FALSE;

    
    // Step 2
    //
    // Open the file to be read

    CHAR        szFullFilename[MAX_SERVER_PATH_SIZE] = { 0 };


    _snprintf( szFullFilename, MAX_SERVER_PATH_SIZE , "%s%s\0", szPath, szFilename );

    HANDLE hRead = CreateFile(
                        szFullFilename,        // file name
                        GENERIC_READ,          // access mode
                        0,                     // share mode - No sharing until handle is closed
                        NULL,                  // Security abilities - Reserved, use NULL
                        OPEN_EXISTING,         // Only open if the files exists
                        FILE_ATTRIBUTE_NORMAL, // file attributes
                        NULL                   // handle to template file, reserved must use NULL
                    );

    if( hRead == INVALID_HANDLE_VALUE )
    {
        return FALSE;
    }


    // Step 3
    //
    // Read the file into memory

    dwBufferSize = 0;
  
    BOOL bRead = ReadFile(
                    hRead,           // handle to file
                    pData,           // data buffer
                    MAX_FILE_SIZE,   // number of bytes to read
                    &dwBufferSize,   // number of bytes read
                    NULL             // overlapped buffer
                );

    CloseHandle( hRead );

    if( !bRead )
        return FALSE;

    assert( dwBufferSize > 0 );


    // Step 5
    //
    // Calculate a signature off the data we are trying to verify

    DWORD dwSaveSigSize = XCalculateSignatureGetSize( XCALCSIG_FLAG_SAVE_GAME );
    BYTE rwSignatureCalced[SAVE_SIG_BUFFER_SIZE];


    BOOL bCalced = XCalculateContentSignature(
                    pData,             // Data to calculate the content signature for
                    dwBufferSize,      // Size of the buffer to calculate from
                    rwSignatureCalced, // Pointer to the signature buffer output
                    &dwSigSize         // Size of the signature generated
                );

    if( dwSigSize != dwSaveSigSize )
        return FALSE;

    if( !bCalced )
        return FALSE;


    // Step 6
    //
    // Compare the signature generated to the
    // signatute that was read in.
    // Return the results.

    BOOL bSaveSigsMatch = ( memcmp( rwSignatureCalced, pSignature, dwSigSize) == 0 );

    return bSaveSigsMatch;
}

//-------------------------------------------------------------------------------------
// Name: DownloadSave
// Desc: Downloads the user save into the saved game directory
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::DownloadSave( ULONGLONG qwUserID, CHAR* szLocation )
{
    assert( szLocation );

    ULONGLONG     qwTeamID     = 0; // This is per user per title, so no team id
    DWORD         dwPathLength = MAX_SERVER_PATH_SIZE;
    DWORD         dwFacility   = XONLINESTORAGE_FACILITY_PER_USER_TITLE;
    WCHAR         wszStorageServerPath[MAX_SERVER_PATH_SIZE];
    CXBOnlineTask hDownloadTask;


    // Step 1
    //
    // Because you must know the name of the file you want to
    // download ( you can not enumerate a file list ),
    // the names should be predictable by the application.
    // In this sample's case we use the user ID of
    // the subscriber sharing data.
    //
    // Print the user's name to a string

    UINT iHi = (UINT)( qwUserID >> 32 );
    UINT iLo = (UINT)qwUserID;
    WCHAR wszFilename[MAX_GAMENAME] = { 0 };
    _snwprintf( wszFilename, MAX_GAMENAME, L"%08x%08x\0",
                iHi, iLo );


    // Step 2
    //
    // Create a string that contains the path of the file
    // on the server. This string is somewhat analagous
    // to a URL

    ZeroMemory( wszStorageServerPath, MAX_SERVER_PATH_SIZE );

    HRESULT hrCreatePath = XOnlineStorageCreateServerPath(
                                dwFacility,           // The type of storage to use
                                qwUserID,             // qwID of the user
                                qwTeamID,             // qwID of the team (not used)
                                wszFilename,          // Name of the file
                                wszStorageServerPath, // Output path on the server
                                &dwPathLength         // Length of server path string
                        );

    if( FAILED( hrCreatePath ) )
        return FALSE;


    // Step 3
    //
    // Start the download

    HRESULT hrDownload = XOnlineStorageDownload(
                            dwFacility,
                            m_dwControllingUserPort,
                            wszStorageServerPath,
                            NULL, //szInstallDirectory, // Install location, NULL puts it in default
                            0, // Download flags should be zerp
                            NULL, // Work event
                            &hDownloadTask
                        );

    assert( SUCCEEDED( hrDownload ) );


    // Step 4
    //
    // Pump the task until it's finished

    WaitForTaskToComplete( hDownloadTask, &hrDownload );


    // Step 5
    //
    // Close the task and return if we failed to download the file

    hDownloadTask.Close();

    if( FAILED( hrDownload ) )
        return FALSE;


    // Step 6
    //
    // Get the fully qualified file name ( includes path )
    // to the file we just downloaded

    DWORD szLocationSize = MAX_SERVER_PATH_SIZE;

    HRESULT hrInstallLocation = XOnlineStorageGetInstallLocation(
                                    dwFacility,           // Facility downloaded from
                                    wszStorageServerPath, // Storage path downloaded from
                                    szLocation,           // Path output parameter
                                    &szLocationSize       // size of path output
                                );

    if( FAILED( hrInstallLocation ) )
        return FALSE;


    // Step 7
    //
    // Open the content signatures metafile
    // The meta file contains it's own signature
    // and is self verifying.
    //
    // This file is autogenerated by XOnlineUploadByServerPath

    CHAR szTruncedPath[MAX_SERVER_PATH_SIZE] = { 0 };

    _snprintf( szTruncedPath, MAX_SERVER_PATH_SIZE, "%s",
               szLocation );

    INT iLastChar = strlen( szLocation ) - 1;
    szTruncedPath[iLastChar] = 0;

    HANDLE hSignatures = XLoadContentSignatures( szTruncedPath );

    assert( hSignatures != NULL );
    assert( hSignatures != INVALID_HANDLE_VALUE );


    // Step 8
    //
    // Read the signatures in the meta file
    // and verify each individual file downloaded

    DWORD       dwCount                  = 0;
    BYTE        pData[MAX_FILE_SIZE]     = { 0 };
    BOOL        bVerified                = FALSE;
    CHAR        szFilename[MAX_GAMENAME] = { 0 };


    // Verify the savegame metafile
    bVerified = VerifyFile( hSignatures,
                            szLocation, FILENAME_SAVEMETA,
                            pData, dwCount );
    assert( bVerified );


    // Verify the file that contains the name of the save.
    // Also the the name of the file we want to share
    // so we can verify it next
    bVerified = VerifyFile( hSignatures,
                            szLocation, FILENAME_INSTALL,
                            pData, dwCount );
    assert( bVerified );
    strncpy( szFilename, (CHAR*)pData, dwCount );


    // Verify the content that we actually want to share
    bVerified = VerifyFile( hSignatures,
                            szLocation, szFilename,
                            pData, dwCount );
    assert( bVerified );


    // Step 9
    //
    // Close the signature file and return our success

    XCloseContentSignatures( hSignatures );

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: ViewUserSave()
// Desc: Attempts to download content shared by the user with the given user ID.
//       Deomstrates DownloadToFile().
//       Returns TRUE if successful.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::ViewUserSave( ULONGLONG qwUserID, CUserContent& userContent )
{
    // Step 1
    //
    // Download the save

    CHAR szLocation[ MAX_SERVER_PATH_SIZE ] = { 0 };

    if( DownloadSave( qwUserID, szLocation ) )
    {

        // Step 2
        //
        // The download was successful so we
        // will try to open and verify everything.

        // Open the "install" file that contains the name
        // of the save we really want to view
        CHAR szFilename[MAX_GAMENAME] = { 0 };
        _snprintf( szFilename, MAX_GAMENAME , "%s%S\0", szLocation, INSTALL_LOCATION_FILE );

        HANDLE hRead = CreateFile(
                            szFilename,            // file name
                            GENERIC_READ,          // access mode
                            0,                     // share mode - No sharing until handle is closed
                            NULL,                  // Security abilities - Reserved, use NULL
                            OPEN_EXISTING,         // Only open if the files exists
                            FILE_ATTRIBUTE_NORMAL, // file attributes
                            NULL                   // handle to template file, reserved must use NULL
                        );

        if( hRead == INVALID_HANDLE_VALUE )
        {
            return FALSE;
        }


        // Step 3
        //
        // Read the name of the saved file

        DWORD  dwNumBytesRead   = 0;
        DWORD  dwNumBytesToRead = MAX_GAMENAME;
        BOOL   bRead            = TRUE;

        ZeroMemory( szFilename, MAX_GAMENAME );
        
        bRead = ReadFile(
                    hRead,              // handle to file
                    szFilename,         // data buffer
                    dwNumBytesToRead,   // number of bytes to read
                    &dwNumBytesRead,    // number of bytes read
                    NULL                // overlapped buffer
                );

        assert( bRead );
        CloseHandle( hRead );


        // Step 4
        //
        // Verify the live signature of the content
        // we want to view and return our success

        WCHAR wszFilename[MAX_GAMENAME] = { 0 };
        _snwprintf( wszFilename, MAX_GAMENAME, L"%S\0", szFilename );

        return userContent.Load( TRUE, szLocation, wszFilename );
    }

    return FALSE;
}

//-------------------------------------------------------------------------------------
// Name: CreateFace()
// Desc: Returns a pointer to an array of four vertices to draw our texture
//       onto. The memory is allocated by the GPU so we do not need to free it.
//       The width and height of the QUAD are the sizes given.
//-------------------------------------------------------------------------------------
LPDIRECT3DVERTEXBUFFER8 CXBoxSample::CreateFace( FLOAT fX, FLOAT fY )
{
    LPDIRECT3DVERTEXBUFFER8 pQuadVertices = NULL;

    // Step 1
    //
    // Create our background vertex buffer

    m_pd3dDevice->CreateVertexBuffer( 
                    4 * sizeof(CUSTOMVERTEX), // Size of buffer
                    0,                        // Usage: ignored
                    0,                        // FVF: ignored
                    NULL,                     // Pool: ignored
                    &pQuadVertices            // Output pointer to the vertex buffer
                );

    assert( pQuadVertices );


    // Step 2
    //
    // Lock the verex buffer so we can write the vertice's coordinates into memory

    CUSTOMVERTEX* pVertices = NULL;
    pQuadVertices->Lock( 0, 0, (BYTE **)&pVertices, 0L );

    FLOAT fTextureSize = (FLOAT)CUserContent::BITMAP_SIZE;

    // This offset's our coordintates so
    // the function's input parameters
    // refer to the upper left corner
    FLOAT fMagicX      = ICON_SIZE - 1.0f;
    FLOAT fMagicY      = ICON_SIZE - 1.0f;

    
    // Step 3
    //
    // Write the coordinates for each of the four points
    // of the quad into memory

    // Lower Left
    pVertices[0].p = D3DXVECTOR4( fX + fMagicX,
                                  fY + ICON_SIZE + fMagicY,
                                  1.0f, 1.0f );
    pVertices[0].t = D3DXVECTOR2( 0.0f, fTextureSize );

    // Upper Left
    pVertices[1].p = D3DXVECTOR4( fX + fMagicX,
                                  fY + fMagicY,
                                  1.0f, 1.0f );
    pVertices[1].t = D3DXVECTOR2( 0.0f, 0.0f );

    // Upper Right
    pVertices[2].p = D3DXVECTOR4( fX + ICON_SIZE + fMagicX,
                                  fY + fMagicY,
                                  1.0f, 1.0f );
    pVertices[2].t = D3DXVECTOR2( fTextureSize, 0.0f ); // Upper right

    // Lower Right
    pVertices[3].p = D3DXVECTOR4( fX + ICON_SIZE + fMagicX,
                                  fY + ICON_SIZE + fMagicY,
                                  1.0f, 1.0f );
    pVertices[3].t = D3DXVECTOR2( fTextureSize, fTextureSize );


    // Step 4
    //
    // Unlock the memory to commit our changes,
    // return our pointer

    pQuadVertices->Unlock();

    return pQuadVertices;
}

//-------------------------------------------------------------------------------------
// Name: SetFacePos()
// Desc: Sets the position of the QUAD in 2D space
//-------------------------------------------------------------------------------------
VOID CXBoxSample::SetFacePos( LPDIRECT3DVERTEXBUFFER8 pVerts,
                              FLOAT fX, FLOAT fY )
{
    assert( pVerts );

    CUSTOMVERTEX* pData  = NULL;
    UINT          uiSize = 4 * sizeof(CUSTOMVERTEX);

    // Step 1
    //
    // Lock the memory so we can make changes to
    // the vertex buffer

    pVerts->Lock( 0,              // offset to the data
                  uiSize,         // Data size we want
                  (PBYTE*)&pData, // Pointer to the data we will modify
                  0               // Flags
                );

    assert( pData );


    // Step 2
    //
    // Write the new coordinates to the
    // vertex buffer

    pData[0].p.y = fY + ICON_SIZE;
    pData[1].p.y = fY;
    pData[2].p.y = fY;
    pData[3].p.y = fY + ICON_SIZE;

    pData[0].p.x = fX;
    pData[1].p.x = fX;
    pData[2].p.x = fX + ICON_SIZE;
    pData[3].p.x = fX + ICON_SIZE;


    // Step 3
    //
    // Unlock to commit our changes

    pVerts->Unlock();
}

//-------------------------------------------------------------------------------------
// Name: TranslateFace()
// Desc: Moves the given Quad to given amount in 2D space
//-------------------------------------------------------------------------------------
VOID CXBoxSample::TranslateFace( LPDIRECT3DVERTEXBUFFER8 pVerts,
                                 FLOAT fX, FLOAT fY )
{
    assert( pVerts );

    CUSTOMVERTEX* pData  = NULL;
    UINT          uiSize = 4 * sizeof(CUSTOMVERTEX);

    // Step 1
    //
    // Lock the vertices and get a pointer to the vertex buffer

    pVerts->Lock( 0,              // offset to the data we want to modify
                  uiSize,         // Data size
                  (PBYTE*)&pData, // Output pointer to the verticies
                  0 );            // Flags

    assert( pData );


    // Step 2
    //
    // Add the translation to the verts

    pData[0].p.y += fY;
    pData[1].p.y += fY;
    pData[2].p.y += fY;
    pData[3].p.y += fY;

    pData[0].p.x += fX;
    pData[1].p.x += fX;
    pData[2].p.x += fX;
    pData[3].p.x += fX;


    // Step 3
    //
    // Unlock the memory to commit our changes

    pVerts->Unlock();
}

//-------------------------------------------------------------------------------------
// Name: RenderSprite()
// Desc: Renders the texture over the given Quad
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderSprite ( LPDIRECT3DVERTEXBUFFER8 pVerts,
                                 LPDIRECT3DTEXTURE8 pTexture )
{
    assert( pVerts );
    assert( pTexture );

    // Step 1
    //
    // Set our render state
    // It is import to use clamped UVs because we are using a
    // linear texture

    m_pd3dDevice->SetTexture( 0, pTexture );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE,         D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_NONE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );


    // Step 2
    //
    // Draw the Quad

    m_pd3dDevice->SetStreamSource( 0, pVerts, sizeof( CUSTOMVERTEX ) );
    m_pd3dDevice->SetVertexShader( D3DFVF_CUSTOMVERTEX );
    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );
    m_pd3dDevice->SetTexture( 0, NULL );
}

//-------------------------------------------------------------------------------------
// Name: GetTeamList()
// Desc: Attempts to get a list of the current user's teams from the
//       Xbox Live service. Returns FALSE if it fails.
//
// Note: Attempting to read the team list several times in
//       quick succession may cause the operation to cause a
//       throttling error
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::GetTeamList()
{
    if( m_ppTeamLogoTextures )
        delete [] m_ppTeamLogoTextures;

    m_ppTeamLogoTextures = NULL;
    m_dwTeamLogoToDL     = 0;

    // Step 1
    //
    // Start the task to enumerate the teams list
    // that the current user is a member of

    CXBOnlineTask hViewMyTeamsTask;

    HRESULT hrTeamFind = XOnlineTeamEnumerateByUserXUID(
        m_dwControllingUserPort,
        m_rwStoredUsers[m_wCurUserIndex].xuid,
        NULL,
        &hViewMyTeamsTask );


    // Unable to start enumeration task
    if( FAILED( hrTeamFind ) )
    {
        hViewMyTeamsTask.Close();

        return FALSE;
    }


    // Step 2
    //
    // Continue the task until complete
    if(! WaitForTaskToComplete( hViewMyTeamsTask, &hrTeamFind ) )
    {
        hViewMyTeamsTask.Close();

        return FALSE;
    }


    // Step 3
    //
    // Get the results from the finished task
    // and populate the list
    m_dwTeamCount = 0;

    ZeroMemory( m_rwTeamXUIDS, sizeof( m_rwTeamXUIDS ) );

    hrTeamFind = XOnlineTeamEnumerateGetResults(
                    hViewMyTeamsTask, // The enumeration task
                    &m_dwTeamCount,   // The number of teams read
                    m_rwTeamXUIDS     // An array of all the team XUIDs read
                 );

    // Unable to get results
    if( FAILED( hrTeamFind ) )
    {
        hViewMyTeamsTask.Close();

        return FALSE;
    }


    // Step 4
    //
    // Now that we have the XUIDs of all the teams
    // the current user is a member of, we have to go
    // and create tasks to fill all the information about
    // each team so we can display
    //
    // NOTE: A real title would only have to display
    // the team details if the user wanted

    ZeroMemory( m_rwTeamInfo, sizeof( m_rwTeamInfo ) );

    for( INT i = 0; i < (INT)m_dwTeamCount; ++i)
    {
        HRESULT hrTeamDetails = XOnlineTeamGetDetails(
                                    hViewMyTeamsTask, // The task that started the team read
                                    m_rwTeamXUIDS[i], // The list of XUIDs that we want details of
                                    &m_rwTeamInfo[i]  // The array of details to be populated
                                );

        // Unable to get team details
        if( FAILED( hrTeamDetails ) )
        {
            hViewMyTeamsTask.Close();

            return FALSE;
        }
    }

    hViewMyTeamsTask.Close();

    m_ppTeamLogoTextures = new LPDIRECT3DTEXTURE8[m_dwTeamCount];
    assert( m_ppTeamLogoTextures );

    if( m_ppTeamLogoTextures )
        ZeroMemory( m_ppTeamLogoTextures, sizeof( LPDIRECT3DTEXTURE8 ) * m_dwTeamCount );

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: GetTeamRoster()
// Desc: Attempts to get a Roster of the current user's selected team from the
//       Xbox Live service. Returns FALSE if it fails.
//
// Note: Attempting to read the team roster several times in
//       quick succession may cause the operation to cause a
//       throttling error
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::GetTeamRoster()
{
    m_dwRosterRenderStart = 0;

    if( m_ppTeammateTextures )
        delete[] m_ppTeammateTextures;

    m_ppTeammateTextures = NULL;

    if( m_phTeamRosterTask.IsOpen() )
        m_phTeamRosterTask.Close();

    // Step 1
    //
    // Start the read of the roster

    // Setting to XONLINE_TEAM_SHOW_RECRUITS
    // will return "members" who have not
    // yet accepted an invitation
    //
    // Setting to 0 will only show
    // members who have accepted an
    // invitation to join the team
    DWORD dwEnumerationFlags = 0;

    HRESULT hrTeamRoster = XOnlineTeamMembersEnumerate(
            m_dwControllingUserPort,        // Controller index (zero-based) of the user making the request. 
            m_rwTeamXUIDS[m_iTeamSelected], // XUID structure that uniquely identifies the team. 
            dwEnumerationFlags,             // Flags indicating how the team members should be enumerated.
                                            // XONLINE_TEAM_SHOW_RECRUITS Indicates that recruits should
                                            // be included in the returned results. 
            NULL,                           // Handle to an event (OPTIONAL)
            &m_phTeamRosterTask             // Pointer to an XONLINETASK_HANDLE returned
        );

    if( FAILED( hrTeamRoster ) )
        return FALSE;


    // Step 2
    //
    // Continue until the task is complete

    if(! WaitForTaskToComplete( m_phTeamRosterTask, &hrTeamRoster ) )
    {
        return FALSE;
    }


    // Step 3
    //
    // Now that the task is finished, get the results

    m_dwTeamMemberCount = 0;
    ZeroMemory( m_rwTeamMembers, sizeof( m_rwTeamMembers ) );

    hrTeamRoster = XOnlineTeamMembersEnumerateGetResults(
                        m_phTeamRosterTask,   // The task that read the roster
                        &m_dwTeamMemberCount, // The number of team members
                        m_rwTeamMembers       // The list of XUIDs to be populated
                    );


    // Step 4
    //
    // Initialize the array of pointers to use for
    // the team member buddy icons

    m_ppTeammateTextures = new LPDIRECT3DTEXTURE8[m_dwTeamMemberCount];
    assert( m_ppTeammateTextures );

    if( m_ppTeammateTextures )
    {
        ZeroMemory( m_ppTeammateTextures,
                    sizeof( LPDIRECT3DTEXTURE8 ) * m_dwTeamMemberCount );
    }

    m_dwTeamMemberTextureToDL = 0;


    return SUCCEEDED( hrTeamRoster );
}


///////////////////////
// UI FSM state code //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: GetEvent()
// Desc: Returns the state of the controller at the given port
//-------------------------------------------------------------------------------------
CXBoxSample::Event CXBoxSample::GetEvent( INT iController ) const
{
    // "A"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
        return EV_BUTTON_A;
    
    // "B"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        return EV_BUTTON_B;

    // "X"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
        return EV_BUTTON_X;

    // "Y"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
        return EV_BUTTON_Y;
    
    // "Back"
    if( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_BACK )
        return EV_BUTTON_BACK;

    // "Start"
    if( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_START )
        return EV_BUTTON_START;
    
    // "White"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
        return EV_BUTTON_WHITE;

    // "Black"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK] )
        return EV_BUTTON_BLACK;
    

    // Movement

    if( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        return EV_UP;

    if( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        return EV_DOWN;

    if( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        return EV_LEFT;

    if( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        return EV_RIGHT;
    

    return EV_NULL;
}

//-------------------------------------------------------------------------------------
// Name: PushState()
// Desc: Transitions the UI and game from it's current state to the requested
//       state. When a transition occurs any code required by the previous
//       is executed. Any code required by the new state before entry is
//       also executed.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::PushState( EUIStates newState )
{
    // Do not execute "double entry"
    // if this happens
    if( newState == m_state )
        return;

    // Execute exit functionality
    switch( m_state )
    {
    case STATE_SELECT_ACCOUNT:          ExitStateSelectAccount();       break;
    case STATE_LOGIN:                   ExitStateLogin();               break;
    case STATE_LOGIN_FAILED:            ExitStateLoginFailed();         break;
    case STATE_NETWORK_ERROR:           ExitStateNetworkError();        break;
    case STATE_MAIN:                    ExitStateMain();                break;
    case STATE_CONTENT_MANAGEMENT:      ExitStateContentManagement();   break;
    case STATE_LIST_SAVED_CONTENT:      ExitStateListSavedContent();    break;
    case STATE_LOCAL_CONTENT_OPTIONS:   ExitStateLocalContentOptions(); break;
    case STATE_RECENT_PLAYERS:          ExitStateRecentPlayers();       break;
    case STATE_VIEW_MY_TEAMS:           ExitStateViewMyTeams();         break;
    case STATE_VIEW_TEAM_ROSTER:        ExitStateViewTeamRoster();      break;
    case STATE_VIEW_TEAMMATE_ICON:      ExitStateViewTeammateIcon();    break;
    case STATE_SETTINGS_EDIT:           ExitStateSettingsEdit();        break;
    case STATE_CONTENT_EDIT:            ExitStateContentEdit();         break;
    case STATE_MESSAGE_WINDOW:          ExitStateMessageWindow();       break;
    default:
        break; //assert(0 && "Unknown/illegal state!");
    };

    // Execute entry functionality
    switch( newState )
    {
    case STATE_SELECT_ACCOUNT:          EnterStateSelectAccount();          break;
    case STATE_LOGIN:                   EnterStateLogin();                  break;
    case STATE_LOGIN_FAILED:            EnterStateLoginFailed();            break;
    case STATE_NETWORK_ERROR:           EnterStateNetworkError();           break;
    case STATE_MAIN:                    EnterStateMain();                   break;
    case STATE_CONTENT_MANAGEMENT:      EnterStateContentManagement();      break;
    case STATE_LIST_SAVED_CONTENT:      EnterStateListSavedContent();       break;
    case STATE_LOCAL_CONTENT_OPTIONS:   EnterStateLocalContentOptions();    break;
    case STATE_RECENT_PLAYERS:          EnterStateRecentPlayers();          break;
    case STATE_VIEW_MY_TEAMS:           EnterStateViewMyTeams();            break;
    case STATE_VIEW_TEAM_ROSTER:        EnterStateViewTeamRoster();         break;
    case STATE_VIEW_TEAMMATE_ICON:      EnterStateViewTeammateIcon();       break;
    case STATE_SETTINGS_EDIT:           EnterStateSettingsEdit();           break;
    case STATE_CONTENT_EDIT:            EnterStateContentEdit();            break;
    case STATE_MESSAGE_WINDOW:          EnterStateMessageWindow();          break;
    default:
        assert( 0 && "Unknown/illegal state!" );
    }

    // Finally transition to the desited state
    // This is set for legacy reasons
    m_state = newState;

    // Push the new state on top of the stack
    assert( m_wStateStackSize < MAX_SIZE_STATE_STACK );
    m_stateStack[m_wStateStackSize++] = m_state;
}

//-------------------------------------------------------------------------------------
// Name: PopState
// Desc: Stops execution of the current state and resumes execution of the previous
//       state.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::PopState( BOOL bReinit )
{
    // Can't exit past the starting state!
    assert( m_wStateStackSize > 1 );

    --m_wStateStackSize;

    m_state = m_stateStack[m_wStateStackSize - 1];

    if( bReinit )
    {
        --m_wStateStackSize;

        EUIStates tempState = m_state;

        m_state = NUM_STATES;

        PushState( tempState );
    }
}

//-------------------------------------------------------------------------------------
// Name: PushMessageWindow
// Desc: Pushes the message window state and transitions to it to
//       display the given text message;
//-------------------------------------------------------------------------------------
VOID CXBoxSample::PushMessageWindow( const CHAR* strTextMessage )
{
    XBUtil_GetWide( strTextMessage,
                    m_szGameMessage, 
                    MAX_MESSAGE_LENGTH );

    PushState( STATE_MESSAGE_WINDOW );
}

//-------------------------------------------------------------------------------------
// Name: RenderMenu()
// Desc: Draws the given menu to the screen along with a point next to the
//       currently selected item
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderMenu( const WCHAR* strMenuName, const WCHAR** rwMenuText,
                 const WORD wNumMenuItems, const INT iCurMenuItem )
{
    // Menu Title
    m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y, m_dwTextColor,
                     strMenuName,
                     XBFONT_CENTER_X );

    // Attempt to center the menu
    FLOAT fMenuStartPos = SCREEN_CENTER_Y - ( wNumMenuItems * DEFAULT_TEXT_PADDING * 0.5f );

    assert( ( fMenuStartPos > POS_SCREEN_TITLE_Y ) && "Menu too large!" );

    // If we are given an empty menu just render
    // the name of the screen and the footer
    if( wNumMenuItems > 0 )
    {
        // Menu Items
        for( WORD i = 0; i < wNumMenuItems; ++i )
        {
            // Highlight the selected item
            DWORD dwColor = ( iCurMenuItem == i ) ? m_dwHighlightColor : m_dwTextColor;

            m_font.DrawText( SCREEN_CENTER_X,
                            fMenuStartPos + (DEFAULT_TEXT_PADDING * i),
                            dwColor, rwMenuText[i], XBFONT_CENTER_X );
        }

        // Show selected item with a little triangle
        FLOAT fTextOffset   = m_font.GetTextWidth( rwMenuText[ iCurMenuItem ] ) / 2.0f;
        FLOAT fTextPos      = SCREEN_CENTER_X - 
                            ( fTextOffset + m_font.GetTextWidth( GLYPH_RIGHT_TICK ) );

        m_font.DrawText( fTextPos,
                        fMenuStartPos + ( DEFAULT_TEXT_PADDING * iCurMenuItem ),
                        COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_CENTER_X );
    }
}

//-------------------------------------------------------------------------------------
// Name: GetMenuPosition()
// Desc: Takes the current menu position, the number of items in the menu
//       and the menu event and returns the new menu position. Handles wrap-around
//       of the menu in both directions
//-------------------------------------------------------------------------------------
INT CXBoxSample::GetMenuPosition( INT iCurMenuPosition, INT iNumMenuItems, Event event , INT iMenuWrap )
{
    switch( event )
    {
    case EV_UP:
        --iCurMenuPosition;

        switch( iMenuWrap )
        {
        case MENU_WRAP_ON:
        // Wrap the input to goto the bottom
            iCurMenuPosition = ( iCurMenuPosition < 0 ) ? 
                                 ( iNumMenuItems - 1 ) : iCurMenuPosition;
            break;

        case MENU_WRAP_OFF:
            // Don't wrap. Just stick to the first index
            iCurMenuPosition = ( iCurMenuPosition < 0 ) ? 
                                 0 : iCurMenuPosition;
            break;
        }

        break;

    case EV_DOWN:
        ++iCurMenuPosition;

        switch( iMenuWrap )
        {
        case MENU_WRAP_ON:
        // Wrap the input to goto the top
            iCurMenuPosition = ( iCurMenuPosition >= iNumMenuItems ) ? 
                                 0 : iCurMenuPosition;
            break;

        case MENU_WRAP_OFF:
            // Don't wrap. Just stick to the last index
            iCurMenuPosition = ( iCurMenuPosition >= iNumMenuItems ) ? 
                                 ( iNumMenuItems - 1 ) : iCurMenuPosition;
            break;
        }

        break;
    }

    return iCurMenuPosition;
}

/////////////////////////
//   Progress Methods  //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: SetProgressTask
// Desc: initializes progress task and states thereof; and creates a progress 
// message showing how much progress has occurred in an upload/download task 
// (note: this should be in a printf style format to allow display of integer 
// percentage number i.e. "Upload content progress so far: %u")
// also, message string should be no longer than MAX_MESSAGE_LENGTH - 1
//-------------------------------------------------------------------------------------
VOID    CXBoxSample::SetProgressTask( DWORD dwProgressActivity , 
                                      const CHAR* szMessageFormat )
{
    // turn progress bar on and assign task to progress task
    m_dwProgressActivity = dwProgressActivity;
    m_bProgressSucceeded = FALSE;
    m_dwProgressPercentage = 0;

    // initialize progress format string
    XBUtil_GetWide( szMessageFormat, m_wszProgressMessageFormat, MAX_MESSAGE_LENGTH );
}
//-------------------------------------------------------------------------------------
// Name: UpdateProgressForTask
// Desc: attempts to get progress for current progress task.  If attempt fails,
//       FALSE is returned, else TRUE
//-------------------------------------------------------------------------------------
BOOL    CXBoxSample::UpdateProgressForTask( CXBOnlineTask& task )
{
    // Get information of progress of upload or download task
    ULONGLONG      qwProgressNum, qwProgressDem;
    DWORD          dwNewProgressPercentage;
    HRESULT hrProgress = XOnlineStorageGetProgress(
                        (XONLINETASK_HANDLE)(task), // progress task     
                        &dwNewProgressPercentage,   // gets percentage    
                        &qwProgressNum,             // gets numerator (not used now)
                        &qwProgressDem );           // gets denominator (not used now)

    // just leave, if this call failed, leave stored previous percentage alone
    if ( FAILED( hrProgress ) )
    {
        return FALSE;
    }

    // assign new percentage
    m_dwProgressPercentage = dwNewProgressPercentage;

    return TRUE;

}

//-------------------------------------------------------------------------------------
// Name: RenderProgressWindow
// Desc: displays a screen with progress message
//-------------------------------------------------------------------------------------
VOID    CXBoxSample::RenderProgressWindow()
{
    // print out the value of the progress in our message string, using the format 
    // string
    _snwprintf( m_wszProgressMessage , MAX_MESSAGE_LENGTH , 
                m_wszProgressMessageFormat , (INT)m_dwProgressPercentage );

    // draw the message in the center
    m_font.DrawText( SCREEN_CENTER_X , SCREEN_CENTER_Y, m_dwTextColor ,
                     m_wszProgressMessage, XBFONT_CENTER_X );

    // draw footer
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

//-------------------------------------------------------------------------------------
// Name: ClearProgressTask
// Desc: deinitializes progress task and states thereof
//-------------------------------------------------------------------------------------
VOID    CXBoxSample::ClearProgressTask()
{
    // deinitialize progress variables
    m_dwProgressActivity = (DWORD)PROGRESS_ACTIVITY_NONE;
    m_bProgressSucceeded = FALSE;

    ZeroMemory( m_wszProgressMessageFormat , sizeof( m_wszProgressMessageFormat ) );
    ZeroMemory( m_wszProgressMessage , sizeof( m_wszProgressMessage ) );

}

/////////////////////////
// State SelectAccount //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateSelectAccount
// Desc: Executes setup code for STATE_SELECT_ACCOUNT
//       Finds the Xbox Live user accounts on the memory units and hard-drive
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateSelectAccount()
{
    // Default colors for the sample's UI
    m_dwBGColor        = COLOR_BLUE;
    m_dwTextColor      = COLOR_WHITE;
    m_dwHighlightColor = COLOR_YELLOW;


    m_wCurUserIndex        = 0;
    m_dwAccountRenderStart = 0;

    if( m_bUserSignedIn )
        m_hLogonTask.Close();

    m_bUserSignedIn = FALSE;

    // If any MUs are inserted/removed, need to update the
    // user account list
    DWORD dwInsertions;
    DWORD dwRemovals;

    // Stall for mem unit mounting
    while ( CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals ) );

    // Keep it in memory so we don't have to worry about insertion
    // and deletion once we get past login
    CXBMemUnit::GetMemUnitSnapshot();

    // First, obtain a list of user accounts on this Xbox. The XOnlineGetUsers
    // function will enumerate both the hard disk and any attached memory units
    // looking for accounts. 
    HRESULT hrGetUsers = XOnlineGetUsers( m_rwStoredUsers, &m_dwNumStoredUsers );

    // Reboot the user to create an Xbox Live account if
    // one isn't found
    if( FAILED( hrGetUsers ) )
    {
        BootToDash( XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP );
    }

    // If no accounts, then player needs to create an account.
    if( m_dwNumStoredUsers == 0 )
    {
        // Titles must give the player the *option* of going to
        // the online dash to create new account. In addition, it is
        // possible for a player to actually insert/remove an MU while
        // the title account selection UI is active.  A title must
        // call XOnlineGetUsers repeatedly to account for this.
        // For demonstration purposes, we boot to the account signup section
        // of the online dash
        BootToDash( XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP );
    }

    m_wCurUserIndex = 0;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateSelectAccount
// Desc: Allows the user to scroll through all accounts stored on the Xbox
//       and to select the account that they wish to logon with.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectAccount( INT iUser, Event event )
{
    switch( event )
    {
    case EV_UP: // Move the roster list up until we hit the top
        m_wCurUserIndex = ( m_wCurUserIndex > 0 ) ? ( m_wCurUserIndex - 1 ) : 0;

        if( m_wCurUserIndex < m_dwAccountRenderStart )
            m_dwAccountRenderStart  = ( m_dwAccountRenderStart > 0 ) ? ( m_dwAccountRenderStart - 1 ) : 0;

        break;

    case EV_DOWN: // Move the roster list down until we hit the bottom
        m_wCurUserIndex = ( m_wCurUserIndex < ( m_dwNumStoredUsers - 1 ) ) ? ( m_wCurUserIndex + 1 ) : m_wCurUserIndex;
        
        if( m_wCurUserIndex >= ( m_dwAccountRenderStart + NUM_ENTRIES_PER_SCREEN ) )
        {
            ++m_dwAccountRenderStart;

            if( m_dwAccountRenderStart > ( m_dwNumContentSaves - NUM_ENTRIES_PER_SCREEN ) )
                m_dwAccountRenderStart = m_dwNumContentSaves - NUM_ENTRIES_PER_SCREEN;
        }
        break;
    }

    switch( event )
    {
    case EV_BUTTON_A:
        // Save the controller port that
        // the user is using. This is needed
        // by the XOnline functions to help
        // with authentication
        m_dwControllingUserPort = iUser;

        PushState( STATE_LOGIN );
        break;

    case EV_BUTTON_B:
        // Skip Xbox Live signon
        m_dwControllingUserPort = iUser;
        m_bUserSignedIn         = FALSE;

        PushState( STATE_MAIN );
        PushMessageWindow( "Some features will not be available" );
        break;

    default:
        break;
    }


    DWORD dwInsertions;
    DWORD dwRemovals;

    // If the user inserts a memory card, go ahead
    // and mount it!
    if ( CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals ) )
    {
        // Cause re-initialization of the memory cards
        // and hard drive and find the accounts added
        // or removed by the device change.
        EnterStateSelectAccount();
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateSelectAccount
// Desc: Renders the screen for STATE_SELECT_ACCOUNT
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateSelectAccount()
{
    m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y, m_dwTextColor,
                     L"SELECT ACCOUNT",
                     XBFONT_CENTER_X );

    FLOAT fAccountNameStart = SCREEN_CENTER_X * 0.75f;

    // If we are starting the render from a position
    // other than the first user in the list
    // the draw a little arrow on the side telling
    // the user they can scroll up
    if( m_dwAccountRenderStart > 0 )
    {
        m_font.DrawText( fAccountNameStart, POS_ACCOUNT_LIST_START,
                         m_dwHighlightColor, GLYPH_UP_ARROW L"    ", XBFONT_RIGHT );
    }

    // Show list of user accounts
    for( DWORD i = m_dwAccountRenderStart; i < m_dwNumStoredUsers; ++i )
    {
        DWORD dwColor = 
            ( (DWORD)m_wCurUserIndex == i ) ? m_dwHighlightColor : m_dwTextColor;

        if( ( i - m_dwAccountRenderStart ) >= NUM_ENTRIES_PER_SCREEN )
        {
            // If more team roster entries are below
            // the last entry drawn, then add a down
            // arrow on the side telling the user
            // can scroll down
            FLOAT fDownArrowY = POS_ACCOUNT_LIST_START
                                + ( DEFAULT_TEXT_PADDING * ( NUM_ENTRIES_PER_SCREEN - 1 ) );

            m_font.DrawText( fAccountNameStart, fDownArrowY,
                             m_dwHighlightColor, GLYPH_DOWN_ARROW L"   ", XBFONT_RIGHT );

            break;
        }

        // Convert user name to WCHAR string
        WCHAR strUserName[XONLINE_GAMERTAG_SIZE] = { 0 };

        XBUtil_GetWide( m_rwStoredUsers[i].szGamertag, strUserName,
                        XONLINE_GAMERTAG_SIZE );

        m_font.DrawText( fAccountNameStart,
                         POS_ACCOUNT_LIST_START + ( DEFAULT_TEXT_PADDING * ( i - m_dwAccountRenderStart ) ),
                         dwColor,
                         strUserName, XBFONT_LEFT );

        if ( i == (DWORD)m_wCurUserIndex )
        {
            m_font.DrawText( fAccountNameStart, POS_ACCOUNT_LIST_START + 
                             ( DEFAULT_TEXT_PADDING * ( m_wCurUserIndex - m_dwAccountRenderStart ) ),
                             COLOR_POINTER,
                             GLYPH_RIGHT_TICK, XBFONT_RIGHT );
        }
    }

    // The user can not really back out of this screen
    // but they can skip logon
    m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                     m_dwTextColor, GLYPH_B_BUTTON L" skip Xbox Live logon", 
                     XBFONT_LEFT );

    RenderFooter( FOOTER_RENDER_SELECT );
}


/////////////////
// State Login //
/////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateLogin()
// Desc: Initialises data needed to login to the Xbox Live service
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateLogin()
{
    // Only get the UI colors and MOTD once per logon
    m_bFirstInit = TRUE;

    HRESULT hr = StartSignIn();

    if ( FAILED( hr ) )
    {
        PushState( STATE_LOGIN_FAILED );
    }
    else
    {
        m_iItemSelected   = 0;
        m_bUserSignedIn   = FALSE;
        m_bIsSigningIn    = TRUE;
    }
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateLogin
// Desc: Attempts to log the user into the Xbox Live service. If login fails
//       the user will be prompted try again or to fix the problem via
//       the Xbox Dashboard
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateLogin( Event event )
{
    switch( event )
    {
    case EV_BUTTON_B: // Allow the user to cancel and use a different account
        // Close the task to allow for somone else to signon
        if ( m_bUserSignedIn || m_bIsSigningIn )
        {
            m_hLogonTask.Close();
        }

        m_bUserSignedIn    = FALSE;
        m_bIsSigningIn = FALSE;

        PopState( TRUE );
        break;
    }

    if ( m_bIsSigningIn )
    {
        // Continue the login task and process
        // any results we get back
        m_iSignInResult = ContinueSignIn();

        switch( m_iSignInResult )
        {
        case S_OK: // The Login is still executing
            return;
            break;

        case XONLINE_S_LOGON_CONNECTION_ESTABLISHED:
            // Finish sign in:
            // Announce our presence to Xbox Live
            // and check the required services that
            // we need to run the demo
            m_iSignInResult = FinishSignIn();

            if ( m_iSignInResult == S_OK )
            {
                m_bUserSignedIn = TRUE;
                m_bIsSigningIn = FALSE;

                PopState();
                PushState( STATE_MAIN );
            }
            else
            {
                PushState( STATE_LOGIN_FAILED );
            }
            return;
            break;

        case E_NETWORK_ERROR:
            m_bUserSignedIn = FALSE;
            m_bIsSigningIn  = FALSE;

            PushState( STATE_LOGIN_FAILED );
            return;
        break;

        case E_ACCOUNT_ERROR:
            m_bUserSignedIn = FALSE;
            m_bIsSigningIn  = FALSE;

            PushState( STATE_LOGIN_FAILED );
            return;
            break;

        default:
            assert( 0 && "Unexpected results" );
        }
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateLogin()
// Desc: Shows the user that they are being logged on to Xbox Live
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateLogin()
{
    // Render the scene
    m_font.DrawText( SCREEN_CENTER_X, POS_MESSAGE_Y, m_dwTextColor,
                     L"Logging In...", 
                     XBFONT_CENTER_X );

    RenderFooter( FOOTER_RENDER_NONE );
}


///////////////////////
// State LoginFailed //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateLoginFailed
// Desc: Allows the user to choose to try again or fix the problem
//       preventing login via the Xbox Dashboard.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateLoginFailed( Event event )
{
    switch( event )
    {
    case EV_BUTTON_A: // Try again with a different account
        ClearStack();
        return;
        break;

    case EV_BUTTON_B: // Reboot to the Xbox Dashboard
                      // Depending on the error the user
                      // will be presented with different options
        switch( m_iSignInResult )
        {
        case E_NETWORK_ERROR: // Reboot to configure the network settings
            BootToDash( XLD_LAUNCH_DASHBOARD_NETWORK_CONFIGURATION );
            break;

        case E_ACCOUNT_ERROR: // Reboot to configure/create accounts
            BootToDash( XLD_LAUNCH_DASHBOARD_ONLINE_MENU );
            break;
        }
        break;

    default:
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateLoginFailed
// Desc: Renders the screen that tells the player their login to Xbox Live
//       failed.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateLoginFailed()
{
    const FLOAT HEADER_OFFSET_X_LOGIN_FAILED    = 75.0;
    const FLOAT HEADER_OFFSET_Y_LOGIN_FAILED    = 100.0;

    const FLOAT MSG_OFFSET_X_LOGIN_FAILED       = 75.0;
    const FLOAT MSG_START_OFFSET_Y_LOGIN_FAILED = 200.0;
    const FLOAT MSG_PADDING_LOGIN_FAILED        = 40.0;
    
    FLOAT curMsgYOffset = MSG_START_OFFSET_Y_LOGIN_FAILED;

    const WCHAR* HEADER_STR_LOGIN_FAILED[NUM_LOGIN_ERRORS] =
    {
        NULL, // (not applicable... we wouldn't be here without an error)
        L"Login Failed: Network Error.",
        L"Login Failed: Account Error."
    };

    const WCHAR* ERROR_CFG_STR_LOGIN_FAILED[NUM_LOGIN_ERRORS] =
    {
        NULL, // (not applicable... we wouldn't be here without an error)
        L"Press " GLYPH_B_BUTTON L" to configure network settings",
        L"Press " GLYPH_B_BUTTON L" to configure XBox Live user accounts"
    };

    // Render the scene

    // Render the header
    m_font.DrawText( HEADER_OFFSET_X_LOGIN_FAILED, HEADER_OFFSET_Y_LOGIN_FAILED, 
                     m_dwTextColor, HEADER_STR_LOGIN_FAILED[m_iSignInResult], 
                     XBFONT_LEFT );

    // Render the messages/instructions
    m_font.DrawText( MSG_OFFSET_X_LOGIN_FAILED, curMsgYOffset, m_dwTextColor, 
                     L"Press " GLYPH_A_BUTTON L" to continue", XBFONT_LEFT );
    curMsgYOffset += MSG_PADDING_LOGIN_FAILED;
    m_font.DrawText( MSG_OFFSET_X_LOGIN_FAILED, curMsgYOffset, m_dwTextColor, 
                     ERROR_CFG_STR_LOGIN_FAILED[m_iSignInResult], XBFONT_LEFT );
}


////////////////////////
// State NetworkError //
////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateNetworkError
// Desc: Allows the user to choose what to do in case of a network error.
//       The user may login in again or reboot to the bashboard.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateNetworkError( Event event )
{
    switch(event)
    {
    case EV_BUTTON_A:
        ClearStack();
        return;
        break;

    case EV_BUTTON_B:
        BootToDash( XLD_LAUNCH_DASHBOARD_NETWORK_CONFIGURATION );
        break;

     default:
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateNetworkError()
// Desc: Displays the screen that tells the player they are
//       experiencing network problems.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateNetworkError()
{
    const FLOAT HEADER_OFFSET_X_NETWORK_ERROR    = 75.0;
    const FLOAT HEADER_OFFSET_Y_NETWORK_ERROR    = 100.0;

    const FLOAT MSG_OFFSET_X_NETWORK_ERROR       = 75.0;
    const FLOAT MSG_START_OFFSET_Y_NETWORK_ERROR = 200.0;
    const FLOAT MSG_PADDING_NETWORK_ERROR        = 40.0;
    
    FLOAT curMsgYOffset = MSG_START_OFFSET_Y_NETWORK_ERROR;

    // Render the scene

    // Render the header
    m_font.DrawText( HEADER_OFFSET_X_NETWORK_ERROR, HEADER_OFFSET_Y_NETWORK_ERROR, 
                     m_dwTextColor, L"Network Error", XBFONT_LEFT );

    // Render the message(s)
    m_font.DrawText( MSG_OFFSET_X_NETWORK_ERROR, curMsgYOffset, m_dwTextColor, 
                     L"Press " GLYPH_A_BUTTON L" to continue", XBFONT_LEFT );
    curMsgYOffset += MSG_PADDING_NETWORK_ERROR;
    m_font.DrawText( MSG_OFFSET_X_NETWORK_ERROR, curMsgYOffset, m_dwTextColor, 
                     L"Press " GLYPH_B_BUTTON L" to configure network settings", 
                     XBFONT_LEFT );

}

//-------------------------------------------------------------------------------------
// Name: ExitStateNetworkError
// Desc: Cleans up the network variables to allow for the user to login again.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::ExitStateNetworkError()
{
    m_hLogonTask.Close();
}


////////////////
// State Main //
////////////////

// Helper functions

//-------------------------------------------------------------------------------------
// Name: GetMessageOfTheDay()
// Desc: attempts to retrieve message of the day from global/publisher storage
//       TRUE if retrieval succeeds, otherwise FALSE
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::GetMessageOfTheDay( PXONLINESTORAGE_FILE_INFO* rwEnumResults,
                                      DWORD dwNumResults )
{
    // Clear out the current message
    lstrcpynW( m_wszMessageOfTheDay , L"" , 
               MAX_MESSAGE_LENGTH );

    // Find the index of the filename in the
    // enumeration results
    INT iIndex = GetFileIndex( FILENAME_MOTD,
                               rwEnumResults,
                               dwNumResults );

    // Bail if the file is not found
    if( iIndex < 0 )
        return FALSE;


    // Create a buffer to download the message into
    CHAR  szBuffer[MIN_XONLINE_DOWNLOAD_BUFFER_SIZE] = { 0 };
    DWORD dwBufferSize = MIN_XONLINE_DOWNLOAD_BUFFER_SIZE;

    // Download the message into our buffer
    HRESULT hrDownloaded = DownloadGlobal( m_dwControllingUserPort,
                                           rwEnumResults[iIndex]->wszPathName,
                                           (PBYTE)szBuffer,
                                           dwBufferSize );

    // Convert to a wide string
    if( SUCCEEDED( hrDownloaded ) )
    {
        _snwprintf( m_wszMessageOfTheDay, MAX_MESSAGE_LENGTH,
                    L"%S\0", szBuffer );
    }
    
    // Return our success
    return SUCCEEDED( hrDownloaded );
}

//-------------------------------------------------------------------------------------
// Name: GetUIColors()
// Desc: Changes the color of the UI elements based on the colors
//       specified by the publisher in the global storage area.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::GetUIColors( PXONLINESTORAGE_FILE_INFO* rwEnumResults,
                               DWORD dwNumResults )
{
    // Find the index of the filename in our enumeration results
    INT iIndex = GetFileIndex( FILENAME_COLORS,
                               rwEnumResults,
                               dwNumResults );

    // Bail if the file was not found
    if( iIndex < 0 )
        return FALSE;


    // Create a buffer to store the download into
    BYTE  rwBuffer[MIN_XONLINE_DOWNLOAD_BUFFER_SIZE] = { 0 };
    DWORD dwBufferSize = MIN_XONLINE_DOWNLOAD_BUFFER_SIZE;

    // Download the colors into our buffer
    HRESULT hrDownloaded = DownloadGlobal( m_dwControllingUserPort,
                                           rwEnumResults[iIndex]->wszPathName,
                                           rwBuffer,
                                           dwBufferSize );

    // If the download was successful, extract and set the UI colors
    if( SUCCEEDED( hrDownloaded ) )
        sscanf( (CHAR*)rwBuffer, "%d%d%d", &m_dwBGColor, &m_dwTextColor, &m_dwHighlightColor );
    
    return SUCCEEDED( hrDownloaded );
}

//-------------------------------------------------------------------------------------
// Name: GetMessageIcon()
// Desc: Gets the message icon from the global publisher storage area
//       to show next to the MOTD.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::GetMessageIcon( PXONLINESTORAGE_FILE_INFO* rwEnumResults,
                                  DWORD dwNumResults )
{
    // Find the index of the file in the enumeration results
    INT iIndex = GetFileIndex( FILENAME_MOTD_ICON,
                               rwEnumResults,
                               dwNumResults );

    // Bail if the file was not found
    if( iIndex < 0 )
        return FALSE;


    // Create a buffer to download the icon into
    const INT BUFFER_SIZE  = CUserContent::DATA_SIZE + ( 2 * LIVE_SIG_BUFFER_SIZE );
    DWORD     dwBufferSize = BUFFER_SIZE;
    BYTE      rwBuffer[BUFFER_SIZE] = { 0 };

    // The MOTD icon that is on the server was copied
    // from a save file. This means it has "extra" data -
    // the Savegame signature, in front of it
    HRESULT hrDownloaded = DownloadGlobal( m_dwControllingUserPort,
                                           rwEnumResults[iIndex]->wszPathName,
                                           rwBuffer,
                                           dwBufferSize );

    // If the download was successful, place
    // the data into the icon
    if( SUCCEEDED( hrDownloaded ) )
    {
        assert( dwBufferSize <= BUFFER_SIZE );

        // This will copy the icon data into the icon properly
        //
        // NOTE: The icon format is
        //  Save signature size
        //  Save signature
        //  Data
        //  Live Signature
        memcpy( &m_motdIcon, rwBuffer + 4 + SAVE_SIG_BUFFER_SIZE, CUserContent::DATA_SIZE );


        // Create a texture if we have not already
        if( !m_pMOTDTexture )
        {
            m_pMOTDTexture = m_motdIcon.CreateTexture( m_pd3dDevice );
            assert( m_pMOTDTexture );
        }

        // Draw the new icon to the texture
        m_motdIcon.UpdateTexture( m_pMOTDTexture );
    }

    return SUCCEEDED( hrDownloaded );
}


//-------------------------------------------------------------------------------------
// Name: EnterStateMain()
// Desc: Initialises the menu varaibles and does sanity checks so the player
//       may select their action
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateMain()
{
    m_iItemSelected = 0;

    if( m_bUserSignedIn && m_bFirstInit )
    {
        PXONLINESTORAGE_FILE_INFO* rwEnumResults = NULL;
        DWORD                      dwNumResults  = 0;
        BOOL                       bEnumerated   =  FALSE;
        
        bEnumerated = EnumerateGlobalStorage( rwEnumResults, dwNumResults );
        
        // If we were able to enumerate the content available then
        // download the global storage content into memory
        if( bEnumerated )
        {
            // retrieve message of the day
            GetMessageOfTheDay( rwEnumResults, dwNumResults );
            GetUIColors( rwEnumResults, dwNumResults );
            GetMessageIcon( rwEnumResults, dwNumResults );

            m_bFirstInit = FALSE;
        }
    }
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateMain
// Desc: Main menu
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateMain( Event event )
{
    m_iItemSelected = GetMenuPosition( m_iItemSelected, NUM_ITEMS_MAIN_MENU, event);

    switch( event )
    {
    case EV_BUTTON_A:
        assert( m_iItemSelected >= 0 );
        assert( m_iItemSelected < NUM_ITEMS_MAIN_MENU );

        switch( m_iItemSelected )
        {
        case MENU_MAIN_CONTENT_MANAGEMENT:
            PushState( STATE_CONTENT_MANAGEMENT );
            break;
            
        case MENU_MAIN_USER_SETTINGS:
            if( m_bUserSignedIn )
            {
                // Edit the stored user settings
                PushState( STATE_SETTINGS_EDIT );
            }
            else
            {
                PushMessageWindow( "Please logon to Xbox Live." );
            }

            break;

        case MENU_MAIN_TEAMS:
            if( !m_bUserSignedIn )
            {
                PushMessageWindow( "Please logon to Xbox Live." );
                break;
            }

            // Attempt to view a list of
            // teams the user is a member of
            if( GetTeamList() )
            {
                PushState( STATE_VIEW_MY_TEAMS );
            }
            else
            {
                PushMessageWindow( "Unable to retrieve team list" );
            }

            break;

        case MENU_MAIN_RECENT_PLAYERS:
            if( !m_bUserSignedIn )
            {
                PushMessageWindow( "Please logon to Xbox Live." );
                break;
            }
            
            // Attempt to view a list of
            // teams the user is a member of
            if( GetTeamList() )
            {
                PushState( STATE_RECENT_PLAYERS );
            }
            else
            {
                PushMessageWindow( "Unable to retrieve team list" );
            }

            break;
        }

        break;

    case EV_BUTTON_B:
        // Back out and login with a different account
        m_userContent.Clear();

        PopState( TRUE );

        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateMain
// Desc: Shows the menu of options the player has.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateMain()
{
    RenderControllingUser();

    RenderMenu( L"MAIN MENU",
                (const WCHAR**)MENU_MAIN, NUM_ITEMS_MAIN_MENU,
                m_iItemSelected );

    // Render Message of the day
    const FLOAT MESSAGE_OF_THE_DAY_Y_OFFSET = 0.75f * SCREEN_SIZE_Y;
    const FLOAT MESSAGE_OF_THE_DAY_X_OFFSET = SCREEN_CENTER_X;

    if( m_bUserSignedIn )
    {
        // Write the MOTD text
        m_font.DrawText( MESSAGE_OF_THE_DAY_X_OFFSET , MESSAGE_OF_THE_DAY_Y_OFFSET ,
                        m_dwTextColor , m_wszMessageOfTheDay , XBFONT_CENTER_X );

        // Render the MOTD icon
        if( m_pMOTDTexture )
        {
            SetFacePos( m_pLogoVerts,
                        MESSAGE_OF_THE_DAY_X_OFFSET,
                        MESSAGE_OF_THE_DAY_Y_OFFSET + ICON_SIZE );

            RenderSprite( m_pLogoVerts, m_pMOTDTexture );
        }
    }

    // Bottom Help text
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

/////////////////////////
// State RecentPlayers //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: RenderControllingUser()
// Desc: Renders the name of the user who has control of the UI in the UL corner
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderControllingUser()
{
    if( !m_bUserSignedIn )
        return;

    assert( ( m_dwControllingUserPort >= 0 ) && "Invalid controlling user" );
    assert( ( m_dwControllingUserPort < MAX_USERS ) && "Invalid controlling user" );

    WCHAR strUserName[XONLINE_GAMERTAG_SIZE] = { 0 };
    XBUtil_GetWide( m_rwStoredUsers[m_wCurUserIndex].szGamertag,
                    strUserName, XONLINE_GAMERTAG_SIZE );

    // Render a header giving the user the name of the demo
    m_font.DrawText( POS_HEADER_RIGHT, POS_HEADER_Y, m_dwTextColor,
                     strUserName, // The user who has control of the menu
                     XBFONT_RIGHT );
}


//-------------------------------------------------------------------------------------
// Name: EnterStateRecentPlayers()
// Desc: Initialize data used by the RecentPlayers state.
//       Builds a list of players that the user can send team invites to.
//       The list is actaully just the other Xbox Live accounts on the HD
//       to simulate the standard "Recent Players" list
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateRecentPlayers()
{
    m_dwPlayerSelected    = 0;
    m_dwPlayerRenderStart = 0;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateRecentPlayers()
// Desc: Takes the user input to scroll through the list of players
//       the user can send team invites to
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateRecentPlayers( Event event )
{
    switch( event )
    {
    case EV_UP: // Move the roster list up until we hit the top
        m_dwPlayerSelected = ( m_dwPlayerSelected > 0 ) ? 
                             ( m_dwPlayerSelected - 1 ) : 0;

        if( m_dwPlayerSelected < m_dwPlayerRenderStart )
            m_dwPlayerRenderStart  = ( m_dwPlayerRenderStart > 0 ) ? 
                                     ( m_dwPlayerRenderStart - 1 ) : 0;

        break;

    case EV_DOWN: // Move the roster list down until we hit the bottom
        m_dwPlayerSelected = ( m_dwPlayerSelected < ( m_dwNumStoredUsers - 1 ) ) ? 
                             ( m_dwPlayerSelected + 1 ) : m_dwPlayerSelected;
        
        if( m_dwPlayerSelected >= ( m_dwPlayerRenderStart + 
                                    NUM_ENTRIES_PER_SCREEN ) )
        {
            ++m_dwPlayerRenderStart;

            if( m_dwPlayerRenderStart > ( m_dwNumStoredUsers - 
                                          NUM_ENTRIES_PER_SCREEN ) )
                m_dwPlayerRenderStart = m_dwNumStoredUsers - 
                                        NUM_ENTRIES_PER_SCREEN;
        }
        break;

    case EV_BUTTON_A: // Select the team to send an invite from
        if ( m_wCurUserIndex != m_dwPlayerSelected )
        {
            // attempt to view selected player's content
            if( ViewUserSave( m_rwStoredUsers[m_dwPlayerSelected].xuid.qwUserID,
                            m_userContent ) )
            {
                PushState( STATE_VIEW_TEAMMATE_ICON );
            }
            else
            {
                PushMessageWindow( "User has not shared content" );
            }
        }

        break;

    case EV_BUTTON_B:
        PopState( TRUE );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateRecentPlayers()
// Desc: Draws the list of players the user can send invites to.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateRecentPlayers()
{
    RenderControllingUser();

    RenderMenu( L"RECENT PLAYERS", NULL, 0, 0 );

    FLOAT fPlayerListStartY = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2 );
    FLOAT fPlayerListStartX = SCREEN_CENTER_X * 0.75f;

    // If we are starting the render from a position
    // other than the first user in the list
    // the draw a little arrow on the side telling
    // the user they can scroll up
    if( m_dwPlayerRenderStart > 0 )
    {
        m_font.DrawText( fPlayerListStartX, fPlayerListStartY,
                         m_dwHighlightColor, GLYPH_UP_ARROW L"    ", XBFONT_RIGHT );
    }

    // Get the details of each team member
    for( DWORD i = m_dwPlayerRenderStart; i < m_dwNumStoredUsers; ++i )
    {
        // Stop rendering if we hit the maximum number
        // team members viewable at once
        if( ( i - m_dwPlayerRenderStart ) >= NUM_ENTRIES_PER_SCREEN )
        {
            // If more team roster entries are below
            // the last entry drawn, then add a down
            // arrow on the side telling the user
            // can scroll down
            FLOAT fDownArrowY = fPlayerListStartY
                                + ( DEFAULT_TEXT_PADDING
                                    * ( NUM_ENTRIES_PER_SCREEN - 1 ) );

            m_font.DrawText( fPlayerListStartX, fDownArrowY,
                             m_dwHighlightColor, GLYPH_DOWN_ARROW L"   ", XBFONT_RIGHT );

            break;
        }

        // Render to the screen!
        INT   iScreenItem = ( i - m_dwPlayerRenderStart );
        FLOAT fPosY       = fPlayerListStartY + ( DEFAULT_TEXT_PADDING * iScreenItem );

        // Allow the user to move the selector
        // up and down to select a specific user
        // to give an permissions to or
        // to remove from the team
        //
        // Show selected item with a little triangle
        FLOAT fIconPosY = fPlayerListStartY
                          + ( DEFAULT_TEXT_PADDING
                              * ( m_dwPlayerSelected - m_dwPlayerRenderStart ) );

        m_font.DrawText( fPlayerListStartX, fIconPosY,
                         COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_RIGHT );


        WCHAR szwGamerTag[XONLINE_GAMERTAG_SIZE] = { 0 };

        XBUtil_GetWide( m_rwStoredUsers[i].szGamertag,
                        szwGamerTag,
                        XONLINE_GAMERTAG_SIZE );

        // Show that we can not select ourselves
        DWORD dwColor = ( m_wCurUserIndex == i )
                          ? COLOR_GREY : m_dwTextColor;

        m_font.DrawText( fPlayerListStartX, fPosY, dwColor,
                         szwGamerTag,
                         XBFONT_LEFT );

    }

    // Tell the user what the purpose of this screen is
    m_font.DrawText( SCREEN_CENTER_X, ( POS_FOOTER_Y - DEFAULT_TEXT_PADDING ),
                     m_dwHighlightColor,
                     L"SELECT A USER TO VIEW SHARED CONTENT", XBFONT_CENTER_X );

    // Bottom Help text
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

////////////////////////////
// State ContentManagement //
////////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateContentManagement()
// Desc: Initializes the content management menu.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateContentManagement()
{
    m_iItemSelected = 0;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateContentManagement()
// Desc: Allows the user to choose to
//       1) Create new content
//       2) Save existing content to the HD
//       3) Download existing content
//       4) Upload existing content
//       5) Load existing content
//       6) Edit content that has been loaded into memory
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateContentManagement( Event event )
{
    // Temp buffer if we need to create a new filename
    WCHAR wszRandName[XONLINE_GAMERTAG_SIZE] = { 0 };

    m_iItemSelected = GetMenuPosition( m_iItemSelected,
                                       NUM_ITEMS_CONTENT_MANAGEMENT_MENU,
                                       event );

    switch( event )
    {
    case EV_BUTTON_A:
        switch( m_iItemSelected )
        {
        case MENU_CONTENT_MANAGEMENT_CREATE_CONTENT:
            // Create new content that can be saved
            m_userContent.Clear();
            m_userContent.SetDead( !m_bUserSignedIn );

            // Generate a random name for the content
            m_bUploadInsteadOfSave = FALSE;
            m_bTeamLogo            = FALSE;

            ZeroMemory( m_swzBaseFilename, sizeof( m_swzBaseFilename ) );
            XBRandName_GetRandomName(wszRandName, XONLINE_GAMERTAG_SIZE );

            // Create our filename string
            if( m_bUserSignedIn )
            {
                // Prefix with the Xbox Live account name if signed in
                _snwprintf( m_swzBaseFilename, MAX_GAMENAME, L"%S-%s\0",
                            m_rwStoredUsers[m_wCurUserIndex].szGamertag, wszRandName );
            }
            else
            {
                // Just use a random name if not logged on
                _snwprintf( m_swzBaseFilename, MAX_GAMENAME, L"%s\0",
                            wszRandName );
            }

            PushState( STATE_CONTENT_EDIT );


            if( !m_bUserSignedIn )
                PushMessageWindow( "Any content created can not be shared on Xbox Live" );

            break;

        case MENU_CONTENT_MANAGEMENT_LIST_LOCAL_CONTENT:
            // Select file to load from list of available content
            m_eContentAction = STATE_LOCAL_CONTENT_OPTIONS;
            PushState( STATE_LIST_SAVED_CONTENT );
            break;
        }

        break;

    case EV_BUTTON_B:
        PopState( TRUE );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateContentManagement()
// Desc: Renders the menu and cursor next to the selected option
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateContentManagement()
{
    RenderControllingUser();

    RenderMenu( L"CONTENT MANAGEMENT",
                (const WCHAR**)MENU_CONTENT_MANAGEMENT,
                NUM_ITEMS_CONTENT_MANAGEMENT_MENU,
                m_iItemSelected );

    // Bottom Help text
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}


////////////////////////////
// State ListSavedContent //
////////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateListSavedContent()
// Desc: Enumerates the saved content on the Xbox so the player may select an
//       individual save to load into memory.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateListSavedContent()
{
    m_dwNumContentSaves = 0;
    m_dwSaveSelected    = 0;
    m_dwSaveRenderStart = 0;

    ZeroMemory( m_savedContentData, sizeof( m_savedContentData ) );


    // Step 1
    //
    // Find the first saved content

    HANDLE hFindSaves = XFindFirstSaveGame(
                            SAVE_DRIVE,            // Root drive
                            &m_savedContentData[0] // Data about first saved game
                        );

    if( hFindSaves == INVALID_HANDLE_VALUE )
    {
        // No saves, so bail
        return;
    }


    // Step 2
    //
    // Find all the saves until we reach our limit or no more exist

    BOOL bFoundSave     = TRUE;

    for( m_dwNumContentSaves = 1;
         bFoundSave && ( m_dwNumContentSaves < MAX_SAVES );
         ++m_dwNumContentSaves )
    {
        DWORD dwSaveIndex = m_dwNumContentSaves;

        bFoundSave = XFindNextSaveGame(
                        hFindSaves,                      // Handle of search
                        &m_savedContentData[dwSaveIndex] // Data to populate
                     );

        m_dwNumContentSaves = bFoundSave ? m_dwNumContentSaves : ( m_dwNumContentSaves - 1);
    }

    // Step
    //
    // Close the handle to the save search

    XFindClose( hFindSaves );
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateListSavedContent()
// Desc: Allows the user to scroll through all the saved content on the
//       Xbox to pick an individual save to load.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateListSavedContent( Event event )
{
    // Controll the selected user and the scrolling mechanism
    switch( event )
    {
    case EV_UP: // Move the roster list up until we hit the top
        m_dwSaveSelected = ( m_dwSaveSelected > 0 ) ? ( m_dwSaveSelected - 1 ) : 0;

        if( m_dwSaveSelected < m_dwSaveRenderStart )
            m_dwSaveRenderStart  = ( m_dwSaveRenderStart > 0 ) ? ( m_dwSaveRenderStart - 1 ) : 0;

        break;

    case EV_DOWN: // Move the roster list down until we hit the bottom
        m_dwSaveSelected = ( m_dwSaveSelected < ( m_dwNumContentSaves - 1 ) ) ? ( m_dwSaveSelected + 1 ) : m_dwSaveSelected;
        
        if( m_dwSaveSelected >= ( m_dwSaveRenderStart + NUM_ENTRIES_PER_SCREEN ) )
        {
            ++m_dwSaveRenderStart;

            if( m_dwSaveRenderStart > ( m_dwNumContentSaves - NUM_ENTRIES_PER_SCREEN ) )
                m_dwSaveRenderStart = m_dwNumContentSaves - NUM_ENTRIES_PER_SCREEN;
        }
        break;
    }

    switch( event )
    {
    case EV_BUTTON_A:
        // Attempt to load the saved game selected.
        if( m_dwSaveSelected < m_dwNumContentSaves )
            PushState( m_eContentAction );

        break;

    case EV_BUTTON_B:
        PopState( TRUE );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateListSavedContent()
// Desc: Renders the list of saves on the Xbox.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateListSavedContent()
{
    RenderControllingUser();

    RenderMenu( L"LOCAL SAVED CONTENT", NULL, 0, 0 );

    FLOAT fSaveIconStartX = SCREEN_CENTER_X * 0.5f;

    // If we are starting the render from a position
    // other than the first user in the list
    // the draw a little arrow on the side telling
    // the user they can scroll up
    if( m_dwSaveRenderStart > 0 )
    {
        m_font.DrawText( fSaveIconStartX, POS_ACCOUNT_LIST_START,

            m_dwHighlightColor, GLYPH_UP_ARROW L"    ", XBFONT_RIGHT );
    }

    // Render each entry's name
    for( INT i = m_dwSaveRenderStart; i < (INT)m_dwNumContentSaves; ++i )
    {
        DWORD dwColor = 
            ( m_dwSaveSelected == (DWORD)i ) ? m_dwHighlightColor : m_dwTextColor;

        // Stop rendering if we hit the maximum number
        // team members viewable at once
        if( ( i - m_dwSaveRenderStart ) >= NUM_ENTRIES_PER_SCREEN )
        {
            // If more team roster entries are below
            // the last entry drawn, then add a down
            // arrow on the side telling the user
            // can scroll down
            FLOAT fDownArrowY = POS_ACCOUNT_LIST_START
                                + ( DEFAULT_TEXT_PADDING * ( NUM_ENTRIES_PER_SCREEN - 1 ) );

            m_font.DrawText( fSaveIconStartX, fDownArrowY,
                             m_dwHighlightColor, GLYPH_DOWN_ARROW L"   ", XBFONT_RIGHT );

            break;
        }

        m_font.DrawText( fSaveIconStartX,
                         POS_ACCOUNT_LIST_START + ( DEFAULT_TEXT_PADDING * ( i - m_dwSaveRenderStart ) ),
                         dwColor,
                         m_savedContentData[i].szSaveGameName,
                         XBFONT_LEFT );

        if ( (DWORD)i == m_dwSaveSelected )
        {
            m_font.DrawText( fSaveIconStartX, POS_ACCOUNT_LIST_START + 
                             ( DEFAULT_TEXT_PADDING * ( m_dwSaveSelected - m_dwSaveRenderStart )),
                             COLOR_POINTER,
                             GLYPH_RIGHT_TICK, XBFONT_RIGHT );
        }
    }

    // Render only the cancel button in the footer
    // if no saves exist
    RenderFooter( FOOTER_RENDER_CANCEL );

    if( m_dwNumContentSaves > 0 )
        RenderFooter( FOOTER_RENDER_SELECT );
}


///////////////////////
// State ViewMyTeams //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateViewMyTeams()
// Desc: Initializes the MyTeams screen after the team list has been read
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateViewMyTeams()
{
    m_iTeamSelected = 0;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateViewMyTeams
// Desc: Updates the ViewMyTeams state. Launches any submenus and
//       commands the user may have. Allows the user to select a
//       team and view it's roster.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateViewMyTeams( Event event)
{

    // Download team logo icons into the background

    if( m_dwTeamLogoToDL < m_dwTeamCount )
    {
        assert( m_ppTeamLogoTextures );

        CUserContent dlBuffer;

        // Attempt to DL their buddy icon
        // If the DL fails then it probably
        // is due to the lack of available content
        if( SUCCEEDED( dlBuffer.Download( m_dwControllingUserPort,
                                          0, // Get the shared team data
                                          m_rwTeamXUIDS[m_dwTeamLogoToDL].qwTeamID,
                                          TEAM_LOGO_FILENAME ) ) )
        {
            // If we already have a texture created, then there is
            // no reason to make the GPU shuffle resources
            if ( m_ppTeamLogoTextures[m_dwTeamLogoToDL] != NULL )
            {
                dlBuffer.UpdateTexture( m_ppTeamLogoTextures[m_dwTeamLogoToDL] );
            }
            else
            {
                m_ppTeamLogoTextures[m_dwTeamLogoToDL] = dlBuffer.CreateTexture( m_pd3dDevice );
            }
        }

        ++m_dwTeamLogoToDL;
    }

    
    // Update the user menu selection and act upon
    // any input if there are any teams to view

    if( m_dwTeamCount > 0 )
        m_iTeamSelected = GetMenuPosition( m_iTeamSelected, m_dwTeamCount, event);
    else
        m_iTeamSelected = 0;

    switch( event )
    {
    case EV_BUTTON_A:
        // Try to view the roster of the team selected

        if( m_dwTeamCount < 1 )
            break;

        if( GetTeamRoster() )
            PushState( STATE_VIEW_TEAM_ROSTER );
        else
            PushMessageWindow( "Unable to retrieve team roster." );

        break;

    case EV_BUTTON_Y:
        if( m_dwTeamCount > 0 )
        {
            // Try to edit the team logo icon
            m_userContent.Clear();

            // Force an update to the icon when we return from editing
            m_dwTeamLogoToDL = 0;

            m_bUploadInsteadOfSave = TRUE;
            m_bTeamLogo            = TRUE;
            PushState( STATE_CONTENT_EDIT );

            m_wszFilename = TEAM_LOGO_FILENAME;

            HRESULT hrDownload = m_userContent.Download( m_dwControllingUserPort,
                                                        0, // Get the shared team logo 
                                                        m_rwTeamXUIDS[m_iTeamSelected].qwTeamID,
                                                        TEAM_LOGO_FILENAME );

            // Attempt to download the existing team logo
            if( FAILED( hrDownload ) && ( hrDownload != XONLINE_E_STORAGE_FILE_NOT_FOUND ))
            {
                PushMessageWindow( "Unable to retrieve previous version" );
            }
        }
        break;

    case EV_BUTTON_B:
        // Return to the previous menu
        PopState( TRUE );

        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateViewMyTeams()
// Desc: Renders the list of teams the user is a member of and the
//       input options available.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateViewMyTeams()
{
    RenderControllingUser();

    RenderMenu( L"MY TEAMS", NULL, 0, 0 );

    FLOAT fTeamListStartY   = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2 );
    FLOAT fTeamIconStartX   = 80.0f;
    FLOAT fTeamNameStartX   = 120.0f;
    FLOAT fTeamDescStartX   = 560.0f;
    FLOAT fTeamNamePaddingY = ICON_SIZE * 1.1f;

    m_font.DrawText( fTeamNameStartX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"TEAM NAME",
                     XBFONT_LEFT );

    m_font.DrawText( fTeamDescStartX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"TEAM DESCRIPTION",
                     XBFONT_RIGHT );


    for( INT i = 0; i < (INT)m_dwTeamCount; ++i)
    {
        FLOAT fPosY = fTeamListStartY + ( fTeamNamePaddingY * i );

        // Render the little buddy icon next to their name
        assert( m_ppTeamLogoTextures );

        if( m_ppTeamLogoTextures[i] )
        {
            SetFacePos( m_pLogoVerts, fTeamIconStartX, fPosY );
            RenderSprite( m_pLogoVerts, m_ppTeamLogoTextures[i] );
        }

        // Draw the name of the team
        m_font.DrawText( fTeamNameStartX, fPosY, m_dwTextColor,
                         m_rwTeamInfo[i].TeamProperties.wszTeamName,
                         XBFONT_LEFT );

        // Draw the team discription
        m_font.DrawText( fTeamDescStartX, fPosY, m_dwTextColor,
                         m_rwTeamInfo[i].TeamProperties.wszDescription,
                         XBFONT_RIGHT );
    }

    // If we have one or more teams
    // then allow the user to find the
    // roster of the selected team
    if( m_iTeamSelected < (INT)m_dwTeamCount )
    {
        // Show selected item with a little triangle
        FLOAT fIconPosY = fTeamListStartY + ( fTeamNamePaddingY * m_iTeamSelected );

        m_font.DrawText( fTeamIconStartX, fIconPosY,
                         COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    }

    // Tell the player that they can change the team icon
    if( m_dwTeamCount > 0 )
    {
        m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y,
                        m_dwTextColor, GLYPH_Y_BUTTON L" Edit Team Icon",
                        XBFONT_CENTER_X );
    }


    // Bottom Help text

    // Show the correct footer options if
    // we have teams to view and edit
    // or if we are note a member of any team
    WORD wFooterFlags = FOOTER_RENDER_CANCEL;

    if( m_dwTeamCount > 0 )
    {
        wFooterFlags = (WORD)( wFooterFlags | FOOTER_RENDER_SELECT );
    }

    RenderFooter( wFooterFlags );
}


//////////////////////////
// State ViewTeamRoster //
//////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateViewTeamRoster()
// Desc: Allows the user to issue commands to the roster (delete team, kick members)
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateViewTeamRoster( Event event)
{
    // Download an icon a tick
    if( m_dwTeamMemberTextureToDL < m_dwTeamMemberCount )
    {
        assert( m_ppTeammateTextures );

        CUserContent dlBuffer;

        // Attempt to DL their buddy icon
        // If the DL fails then it probably
        // is due to the lack of available content
        if( SUCCEEDED( dlBuffer.Download( m_dwControllingUserPort,
                                          m_rwTeamMembers[m_dwTeamMemberTextureToDL].qwUserID,
                                          m_rwTeamXUIDS[m_iTeamSelected].qwTeamID ) ) )
        {
            // If we already have a texture created, then there is
            // no reason to make the GPU shuffle resources
            if ( m_ppTeammateTextures[m_dwTeamMemberTextureToDL] != NULL )
            {
                dlBuffer.UpdateTexture( m_ppTeammateTextures[m_dwTeamMemberTextureToDL] );
            }
            else
            {
                m_ppTeammateTextures[m_dwTeamMemberTextureToDL] = dlBuffer.CreateTexture( m_pd3dDevice );
            }
        }

        ++m_dwTeamMemberTextureToDL;
    }

    switch( event )
    {
    case EV_UP: // Move the roster list up until we hit the top
        m_dwTeamMemberSelected = ( m_dwTeamMemberSelected > 0 ) ? ( m_dwTeamMemberSelected - 1 ) : 0;

        if( m_dwTeamMemberSelected < m_dwRosterRenderStart )
            m_dwRosterRenderStart  = ( m_dwRosterRenderStart > 0 ) ? ( m_dwRosterRenderStart - 1 ) : 0;

        break;

    case EV_DOWN: // Move the roster list down until we hit the bottom
        m_dwTeamMemberSelected = ( m_dwTeamMemberSelected < ( m_dwTeamMemberCount - 1 ) ) ? ( m_dwTeamMemberSelected + 1 ) : m_dwTeamMemberSelected;
        
        if( m_dwTeamMemberSelected >= ( m_dwRosterRenderStart + NUM_ENTRIES_PER_SCREEN ) )
        {
            ++m_dwRosterRenderStart;

            if( m_dwRosterRenderStart > ( m_dwTeamMemberCount - NUM_ENTRIES_PER_SCREEN ) )
                m_dwRosterRenderStart = m_dwTeamMemberCount - NUM_ENTRIES_PER_SCREEN;
        }
        break;

    case EV_BUTTON_A:
        if( m_rwTeamMembers[m_dwTeamMemberSelected].qwUserID
            != m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID )
        {
            HRESULT hrDownload = m_userContent.Download( m_dwControllingUserPort,
                                                         m_rwTeamMembers[m_dwTeamMemberSelected].qwUserID,
                                                         m_rwTeamXUIDS[m_iTeamSelected].qwTeamID );
            if( SUCCEEDED( hrDownload ) )
            {
                PushState( STATE_VIEW_TEAMMATE_ICON );
            }
            else
            {
                PushMessageWindow( "Teammate does not have any special content" );
            }
        }
        else
        {
            m_userContent.Clear();

            // Force an update to the icon when we return from editing
            m_dwTeamMemberTextureToDL = 0;

            m_bUploadInsteadOfSave = TRUE;
            m_bTeamLogo            = FALSE;
            PushState( STATE_CONTENT_EDIT );


            HRESULT hrDownload =  m_userContent.Download( m_dwControllingUserPort,
                                                          m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID,
                                                          m_rwTeamXUIDS[m_iTeamSelected].qwTeamID );

            if( FAILED( hrDownload ) && ( hrDownload != XONLINE_E_STORAGE_FILE_NOT_FOUND ) )
            {
                PushMessageWindow( "Unable to retrieve previous version" );
            }
        }
        break;

    case EV_BUTTON_B:
        m_phTeamRosterTask.Close();
        GetTeamList();
        PopState( TRUE );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateViewTeamRoster()
// Desc: Renders the team roster to the screen.
//       Calls the function to get team member details to show specific details
//       about each member.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateViewTeamRoster()
{
    RenderControllingUser();

    RenderMenu( L"TEAM ROSTER", NULL, 0, 0 );

    FLOAT fTeamListStartY     = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2.0f );
    FLOAT fTeamIconStartX     = 80.0f;
    FLOAT fTeammateNameStartX = 120.0f;
    FLOAT fTeamDescStartX     = 560.0f;
    FLOAT fRosterPaddingY     = ICON_SIZE * 1.25f;


    m_font.DrawText( fTeammateNameStartX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"MEMBER NAME",
                     XBFONT_LEFT );

    m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"RANK",
                     XBFONT_LEFT );

    m_font.DrawText( fTeamDescStartX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"JOIN DATE",
                     XBFONT_RIGHT );


    XONLINE_TEAM_MEMBER memberInfo;
    HRESULT             hrMemberDetail = S_OK;


    // If we are starting the render from a position
    // other than the first user in the list
    // the draw a little arrow on the side telling
    // the user they can scroll up
    if( m_dwRosterRenderStart > 0 )
    {
        m_font.DrawText( fTeamIconStartX, fTeamListStartY,
                         m_dwHighlightColor, GLYPH_UP_ARROW L"    ", XBFONT_RIGHT );
    }


    // Get the details of each team member
    for( DWORD i = m_dwRosterRenderStart; i < m_dwTeamMemberCount; ++i )
    {
        // Stop rendering if we hit the maximum number
        // team members viewable at once
        if( ( i - m_dwRosterRenderStart ) >= NUM_ENTRIES_PER_SCREEN )
        {
            // If more team roster entries are below
            // the last entry drawn, then add a down
            // arrow on the side telling the user
            // can scroll down
            FLOAT fDownArrowY = fTeamListStartY
                                + ( fRosterPaddingY * ( NUM_ENTRIES_PER_SCREEN - 1 ) );

            m_font.DrawText( fTeamIconStartX, fDownArrowY,
                             m_dwHighlightColor, GLYPH_DOWN_ARROW L"   ", XBFONT_RIGHT );

            break;
        }

        hrMemberDetail = XOnlineTeamMemberGetDetails(
                            m_phTeamRosterTask, // The task used to retrieve the team roster
                            m_rwTeamMembers[i], // The XUID of the member to retrieve
                            &memberInfo         // The structure to populate with details
                         );


        // Bail if we fail
        if( hrMemberDetail != S_OK )
            break;


        // Render to the screen!
        INT   iScreenItem = ( i - m_dwRosterRenderStart );
        FLOAT fPosY       = fTeamListStartY + ( fRosterPaddingY * iScreenItem );

        // Allow the user to move the selector
        // up and down to select a specific user
        // to give an permissions to or
        // to remove from the team
        //
        // Show selected item with a little triangle

        FLOAT fIconPosY = fTeamListStartY + ( fRosterPaddingY * ( m_dwTeamMemberSelected - m_dwRosterRenderStart ) );

        m_font.DrawText( fTeamIconStartX, fIconPosY,
                         COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_RIGHT );

        // Render the little buddy icon next to their name
        assert( m_ppTeammateTextures );

        if( m_ppTeammateTextures[i] )
        {
            SetFacePos( m_pLogoVerts, fTeamIconStartX, fPosY );
            RenderSprite( m_pLogoVerts, m_ppTeammateTextures[i] );
        }

        DWORD dwColor = ( m_rwTeamMembers[i].qwUserID == m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID )
                        ? m_dwHighlightColor : m_dwTextColor;

        WCHAR szwGamerTag[XONLINE_GAMERTAG_SIZE] = { 0 };

        XBUtil_GetWide( memberInfo.szGamertag, szwGamerTag, XONLINE_GAMERTAG_SIZE );

        m_font.DrawText( fTeammateNameStartX, fPosY, dwColor,
                         szwGamerTag,
                         XBFONT_LEFT );

        // create a string representing their privilege level
        if( memberInfo.TeamMemberProperties.dwPrivileges & XONLINE_TEAM_DELETE_MEMBER )
            wcscpy( szwGamerTag, L"Owner" );
        else if( memberInfo.TeamMemberProperties.dwPrivileges & XONLINE_TEAM_RECRUIT_MEMBERS )
            wcscpy( szwGamerTag, L"Recruiter" );
        else 
            wcscpy( szwGamerTag, L"Peon" );

        m_font.DrawText( SCREEN_CENTER_X, fPosY, dwColor,
                     szwGamerTag,
                     XBFONT_LEFT );

        // Show the date of when the member joined
        SYSTEMTIME systemTime;

        FileTimeToSystemTime( &memberInfo.JoinDate, &systemTime );

        _snwprintf( szwGamerTag, XONLINE_GAMERTAG_SIZE, L"%d/%d/%d\0",
            systemTime.wMonth, systemTime.wDay, systemTime.wYear );

        m_font.DrawText( fTeamDescStartX, fPosY, dwColor,
                         szwGamerTag,
                         XBFONT_RIGHT );
    }


    // Tell the player that they can change their buddy icon
    // or that they can view a  teammates icon depending
    // if they have themselves or a teammate selected
    if( m_rwTeamMembers[m_dwTeamMemberSelected].qwUserID
        == m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID )
    {
        m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                         m_dwTextColor, GLYPH_A_BUTTON L" Edit My Buddy Icon", 
                         XBFONT_RIGHT );
    }
    else
    {
        m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                         m_dwTextColor, GLYPH_A_BUTTON L" View Teammate Buddy Icon", 
                         XBFONT_RIGHT );
    }


    // Bottom Help text
    RenderFooter( FOOTER_RENDER_CANCEL );
}

///////////////////////////////
// State LocalContentOptions //
///////////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateLocalContentOptions()
// Desc: Intializes the menu for local content options
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateLocalContentOptions()
{
    m_iItemSelected = 0;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateLocalContentOptions()
// Desc: Allows the user to choose what they want to do to local content
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateLocalContentOptions( Event event )
{
    m_iItemSelected = GetMenuPosition( m_iItemSelected,
                                       NUM_ITEMS_LOCAL_CONTENT_OPTIONS_MENU,
                                       event );

    switch( event )
    {
    case EV_BUTTON_A:
        switch( m_iItemSelected )
        {
        case MENU_LOCAL_CONTENT_OPTIONS_EDIT:
            // Set aside the name of the file so
            // the content editor knows what save to
            // call it
            lstrcpynW( m_swzBaseFilename,
                        m_savedContentData[m_dwSaveSelected].szSaveGameName,
                        MAX_GAMENAME );

            // Attempt to load the content and edit it
            if( !m_userContent.Load( m_bUserSignedIn,
                                     m_savedContentData[m_dwSaveSelected].szSaveGameDirectory,
                                     m_savedContentData[m_dwSaveSelected].szSaveGameName ) )
            {
                // Unable to load the normal save,
                // content goes completely dead
                PushMessageWindow( "Unable to load content" );
            }
            else if( m_userContent.IsDead() )
            {
                // If the content is flagged as dead, it can not be
                // uploaded to Xbox Live, crippling it as usable only offline.
                // We can still save a local copy.

                m_bUploadInsteadOfSave = FALSE;
                PushState( STATE_CONTENT_EDIT );
                PushMessageWindow( "Content can not be shared on Xbox Live." );
            }
            else if( !m_bUserSignedIn )
            {
                m_bUploadInsteadOfSave = FALSE;
                
                m_userContent.SetDead( TRUE );

                PushState( STATE_CONTENT_EDIT );

                PushMessageWindow( "Saving will disable sharing on Xbox Live." );
            }
            else
            {
                // The load fully succeeded, so the
                // content can be uploaded to Xbox Live
                // as long as we are signed on when we
                // save it again

                m_bUploadInsteadOfSave = FALSE;

                PushState( STATE_CONTENT_EDIT );

                // Warn the user if we are not signed on to
                // Xbox Live that editing and saving this content
                // will disable sharing of the file over Xbox Live
                if( !m_bUserSignedIn )
                    PushMessageWindow( "Editing and saving this content will disable it's sharing accross Xbox Live" );
            }
            break;

        case MENU_LOCAL_CONTENT_UPLOAD:
            if( !m_bUserSignedIn )
            {
                PushMessageWindow( "Please Logon to Xbox Live" );
            }
            else if( UploadSave( m_dwSaveSelected ) )
            {
                PushMessageWindow( "Uploaded file as your shared save" );
            }
            else
            {
                PushMessageWindow( "Unable to upload save" );
            }

            break;

        case MENU_LOCAL_CONTENT_OPTIONS_VIEW:
            if(! m_userContent.Load( m_bUserSignedIn,
                                     m_savedContentData[m_dwSaveSelected].szSaveGameDirectory,
                                     m_savedContentData[m_dwSaveSelected].szSaveGameName ) )
            {
                PushMessageWindow( "Unable to load content" );
            }
            else
            {
                PushState( STATE_VIEW_TEAMMATE_ICON );
            }

            break;

        case MENU_LOCAL_CONTENT_OPTIONS_DELETE:
            // Delete the save
            DWORD dwDeleteResult = XDeleteSaveGame(
                                        SAVE_DRIVE, // Root Path
                                        m_savedContentData[m_dwSaveSelected].szSaveGameName // Save name
                                    );

            if( dwDeleteResult != ERROR_SUCCESS )
            {
                PushMessageWindow( "Unable to delete save." );
            }
            else
            {
                PopState( TRUE );
                PushMessageWindow( "Save Deleted" );
            }

            break;

        }

        break;

    case EV_BUTTON_B:
        // Return to the previous menu
        PopState();
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateLocalContentOptions()
// Desc: Renders the menu of options for the local content
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateLocalContentOptions()
{
    RenderControllingUser();

    RenderMenu( L"LOCAL CONTENT OPTIONS",
                (const WCHAR**)MENU_LOCAL_CONTENT_OPTIONS,
                NUM_ITEMS_LOCAL_CONTENT_OPTIONS_MENU,
                m_iItemSelected );

    // Bottom Help text
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}


////////////////////////////
// State ViewTeammateIcon //
////////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateViewTeammateIcon
// Desc: Takes user input and allows them to dismiss the screen
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateViewTeammateIcon( Event event )
{
    switch( event )
    {
    case EV_BUTTON_B:
        PopState();
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateViewTeammateIcon
// Desc: Renders the teammate's icon to the screen
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateViewTeammateIcon()
{
    RenderControllingUser();

    RenderMenu( L"ICON VIEWER", NULL, 0, 0 );

    m_font.SetScaleFactors( 0.5f, 0.5f );

    FLOAT fTextWidth  = m_font.GetTextWidth( GLYPH_FILLED_CIRCLE );
    FLOAT fTextHeight = m_font.GetFontHeight() * 0.5f;

    FLOAT fStartX = SCREEN_CENTER_X - ( 0.5f * fTextWidth * m_userContent.GetSize() );
    FLOAT fStartY = SCREEN_CENTER_Y - ( 0.5f * fTextHeight * m_userContent.GetSize() );

    FLOAT fPosX = fStartX;
    FLOAT fPosY = fStartY;

    // Draw the lightbrite text
    for( INT iY = 0; iY < m_userContent.GetSize(); ++iY )
    {
        fPosX = fStartX;

        for( INT iX = 0; iX < m_userContent.GetSize(); ++iX )
        {
            m_font.DrawText( fPosX, fPosY,
                             m_userContent.GetColor( iX, iY ),
                             GLYPH_FILLED_CIRCLE,
                             XBFONT_CENTER_X );

            fPosX += fTextWidth;
        }

        fPosY += fTextHeight;
    }

    m_font.SetScaleFactors( 1.0f, 1.0f );

    // Tel users they can dismiss the screen
    RenderFooter( FOOTER_RENDER_CANCEL );
}

////////////////////////
// State SettingsEdit //
////////////////////////

// State functions  //

//-------------------------------------------------------------------------------------
// Name: EnterStateSettingsEdit()
// Desc: Intializes the settings editing system
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateSettingsEdit()
{
    // Reset the menu cursor
    m_iItemSelected = 0;

    // get current user's user ID
    ULONGLONG qwCurUserID = m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID;

    // call initialize of downloading settings
    HRESULT hrDownload = m_userSettings.EnterDownload( 
                         m_hUserSettingsTask ,               // download task
                         qwCurUserID ,                       // user index
                         m_dwControllingUserPort ,           // controller index              
                         m_pSettingsReceiveBuffer ,          // download buffer
                         sizeof( m_pSettingsReceiveBuffer )  // d/l buffer suze
                         );
    
    // if the download failed 
    if ( FAILED( hrDownload ) )
    {
        // reset editable settings to default values
        m_editableUserSettings.SetToDefaults();

        // if the download was due to not finding anything (because every user
        // that hasn't created settings before will get the "Storage File Not Found
        // Error"), then proceed to the Edit Settings screen.  Otherwise, display
        // an error beforehand
        if ( hrDownload != XONLINE_E_STORAGE_FILE_NOT_FOUND )
        {
            PushMessageWindow( "Unable to download previous settings" );
        }
    }
    else
    {
        // initialize progress screen for a download activity
        SetProgressTask( PROGRESS_ACTIVITY_DOWNLOAD , 
                         "Download User Settings : %u%% complete" );
    }
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateSettingsEdit
// Desc: Allows the user to edit and save settings
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSettingsEdit( Event event )
{
    HRESULT hrSettingsTaskResult;

    switch ( m_dwProgressActivity )
    {
    case PROGRESS_ACTIVITY_NONE:
        // no activity currently running worth polling its progress
        break;

    case PROGRESS_ACTIVITY_DOWNLOAD:
        // update user settings task, whether is downloading or uploading
        hrSettingsTaskResult = m_userSettings.UpdateDownload( m_hUserSettingsTask );
        
        // update progress bar
        UpdateProgressForTask( m_hUserSettingsTask );

        // if download/upload task is done
        if ( hrSettingsTaskResult != XONLINETASK_S_RUNNING )
        {
            // make sure it succeeded
            if ( FAILED( hrSettingsTaskResult ) )
            {
                // set editable settings to default parameters
                m_editableUserSettings.SetToDefaults();

                // clear progress bar
                ClearProgressTask();

                // close download task
                m_hUserSettingsTask.Close();

                // set user settings to dirty
                m_editableUserSettings.SetDirty( TRUE );

                // if download failed because nothing was found, that's fine.
                // Proceed as usual.  If there was another reason, display an error
                if( hrSettingsTaskResult != XONLINE_E_STORAGE_FILE_NOT_FOUND )
                    PushMessageWindow( "Unable to download previous settings" );

            }
            else
            {
                // get current user's user ID
                ULONGLONG qwCurUserID = m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID;

                // make sure progress bar is in sync
                assert( ProgressCompleted() );
                
                // clear progress bar
                ClearProgressTask();

                // if we succeed in getting the results
                if ( m_userSettings.ExitDownload( m_hUserSettingsTask ,     // d/l task
                                                  qwCurUserID ,             // user ID
                                                  m_pSettingsReceiveBuffer  // buffer  
                                                  ) ) 
                {
                    // copy over the results to the editable settings
                    m_editableUserSettings = m_userSettings;
                }
                else
                {
                    // reset editable settings to default parameters
                    m_editableUserSettings.SetToDefaults();
                 
                    // display an error, since there was a mismatch in the results
                    PushMessageWindow( "Error downloading results from settings" );
               }
            }
        }
        break;

    case PROGRESS_ACTIVITY_UPLOAD:
        // update user settings task, whether is downloading or uploading
        hrSettingsTaskResult = m_userSettings.UpdateUpload( m_hUserSettingsTask );
        
        // update progress bar
        UpdateProgressForTask( m_hUserSettingsTask );

        // if download/upload task is done
        if ( hrSettingsTaskResult != XONLINETASK_S_RUNNING )
        {
            // make sure it succeeded
            if ( FAILED( hrSettingsTaskResult ) )
            {
                // clear progress bar
                ClearProgressTask();

                // close download task
                m_hUserSettingsTask.Close();

                // display error
                PushMessageWindow( "Unable to upload settings" );

            }
            else
            {
                // make sure progress bar is in sync
                assert( ProgressCompleted() );
                
                // clear progress bar
                ClearProgressTask();

                // if we succeed in getting the results, deinitialize upload parameters
                m_userSettings.ExitUpload( m_hUserSettingsTask );
            }
        }
        break;

    }

    // Move the cursor / turtle around
    switch( event )
    {
    case EV_UP:
        // decrement item/setting index
        --m_iItemSelected;
        m_iItemSelected = ( m_iItemSelected < 0 ) ? 
                          ( NUM_USER_SETTING_INDICES - 1 ) : m_iItemSelected;
        break;

    case EV_DOWN:
        // increment item/setting index
        ++m_iItemSelected;
        m_iItemSelected = ( m_iItemSelected >= NUM_USER_SETTING_INDICES ) ? 
                            0 : m_iItemSelected;
        break;

    case EV_BUTTON_A:
        // increment item/setting value
        m_editableUserSettings.IncrementValue( (WORD)m_iItemSelected );
        break;

    case EV_BUTTON_B:
        // decrement item/setting value
        m_editableUserSettings.DecrementValue( (WORD)m_iItemSelected );
        break;

    case EV_BUTTON_START:
        {
            // do saving only if the editable settings are dirty
            if ( m_editableUserSettings.IsDirty() )
            {
                // get current user's user ID
                ULONGLONG qwCurUserID = m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID;
                
                // set editable settings to false before copying them to 
                // settings to be uploaded
                m_editableUserSettings.SetDirty( FALSE );
                m_userSettings = m_editableUserSettings;

                // initialize uploading
                BOOL bEnterUploadSucceeded = m_userSettings.EnterUpload(
                                             m_hUserSettingsTask ,   // upload task
                                             qwCurUserID ,           // user ID
                                             m_dwControllingUserPort // controller idx
                                             );
	                
                // if initialize did not succeed
                if ( !bEnterUploadSucceeded )
                {
                    // display error
                    PushMessageWindow( "Unable to download previous settings" );
                }
                else
                {
                    // initialize progress screen for an upload activity
                    SetProgressTask( PROGRESS_ACTIVITY_UPLOAD , 
                                     "Upload User Settings : %u%% complete" );    
                }
                
                return;
 	        }
                
            // otherwise, chastise user for trying to save changes that weren't made
            PushMessageWindow( "No changes were made to current settings" );
            return;
        }
        break;

    case EV_BUTTON_BACK:
        PopState( TRUE );

        // if changes were made, warn user that changes will be lost
        if( m_editableUserSettings.IsDirty() )
        {
            PushMessageWindow( "Throwing out changes." );
        }

        break;

    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateSettingsEdit()
// Desc: Renders the user settings to the screen and shows the user
//       their input options.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateSettingsEdit()
{
    RenderControllingUser();

    // if we're in the middle of a progress activity, render the progress
    // screen instead of displaying settings
    if ( m_dwProgressActivity != PROGRESS_ACTIVITY_NONE )
    {
        RenderProgressWindow();
    }
    else
    {
        // Display screen caption
        RenderMenu( L"EDIT SETTINGS", NULL, 0, 0 );

        // define constants for drawing the settings table
        const FLOAT SETTINGS_START_Y   = POS_SCREEN_TITLE_Y + 65.0f;
        const FLOAT SETTINGS_PAD_Y     = 40.0f;

        const FLOAT CHANGE_VALUE_X     = POS_HEADER_LEFT + 60.0f;
        const FLOAT CHANGE_SETTING_X   = POS_HEADER_RIGHT - 60.0f;
        const FLOAT SETTINGS_POINTER_X = 65.0f;
        const FLOAT SETTINGS_INDEX_X   = 220.0f;
        const FLOAT SETTINGS_VALUE_X   = 450.0f;

        // *** Draw the settings table
        for( WORD i = 0; i < NUM_USER_SETTING_INDICES; ++i )
        {
            // Highlight the selected item
            DWORD dwIndexColor = ( m_iItemSelected == i ) ? m_dwHighlightColor : 
                                    m_dwTextColor;

            // Highlight the selected item's value in red, signalling that it can 
            // change value
            DWORD dwValueColor = ( m_iItemSelected == i ) ? COLOR_RED : 
                                    m_dwTextColor;

            // draw the name of the setting
            m_font.DrawText( SETTINGS_INDEX_X,
                             SETTINGS_START_Y + ( SETTINGS_PAD_Y * i ),
                             dwIndexColor, USER_SETTING_STR_INDEX[i], XBFONT_CENTER_X );

            // construct the string that displays the setting's value
            WCHAR strImageValue[ MAX_SETTINGS_IMAGE_VALUE_SIZE ] = { 0 };
            m_editableUserSettings.PutWideValueImage( strImageValue , i );

            // draw the value
            m_font.DrawText( SETTINGS_VALUE_X,
                            SETTINGS_START_Y + ( SETTINGS_PAD_Y * i ),
                            dwValueColor, strImageValue , XBFONT_CENTER_X );
                             
        }

        // Show selected item with a little triangle
        m_font.DrawText( SETTINGS_POINTER_X,
                        SETTINGS_START_Y + ( SETTINGS_PAD_Y * m_iItemSelected ),
                        COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_LEFT );
        
        // *** Draw the instructions in the footer

        // define constant for instructions
        FLOAT fFooterStartY = POS_FOOTER_Y - DEFAULT_TEXT_PADDING;

        // Tell the user how to change values
        m_font.DrawText( CHANGE_VALUE_X, fFooterStartY,
                         m_dwTextColor,
                         GLYPH_A_BUTTON GLYPH_B_BUTTON ,
                         XBFONT_LEFT );

        m_font.DrawText( POS_HEADER_LEFT, POS_FOOTER_Y,
                         m_dwTextColor,
                         L"CHANGE VALUE +/-",
                         XBFONT_LEFT );

        // Tell the user how to move
        m_font.DrawText( CHANGE_SETTING_X, fFooterStartY,
                         m_dwTextColor,
                         GLYPH_UP_ARROW GLYPH_DOWN_ARROW ,
                         XBFONT_RIGHT );

        m_font.DrawText( POS_HEADER_RIGHT, POS_FOOTER_Y,
                         m_dwTextColor,
                         L"CHOOSE SETTING",
                         XBFONT_RIGHT );
        
        // How to save
        m_font.DrawText( SCREEN_CENTER_X, fFooterStartY,
                         m_dwTextColor,
                         GLYPH_START1_BUTTON GLYPH_START2_BUTTON,
                         XBFONT_RIGHT );

        m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y,
                         m_dwTextColor,
                         L"SAVE ",
                         XBFONT_RIGHT );

        // How to exit
        m_font.DrawText( SCREEN_CENTER_X, fFooterStartY,
                         m_dwTextColor,
                         GLYPH_BACK1_BUTTON GLYPH_BACK2_BUTTON,
                         XBFONT_LEFT );

        m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y,
                         m_dwTextColor,
                         L" EXIT",
                         XBFONT_LEFT );
    }
}


///////////////////////
// State ContentEdit //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateContentEdit()
// Desc: Intializes the content editing system
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateContentEdit()
{
    m_iTurtleX            = 0;
    m_iTurtleY            = 0;
    m_dwTurtleColor       = COLOR_WHITE;

    if( !m_lpPreviewTexture )
    {
        m_lpPreviewTexture = m_userContent.CreateTexture( m_pd3dDevice );

        assert( m_lpPreviewTexture );
    }

    m_turtleFlashTimer.StartZero();
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateContentEdit
// Desc: Allows the user to edit and save (or discard) content
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateContentEdit( Event event )
{
    const FLOAT fTurtleBlinkInterval = 0.5f;

    m_userContent.UpdateTexture( m_lpPreviewTexture );

    // Flip the color of the turtle to
    // make a blinking effect
    if( m_turtleFlashTimer.GetElapsedSeconds() > fTurtleBlinkInterval )
    {
        m_dwTurtleColor = ( m_dwTurtleColor == m_dwTextColor ) ? COLOR_CLEAR : m_dwTextColor;

        m_turtleFlashTimer.StartZero();
    }


    // Move the cursor / turtle around
    switch( event )
    {
    case EV_UP:
        // Move the turtle up and wrap to the bottom when we go past the top
        --m_iTurtleY;

        m_iTurtleY = ( m_iTurtleY < 0 ) ? ( m_userContent.GetSize() - 1 ) : m_iTurtleY;
        break;

    case EV_DOWN:
        // Move the turtle down and wrap to the top when go past the bottom
        ++m_iTurtleY;

        m_iTurtleY = ( m_iTurtleY >= m_userContent.GetSize() ) ? 0 : m_iTurtleY;
        break;

    case EV_LEFT:
        // Move the turtle left and wrap to the right side when we go past the border
        --m_iTurtleX;

        m_iTurtleX = ( m_iTurtleX < 0 ) ? ( m_userContent.GetSize() - 1 ) : m_iTurtleX;
        break;

    case EV_RIGHT:
        // Move the turtle right and wrap to the left side when we go past the border
        ++m_iTurtleX;

        m_iTurtleX = ( m_iTurtleX >= m_userContent.GetSize() ) ? 0 : m_iTurtleX;
        break;
    }

    // Paint the scene with user input!
    switch( event )
    {
    case EV_BUTTON_START:
        m_userContent.SetDirty( FALSE );

        // If we want to store the content locally
        if( !m_bUploadInsteadOfSave )
        {
            // Attempt to save it to the Xbox
            BOOL bSavedSave = m_userContent.Save( m_bUserSignedIn, m_swzBaseFilename );
            
            CHAR szSaveMessage[MAX_MESSAGE_LENGTH] = { 0 };

            if( bSavedSave )
            {
                if( m_userContent.IsDead() )
                    _snprintf( szSaveMessage, MAX_MESSAGE_LENGTH,
                               "Saved local copy as %S\0", m_swzBaseFilename );

                else
                    _snprintf( szSaveMessage, MAX_MESSAGE_LENGTH,
                               "Saved Live enabled progress as %S\0", m_swzBaseFilename );
            }
            else
            {
                _snprintf( szSaveMessage, MAX_MESSAGE_LENGTH,
                           "Unable to save progress\0" );
            }

            PushMessageWindow( szSaveMessage );
        }
        else
        {
            // if there doesn't exist a filename, pass the user's ID, otherwise, pass
            // the team's ID to the upload content method
            ULONGLONG qwID = ( m_bTeamLogo ? 0 : m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID );

            // Save the content in memory to the HD
            if( m_userContent.Upload( m_dwControllingUserPort,
                                      qwID,
                                      m_rwTeamXUIDS[m_iTeamSelected].qwTeamID,
                                      m_wszFilename ) )
            {
                PushMessageWindow( "Uploaded progress" );
            }
            else
            {
                PushMessageWindow( "Unable to upload progress" );
            }
        }
        break;

    case EV_BUTTON_BACK:
        m_wszFilename = NULL;

        PopState( TRUE );

        if( m_userContent.IsDirty() )
        {
            PushMessageWindow( "Throwing out progress." );
        }

        break;

    case EV_BUTTON_A:
    case EV_BUTTON_B:
    case EV_BUTTON_X:
    case EV_BUTTON_Y:
    case EV_BUTTON_BLACK:
    case EV_BUTTON_WHITE:
        // Set the color of the block the cursor is on
        // to the color mapped to the controller button
        m_userContent.SetColor( m_iTurtleX, m_iTurtleY,
                                        m_rwButtonColorMap[(INT)event] );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateContentEdit()
// Desc: Renders the user content to the screen and shows the user
//       their input options.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateContentEdit()
{
    RenderControllingUser();

    if( m_lpPreviewTexture )
    {
        float fPreviewPosX = 96.0f;
        float fPreviewPosY = 160.0f;

        m_font.DrawText( ( fPreviewPosX + ( ICON_SIZE * .5f ) ), ( fPreviewPosY - ICON_SIZE ),
                         m_dwTextColor,
                         L"Preview",
                         XBFONT_CENTER_X );

        SetFacePos( m_pLogoVerts, fPreviewPosX, fPreviewPosY );
        RenderSprite( m_pLogoVerts, m_lpPreviewTexture );
    }

    // RenderControllingUser();
    RenderMenu( L"EDIT CONTENT", NULL, 0, 0 );

    m_font.SetScaleFactors( 0.5f, 0.5f );

    FLOAT fTextWidth  = m_font.GetTextWidth( GLYPH_FILLED_CIRCLE );
    FLOAT fTextHeight = m_font.GetFontHeight() * 0.5f;

    FLOAT fStartX = SCREEN_CENTER_X - ( 0.5f * fTextWidth * m_userContent.GetSize() );
    FLOAT fStartY = SCREEN_CENTER_Y - ( 0.5f * fTextHeight * m_userContent.GetSize() );

    FLOAT fPosX = fStartX;
    FLOAT fPosY = fStartY;

    // Draw the lightbrite text
    for( INT iY = 0; iY < m_userContent.GetSize(); ++iY )
    {
        fPosX = fStartX;

        for( INT iX = 0; iX < m_userContent.GetSize(); ++iX )
        {
            m_font.DrawText( fPosX, fPosY,
                             m_userContent.GetColor( iX, iY ),
                             GLYPH_FILLED_CIRCLE,
                             XBFONT_CENTER_X );

            fPosX += fTextWidth;
        }

        fPosY += fTextHeight;
    }

    // Draw the blinking turtle
    FLOAT fTurtlePosX = fStartX + ( m_iTurtleX * fTextWidth );
    FLOAT fTurtlePosY = fStartY + ( m_iTurtleY * fTextHeight );

    m_font.DrawText( fTurtlePosX, fTurtlePosY,
                     m_dwTurtleColor,
                     GLYPH_HAND,
                     XBFONT_CENTER_X );

    m_font.SetScaleFactors( 1.0f, 1.0f );

    // Draw the instructions in the footer

    FLOAT fFooterStartY = POS_FOOTER_Y - DEFAULT_TEXT_PADDING;

    // Tell the user how to set the color

    m_font.DrawText( POS_HEADER_LEFT, fFooterStartY,
                     m_dwTextColor,
                     GLYPH_A_BUTTON GLYPH_B_BUTTON GLYPH_X_BUTTON GLYPH_Y_BUTTON GLYPH_WHITE_BUTTON GLYPH_BLACK_BUTTON,
                     XBFONT_LEFT );

    m_font.DrawText( POS_HEADER_LEFT, POS_FOOTER_Y,
                     m_dwTextColor,
                     L"SET PIXEL COLOR",
                     XBFONT_LEFT );

    // Tell the user how to move

    m_font.DrawText( POS_HEADER_RIGHT, fFooterStartY,
                     m_dwTextColor,
                     GLYPH_UP_ARROW GLYPH_DOWN_ARROW GLYPH_LEFT_ARROW GLYPH_RIGHT_ARROW,
                     XBFONT_RIGHT );

    m_font.DrawText( POS_HEADER_RIGHT, POS_FOOTER_Y,
                     m_dwTextColor,
                     L"MOVE TURTLE",
                     XBFONT_RIGHT );

    // How to save
    m_font.DrawText( SCREEN_CENTER_X, fFooterStartY,
                     m_dwTextColor,
                     GLYPH_START1_BUTTON GLYPH_START2_BUTTON,
                     XBFONT_RIGHT );

    m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y,
                     m_dwTextColor,
                     L"SAVE ",
                     XBFONT_RIGHT );

    // How to exit
    m_font.DrawText( SCREEN_CENTER_X, fFooterStartY,
                     m_dwTextColor,
                     GLYPH_BACK1_BUTTON GLYPH_BACK2_BUTTON,
                     XBFONT_LEFT );

    m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y,
                     m_dwTextColor,
                     L" EXIT",
                     XBFONT_LEFT );
}


/////////////////////////
// State MessageWindow //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateMessageWindow
// Desc: Waits for the user to dismiss the message window
//       then returns to the calling state.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateMessageWindow( Event event )
{
    switch( event )
    {
    case EV_BUTTON_A:
        PopState();
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateMessageWindow()
// Desc: Draws the message supplied to the screen.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateMessageWindow()
{
    m_font.DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y, m_dwTextColor,
                     m_szGameMessage,
                     XBFONT_CENTER_X );

    RenderFooter( FOOTER_RENDER_SELECT );
}

// Extra rendering functions

//-------------------------------------------------------------------------------------
// Name: RenderHeader()
// Desc: Renders a small header at the top of the screen that is shown in
//       most of the UI states
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderHeader()
{
    // Render a header giving the user the name of the demo
    m_font.DrawText( POS_HEADER_LEFT, POS_HEADER_Y, m_dwTextColor,
                     L"Storage",
                     XBFONT_LEFT );
}

//-------------------------------------------------------------------------------------
// Name: RenderFooter()
// Desc: Renders a footer at the bottom of the screen. Takes a bitflag to
//       determine which items to render.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderFooter( WORD flags )
{
    if( flags & FOOTER_RENDER_CANCEL )
    {
        // Bottom Help text
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                         m_dwTextColor, GLYPH_B_BUTTON L" back", 
                         XBFONT_LEFT );
    }

    if( flags & FOOTER_RENDER_SELECT )
    {
        m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                         m_dwTextColor, GLYPH_A_BUTTON L" select", 
                         XBFONT_RIGHT );
    }
}


// Overloaded functions defined by the application
// class to execute game logic and rendering

//-------------------------------------------------------------------------------------
// Name: Render()
// Desc: Render the game and the proper screen.
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the framebuffer
    m_pd3dDevice->Clear( 0L, NULL,
                         D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 
                         m_dwBGColor, 1.0f, 0L );

    RenderHeader();

    // Render the screen for the current state
    switch( m_state )
    {
    case STATE_SELECT_ACCOUNT:          RenderStateSelectAccount();         break;
    case STATE_LOGIN:                   RenderStateLogin();                 break;
    case STATE_LOGIN_FAILED:            RenderStateLoginFailed();           break;
    case STATE_NETWORK_ERROR:           RenderStateNetworkError();          break;
    case STATE_MAIN:                    RenderStateMain();                  break;
    case STATE_CONTENT_MANAGEMENT:      RenderStateContentManagement();     break;
    case STATE_LIST_SAVED_CONTENT:      RenderStateListSavedContent();      break;
    case STATE_LOCAL_CONTENT_OPTIONS:   RenderStateLocalContentOptions();   break;
    case STATE_RECENT_PLAYERS:          RenderStateRecentPlayers();         break;
    case STATE_VIEW_MY_TEAMS:           RenderStateViewMyTeams();           break;
    case STATE_VIEW_TEAM_ROSTER:        RenderStateViewTeamRoster();        break;
    case STATE_VIEW_TEAMMATE_ICON:      RenderStateViewTeammateIcon();      break;
    case STATE_SETTINGS_EDIT:           RenderStateSettingsEdit();          break;
    case STATE_CONTENT_EDIT:            RenderStateContentEdit();           break;
    case STATE_MESSAGE_WINDOW:          RenderStateMessageWindow();         break;
    default:
        RenderStateNetworkError();
        break; //assert(0 && "Unknown/illegal state!");
    };

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    
    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Update the game logic of the game for one tick/frame
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    Event ev = EV_NULL;

    // Once the user logs in,
    // only accept input from the controller
    // used to log in.
    if( m_state == STATE_SELECT_ACCOUNT )
    {
        for( INT iUser = 0; iUser < MAX_USERS; ++iUser)
        {
            ev = GetEvent( iUser );

            UpdateStateSelectAccount( iUser, ev );
        }

        return S_OK;
    }

    // Only get events from the controller
    // used to logon to Xbox Live
    ev = GetEvent( m_dwControllingUserPort );

    switch( m_state )
    {
    case STATE_LOGIN:                   UpdateStateLogin( ev );                 break;
    case STATE_LOGIN_FAILED:            UpdateStateLoginFailed( ev );           break;
    case STATE_NETWORK_ERROR:           UpdateStateNetworkError( ev );          break;
    case STATE_MAIN:                    UpdateStateMain( ev );                  break;
    case STATE_CONTENT_MANAGEMENT:      UpdateStateContentManagement( ev );     break;
    case STATE_LIST_SAVED_CONTENT:      UpdateStateListSavedContent( ev );      break;
    case STATE_LOCAL_CONTENT_OPTIONS:   UpdateStateLocalContentOptions( ev );   break;
    case STATE_RECENT_PLAYERS:          UpdateStateRecentPlayers( ev );         break;
    case STATE_VIEW_MY_TEAMS:           UpdateStateViewMyTeams( ev );           break;
    case STATE_VIEW_TEAM_ROSTER:        UpdateStateViewTeamRoster( ev );        break;
    case STATE_VIEW_TEAMMATE_ICON:      UpdateStateViewTeammateIcon( ev );      break;
    case STATE_SETTINGS_EDIT:           UpdateStateSettingsEdit( ev );          break;
    case STATE_CONTENT_EDIT:            UpdateStateContentEdit( ev );           break;
    case STATE_MESSAGE_WINDOW:          UpdateStateMessageWindow( ev );         break;
    default:
        assert(0 && "Unknown/illegal state!");
    };

    // If the player is signed in, check the status of the network
    // and report any found network errors
    if( m_bUserSignedIn )
    {
        if( !SUCCEEDED( m_hLogonTask.Continue() ) )
        {
            m_bUserSignedIn = FALSE;

            PushState( STATE_NETWORK_ERROR );
        }
    }

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Setup the clean initial values for the
//       member variables of the game class
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create some quads and initialize our
    // texture array pointer
    m_dwTeamMemberTextureToDL = 0;
    m_ppTeamLogoTextures      = NULL;
    m_ppTeammateTextures      = NULL;
    m_lpPreviewTexture        = NULL;
    m_pLogoVerts              = CreateFace( 0.0f, 0.0f );

    m_wszFilename             = NULL;
    m_bUploadInsteadOfSave    = FALSE;
    m_bTeamLogo               = FALSE;

    // Message of the day
    ZeroMemory( m_wszMessageOfTheDay , sizeof( m_wszMessageOfTheDay ) );
    m_pMOTDTexture = NULL;

    m_eContentAction          = STATE_LOCAL_CONTENT_OPTIONS;

    assert( m_pLogoVerts );


    // Initialize RNG for random game size and random game name.
    // Your game does not need to do this
    srand( GetTickCount() );


    // Assign a nifty color map to make the
    // content editing code nicer
    assert( EV_BUTTON_A == 0 );
    assert( EV_BUTTON_BLACK == 5 );

    m_rwButtonColorMap[EV_BUTTON_A]     = COLOR_GREEN;
    m_rwButtonColorMap[EV_BUTTON_B]     = COLOR_RED;
    m_rwButtonColorMap[EV_BUTTON_X]     = COLOR_BLUE;
    m_rwButtonColorMap[EV_BUTTON_Y]     = COLOR_YELLOW;
    m_rwButtonColorMap[EV_BUTTON_WHITE] = COLOR_WHITE;
    m_rwButtonColorMap[EV_BUTTON_BLACK] = COLOR_BLACK;


    m_state            = NUM_STATES;
    m_iItemSelected    = 0;
    m_dwNumStoredUsers = 0;

    m_bIsSigningIn     = FALSE;
    m_bUserSignedIn    = FALSE;
    m_iSignInResult    = S_OK;

    m_dwPlayerSelected    = 0;
    m_dwPlayerRenderStart = 0;

    m_dwControllingUserPort  = 0;

    ZeroMemory( m_pSettingsReceiveBuffer , sizeof( m_pSettingsReceiveBuffer ) );

    m_dwProgressActivity     = (DWORD)PROGRESS_ACTIVITY_NONE;
    m_bProgressSucceeded     = FALSE;
    m_dwProgressPercentage   = 0;
    ZeroMemory( m_wszProgressMessageFormat , sizeof( m_wszProgressMessageFormat ) );
    ZeroMemory( m_wszProgressMessage , sizeof( m_wszProgressMessage ) );

    ZeroMemory( m_stateStack, sizeof( m_stateStack ) );

    m_wStateStackSize = 0;

    XBUtil_GetWide( "", m_szGameMessage, MAX_MESSAGE_LENGTH );
    XBUtil_GetWide( "", m_swzBaseFilename, XONLINE_GAMERTAG_SIZE );

    // Initialize the network stack
    if( FAILED( XBNet_OnlineInit( 0 ) ) )
        return E_FAIL;


    // Create the font
    if( FAILED( m_font.Create( "Font.xpr" ) ) )
        return E_FAIL;


    // Initialize Xbox Live!

    // Wait for any inserted MUs to mount
    while( XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY );
    
    // Before any of the Xbox online APIs can be used, XOnlineStartup must be 
    // called.  XOnlineStartup automatically calls XNetStartup and 
    // WSAStartup with default parameters in order to initialize the 
    // Xbox Secure Network Libary and the Winsock layer. To specify non-default
    // startup parameters for XNetStartup or WSAStartup, call those functions 
    // prior to calling XOnlineStartup.
    
    HRESULT hrStartup = XOnlineStartup( NULL );

    if( !SUCCEEDED( hrStartup ) ) return E_FAIL;

    PushState( STATE_SELECT_ACCOUNT );

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: Destructor
// Desc: Deletes any allocated memory
//-------------------------------------------------------------------------------------
CXBoxSample::~CXBoxSample()
{
    if( m_ppTeammateTextures )
        delete [] m_ppTeammateTextures;

    if( m_ppTeamLogoTextures )
        delete [] m_ppTeamLogoTextures;
}

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-------------------------------------------------------------------------------------
VOID __cdecl main()
{
    CXBoxSample xbApp;

    if( FAILED( xbApp.Create() ) )
        return;

    xbApp.Run();
}
