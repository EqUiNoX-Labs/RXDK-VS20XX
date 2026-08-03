//-----------------------------------------------------------------------------
// File: DebugCmd.h
//
// Desc: Header file for communicating with a remote debug console. Please read
//       the comments in the DebugCmd.cpp file for more info on the API.
//
//       This header defines the following arrays:
//
//          g_RemoteCommands -  This is the list of commands your application provides.
//                              Note that "help" and "set" are provided automatically
//                              This is implemented in DebugCmd.cpp
//
//          g_RemoteVariables - This is a list of variables that your application
//                              exposes. They can be examined and modified by the
//                              remote debug console with the "set" command. 
//                              This is implemented in DebugChannel.cpp
//
// Hist: 02.05.01 - Initial creation for March XDK release
//       08.21.02 - Revision and code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef DEBUGCMD_H
#define DEBUGCMD_H




//-----------------------------------------------------------------------------
// Callback handlers for the remote commands
//-----------------------------------------------------------------------------
void RCmdHelp( int argc, char* argv[] );        // Help command
void RCmdSet( int argc, char* argv[] );         // Set command
void RCmdTexture( int argc, char* argv[] );     // Sets the texture
void RCmdSpin( int argc, char* argv[] );        // Sets the spin velocity
void RCmdLightChange( void* );                  // Notified on light changes



#define MAXRCMDLENGTH       256                 // Size of the remote cmd buffer

typedef void (*RCMDHANDLER)(int argc, char *argv[]);

// Command definition structure
struct REMOTE_COMMAND
{
    CHAR*       strCommand;                    // Name of command
    RCMDHANDLER pfnHandler;                    // Handler function
    CHAR*       strHelp;                       // Description of command
};

// Array of remote commands
extern const REMOTE_COMMAND g_RemoteCommands[];
extern const DWORD          g_dwNumRemoteCommands;




//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------

enum REMOTEVARDATATYPES { SDOS_BOOL, SDOS_WORD, SDOS_INT, SDOS_FLOAT };
typedef void (*DCCMDSETNOTIF)( VOID* pAddr );

// Definition structure for app variables that are exposed for remote viewing
// and modification by the debug console
struct REMOTE_VARIABLE
{
    const CHAR*        strName;               // Name of variable
    VOID*              pAddr;                 // Address of variable
    REMOTEVARDATATYPES ddtDataType;           // Data type of variable
    DCCMDSETNOTIF      pfnNotifFunc;          // Function to call upon change
};

// List of exposed application variables.
extern const REMOTE_VARIABLE g_RemoteVariables[];
extern const DWORD           g_dwNumRemoteVariables;




//-----------------------------------------------------------------------------
// Misc
//-----------------------------------------------------------------------------

// Handle any remote commands that have been sent - this should be called
// periodically by the application
BOOL DebugConsoleHandleCommands();

// Asynchronous printf - this is used to send responses back to the debug console
BOOL DebugConsolePrintf( const CHAR* strFormat, ... );




#endif // DEBUGCMD_H