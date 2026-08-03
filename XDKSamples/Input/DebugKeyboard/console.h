//-----------------------------------------------------------------------------
// File: Console.h
//
// Desc: Text console functions
//
// Hist: 05.31.01 - New
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef CONSOLE_H
#define CONSOLE_H




//-----------------------------------------------------------------------------
// Name: InitConsole()
// Desc: Initialises console
//-----------------------------------------------------------------------------
VOID InitConsole( CXBFont* pFont );




//-----------------------------------------------------------------------------
// Name: ShutdownConsole()
// Desc: Tidies up console
//-----------------------------------------------------------------------------
VOID ShutdownConsole();




//-----------------------------------------------------------------------------
// Name: OpenConsole()
// Desc: Sets the console to be active and initializes the current text line
//-----------------------------------------------------------------------------
VOID OpenConsole();




//-----------------------------------------------------------------------------
// Name: CloseConsole()
// Desc: Sets the console to be inactive
//-----------------------------------------------------------------------------
VOID CloseConsole();




//-----------------------------------------------------------------------------
// Name: ToggleConsole
// Desc: Switches between open and close
//-----------------------------------------------------------------------------
VOID ToggleConsole();




//-----------------------------------------------------------------------------
// Name: ClearCurrentTextLine()
// Desc: Clear the text buffer and reset the current cursor position within the
//            string
//-----------------------------------------------------------------------------
VOID ClearCurrentTextLine();




//-----------------------------------------------------------------------------
// Name: ClearConsole()
// Desc: Clear console buffer
//-----------------------------------------------------------------------------
VOID ClearConsole();




//-----------------------------------------------------------------------------
// Name: IsConsoleActive()
// Desc: Returns whether the console is currently active or not
//-----------------------------------------------------------------------------
BOOL IsConsoleActive();




//-----------------------------------------------------------------------------
// Name: DrawConsole()
// Desc: Draws the console on top of the screen
//-----------------------------------------------------------------------------
VOID DrawConsole();




//-----------------------------------------------------------------------------
// Name: ProcessConsole()
// Desc: Get keypress and perform any necessary commands
//-----------------------------------------------------------------------------
VOID ProcessConsole();




#endif

