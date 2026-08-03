//-----------------------------------------------------------------------------
// File: SimpleMessaging.cpp
//
// Desc: Illustrates the use of the Xbox Live Messaging APIs
//
// Hist: 09.25.03 - New for November release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>                  
#include <xonline.h>              
#include <assert.h> 
#include <stdio.h>
#include <xhv.h>

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
BOOL SignIn( XONLINE_USER*pLogonUsers );
VOID __cdecl Print( const WCHAR*strFormat, ... );
VOID __cdecl Error( const WCHAR*strFormat, ... );
VOID UIMsg( const WCHAR* strText );
VOID BootToDash( DWORD dwReason );

VOID DisplayMessageSummaries( DWORD dwUser, XONLINE_MSG_SUMMARY *pSummaries, DWORD dwNumSummaries );
VOID WaitForSendToComplete( XONLINETASK_HANDLE hTask );
VOID WaitForDownloadToComplete( XONLINETASK_HANDLE hTask );
VOID WaitUntilInitialMessageSync( DWORD dwUser );
VOID WaitUntilMessageReceived( DWORD dwUser );
VOID WaitForMessageDetailsTask( XONLINETASK_HANDLE hTask );



//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

const DWORD FIRST_USER_INDEX = 0;
const DWORD SECOND_USER_INDEX = 1;

const DWORD SIMPLEMESSAGING_STRING_SIZE = 60;
const DWORD SIMPLEMESSAGING_RANDOM_PROP_TYPE = 1;

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

    // Before using the XBox Live APIs, a title  must first call XOnlineStartup.     
    // XOnlineStartup will automatically call XNetStartup and WSAStartup with
    // reasonable defaults in order to initialize the Xbox Secure Networking Library and
    // Winsock respectively. If you require special parameters for those functions
    // your title should can call them first before calling
    // XOnlineStartup.
    HRESULT hr = XOnlineStartup( NULL );
    assert( SUCCEEDED( hr ) );
    
    // The XOnlineGetUsers function will enumerate both the hard disk and any     
    // attached memory units looking for user accounts.

    hr = XOnlineGetUsers( StoredUsers, &dwNumStoredUsers );
    assert( SUCCEEDED( hr ) );
    
    // If no accounts were found, a tile must give the player the  option of 
    // going to the online dash to create new account. In addition, it is
    // possible for a player to actually insert/remove an MU while
    // the title account selection UI is active.  A title must
    // call XOnlineGetUsers repeatedly to account for this.
    // For demonstration purposes, we just boot to the account signup section
    // of the online dash if no accounts are found.

    if( dwNumStoredUsers < 2 )
    {
        Print( L"You will need at least 2 accounts to run the SimpleMessaging sample.");
        BootToDash( XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP );
    }
    
    // For SimpleMessaging, we just take the first two players on the Xbox.

    LogonUsers[ FIRST_USER_INDEX ] =  StoredUsers[ FIRST_USER_INDEX ];
    LogonUsers[ SECOND_USER_INDEX ] = StoredUsers[ SECOND_USER_INDEX ];

    // Sign onto the Live Service.  The sample requires no special services.
    
    if( SignIn( LogonUsers ) )
    {        
        XONLINE_MSG_SUMMARY     MessageSummaries[ XONLINE_MAX_NUM_MESSAGES ];        
        XONLINE_MSG_HANDLE      hMessage;
        XONLINETASK_HANDLE      hSendTask;
        WCHAR                   TextMessage[ SIMPLEMESSAGING_STRING_SIZE ];
        WCHAR                   ReceivedMessage[ SIMPLEMESSAGING_STRING_SIZE ];
        DWORD                   dwNumMessages;
        DWORD                   dwLanguage;
        XUID                    xuidRecipient;
        XONLINETASK_HANDLE      hMessageDetailsTask;    
        XONLINETASK_HANDLE      hMessageDownloadTask;
        DWORD                   dwReceivedSize;

        Print( L"Waiting for messages to be synchronized" );
        
        // wait until we have our messages downloaded (function defined below)
        WaitUntilInitialMessageSync( FIRST_USER_INDEX );        
   
        // Enumerate and display existing message summaries for the first user
        // Summaries include who it is from, the type of message, etc.  For
        // full message information, we'll have to reference the details
        dwNumMessages = XONLINE_MAX_NUM_MESSAGES;
        XOnlineMessageEnumerate( FIRST_USER_INDEX, MessageSummaries, &dwNumMessages );        
        DisplayMessageSummaries( FIRST_USER_INDEX, MessageSummaries, dwNumMessages );

        // Create a message:
        // size set to 0 uses the default
        // context is up to the title for a TITLE_CUSTOM type message
        // Here we set the notify flag so that a notification is received with the message, and we can 
        // detect it easily when it arrives.  For passive messages you would not set the notification
        // flag- i.e. if a MOTD is changed, the users don't need to know it immediately- it's fine
        // to wait until message enumeration.
        
        Print( L"Creating a message");
        XOnlineMessageCreate( XONLINE_MSG_TYPE_TITLE_CUSTOM, 
                              6, 0, 0, 
                              XONLINE_MSG_FLAG_HAS_TEXT | 
                              XONLINE_MSG_FLAG_HAS_VOICE,                              
                              0, &hMessage );
       
        // For a message with text, there are 2 properties - text and language
        // You can get language with a call to XGetLanguage(), but since the string is listed
        // in the source code here, and we know it's English, we just set it to English.        
        wcscpy( TextMessage, L"Hello, user 0" );
        dwLanguage = XC_LANGUAGE_ENGLISH;

        Print( L"Adding text property '%s' to message", TextMessage );

        XOnlineMessageSetProperty( hMessage, XONLINE_MSG_PROP_TEXT,         
                                   (wcslen(TextMessage) + 1) * sizeof(WCHAR), TextMessage, 0 );
                
        XOnlineMessageSetProperty( hMessage, XONLINE_MSG_PROP_TEXT_LANGUAGE, 
                                   sizeof(DWORD), &dwLanguage, 0 );
        // now we add the voice property- normally you would get the data from XHV or your
        // voice library.  Since this is a non-interactive app, we just append it
        // You could use 
        //  pXHVEngine->VoiceMailRecord( port, time, dwVoiceBufferSize, VoiceBuf);
        // to start recording the buffer

        // 5 seconds of voice

        const DWORD dwVoiceDuration = 5000;
        const DWORD dwVoiceBufferSize = XHVGetVoiceMailBufferSize( dwVoiceDuration );
        const WORD wCodec = XONLINE_PROP_VOICE_DATA_CODEC_WMAVOICE_V90;
        BYTE VoiceBuf[dwVoiceBufferSize];
        DWORD dwPropSize;
        DWORD dwAttachFlags;

        ZeroMemory( VoiceBuf, dwVoiceBufferSize );

        XOnlineMessageSetProperty( hMessage, XONLINE_MSG_PROP_VOICE_DATA, 
                                    dwVoiceBufferSize, VoiceBuf, 0 );

        XOnlineMessageSetProperty( hMessage, XONLINE_MSG_PROP_VOICE_DATA_CODEC,
                                     sizeof(WORD), &wCodec, 0 );       

        XOnlineMessageSetProperty( hMessage, XONLINE_MSG_PROP_VOICE_DATA_DURATION,
                                     sizeof(DWORD), &dwVoiceDuration, 0 );       

        DWORD dwRandomDWORD;

        dwRandomDWORD = 100;
        // You can create your own property type here with the XONLINE_MSG_PROP_TAG macro

        XOnlineMessageSetProperty( hMessage, 
            XONLINE_MSG_PROP_TAG( XONLINE_MSG_PROP_TYPE_I4, SIMPLEMESSAGING_RANDOM_PROP_TYPE ),
            sizeof(DWORD), &dwRandomDWORD, 0 );

        // we don't read this property down below, but you would probably #define
        // XONLINE_MSG_PROP_TAG( XONLINE_MSG_PROP_TYPE_I4, 1) to some useful constant, and use
        // it to read and write the property from the message

        // we only have one recipient, although the APIs support up to 
        // XONLINE_MAX_MESSAGE_RECIPIENTS recipients
        xuidRecipient = LogonUsers[ FIRST_USER_INDEX ].xuid;

        // Send the message
        Print( L"Sending message from user 1 to user 0");
        XOnlineMessageSend( SECOND_USER_INDEX, hMessage, 1, &xuidRecipient, NULL, &hSendTask );

        // Loop on the task handle until it has been sent
        WaitForSendToComplete( hSendTask );
        XOnlineTaskClose( hSendTask );

        // Free up the structure associated with the message
        XOnlineMessageDestroy( hMessage );

        // Wait until notification for user 0        
        Print( L"Waiting for messages to be synchronized" );
        WaitUntilMessageReceived( FIRST_USER_INDEX );        

        // Enumerate and display existing message summaries for user 0        
        dwNumMessages = XONLINE_MAX_NUM_MESSAGES;
        XOnlineMessageEnumerate( FIRST_USER_INDEX, MessageSummaries, &dwNumMessages );        
        DisplayMessageSummaries( FIRST_USER_INDEX, MessageSummaries, dwNumMessages );               

        // Get the message details of the first message in the list
        // Message details include the actual content of the message
        
        // We set the FLAG_READ when requesting details, so as not to waste bandwidth with a 
        // XOnlineMessageSetFlags() call later

        XOnlineMessageDetails( FIRST_USER_INDEX, MessageSummaries[ 0 ].dwMessageID,  XONLINE_MSG_FLAG_READ,
                               0, NULL, &hMessageDetailsTask );        
        WaitForMessageDetailsTask( hMessageDetailsTask );

        // Get the individual properties based on the message type  
        // If a message is title-specific, you will need to read in your custom properties here
        // For text messages, you can just look at the text and language properties

        // You could also get the other properties of the message here in the same way

        XOnlineMessageDetailsGetResultsProperty( hMessageDetailsTask, XONLINE_MSG_PROP_TEXT, SIMPLEMESSAGING_STRING_SIZE,
                                                 ReceivedMessage, &dwReceivedSize, NULL );       

        Print( L"Received message with text property '%s'", ReceivedMessage );

        hr = XOnlineMessageDetailsGetResultsProperty( hMessageDetailsTask, XONLINE_MSG_PROP_VOICE_DATA, 
                                                 dwVoiceBufferSize, VoiceBuf, &dwPropSize, &dwAttachFlags );

        if ( hr == XONLINE_E_MESSAGE_PROPERTY_DOWNLOAD_REQUIRED )
        {
            Print( L"Downloading voicemail attachment" );
            // we have to download the message
            XOnlineMessageDownloadAttachmentToMemory( hMessageDetailsTask,
                                                      XONLINE_MSG_PROP_VOICE_DATA,
                                                      VoiceBuf,
                                                      dwPropSize,                                                      
                                                      NULL,
                                                      &hMessageDownloadTask );

            WaitForDownloadToComplete( hMessageDownloadTask );

            XOnlineTaskClose( hMessageDownloadTask );
            // voice mail is in memory now
        }


        // close message details task
        XOnlineTaskClose( hMessageDetailsTask );

        Print( L"Deleting message");
        // delete the message from the first users queue
        
        // XOnlineMessageDelete immediately removes the message from the local queue, but takes time
        // (and calls to XOnlineTaskContinue on the main logon task) to propogate to the server
        // because we close immediately here, messages will be left in the queue- in a real 
        // game, you shouldn't have this problem, since the game continues running after a 
        // message is deleted.
        
        XOnlineMessageDelete( FIRST_USER_INDEX, MessageSummaries[ 0 ].dwMessageID, false );        

        // enumerate and display existing message summaries for user 0        
        dwNumMessages = XONLINE_MAX_NUM_MESSAGES;
        XOnlineMessageEnumerate( FIRST_USER_INDEX, MessageSummaries, &dwNumMessages );        
        DisplayMessageSummaries( FIRST_USER_INDEX, MessageSummaries, dwNumMessages );                       

        // close task
        XOnlineTaskClose( hLogonTask );
    }
    
    // When a title is through with the XBox Live APIs, it can call XOnlineCleanup
    // to perform final cleanup for the online functions.
    XOnlineCleanup();
    
    ::Sleep( 10000 ); // Wait for any debug output to finish
    BootToDash( XLD_LAUNCH_DASHBOARD_MAIN_MENU );
}

//-----------------------------------------------------------------------------
// Name: DisplayMessageSummaries()
// Desc: Output message summaries on the debug channel.
//-----------------------------------------------------------------------------
VOID DisplayMessageSummaries( DWORD dwUser, XONLINE_MSG_SUMMARY *pSummaries, DWORD dwNumSummaries )
{
    SYSTEMTIME systime;
    FILETIME localfiletime;

    Print( L"User %d messages:", dwUser );

    // if no messages, display nothing
    if( dwNumSummaries == 0 )
    {
        Print( L"No messages for user %d", dwUser );
        return;
    }
    
    // loop through the messages, displaying ID, sender name, and time
    for( DWORD i = 0; i < dwNumSummaries; i++ )
    {
        FileTimeToLocalFileTime( &( pSummaries[ i ].ftSentTime ), &localfiletime );
        FileTimeToSystemTime( &localfiletime, &systime );
        
        
        Print( L"Message ID %d from %S on %d-%d-%d (M-D-Y) at %02d:%02d:%02d", 
            pSummaries[i].dwMessageID, 
            pSummaries[i].szSenderName,
            systime.wMonth,
            systime.wDay, 
            systime.wYear,
            systime.wHour,
            systime.wMinute,
            systime.wSecond );           
    }
}

//-----------------------------------------------------------------------------
// Name: WaitForSendToComplete
// Desc: Helper function for waiting for a messaging send to complete.
//       In a real game, this would be part of your game loop.
//-----------------------------------------------------------------------------
VOID WaitForSendToComplete( XONLINETASK_HANDLE hTask )
{
    HRESULT hr = XONLINETASK_S_RUNNING;

    DWORD dwPct;
    ULONGLONG qwNum, qwDen;
    
    while ( hr == XONLINETASK_S_RUNNING )
    {
        // have to pump logon and the task
        // In a real game, this would be part of your game loop- you wouldn't block on
        // this.
        
        // Get send message progress
        XOnlineMessageSendGetProgress( hTask, &dwPct, &qwNum, &qwDen );
        Print(L"Messaging sending.. %d%% done, %d\\%d bytes", dwPct, (DWORD)qwNum, (DWORD)qwDen );

        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr )) Error( L"Logon task failed with 0x%x", hr );
        hr = XOnlineTaskContinue( hTask );
        if ( FAILED( hr )) Error( L"Messaging task failed with 0x%x", hr );        
        
        // put in a delay of about 1 frame so as not to spam with MessageSendGetProgress
        Sleep( 15 );
    }
    
    if ( hr != XONLINETASK_S_SUCCESS )
            Error( L"XOnlineMessageSend task failed with 0x%x", hr );       
}

//-----------------------------------------------------------------------------
// Name: WaitForDownloadToComplete
// Desc: Helper function for waiting for a messaging download to complete.
//       In a real game, this would be part of your game loop.
//-----------------------------------------------------------------------------
VOID WaitForDownloadToComplete( XONLINETASK_HANDLE hTask )
{
    HRESULT hr = XONLINETASK_S_RUNNING;

    DWORD dwPct;
    ULONGLONG qwNum, qwDen;
    
    while ( hr == XONLINETASK_S_RUNNING )
    {
        // have to pump logon and the task
        // In a real game, this would be part of your game loop- you wouldn't block on
        // this.
        
        // Get send message progress
        XOnlineMessageDownloadAttachmentGetProgress( hTask, &dwPct, &qwNum, &qwDen );
        Print(L"Attachment downloading.. %d%% done, %d\\%d bytes", dwPct, (DWORD)qwNum, (DWORD)qwDen );

        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr )) Error( L"Logon task failed with 0x%x", hr );
        hr = XOnlineTaskContinue( hTask );
        if ( FAILED( hr )) Error( L"Downloading task failed with 0x%x", hr );        
        
        // put in a delay of about 1 frame so as not to spam with MessageSendGetProgress
        Sleep( 15 );
    }
    
    if ( hr != XONLINETASK_S_SUCCESS )
            Error( L"XOnlineMessageSend task failed with 0x%x", hr );       
}

//-----------------------------------------------------------------------------
// Name: WaitUntilInitialMessageSync
// Desc: Helper function to wait until initial message enumerate is complete
//       Since we don't have a task handle, we have to poll while pumping
//       the logon task
//-----------------------------------------------------------------------------

VOID WaitUntilInitialMessageSync( DWORD dwUser )        
{
    HRESULT hr = XONLINETASK_S_RUNNING;
    DWORD dwFlags;
    XONLINE_NOTIFICATION_EX_INFO xnei;

    Print( L"Pumping online task handle until message notification received" );

    // loop until we are sync'd    
    do 
    {
        // have to pump logon and the task
        // In a real game, this would be part of your game loop- you wouldn't block on
        // this.
        
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr )) Error( L"Logon task failed with 0x%x", hr );
       
        // put in a delay of about 1 frame - not because it is necessary, but 
        // because it mimics actual conditions
        Sleep( 15 );
        
        // check to see if the pending sync flag is set
        XOnlineGetNotificationEx( dwUser, &xnei, &dwFlags );

    } while ( dwFlags & XONLINE_NOTIFICATION_STATE_FLAG_PENDING_SYNC );

}

//-----------------------------------------------------------------------------
// Name: WaitUntilMessageReceived
// Desc: Wait until the message we just sent is received.
//       In a real game, this is unnecessary- you just send the message
//       and go on your way.
//-----------------------------------------------------------------------------
VOID WaitUntilMessageReceived( DWORD dwUser )
{
    HRESULT hr = XONLINETASK_S_RUNNING;
    DWORD dwFlags;
    XONLINE_NOTIFICATION_EX_INFO xnei;
   
    Print( L"Pumping online task handle until message notification received" );

    // loop until we get a notification 
    while ( !XOnlineGetNotificationEx( dwUser, &xnei, &dwFlags) || 
            (xnei.bMessageType != XONLINE_MSG_TYPE_TITLE_CUSTOM) )
    {
        // have to pump logon and the task
        // In a real game, this would be part of your game loop- you wouldn't block on
        // this.
        
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr )) Error( L"Logon task failed with 0x%x", hr );
       
        // put in a delay of about 1 frame - not because it is necessary, but 
        // because it mimics actual conditions
        Sleep( 15 );
        
    }    
}

//-----------------------------------------------------------------------------
// Name: WaitForMessageDetailsTask()
// Desc: Loop on task handle until message details have been received.
//-----------------------------------------------------------------------------
VOID WaitForMessageDetailsTask( XONLINETASK_HANDLE hMessageDetailsTask )
{
    HRESULT hr = XONLINETASK_S_RUNNING;    

    Print( L"Pumping task handles until message details received" );
    
    while ( hr == XONLINETASK_S_RUNNING )
    {
        // have to pump logon and the task
        // In a real game, this would be part of your game loop- you wouldn't block on
        // this.
        
        hr = XOnlineTaskContinue( hLogonTask );
        if ( FAILED( hr )) Error( L"Logon task failed with 0x%x", hr );
        hr = XOnlineTaskContinue( hMessageDetailsTask );
        if ( FAILED( hr )) Error( L"Message details task failed with 0x%x", hr );

        // put in a delay of about 1 frame - not because it is necessary, but 
        // because it mimics actual conditions
        Sleep( 15 );
    }    
}


//-----------------------------------------------------------------------------
// Name: Print()
// Desc: Send formatted output to the debug window.
//-----------------------------------------------------------------------------
VOID __cdecl Print( const WCHAR* strFormat, ... )
{
    const int MAX_OUTPUT_STR = 512;
    WCHAR strBuffer[MAX_OUTPUT_STR];
    va_list pArglist;
    va_start( pArglist, strFormat );
    INT iChars= wvsprintfW( strBuffer, strFormat, pArglist );
    assert( iChars < MAX_OUTPUT_STR );
    OutputDebugStringW( L"\n*** SimpleMessaging: " );
    OutputDebugStringW( strBuffer ); 
    OutputDebugStringW( L"\n" );
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
    OutputDebugStringW( L"\n*** SimpleMessaging: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n" );
    ( VOID ) iChars;
    va_end( pArglist );
    ::Sleep( 10000 ); // Wait for output to complete
    BootToDash( XLD_LAUNCH_DASHBOARD_MAIN_MENU );
}




//-----------------------------------------------------------------------------
// Name: UIMsg()
// Desc: Display a recommended user interface message.
//       See Xbox_Terminology_List.xls for additional information.
//-----------------------------------------------------------------------------
VOID UIMsg( const WCHAR* strText )
{
    OutputDebugStringW( L"\n*** SimpleMessaging: UI Message:\n" );
    OutputDebugStringW( strText );
    OutputDebugStringW( L"\n" );
}




//-----------------------------------------------------------------------------
// Name: BootToDash()
// Desc: Boot back into either the main or online dashboards.
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

