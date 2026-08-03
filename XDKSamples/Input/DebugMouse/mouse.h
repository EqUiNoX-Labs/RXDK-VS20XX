//-----------------------------------------------------------------------------
// File: mouse.h
//
// Desc: Handles debug keyboard.
//
// Hist: 04.07.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef MOUSE_H
#define MOUSE_H

// Enable a debug mouse if one is attached to the Xbox Debug/Dev Kit.
#define DEBUG_MOUSE





#include <xtl.h>




//-----------------------------------------------------------------------------
// Name: XBInput_InitDebugMouse()
// Desc: Initialise Debug Mouse for use
//-----------------------------------------------------------------------------
HRESULT XBInput_InitDebugMouse();




//-----------------------------------------------------------------------------
// Name: XBInput_GetMouseInput()
// Desc: Processes input from a debug mouse.  Returns mask values for the 
//       ports that the mouse state changed on.
//       (i.e. someone moved the mouse or pushed a mouse button)
//-----------------------------------------------------------------------------
DWORD XBInput_GetMouseInput();




// Stores tha actual mouse data.
extern XINPUT_STATE             g_MouseState[4];




#endif

