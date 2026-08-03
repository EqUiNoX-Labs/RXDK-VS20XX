//-----------------------------------------------------------------------------
// File: DebugCmd.cpp
//
// Desc: Helps an application expose functionality through the debug channel
//       to a debug console running on a remote dev machine.
//
//       Commands are sent from the remote debug console through the debug
//       channel to the debug monitor on the Xbox machine.  The Xbox machine
//       receives the commands on a separate thread through a registered command 
//       processor callback function. The callback function will store commands
//       in a buffer, and the app should poll this buffer once per frame and
//       then decipher and handle the commands.
//
// Hist: 02.05.01 - Initial creation for March XDK release
//       08.21.02 - Revision and code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xbdm.h>
#include <stdio.h>
#include "DebugCmd.h"




// Command prefix for things sent across the dubg channel
static const CHAR g_strDebugConsoleCommandPrefix[] = "XCMD";


// Global buffer to receive remote commands from the debug console. Note that
// since this data is accessed by the app's main thread, and the debug monitor
// thread, we need to protect access with a critical section
static CHAR g_strRemoteBuf[MAXRCMDLENGTH];


// The critical section used to protect data that is shared between threads
static CRITICAL_SECTION g_CriticalSection;




// List of app-defined remote commands
const REMOTE_COMMAND g_RemoteCommands[] =
{
    // Command,  Handler,     Help string
    { "help",    RCmdHelp,    " [CMD]: List commands / usage" },
    { "set",     RCmdSet,     " var [=] <val>: set a variable" },
    { "texture", RCmdTexture, " <filename>: Sets the texture to be used" },
    { "spin",    RCmdSpin,    " <rad/s>: Sets spin velocity in radians per second" },
};

const DWORD g_dwNumRemoteCommands = (sizeof(g_RemoteCommands)/sizeof(g_RemoteCommands[0]));




//-----------------------------------------------------------------------------
// Name: RCmdHelp()
// Desc: Callback handler for remote "help" command. Iterates over the list of
//       commands and displays a help string for each one
//-----------------------------------------------------------------------------
VOID RCmdHelp( int argc, char* argv[] )
{
    for( DWORD i = 0; i < g_dwNumRemoteCommands; i++ )
    {
        DebugConsolePrintf( "%s\t%s\n", g_RemoteCommands[i].strCommand,
                                        g_RemoteCommands[i].strHelp );
    }
}




//-----------------------------------------------------------------------------
// Name: RCmdSet()
// Desc: Callback handler for  remote "set" command. This function sets or
//       displays values of variables exposed by the app to the debug console
//-----------------------------------------------------------------------------
VOID RCmdSet( int argc, char* argv[] )
{
    // If we aren't passed any arguments, then just list all the variables and
    // what their current values are.
    if( argc == 1 )
    {
        for( DWORD nIndex = 0; nIndex < g_dwNumRemoteVariables; nIndex++ )
        {
            const REMOTE_VARIABLE* pCommandVarDef = &g_RemoteVariables[nIndex];

            switch( pCommandVarDef->ddtDataType )
            {
                case SDOS_BOOL:
                    DebugConsolePrintf( "%s\t= %d\n", pCommandVarDef->strName,
                                                      *(BOOL*)pCommandVarDef->pAddr );
                    break;
                case SDOS_INT:
                    DebugConsolePrintf( "%s\t= %d\n", pCommandVarDef->strName,
                                                      *(INT*)pCommandVarDef->pAddr );
                    break;
                case SDOS_WORD:
                    DebugConsolePrintf( "%s\t= %d\n", pCommandVarDef->strName,
                                                      *(WORD*)pCommandVarDef->pAddr );
                    break;
                case SDOS_FLOAT:
                    DebugConsolePrintf( "%s\t= %0.1f\n", pCommandVarDef->strName,
                                                         *(FLOAT*)pCommandVarDef->pAddr );
                    break;
            }
        }

        return;
    }

    // Else, set the variable to the amount specified    
    
    // If the user did a set "foo = 2" move arg3 to arg2
    if( argv[2][0] == '=' )
        argv[2] = argv[3];

    // Find the entry for this variable, if we can
    for( DWORD nIndex = 0; nIndex < g_dwNumRemoteVariables; nIndex++ )
    {
        const REMOTE_VARIABLE* pCommandVarDef = &g_RemoteVariables[nIndex];

        if( !lstrcmpiA( argv[1], pCommandVarDef->strName ) )
        {
            CHAR* endptr;
            DWORD dwVal = (argv[2][0] == '0' && argv[2][1] == 'x') ?
                          strtoul(argv[2], &endptr, 16) : atoi(argv[2]);

            // Set the appropriate type of data
            VOID* pAddr = pCommandVarDef->pAddr;
            
            switch( pCommandVarDef->ddtDataType )
            {
                case SDOS_BOOL:
                    DebugConsolePrintf( "set %s = %d\n", argv[1], *(BOOL*)pAddr = !!dwVal );
                    break;
                case SDOS_INT:
                    DebugConsolePrintf( "set %s = %d\n", argv[1], *(INT*)pAddr = dwVal );
                    break;
                case SDOS_WORD:
                    DebugConsolePrintf( "set %s = %d\n", argv[1], *(WORD*)pAddr = (WORD)dwVal );
                    break;
                case SDOS_FLOAT:
                    DebugConsolePrintf( "set %s = %0.1f\n", argv[1], *(FLOAT*)pAddr = (float)atof(argv[2]) );
                    break;
            }

            // Call the notif func if there was one
            if( pCommandVarDef->pfnNotifFunc )
                pCommandVarDef->pfnNotifFunc( pAddr );

            return;
        }
    }

    // If we get here, print a message saying we couldn't find the variable
    DebugConsolePrintf( "variable '%s' not found\n", argv[1] );
}




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

