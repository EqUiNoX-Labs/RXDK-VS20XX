//-----------------------------------------------------------------------------
// File: SimpleSignatureService.cpp
//
// Desc: Illustrated the use of the Xbox Live Signature Service APIs
//
// Hist: 4.28.03 - New for June release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>
#include <xonline.h>
#include <assert.h>

// Xbox Live Signature Service 

// To protect customers and optimize the online gaming experience, the Xbox Live
// service seeks to maximize the security of content that is distributed through 
// Xbox Live. Whether the data consists of bonus levels, game statistics and 
// rankings, game replays, or even user-created content such as custom tracks, 
// fields, or team emblems, a user who downloads data from Xbox Live should feel 
// confident that the data is accurate, reliable, and non-malicious.

// The Xbox Live Signature Service APIs allow you to get a signature for the 
// digest of any piece of shared online content.  It is more secure than the 
// signatures for harddrive content because it requires an Xbox Live key that
// is only available to titles while they are connected to Xbox Live.  Signatures
// can be expired or denied in the future based on time, title, Xbox, or users
// in response to a discovered security issue or other policy.

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
BOOL SignIn( XONLINE_USER*pLogonUsers );
VOID __cdecl Print( const WCHAR*strFormat, ... );
VOID __cdecl Error( const WCHAR*strFormat, ... );
VOID UIMsg( const WCHAR* strText );
VOID BootToDash( DWORD dwReason );

VOID GenerateRandomData( BYTE *pbData, DWORD dwSize );
VOID CalculateDigest( BYTE *pbData, DWORD dwSize, BYTE *pDigest, DWORD dwDigestSize );
VOID GetOnlineSignature( BYTE *pData, DWORD dwSize, BYTE *pvOnlineSig, DWORD dwOnlineSigSize );
VOID WriteBlob( BYTE *pbData, DWORD dwSize, BYTE *pvOnlineSig, DWORD dwOnlineSigSize );
VOID ReadBlob( BYTE *pbData, DWORD dwSize, BYTE *pvOnlineSig, DWORD dwOnlineSigSize );

HRESULT VerifyOnlineSignature( BYTE *pDigest, DWORD dwDigestSize, BYTE *pvOnlineSig, DWORD dwOnlineSigSize );


//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Size of the random 'blob' of data we are using for our content
const DWORD DATA_SIZE = 16000;

// path to our demonstration file 
const CHAR SAMPLEFILE_PATH[] = "z:\\sample.bin";

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

    // Before using the XBox Live APIs, a title must first call XOnlineStartup.     
    // XOnlineStartup will automatically call XNetStartup and WSAStartup with
    // reasonable defaults in order to initialize the Xbox Secure Networking Library and
    // Winsock respectively. If you require special parameters for those functions
    // your title should can call them first before calling
    // XOnlineStartup.
    HRESULT hr = XOnlineStartup( NULL );
    if ( FAILED( hr ) )
        Error( L"Error starting online system" );

    // The XOnlineGetUsers function will enumerate both the hard disk and any     
    // attached memory units looking for user accounts.

    hr = XOnlineGetUsers( StoredUsers, &dwNumStoredUsers );
    if ( FAILED( hr ) )
        Error( L"Could not get logged on users" );

    // If no accounts were found, a tile must give the player the option of 
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

    // Sign onto the Live Service.  This sample only requires the 
    // XONLINE_SIGNATURE_SERVICE service, but if you are using stats with
    // attachments you will need XONLINE_STORAGE_SERVICE and 
    // XONLINE_STATISTICS_SERVICE as well.   

    if( SignIn( LogonUsers ) )
    {
        BYTE                byDataBlob[ DATA_SIZE ];
        BYTE                *sigDigest;
        BYTE                *sigOnline;
        DWORD               dwDigestLength;
        DWORD               dwOnlineLength;

        // get the length of the digest and online signatures - use these APIs, and not
        // predefined constants

        dwDigestLength = XCalculateSignatureGetSize( XCALCSIG_FLAG_DIGEST );
        dwOnlineLength = XCalculateSignatureGetSize( XCALCSIG_FLAG_ONLINE );

        // Allocate sigDigest and sigOnline on the stack

        sigDigest = (BYTE *) HeapAlloc( GetProcessHeap(), 0, dwDigestLength );
        sigOnline = (BYTE *) HeapAlloc( GetProcessHeap(), 0, dwOnlineLength );
        
        // generate a file
        GenerateRandomData( byDataBlob, DATA_SIZE );         

        // get the signature of the data through the signature service
        // Note that we use the entire data to generate the signature, but when verifying
        // we only use the DIGEST of the data
        GetOnlineSignature( byDataBlob, DATA_SIZE, sigOnline, dwOnlineLength );

        // write the file to the disk, with the signature
        WriteBlob( byDataBlob, DATA_SIZE, sigOnline, dwOnlineLength );
       
        // ---------------------------------------------------------------------------------------------
        // Reading and verifying would happen later, but for the purposes of this sample, we'll
        // do it immediately    
            
        // read the file back into memory
        ReadBlob( byDataBlob, DATA_SIZE, sigOnline, dwOnlineLength );

        // calculate the digest for the file
        CalculateDigest( byDataBlob, DATA_SIZE, sigDigest, dwDigestLength );  
        
        // verify the digest to the online signature
        if ( VerifyOnlineSignature( sigDigest, dwDigestLength, sigOnline, dwOnlineLength ) != S_OK )
        {
            // if the content is invalid, you should display a message to the user and ignore the 
            // content
        
            HeapFree( GetProcessHeap(), 0, sigDigest );
            HeapFree( GetProcessHeap(), 0, sigOnline );

            Error( L"Content was invalid" );
        }
        else
        {
            // Content verification succeeded, we can proceed to process the content

            Print( L"Content verification succeeded" );
        }

        // A title signs off users by calling XOnlineTaskClose on the
        // task handle returned by XOnlineLogon.  Another situation in which users
        // are signed off is if the Xbox Live Service realizes the task handle 
        // returned by XOnlineLogon is not being serviced by the title (
        // e.g. the user turned the console off).
        
        HeapFree( GetProcessHeap(), 0, sigDigest );
        HeapFree( GetProcessHeap(), 0, sigOnline );

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
// Name: GenerateRandomData
// Desc: Generate random data which represents the data blob you need to sign
//-----------------------------------------------------------------------------
VOID GenerateRandomData( BYTE *pbData, DWORD dwSize )
{
    srand( GetTickCount() ); 
    
    // fill with random bytes
    for ( DWORD idx = 0; idx < dwSize; idx++ )
    {
        *(pbData++) = rand() % 256;
    }
}

//-----------------------------------------------------------------------------
// Name: CalculateDigest
// Desc: Calculate the digest of the data to sign
//-----------------------------------------------------------------------------

VOID CalculateDigest( BYTE *pbData, DWORD dwSize, BYTE *pDigest, DWORD dwDigestSize )
{
    assert( dwDigestSize = XCalculateSignatureGetSize( XCALCSIG_FLAG_DIGEST ) );

    HANDLE hCalcSig = XCalculateSignatureBegin( XCALCSIG_FLAG_DIGEST );

    // you can call XCalculateSignatureUpdate multiple times iteratively
    // to calculate a digest over multiple blocks of data.  For this sample
    // we have it all in one piece, so we just call it once

    XCalculateSignatureUpdate( hCalcSig, pbData, dwSize );

    // Get the digest
    XCalculateSignatureEnd( hCalcSig, pDigest );
        
}

//-----------------------------------------------------------------------------
// Name: GetOnlineSignature
// Desc: Get the secure signature for a digest
//-----------------------------------------------------------------------------

VOID GetOnlineSignature( BYTE *pData, DWORD dwSize, BYTE *pvOnlineSig, DWORD dwOnlineSigSize )
{      
    assert( dwOnlineSigSize = XCalculateSignatureGetSize( XCALCSIG_FLAG_ONLINE ) );

    HANDLE hCalcSig = XCalculateSignatureBegin( XCALCSIG_FLAG_ONLINE );

    // calculate the signature over the data
    XCalculateSignatureUpdate( hCalcSig, pData, dwSize );

    // Retrieve the live signature.
    XCalculateSignatureEnd( hCalcSig, pvOnlineSig );
}

//-----------------------------------------------------------------------------
// Name: WriteBlob
// Desc: Write our data to disk, signed
//-----------------------------------------------------------------------------

VOID WriteBlob( BYTE *pbData, DWORD dwSize, BYTE *pvOnlineSig, DWORD dwOnlineSigSize )
{      
    // open the file for writing

    HANDLE hFile = ::CreateFile( SAMPLEFILE_PATH, 
                        GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

    if( hFile == INVALID_HANDLE_VALUE )
        Error( L"Unable to create file" );

    DWORD dwNumBytesWritten;
    BOOL bResult;

    // write out the file

    bResult = WriteFile( hFile, pbData, dwSize, &dwNumBytesWritten, NULL );
    
    if( !bResult || dwNumBytesWritten != dwSize )
    {
        CloseHandle( hFile );
        Error( L"File write error attempting to write file data" );
    }

    // write out the signature
    
    bResult = WriteFile( hFile, (BYTE *)pvOnlineSig, dwOnlineSigSize, &dwNumBytesWritten, NULL );
    CloseHandle( hFile );

    if( !bResult || dwNumBytesWritten != dwOnlineSigSize )
        Error( L"File write error attempting to write signature" );
    
    
    Print( L"Wrote file and signature" );

}


//-----------------------------------------------------------------------------
// Name: ReadBlob
// Desc: Read in data and signature (does not verify signature)
//-----------------------------------------------------------------------------

VOID ReadBlob( BYTE *pbData, DWORD dwSize, BYTE *pvOnlineSig, DWORD dwOnlineSigSize )
{
    // open the file for reading

    HANDLE hFile = ::CreateFile(  SAMPLEFILE_PATH,
                        GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

    if( hFile == INVALID_HANDLE_VALUE )
        Error( L"Unable to read file" );

    DWORD dwNumBytesRead;
    BOOL bResult;

    // read the data

    bResult = ReadFile( hFile, pbData, dwSize, &dwNumBytesRead, NULL );

    if( !bResult || dwNumBytesRead != dwSize )
    {
        CloseHandle( hFile );
        Error( L"File read error attempting to read file data" );
    }

    // read the signature
    
    bResult = ReadFile( hFile, pvOnlineSig, dwOnlineSigSize, &dwNumBytesRead, NULL );
    CloseHandle( hFile );
    
    if( !bResult || dwNumBytesRead != dwOnlineSigSize )
        Error( L"File read error attempting to read signature" );
    
    Print( L"Read file and signature" );
}

//-----------------------------------------------------------------------------
// Name: VerifyOnlineSignature
// Desc: Verify the signature with the signature service.
//-----------------------------------------------------------------------------

HRESULT VerifyOnlineSignature( BYTE *pDigest, DWORD dwDigestSize, BYTE *pvOnlineSig, DWORD dwOnlineSigSize )
{
    
    XONLINE_SIGNATURE_TO_VERIFY xstv;
    XONLINETASK_HANDLE hVerify;
    HRESULT hr, *hrVerify, rtn;
    DWORD cResults;

    // Fill in XONLINE_SIGNATURE_TO_VERIFY stucture
    assert( dwDigestSize <= XCalculateSignatureGetSize(XCALCSIG_FLAG_DIGEST) );
    assert( dwOnlineSigSize <= XCalculateSignatureGetSize(XCALCSIG_FLAG_ONLINE) );

    xstv.cbDigest = dwDigestSize;
    xstv.pbDigest = pDigest;
    xstv.cbOnlineSignature = dwOnlineSigSize;
    xstv.pbOnlineSignature = (PBYTE)pvOnlineSig;

    // Contact the signature verification service to verify the signature.
    // You can verify more than one digest at a time, but for this sample we only have one

    hr = XOnlineSignatureVerify( &xstv, 1, NULL, &hVerify );
    
    // loop until we get some results.  In this sample we just block and loop on the 
    // task handle, but in a real application you'd want to continue to update your UI

    do {
        // we have to service the logon task in all loops
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr ) ) Error ( L"Logon Task Failed with 0x%x", hr );

        hr = XOnlineTaskContinue( hVerify );        
    } while ( hr == XONLINETASK_S_RUNNING );

    // hrVerify receives an array which contains the verification results for the signature-digest pair.
    // there will be cResults elements in the array

    XOnlineSignatureVerifyGetResults( hVerify, &hrVerify, &cResults );

    // make sure we got back a single result for our single request
    if( cResults != 1 )
    {
        Error(L"No signature results recieved" );
    }

    rtn = hrVerify[0];

    // if the value is S_OK, we authenticated; anything else we should discard the data
    Print(L"%x\n", rtn );

    XOnlineTaskClose( hVerify );

    return rtn;   
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
    OutputDebugStringW( L"\n*** SimpleSignatureService: " );
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
    OutputDebugStringW( L"\n*** SimpleSignatureService: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );
    ( VOID ) iChars;
    va_end( pArglist );
    ::Sleep( 10000 ); // Wait for output to complete
    BootToDash( XLD_LAUNCH_DASHBOARD_MAIN_MENU );
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




//-----------------------------------------------------------------------------
// Name: UIMsg()
// Desc: Display a recommended user interface message
//       See Xbox_Terminology_List.xls for additional information.
//-----------------------------------------------------------------------------
VOID UIMsg( const WCHAR* strText )
{
    OutputDebugStringW( L"\n*** SimpleSignatureService: UI Message:\n" );
    OutputDebugStringW( strText );
    OutputDebugStringW( L"\n" );
}