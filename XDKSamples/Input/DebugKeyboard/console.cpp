//-----------------------------------------------------------------------------
// File: Console.cpp
//
// Desc: Text console functions
//
// Hist: 05.31.01 - New
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <stdio.h>
#include <XBFont.h>
#include <XBUtil.h>
#include "keyboard.h"
#include "commands.h"
#include "console.h"


static VOID ClearBuffer();
static VOID Execute();
static VOID AddCurrentStringToBuffer();

// Internal functions to link to console
static VOID RebootFunction();
static VOID ClearFunction();
static VOID HelpFunction();


// Boolean to indicate whether the console is active
static BOOL     g_bConsoleActive = FALSE;

// Table of strings holding the previous 10 command lines.
// Previous command recall is not implemented and if it was we would have to
// implement two buffers, one for the command line as below and an additional
// screen buffer to hold the commands and output feedback
#define NUM_TEXTLINES    10
#define TEXTLINE_LENGTH  80

static WCHAR    g_strTextLine[NUM_TEXTLINES][TEXTLINE_LENGTH];
static INT      g_iTextLineInsertIndex = 0;

// Current text buffer
static WCHAR    g_strCurrentTextLine[TEXTLINE_LENGTH];
static INT      g_iCurrentCursorPosition;

// Font for console
static CXBFont* g_pFont = NULL;




//-----------------------------------------------------------------------------
// Name: InitConsole()
// Desc: Initializes console
//-----------------------------------------------------------------------------
VOID InitConsole( CXBFont* pFont )
{
    g_bConsoleActive = FALSE;
    g_pFont          = pFont;

    ClearBuffer();
    ClearCurrentTextLine();

    // Setup command structure for holding commands and functions to call
    InitCommands();
    AddCommand( L"reboot", RebootFunction );
    AddCommand( L"clear",  ClearFunction );
    AddCommand( L"help",   HelpFunction );
}




//-----------------------------------------------------------------------------
// Name: OpenConsole()
// Desc: Sets the console to be active and initializes the current text line
//-----------------------------------------------------------------------------
VOID OpenConsole()
{
    // If we are already open then just return
    if( g_bConsoleActive == TRUE )
        return;

    // Open the keyboard device if it requires it, this allows us to share the
    // keyboard with other areas of the game
    if( FAILED( XBInput_InitDebugKeyboard() ) )
        return;

    // Set the console window to be active and clear the current text line
    g_bConsoleActive = TRUE;

    ClearCurrentTextLine();
}




//-----------------------------------------------------------------------------
// Name: CloseConsole()
// Desc: Sets the console to be inactive
//-----------------------------------------------------------------------------
VOID CloseConsole()
{
    // Remove any existing text from the input line
    ClearCurrentTextLine();

    g_bConsoleActive = FALSE;
}




//-----------------------------------------------------------------------------
// Name: ToggleConsole()
// Desc: Switches between open and close
//-----------------------------------------------------------------------------
VOID ToggleConsole()
{
    if( g_bConsoleActive )
        CloseConsole();
    else
        OpenConsole();
}




//-----------------------------------------------------------------------------
// Name: IsConsoleActive()
// Desc: Returns whether the console is currently active or not
//-----------------------------------------------------------------------------
BOOL IsConsoleActive()
{
    return g_bConsoleActive;
}




//-----------------------------------------------------------------------------
// Name: ClearCurrentTextLine()
// Desc: Clear the text buffer and reset the current cursor position within the
//       string
//-----------------------------------------------------------------------------
VOID ClearCurrentTextLine()
{
    g_iCurrentCursorPosition = 0;

    ZeroMemory( g_strCurrentTextLine, sizeof(g_strCurrentTextLine) );
}




//-----------------------------------------------------------------------------
// Name: ClearConsole()
// Desc: Clear console buffer and current input string
//-----------------------------------------------------------------------------
VOID ClearConsole()
{
    ClearBuffer();
    ClearCurrentTextLine();
}




//-----------------------------------------------------------------------------
// Name: ProcessConsole()
// Desc: Get keypress and perform any necessary commands
//-----------------------------------------------------------------------------
VOID ProcessConsole()
{
    // handle input from keyboard
    CHAR cInputKey = XBInput_GetKeyboardInput();

    if( cInputKey != '\0' )
    {
        // First handle a reserved key, in this case either the ESC character to
        // close the console or the TAB key to attempt command line completion.
        // Then check for return, anything else must be an ASCII value in this
        // sample. This could be expanded to handle all buttons
        switch( cInputKey )
        {
            case DELETE_KEY:
            {
                    if( g_iCurrentCursorPosition != 0 )
                {
                    g_iCurrentCursorPosition--;
                    g_strCurrentTextLine[ g_iCurrentCursorPosition ] = '\0';
                }
                break;
            }

            case RETURN_KEY:
            {
                Execute();
                break;
            }

            case TAB_KEY:
            {
                // Pass in whole of buffer string to find name match. If match is
                // returned then copy it into the current buffer
                WCHAR* strMatchedName = CompleteCommand( g_strCurrentTextLine );

                if( strMatchedName )
                {
                    wcsncpy( g_strCurrentTextLine, strMatchedName, wcslen( strMatchedName ) );
                    g_iCurrentCursorPosition = wcslen( g_strCurrentTextLine );
                }
                break;
            }
            
            case ESC_KEY:
            {
                CloseConsole();
                break;
            }
            
            default:
            {
                // Not really necessary to check value here, as we are using
                // the ASCII value returned by the keyboard, so it should be
                // some form of printable character
                if( g_iCurrentCursorPosition < TEXTLINE_LENGTH - 1 )
                {
                    g_strCurrentTextLine[ g_iCurrentCursorPosition ] = cInputKey;
                    g_iCurrentCursorPosition++;
                }
                break;
            }
        }
    }
}




//-----------------------------------------------------------------------------
// Name: DrawConsole()
// Desc: Draws the console on top of the screen
//-----------------------------------------------------------------------------
VOID DrawConsole()
{
    if( !g_bConsoleActive )
        return;

    // Draw a background
    XBUtil_DrawRect( 0, 0, 640, 241, 0x90000000, 0xffffff00 );

    // Draw the current text line after a prompt
    g_pFont->DrawText( 50, 220, 0xffffff00, L"> " );
    g_pFont->DrawText( 0xffffff00, g_strCurrentTextLine );

    // Draw the command history
    for( INT i = 0; i < NUM_TEXTLINES; i++ )
    {
        WCHAR* strText = g_strTextLine[(g_iTextLineInsertIndex+i) % NUM_TEXTLINES];
        g_pFont->DrawText( 40.0f, 20.0f+(20.0f*i), 0xffffffff, strText );
    }
}




//-----------------------------------------------------------------------------
// Name: ShutdownConsole()
// Desc: Tidies up console
//-----------------------------------------------------------------------------
VOID ShutdownConsole()
{
    RemoveAllCommands();

    g_bConsoleActive = FALSE;

    ClearBuffer();
    ClearCurrentTextLine();

    g_pFont = NULL;
}




//-----------------------------------------------------------------------------
// Name: Execute()
// Desc: Processes return key pressed, checks whether input is a command and
//       whether to execute the command, then adds command to the command
//       history buffer
//-----------------------------------------------------------------------------
VOID Execute()
{
    // Check that we have a string of some info first, if not skip next two
    // operations
    if( wcslen( g_strCurrentTextLine ) != 0 )
    {
        // First try to find command and execute that, because command may
        // effect console, if not command than just display text
        FindAndExecuteCommand( g_strCurrentTextLine );
    }

    // Then add current string to keyboard buffer
    AddCurrentStringToBuffer();
}




//-----------------------------------------------------------------------------
// Name: AddCurrentStringToBuffer()
// Desc: Copies last string input into the correct location in the command
//       history buffer
//-----------------------------------------------------------------------------
VOID AddCurrentStringToBuffer()
{
    // We just keep a track of the index which saves us having to copy the
    // whole buffer around, but does mean we need to use extra logic to
    // display on screen

    ZeroMemory( g_strTextLine[g_iTextLineInsertIndex], sizeof(g_strTextLine[g_iTextLineInsertIndex]) );

    wcsncpy( g_strTextLine[g_iTextLineInsertIndex], g_strCurrentTextLine, TEXTLINE_LENGTH );

    // Move table pointer
    g_iTextLineInsertIndex = (g_iTextLineInsertIndex+1) % NUM_TEXTLINES;

    // Clear string we are about to overwrite, then add current string
    ClearCurrentTextLine();
}




//-----------------------------------------------------------------------------
// Name: ClearBuffer()
// Desc: Resets all of the command history buffer
//-----------------------------------------------------------------------------
VOID ClearBuffer()
{
    for( int i = 0; i < NUM_TEXTLINES; i++ )
    {
        ZeroMemory( g_strTextLine[i], sizeof(g_strTextLine[i]) );
    }

    g_iTextLineInsertIndex = 0;
}




//-----------------------------------------------------------------------------
// Name: RebootFunction()
// Desc: Internal function to call from console
//-----------------------------------------------------------------------------
VOID RebootFunction()
{
    XLaunchNewImage( NULL, NULL );
}




//-----------------------------------------------------------------------------
// Name: ClearFunction()
// Desc: Internal function to call from console
//-----------------------------------------------------------------------------
VOID ClearFunction()
{
    ClearConsole();
}




//-----------------------------------------------------------------------------
// Name: HelpFunction()
// Desc: Internal function to call from console
//-----------------------------------------------------------------------------
VOID HelpFunction()
{
    // Then add current string to keyboard buffer
    AddCurrentStringToBuffer();

    wcscpy( g_strCurrentTextLine, L"   reboot - Reboots the system" );
    AddCurrentStringToBuffer();
    wcscpy( g_strCurrentTextLine, L"   clear  - Clears the console" );
    AddCurrentStringToBuffer();
    wcscpy( g_strCurrentTextLine, L"   help   - Displays list of commands" );
}
