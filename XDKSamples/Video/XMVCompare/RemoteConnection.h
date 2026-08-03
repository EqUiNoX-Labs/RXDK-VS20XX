#pragma once

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
// Callback handlers for the remote commands
//-----------------------------------------------------------------------------
void RCmdPlayLeft( int argc, char* argv[] );        // Help command
void RCmdPlayRight( int argc, char* argv[] );       // Set command
void RCmdPlay( int argc, char* argv[] );       // Set command