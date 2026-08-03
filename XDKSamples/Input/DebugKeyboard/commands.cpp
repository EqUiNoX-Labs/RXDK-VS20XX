//-----------------------------------------------------------------------------
// File: Commands.cpp
//
// Desc: Functions to control a list of commands and functions assigned to the
//       console
//
// Hist: 06.11.01 - New
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include "commands.h"


// Linked list structure for holding command list
struct COMMANDS_STRUCT
{
    WCHAR*           strName;
    COMMAND_FUNCTION fnCommand;
    COMMANDS_STRUCT* pNext;
};

static COMMANDS_STRUCT* g_pCommands;




//-----------------------------------------------------------------------------
// Name: InitCommands()
// Desc: Just ensures command list is set correctly
//-----------------------------------------------------------------------------
VOID InitCommands()
{
    g_pCommands = NULL;
}




//-----------------------------------------------------------------------------
// Name: AddCommand()
// Desc: Adds the command address and name into a list of commands, ensuring
//       that the command does not already exist in the list.
//-----------------------------------------------------------------------------
VOID AddCommand( WCHAR* strNewCommandName, COMMAND_FUNCTION fnNewCommand )
{
    COMMANDS_STRUCT* pNewCommand;
    
    // Ensure that the command isn't previously defined
    for( pNewCommand = g_pCommands; pNewCommand; pNewCommand = pNewCommand->pNext )
    {
        if( wcscmp( strNewCommandName, pNewCommand->strName ) == 0 )
        {
//          DebugMessageString( __FILE__, __LINE__, WARNING, "AddCommand() failed, command already exists" );
            return;
        }
    }

    // We went through the entire command list without finding this command,
    // so add it to the list.
    pNewCommand = (COMMANDS_STRUCT*)malloc( sizeof(COMMANDS_STRUCT) );
    if( pNewCommand == NULL )
    {
//      DebugMessageString( __FILE__, __LINE__, ERROR, "AddCommand() failed, command malloc returned NULL" );
        return;
    }
    
    pNewCommand->strName   = strNewCommandName;
    pNewCommand->fnCommand = fnNewCommand;
    pNewCommand->pNext     = g_pCommands;
    
    g_pCommands = pNewCommand;
}




//-----------------------------------------------------------------------------
// Name: RemoveAllCommands()
// Desc: 
//-----------------------------------------------------------------------------
VOID RemoveAllCommands()
{
//  g_pCommands = NULL;
//  #pragma message(Reminder "Complete remove all commands !")
}




//-----------------------------------------------------------------------------
// Name: RemoveCommand()
// Desc: 
//-----------------------------------------------------------------------------
VOID RemoveCommand( char *i_szCommandName )
{
    // Need to track for the special case of start and end of the list
//  g_pCommands = NULL;
//  #pragma message(Reminder "Complete remove command !")
}




//-----------------------------------------------------------------------------
// Name: CompleteCommand()
// Desc: Called to see if we have a command in the list that matches the partial
//       name passed in
//-----------------------------------------------------------------------------
WCHAR* CompleteCommand( WCHAR* strPartialCommand )
{
    INT wPartialCommandLength = wcslen( strPartialCommand );
    
    if( wPartialCommandLength == 0 )
        return NULL;
        
    for( COMMANDS_STRUCT* cmd = g_pCommands; cmd; cmd = cmd->pNext )
    {
        if( wcsncmp( strPartialCommand, cmd->strName, wPartialCommandLength ) == 0 )
            return cmd->strName;
    }

    return NULL;
}




//-----------------------------------------------------------------------------
// Name: FindAndExecuteCommand()
// Desc: Parses list of commands and executes function if available
//-----------------------------------------------------------------------------
VOID FindAndExecuteCommand( WCHAR* strCommand )
{
    INT wCommandLength = wcslen( strCommand );
    
    if( 0 == wCommandLength )
        return;
        
    for( COMMANDS_STRUCT* cmd = g_pCommands; cmd; cmd = cmd->pNext )
    {
        if( wcsncmp( strCommand, cmd->strName, wCommandLength ) == 0 )
        {
            if( cmd->fnCommand != NULL )
                cmd->fnCommand();
            return;
        }
    }
}


