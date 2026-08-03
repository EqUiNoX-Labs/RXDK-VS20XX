//-----------------------------------------------------------------------------
// File: RemoteConnection.cpp
//
// Desc: Module for connecting to XMVEncoder tool
//
// Hist: 12.01.03 - New for December 2003 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


#include "XMVCompare.h"


// Command prefix for things sent across the debug channel
static const CHAR g_strDebugConsoleCommandPrefix[] = "XCMD";

// The critical section used to protect data that is shared between threads
static CRITICAL_SECTION g_CriticalSection;

// List of app-defined remote commands
const REMOTE_COMMAND g_RemoteCommands[] =
{
    // Command,  Handler,     Help string
    { "playleft",  RCmdPlayLeft,  " <filename>: Sets the file to be used" },
    { "playright", RCmdPlayRight, " <filename>: Sets the file to be used" },
    { "play",      RCmdPlay,      " Plays currently set videos" },
};

const DWORD g_dwNumRemoteCommands = (sizeof(g_RemoteCommands)/sizeof(g_RemoteCommands[0]));

// Global buffer to receive remote commands from the debug console. Note that
// since this data is accessed by the app's main thread, and the debug monitor
// thread, we need to protect access with a critical section
static CHAR g_strRemoteBuf[MAXRCMDLENGTH];

// filepaths of vidoes to be played via debug channel request
CHAR    g_strRequestedA[MAX_PATH];
CHAR    g_strRequestedB[MAX_PATH];





//-----------------------------------------------------------------------------
// Name: CmdToArgv()
// Dsec: Parses a string into argv and return # of args.
//-----------------------------------------------------------------------------
int CmdToArgv( char* str, char* argv[], int maxargs )
{
    int   argc   = 0;
    int   argcT  = 0;
    char* strNil = str + strlen(str);

    while( argcT < maxargs )
    {
        // Eat whitespace
        while( *str && (*str == ' ') )
            str++;

        if( !*str )
        {
            argv[argcT++] = (char*)strNil;
        }
        else
        {
            // Find the end of this arg
            char chEnd = (*str == '"' || *str == '\'') ? *str++ : ' ';
            char *strArgEnd = str;
            while(*strArgEnd && (*strArgEnd != chEnd))
                strArgEnd++;

            // Record this bad boy
            argv[argcT++] = str;
            argc = argcT;

            // Move szArg to the next argument (or not)
            str = *strArgEnd ? strArgEnd + 1 : strArgEnd;
            *strArgEnd = 0;
        }
    }

    return argc;
}




// Temporary replacement for CRT string funcs, since
// we can't call CRT functions on the debug monitor
// thread right now.




//-----------------------------------------------------------------------------
// Name: dbgstrlen()
// Desc: Critical section safe strlen() function
//-----------------------------------------------------------------------------
int dbgstrlen( const CHAR* str )
{
    const CHAR* strEnd = str;

    while( *strEnd )
        strEnd++;

    return strEnd - str;
}




//-----------------------------------------------------------------------------
// Name: dbgtolower()
// Desc: Returns lowercase of char
//-----------------------------------------------------------------------------
inline CHAR dbgtolower( CHAR ch )
{
    if( ch >= 'A' && ch <= 'Z' )
        return ch - ( 'A' - 'a' );
    else
        return ch;
}




//-----------------------------------------------------------------------------
// Name: dbgstrnicmp()
// Desc: Critical section safe string compare.
//-----------------------------------------------------------------------------
BOOL dbgstrnicmp( const CHAR* str1, const CHAR* str2, int n )
{
    while( ( dbgtolower( *str1 ) == dbgtolower( *str2 ) ) && *str1 && n > 0 )
    {
        --n;
        ++str1;
        ++str2;
    }

    return( n == 0 || dbgtolower( *str1 ) == dbgtolower( *str2 ) );
}




//-----------------------------------------------------------------------------
// Name: dbgstrcpy()
// Desc: Critical section safe string copy
//-----------------------------------------------------------------------------
VOID dbgstrcpy( CHAR* strDest, const CHAR* strSrc )
{
    while( ( *strDest++ = *strSrc++ ) != 0 );
}

//-----------------------------------------------------------------------------
// Name: DebugConsoleCmdProcessor()
// Desc: Command notification proc that is called by the Xbox debug monitor to
//       have us process a command.  What we'll actually attempt to do is tell
//       it to make calls to us on a separate thread, so that we can just block
//       until we're able to process a command.
//
// Note: Do NOT include newlines in the response string! To do so will confuse
//       the internal WinSock networking code used by the debug monitor API.
//-----------------------------------------------------------------------------
HRESULT __stdcall DebugConsoleCmdProcessor( const CHAR* strCommand,
                                            CHAR* strResponse, DWORD dwResponseLen,
                                            PDM_CMDCONT pdmcc )
{
    
    // Skip over the command prefix and the exclamation mark
    strCommand += strlen(g_strDebugConsoleCommandPrefix) + 1;
    
        // Check if this is the initial connect signal
    if( dbgstrnicmp( strCommand, "__connect__", 11 ) )
    {
        // If so, respond that we're connected
        lstrcpynA( strResponse, "Connected.", dwResponseLen );
        return XBDM_NOERR;
    }

    // Check to see if the cmd exists
    BOOL bKnownCommand = FALSE;

    for( DWORD i = 0; i < g_dwNumRemoteCommands; i++ )
    {
        if( dbgstrnicmp( g_RemoteCommands[i].strCommand, strCommand, dbgstrlen( g_RemoteCommands[i].strCommand ) ) )
        {
            // If we find the string, copy it into the command buffer
            // to be examined by the polling function
            bKnownCommand = TRUE;
            break;
        }
    }

    if( bKnownCommand )
    {
        // g_strRemoteBuf needs to be protected by the critical section
        EnterCriticalSection( &g_CriticalSection );
        if( g_strRemoteBuf[0] )
        {
            // This means the application has probably stopped polling for debug commands
            dbgstrcpy( strResponse, "Cannot execute - previous command still pending" );
        }
        else
        {
            dbgstrcpy( g_strRemoteBuf, strCommand );
        }
        LeaveCriticalSection( &g_CriticalSection );
    }
    else
    {
        dbgstrcpy( strResponse, "unknown command" );
    }

    return XBDM_NOERR;
}

//-----------------------------------------------------------------------------
// Name: DebugConsoleHandleCommands()
// Desc: Poll routine called periodically (typically every frame) by the Xbox
//       app to see if there is a command waiting to be executed, and if so,
//       execute it.
//-----------------------------------------------------------------------------
BOOL DebugConsoleHandleCommands()
{
    static BOOL bInitialized = FALSE;
    CHAR* argv[10];
    int   argc;
    CHAR  strLocalBuf[MAXRCMDLENGTH]; // local copy of command

    // Initialize ourselves when we're first called.
    if( !bInitialized )
    {
        // Register our command handler with the debug monitor
        HRESULT hr = DmRegisterCommandProcessor( g_strDebugConsoleCommandPrefix, 
                                                 DebugConsoleCmdProcessor );
        if( FAILED(hr) )
            return FALSE;

        // We'll also need a critical section to protect access to g_strRemoteBuf
        InitializeCriticalSection( &g_CriticalSection );

        bInitialized = TRUE;
    }

    // If there's nothing waiting, return.
    if( !g_strRemoteBuf[0] )
        return FALSE;

    // Grab a local copy of the command received in the remote buffer
    EnterCriticalSection( &g_CriticalSection );

    lstrcpyA( strLocalBuf, g_strRemoteBuf );
    g_strRemoteBuf[0] = 0;

    LeaveCriticalSection( &g_CriticalSection );

    // Now parse the newly received command
    argc = CmdToArgv( strLocalBuf, argv, 10 );

    // Find the entry in our command list
    for( DWORD i = 0; i < g_dwNumRemoteCommands; i++ )
    {
        if( !lstrcmpiA( g_RemoteCommands[i].strCommand, argv[0] ) )
        {
            g_RemoteCommands[i].pfnHandler( argc, argv );
            break;
        }
    }

    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: DebugConsolePrintf()
// Desc: Asynchronous printf routine that sends the string to the remote debug
//       console
//-----------------------------------------------------------------------------
BOOL DebugConsolePrintf( const CHAR* strFormat, ... )
{
    // Copy command prefix into buffer
    CHAR strBuffer[MAXRCMDLENGTH];
    int length = _snprintf( strBuffer, MAXRCMDLENGTH, "%s!", g_strDebugConsoleCommandPrefix );

    // Format arguments
    va_list arglist;
    va_start( arglist, strFormat );
    _vsnprintf( strBuffer + length, MAXRCMDLENGTH - length, strFormat, arglist );
    va_end( arglist );

    // Send it out the string
    DmSendNotificationString( strBuffer );
    
    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: RCmdPlayLeft()
// Desc: Sets/Plays the left-hand video file
//-----------------------------------------------------------------------------
VOID RCmdPlayLeft( int argc, char* argv[] )
{
    // Check our arguments
    if( argc < 2 )
    {
        DebugConsolePrintf( "ERROR: Need to specify a filepath\n" );
        return;
    }

    strcpy(g_strRequestedA, argv[1]);

    // set the left hand video
    if ( !g_pxbApp->SetLeftVideo(g_strRequestedA) )
    {
        DebugConsolePrintf( "ERROR: Could not set left video\n" );
        return;
    }

}

//-----------------------------------------------------------------------------
// Name: RCmdPlayRight()
// Desc: Sets/Plays the right-hand video file
//-----------------------------------------------------------------------------
VOID RCmdPlayRight( int argc, char* argv[] )
{

    // Check our arguments
    if( argc < 2 )
    {
        DebugConsolePrintf( "ERROR: Need to specify a filepath\n" );
        return;
    }

    strcpy(g_strRequestedB, argv[1]);

    // set the left hand video
    if ( !g_pxbApp->SetRightVideo(g_strRequestedB) )
    {
        DebugConsolePrintf( "ERROR: Could not set right video\n" );
        return;
    }
}


//-----------------------------------------------------------------------------
// Name: RCmdPlayRight()
// Desc: Sets/Plays the right-hand video file
//-----------------------------------------------------------------------------
VOID RCmdPlay( int argc, char* argv[] )
{
    g_pxbApp->Play();
}
