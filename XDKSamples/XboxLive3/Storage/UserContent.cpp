//-------------------------------------------------------------------------------------
// File: UserContent.cpp
//
// Desc: Holds the implementation for a user content object used to demonstrate
//       Xbox Live storage functionality
//
// Hist: 08.10.04 - New for Sept release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include <assert.h>
#include <stdio.h>
#include <xtl.h>
#include <xonline.h>
#include "XBOnlineTask.h"
#include "UserContent.h"
#include "Common.h"

//-------------------------------------------------------------------------------------
// Name: CalculateSignature()
// Desc: Calculates a signature for the user content currently in the editor.
//       The signature is saved into the array pointed to by rwSignature.
//       Returns the size of the signature. If 0, or less, then an error occured
//-------------------------------------------------------------------------------------
DWORD CalculateSignature( PBYTE rwSignature, DWORD dwMaxSigSize,
                          const PBYTE pUserData, const DWORD dwUserDataSize,
                          const dwSigType )
{
    assert( rwSignature != NULL );


    // Step 1
    //
    // Start the signature calculation with the ONLINE
    // flag to indicate that we are signing ONLINE
    // content

    HANDLE hCalcSig = XCalculateSignatureBegin( dwSigType );

    if ( hCalcSig == INVALID_HANDLE_VALUE )
    {
        return 0;
    }


    // Step 2
    //
    // Continue the content signing by giving the data we wish to
    // sign to calculate the signature

    DWORD dwCalcErr = XCalculateSignatureUpdate(
                            hCalcSig,      // The handle given by SigBegin
                            pUserData,     // The data we want to sign
                            dwUserDataSize // The size of the data we are signing
                        );

    assert( dwCalcErr == ERROR_SUCCESS );


    // Step 3
    //
    // Find out how large the signature is and zero
    // the memory used to store it
    
    DWORD dwSignatureSize = XCalculateSignatureGetSize( dwSigType );
    assert( dwSignatureSize <= dwMaxSigSize );
    ZeroMemory( rwSignature, dwMaxSigSize );


    // Step 4
    //
    // Finish the content signing and get the signature

    dwCalcErr = XCalculateSignatureEnd(
                    hCalcSig,          // The handle given by StartSig
                    (PVOID)rwSignature // Pointer to the signature
                );

    assert( dwCalcErr == ERROR_SUCCESS );


    return dwSignatureSize;
}

//-------------------------------------------------------------------------------------
// Name: CalculateDigest()
// Desc: Creates a digest of the given data. This digest is used to verify Xbox Live
//       signatures.
//-------------------------------------------------------------------------------------
VOID CalculateDigest( BYTE *pbData, DWORD dwSize, BYTE *pDigest, DWORD dwDigestSize )
{
    assert( dwDigestSize == XCalculateSignatureGetSize( XCALCSIG_FLAG_DIGEST ) );

    HANDLE hCalcSig = XCalculateSignatureBegin( XCALCSIG_FLAG_DIGEST );

    // you can call XCalculateSignatureUpdate multiple times iteratively
    // to calculate a digest over multiple blocks of data.  For this sample
    // we have it all in one piece, so we just call it once

    XCalculateSignatureUpdate( hCalcSig, pbData, dwSize );

    // Get the digest
    XCalculateSignatureEnd( hCalcSig, pDigest );       
}

//-------------------------------------------------------------------------------------
// Name: IsValidLiveSignature()
// Desc: Returns TRUE if the Live signature is validated against the given digest.
//       The digest is generated from the data that has been signed.
//-------------------------------------------------------------------------------------
BOOL IsValidLiveSignature( DWORD dwSigSize, PBYTE pSignature,
                           DWORD dwDigestSize, PBYTE pDigest )
{
    assert( pSignature );
    assert( pDigest );


    // Step 1
    //
    // Make sure that our buffer sizes are correct for the signature
    // and for the digest

    if( ( dwSigSize < XCalculateSignatureGetSize( XCALCSIG_FLAG_ONLINE ) )
        || ( dwDigestSize < XCalculateSignatureGetSize( XCALCSIG_FLAG_DIGEST ) ) )
    {
        return FALSE;
    }


    // Step 2
    //
    // Create a pairing of the signature and
    // the digest to submit to the Xbox Live
    // signature verification service
    //
    // NOTE:
    // You can build an array of signature/digest
    // pairs to submit to Xbox live whose results
    // will be returned simultaneously

    CXBOnlineTask               hVerifyTask;
    XONLINE_SIGNATURE_TO_VERIFY sigDigestPair;

    ZeroMemory( &sigDigestPair, sizeof( sigDigestPair ) );

    sigDigestPair.cbDigest          = dwDigestSize;
    sigDigestPair.cbOnlineSignature = dwSigSize;
    sigDigestPair.pbDigest          = (PBYTE)pDigest;
    sigDigestPair.pbOnlineSignature = pSignature;


    // Step 3
    //
    // Start the signature verification process
    // by submitting our signature read to XOnline

    HRESULT hrSigVerify = XOnlineSignatureVerify(
                                &sigDigestPair, // Array of sig/digest pairs to verify
                                1,              // Number of signatures given
                                NULL,           // Work event
                                &hVerifyTask    // Task to assign
                            );

    if( FAILED( hrSigVerify ) )
        return FALSE;


    // Step 4
    //
    // Pump the task until finished

    do
    {
        hrSigVerify = hVerifyTask.Continue();
    }
    while( hrSigVerify == XONLINETASK_S_RUNNING );

    if( FAILED( hrSigVerify ) )
        return FALSE;


    // Step 5
    //
    // Get the results, return them and close the task

    HRESULT* hrVerifyResults = NULL;
    DWORD    dwNumResults    = 0;

    hrSigVerify =  XOnlineSignatureVerifyGetResults(
                        hVerifyTask,      // Task used to start verification
                        &hrVerifyResults, // Pointer to array of results
                        &dwNumResults     // Number of results returned
                    );

    assert( dwNumResults > 0 );
    assert( hrVerifyResults != NULL );

    hVerifyTask.Close();


    return SUCCEEDED( hrVerifyResults[0] );
}


///////////////////////////////////
// CUserContent Member Functions //
///////////////////////////////////

// User creatable content object
// Made a class to help support ease
// of content signing

//-------------------------------------------------------------------------------------
// Name: GetColor()
// Desc: Returns the color at the given pixel
//-------------------------------------------------------------------------------------
DWORD CUserContent::GetColor( INT iX, INT iY )
{
    assert( iX < BITMAP_SIZE );
    assert( iX >= 0 );
    assert( iY < BITMAP_SIZE );
    assert( iY >= 0 );

    INT iBitmapIndex = ( iY * BITMAP_SIZE ) + iX;

    return m_rwBitmap[iBitmapIndex];
}

//-------------------------------------------------------------------------------------
// Name: SetColor()
// Desc: Sets the color at the given pixel
//-------------------------------------------------------------------------------------
VOID CUserContent::SetColor( INT iX, INT iY, DWORD dwColor )
{
    assert( iX < BITMAP_SIZE );
    assert( iX >= 0 );
    assert( iY < BITMAP_SIZE );
    assert( iY >= 0 );

    INT iBitmapIndex = ( iY * BITMAP_SIZE ) + iX;

    m_rwBitmap[iBitmapIndex] = dwColor;

    // Get track of this so we know if
    // it needs to be re-signed
    SetDirty( TRUE );
}

//-------------------------------------------------------------------------------------
// Name: CreateTexture()
// Desc: Takes the user edited content and creates a linear texture that
//       can be used to display as an icon.
//-------------------------------------------------------------------------------------
LPDIRECT3DTEXTURE8 CUserContent::CreateTexture( LPDIRECT3DDEVICE8  lpD3dDevice )
{
    LPDIRECT3DTEXTURE8 lpNewTexture = NULL;

    // Step 1
    //
    // Get a texture pointer from the GPU

    HRESULT hrTextureCreate = lpD3dDevice->CreateTexture(
                BITMAP_SIZE,         // Size of texture X
                BITMAP_SIZE,         // Size of texture Y
                1,                   // Create only 1 mipmap level
                0,                   // No usage flags
                D3DFMT_LIN_A8R8G8B8, // A Linear 32BIT color format
                NULL,                // Pool - ignored
                &lpNewTexture        // New texture address
            );

    if( FAILED( hrTextureCreate ) )
        return NULL;


    // Step 2
    //
    // Now lock a rectangle the fills the entire texture

    D3DLOCKED_RECT lockedRect;

    HRESULT hrLock = lpNewTexture->LockRect(
                            0,           // Level
                            &lockedRect, // Locked rectangle
                            NULL,        // NULL fills the entire range
                            0            // No locking flags
                        );

    if( FAILED( hrLock ) )
        return NULL;


    // Step 3
    //
    // Copy the linear bitmap into the locked memory area
    // so it can be displayed

    DWORD dwSize = BITMAP_SIZE * BITMAP_SIZE * sizeof( DWORD );
    memcpy( lockedRect.pBits, GetBitmap(), dwSize );


    // Step 4
    //
    // Unlock to commit our changes and then return
    // the pointer to the texture

    lpNewTexture->UnlockRect( 0 );

    // Return the pointer to the new texture
    return lpNewTexture;
}

//-------------------------------------------------------------------------------------
// Name: UpdateTexture()
// Desc: Takes a pointer to a texture and updates the contents using the
//       user content as the new texture
//-------------------------------------------------------------------------------------
VOID CUserContent::UpdateTexture( LPDIRECT3DTEXTURE8 lpTexture )
{
    assert( lpTexture );


    // Step 1
    //
    // Lock a rectangle that fills the entire texture

    D3DLOCKED_RECT lockedRect;

    HRESULT hrLock = lpTexture->LockRect(
                            0,           // Level
                            &lockedRect, // Locked rectangle
                            NULL,        // NULL fills the entire range
                            0            // No locking flags
                        );

    if( FAILED( hrLock ) )
        return;


    // Step 2
    //
    // Copy the linear bitmap into the locked memory area
    // so it can be displayed

    DWORD dwSize = BITMAP_SIZE * BITMAP_SIZE * sizeof( DWORD );
    memcpy( lockedRect.pBits, GetBitmap(), dwSize );


    // Step 3
    //
    // Unlock to commit our changes

    lpTexture->UnlockRect( 0 );
}

//-------------------------------------------------------------------------------------
// Name: Save()
// Desc: Saves the content to the Xbox Hard drive with the given filename.
//       The type of authentication signatures written depend on the content's
//       status. If the user is offline, the content is only written with a standard
//       save game signature and can never be shared via Live. If the user is signed
//       on and the content is NOT dead, then a Live signature is written also to
//       verify the authenticity of the data. Content written with a Live signature
//       can be moved to memory cards and loaded on other Xboxes. Editing live signed
//       data offline should remove the data's ability to ever go online again and
//       should not get written with a live signature.
//-------------------------------------------------------------------------------------
BOOL CUserContent::Save( BOOL bUserSignedIn, const WCHAR* wszFilename )
{
    if( wszFilename == NULL )
        return FALSE;

    if( wcslen( wszFilename ) < 1 )
        return FALSE;


    // Step 1
    //
    // Create a path to the saved content file

    CHAR    szPath[MAX_SAVE_PATH_SIZE] = { 0 };
    HRESULT hrSave = XCreateSaveGame(
                            SAVE_DRIVE,        // Drive to save to
                            wszFilename,       // File name
                            OPEN_ALWAYS,       // Creation disposition
                            0,                 // Creation flags - Copyable
                            szPath,            // Output path buffer
                            MAX_SAVE_PATH_SIZE // Size of buffer
                        );

    if( FAILED( hrSave ) )
    {
        return FALSE;
    }


    // Step 2
    //
    // Create the file handle for writing

    // Create a fully qualified file name that includes the path
    CHAR szFilename[MAX_GAMENAME] = { 0 };

    _snprintf( szFilename, MAX_GAMENAME , "%s%S\0", szPath, wszFilename );

    HANDLE hSave = CreateFile(
                        szFilename,            // file name
                        GENERIC_WRITE,         // access mode
                        0,                     // share mode - No sharing until handle is closed
                        NULL,                  // Security abilities - Reserved, use NULL
                        CREATE_ALWAYS,         // Create new file or overwrite existing file
                        FILE_ATTRIBUTE_NORMAL, // file attributes
                        NULL                   // handle to template file, reserved must use NULL
                     );

    if( hSave == INVALID_HANDLE_VALUE )
    {
        return FALSE;
    }


    // Step 3
    //
    // Generate the save signature that every save gets
    // and then write the size of the signature to the file

    DWORD dwSigSize     = 0;
    DWORD dwSaveSigSize = XCalculateSignatureGetSize( XCALCSIG_FLAG_SAVE_GAME );

    BYTE  rwSaveSignature[SAVE_SIG_BUFFER_SIZE] = { 0 };

    assert( dwSaveSigSize <= SAVE_SIG_BUFFER_SIZE );

    // Generate the normal save signature
    dwSigSize = CalculateSignature( rwSaveSignature,
                                    dwSaveSigSize,
                                    (PBYTE)this,
                                    DATA_SIZE,
                                    XCALCSIG_FLAG_SAVE_GAME );

    assert( dwSigSize == dwSaveSigSize );


    DWORD dwNumBytesWritten = 0;
    DWORD dwNumBytesToWrite = sizeof( DWORD );
    BOOL  bWriteResult      = FALSE;

    bWriteResult = WriteFile(
                        hSave,              // handle to file
                        &dwSigSize,         // data buffer
                        dwNumBytesToWrite,  // number of bytes to write
                        &dwNumBytesWritten, // number of bytes written
                        NULL                // overlapped buffer
                    );

    assert( bWriteResult );
    assert( dwNumBytesToWrite == dwNumBytesWritten );


    // Step 4
    //
    // Write the signature

    dwNumBytesWritten = 0;

    bWriteResult = WriteFile(
                        hSave,              // handle to file
                        rwSaveSignature,    // data buffer
                        dwSaveSigSize,      // number of bytes to write
                        &dwNumBytesWritten, // number of bytes written
                        NULL                // overlapped buffer
                    );

    assert( bWriteResult );
    assert( dwSaveSigSize == dwNumBytesWritten );


    // Step 5
    //
    // Write the content to the file

    dwNumBytesWritten = 0;
    dwNumBytesToWrite = DATA_SIZE;

    bWriteResult = WriteFile(
                        hSave,              // handle to file
                        this,               // data buffer
                        dwNumBytesToWrite,  // number of bytes to write
                        &dwNumBytesWritten, // number of bytes written
                        NULL                // overlapped buffer
                    );

    assert( bWriteResult );
    assert( dwNumBytesToWrite == dwNumBytesWritten );


    // Step 6
    //
    // If the content is not "dead" and we are signed on
    // then we will sign it with an Xbox Live signature
    // for security purposes

    if( bUserSignedIn && (! IsDead() ) )
    {
        DWORD dwLiveSigSize = XCalculateSignatureGetSize( XCALCSIG_FLAG_ONLINE );
        BYTE  rwLiveSig[LIVE_SIG_BUFFER_SIZE] = { 0 };

        assert( dwLiveSigSize <= LIVE_SIG_BUFFER_SIZE );

        dwSigSize = CalculateSignature( rwLiveSig,
                                        dwLiveSigSize,
                                        (PBYTE)this,
                                        DATA_SIZE,
                                        XCALCSIG_FLAG_ONLINE );

        assert( dwSigSize == dwLiveSigSize );

        dwNumBytesWritten = 0;

        // Write the signature to file
        bWriteResult = WriteFile(
                            hSave,              // handle to file
                            rwLiveSig,          // data buffer
                            dwLiveSigSize,      // number of bytes to write
                            &dwNumBytesWritten, // number of bytes written
                            NULL                // overlapped buffer
                        );

        assert( bWriteResult );
        assert( dwLiveSigSize == dwNumBytesWritten );
    }
    else
    {
        SetDead( TRUE );
    }


    // Step 7
    //
    // Close the file and return our result

    BOOL bCloseResult = CloseHandle( hSave );

    assert( bCloseResult );

    return bCloseResult;
}

//-------------------------------------------------------------------------------------
// Name: Load()
// Desc: Loads user content with the given name. If the user is not signed onto
//       Xbox Live, or the content does not contain a Live signature, then
//       the content is marked as "dead" so if it is saved it can not be
//       shared across Xbox Live.
//       If the user is signed on AND the content contains an Xbox signature,
//       then the signature is verified by the Xbox Live Signature service
//       before the content is allowed to be loaded or modified.
//-------------------------------------------------------------------------------------
BOOL CUserContent::Load( BOOL bUserSignedIn,
                         const CHAR*  szPath,
                         const WCHAR* wszFilename )
{
    // Step 1
    //
    // Reset internally and do safety checks

    if( wszFilename == NULL )
        return FALSE;

    if( szPath == NULL )
        return FALSE;

    if( wcslen( wszFilename ) < 1 )
        return FALSE;

    if( strlen( szPath ) < 1 )
        return FALSE;

    Clear();


    // Step 2
    //
    // Open the data file and get a handle

    // Create a fully qualified file name that includes the path
    CHAR szFilename[MAX_GAMENAME] = { 0 };
    _snprintf( szFilename, MAX_GAMENAME , "%s%S\0", szPath, wszFilename );

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
    // Read the size of the signature
    // and double check that it matches

    DWORD dwSigSize = 0;

    DWORD dwNumBytesRead    = 0;
    DWORD dwNumBytesToRead  = sizeof( DWORD );

    ReadFile(
            hRead,              // handle to file
            &dwSigSize,         // data buffer
            dwNumBytesToRead,   // number of bytes to read
            &dwNumBytesRead,    // number of bytes read
            NULL                // overlapped buffer
        );

    assert( dwNumBytesToRead == dwNumBytesRead );


    // Step 4
    //
    // Read the signature

    BYTE rwSignature[SAVE_SIG_BUFFER_SIZE] = { 0 };
    assert( dwSigSize <= SAVE_SIG_BUFFER_SIZE );

    dwNumBytesRead = 0;

    ReadFile(
            hRead,           // handle to file
            rwSignature,     // data buffer
            dwSigSize,       // number of bytes to read
            &dwNumBytesRead, // number of bytes read
            NULL             // overlapped buffer
        );

    assert( dwSigSize == dwNumBytesRead );


    // Step 5
    //
    // Read the user content

    dwNumBytesRead   = 0;
    dwNumBytesToRead = DATA_SIZE;

    ReadFile(
            hRead,            // handle to file
            this,             // data buffer
            dwNumBytesToRead, // number of bytes to read
            &dwNumBytesRead,  // number of bytes read
            NULL              // overlapped buffer
        );

    assert( dwNumBytesToRead == dwNumBytesRead );


    // Step 6
    //
    // If we are logged into Xbox live
    // and we are not just getting a read only
    // copy, then we need to check the XONLINE
    // signature. If the signature is bad
    // of we are offline, then we need to
    // mark the content as "dead".
    //
    // This signature check is really calculated AFTER
    // the read, but we need to read the signature
    // before we close the file handle

    DWORD dwLiveSigSize   = 0;
    BYTE  rwLiveSignature[LIVE_SIG_BUFFER_SIZE] = { 0 };

    // Read the Live signature
    dwLiveSigSize   = XCalculateSignatureGetSize( XCALCSIG_FLAG_ONLINE );
    dwNumBytesRead  = 0;

    assert( dwLiveSigSize <= LIVE_SIG_BUFFER_SIZE );

    ReadFile(
            hRead,           // handle to file
            rwLiveSignature, // data buffer
            dwLiveSigSize,   // number of bytes to read
            &dwNumBytesRead, // number of bytes read
            NULL             // overlapped buffer
    );

    // No signature, or an obviously bad signature
    // will cause the content to be considered dead

    SetDead( dwLiveSigSize != dwNumBytesRead );

    CloseHandle( hRead );


    // Step 7
    //
    // Check the normal saved game signature
    // by calculating a new signature
    // and comparing it to the signure we
    // just read in. If the signatures match
    // then

    DWORD dwSaveSigSize = XCalculateSignatureGetSize( XCALCSIG_FLAG_SAVE_GAME );
    BYTE rwSignatureCalced[SAVE_SIG_BUFFER_SIZE];

    dwSigSize   = CalculateSignature( rwSignatureCalced,
                                      dwSaveSigSize,
                                      (PBYTE)this,
                                      DATA_SIZE,
                                      XCALCSIG_FLAG_SAVE_GAME );

    assert( dwSigSize == dwSaveSigSize );

    BOOL bSaveSigsMatch = ( memcmp( rwSignatureCalced, rwSignature, dwSigSize) == 0 );

    SetDead( !bSaveSigsMatch || IsDead() );

    if( !bSaveSigsMatch )
    {
        return FALSE;
    }

    // Step 8
    //
    // Check the save against the XOnline signature
    // If the user is not logged on to Xbox
    // Live then the content goes "dead"
    // and can never be uploaded to Xbox live

    if( IsDead() || ( !bUserSignedIn ) )
    {
        return TRUE;
    }


    // Step 9
    //
    // Calculate the digest. The digest is submitted
    // along with the signature we read to Xbox Live
    // via XOnlineSignatureVerify. The server verifies
    // the authenticity of the signature against the
    // digest.

    DWORD dwDigestSize = XCalculateSignatureGetSize( XCALCSIG_FLAG_DIGEST );
    BYTE  rwDigest[DIGEST_BUFFER_SIZE] = { 0 };

    assert( dwDigestSize <= DIGEST_BUFFER_SIZE );

    CalculateDigest( (PBYTE)this, DATA_SIZE,
                     rwDigest, dwDigestSize );


    // Step 10
    //
    // Submit the digest/signature pair to Xbox Live for verification
    // and then return the results.

    BOOL bLiveSigsMatch = IsValidLiveSignature( dwLiveSigSize, rwLiveSignature,
                                                dwDigestSize, rwDigest );

    SetDead( !bLiveSigsMatch );


    return bLiveSigsMatch;
}

//-------------------------------------------------------------------------------------
// Name: Upload();
// Desc: Uploads the content to Xbox Live storage (for the title ID)
//       If a filename is provided, then the content is uploaded as that filename
//       otherwise a filename is created based on the users XUID
// Note: UploadFromMemory automatically generates a Live signature transparently
//       that is transparently authenticated when DownloadToMemory is used
//-------------------------------------------------------------------------------------
BOOL CUserContent::Upload( const DWORD dwControllingUserPort,
                           const ULONGLONG qwUserID,
                           const ULONGLONG qwTeamID,
                           WCHAR* wszFilename )
{

    // The signature must have been generated
    // And the user content must not have been tampered with
    // and it must have been verified by the XOnline signature service

    assert( !IsDirty() );
    assert( !IsDead() );


    // The data can be uploaded to a team
    // or each user has individual space PER title
    // For this sample we will just upload from memory
    // to the teams facility

    DWORD dwFacility = XONLINESTORAGE_FACILITY_TEAMS;


    // Flag to remember if we have to
    // allocate for the filename string

    BOOL bAllocated = FALSE;

    // If no filename was supplied then
    // this is a user icon.
    // If a filename was provided then
    // we are downloading the team global icon
    if( !wszFilename )
    {
        // Create a new filename that
        // is the user ID

        UINT iHi = (UINT)( qwUserID >> 32 );
        UINT iLo = (UINT)qwUserID;

        wszFilename = new WCHAR[MAX_GAMENAME];
        assert( wszFilename );

        if( wszFilename )
            ZeroMemory( wszFilename, sizeof( wszFilename ) );

        _snwprintf( wszFilename, MAX_GAMENAME, L"%08x%08x",
                    iHi, iLo );

        bAllocated = TRUE;
    }

    assert( wcslen( wszFilename ) > 0 );


    // Step 1
    //
    // Create a name for the file to be placed
    // on the server

    DWORD dwPathLength                               = MAX_SERVER_PATH_SIZE;
    WCHAR wszStorageServerPath[MAX_SERVER_PATH_SIZE] = { 0 };

    HRESULT hrCreatePath = XOnlineStorageCreateServerPath(
                                dwFacility,           // The type of storage to use
                                qwUserID,             // qwID of the user
                                qwTeamID,             // qwID of the team
                                wszFilename,          // Name of the file
                                wszStorageServerPath, // Output path on the server
                                &dwPathLength         // Length of server path string
                           );

    if( FAILED( hrCreatePath ) )
    {
        if( bAllocated )
            delete [] wszFilename;

        return FALSE;
    }


    // Step 2
    //
    // Start the upload from memory to the server

    CXBOnlineTask hUploadTask;
    FILETIME      ftServerExpirationDate;
    
    // Never expire this data
    ZeroMemory( &ftServerExpirationDate, sizeof( ftServerExpirationDate ) );

    HRESULT hrUpload = XOnlineStorageUploadFromMemory(
                            dwFacility,             // Which type of storage
                            dwControllingUserPort,  // Controller port used
                            wszStorageServerPath,   // Name of the destination file
                            ftServerExpirationDate, // Expiration date of the data
                            (PBYTE)this,            // Pointer to data to upload
                            DATA_SIZE,              // Amount of data to upload
                            0,                      // Upload flags, must be set to zero
                            NULL,                   // Work event
                            &hUploadTask            // Task for this work
                        );

    if( FAILED( hrUpload ) )
    {
        if( bAllocated )
            delete [] wszFilename;

        return FALSE;
    }


    // Step 3
    //
    // Pump the task until finished

    do
    {
        hrUpload  = hUploadTask.Continue();
    }
    while( hrUpload == XONLINETASK_S_RUNNING );


    // Step 4
    //
    // Finished! Return our success code

    hUploadTask.Close();

    if( bAllocated )
        delete [] wszFilename;


    return SUCCEEDED( hrUpload );
}

//-------------------------------------------------------------------------------------
// Name: Download()
// Desc: Downloads any stored user content into the user content object
//       If a filename is provided, then the function attempts to download
//       a file with the given name, otherwise a filename is created
//       based on the users XUID
// Note: DownloadToMemory automatically authenticates a Live signature transparently
//       that was generated by UploadFromMemory.
//-------------------------------------------------------------------------------------
HRESULT CUserContent::Download( const DWORD dwControllingUserPort,
                                const ULONGLONG qwUserID,
                                const ULONGLONG qwTeamID,
                                WCHAR* wszFilename )
{
    // The data can be from "per tiltle per team" space
    // or from "per title per user" space.
    // For this sample we only upload and download to
    // memory from the TEAMS facility

    DWORD dwFacility = XONLINESTORAGE_FACILITY_TEAMS;
    BOOL bAllocated  = FALSE;

    Clear();

    // If no filename was supplied then
    // this is a user icon.
    // If a filename was provided then
    // we are downloading the team global icon
    if( !wszFilename )
    {
        UINT iHi = (UINT)( qwUserID >> 32 );
        UINT iLo = (UINT)qwUserID;

        wszFilename = new WCHAR[MAX_GAMENAME];
        assert( wszFilename );

        if( wszFilename )
            ZeroMemory( wszFilename, sizeof( wszFilename ) );

        _snwprintf( wszFilename, MAX_GAMENAME, L"%08x%08x",
                    iHi, iLo );

        bAllocated = TRUE;
    }

    assert( wcslen( wszFilename ) > 0 );


    // Step 1
    //
    // Generate the string that points to the file on
    // the server

    WCHAR  wszStorageServerPath[MAX_SERVER_PATH_SIZE] = { 0 };
    DWORD  dwPathLength                               = MAX_SERVER_PATH_SIZE;

    HRESULT hrCreatePath = XOnlineStorageCreateServerPath(
                                dwFacility,           // The type of storage to use
                                qwUserID,             // qwID of the user
                                qwTeamID,             // wqID of the team
                                wszFilename,          // Name of the file
                                wszStorageServerPath, // Output path on the server
                                &dwPathLength         // Length of server path string
                            );

    if( FAILED( hrCreatePath ) )
    {
        if( bAllocated )
            delete [] wszFilename;

        return hrCreatePath;
    }


    // Step 2
    //
    // Start the download into memory

    CXBOnlineTask hDownloadTask;

    HRESULT hrDownload = XOnlineStorageDownloadToMemory(
                            dwFacility,            // The type of storage
                            dwControllingUserPort, // The port of the user
                            wszStorageServerPath,  // The full name of the data
                            (PBYTE)this,           // Where to place
                            DATA_SIZE,             // size of data buffer
                            0,                     // Download flags, must be 0
                            NULL,                  // Work event
                            &hDownloadTask         // Task for the work
                         );


    // Step 3
    //
    // Pump the task until finished

    do
    {
        hrDownload = hDownloadTask.Continue();
    }
    while( hrDownload == XONLINETASK_S_RUNNING );

    if( FAILED( hrDownload ) )
    {
        if( bAllocated )
            delete [] wszFilename;

        hDownloadTask.Close();

        return hrDownload;
    }


    // Step 4
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

    assert( dwSizeDownloaded == DATA_SIZE );
    assert( SUCCEEDED( hrDownload ) );
    assert( pDataLocation == (PBYTE)this );
    assert( !IsDirty() );
    assert( !IsDead() );

    
    // Step 5
    //
    // Finished!
    // Close the task and assign the new content

    hDownloadTask.Close();

    if( bAllocated )
        delete [] wszFilename;

    return hrDownload;
}

//-------------------------------------------------------------------------------------
// Name: Clear()
// Desc: Clears the user content
//-------------------------------------------------------------------------------------
VOID CUserContent::Clear()
{
    for( INT iX = 0; iX < BITMAP_SIZE; ++iX )
        for( INT iY = 0; iY < BITMAP_SIZE; ++iY )
            SetColor( iX, iY, COLOR_BLUE );

    SetDirty( FALSE );
    SetDead( FALSE );
}

//-------------------------------------------------------------------------------------
// Name: Copy Operator
// Desc: Copies the source user content into the destination
//-------------------------------------------------------------------------------------
CUserContent& CUserContent::operator=( const CUserContent& rhs )
{
    if( this != &rhs )
    {
        m_bDirty       = rhs.m_bDirty;
        m_bContentDead = rhs.m_bContentDead;

        memcpy( m_rwBitmap, rhs.m_rwBitmap, sizeof( m_rwBitmap ) );
    }

    return *this;
}

//-------------------------------------------------------------------------------------
// Name: CUserContent()
// Desc: Creates an inert and safe object. Sets the entire bitmap to BLACK
//-------------------------------------------------------------------------------------
CUserContent::CUserContent() : m_bDirty( FALSE ), m_bContentDead( FALSE )
{
    Clear();
}

