//-----------------------------------------------------------------------------
// File: XbRandName.h
//
// Desc: Generates a random and somewhat readable name. Useful for fake
//       player names, session names, etc.
//
// Hist: 10.10.01 - New for Nov release
//       11.07.02 - Changed to simpler, non-class interface
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef XBRANDNAME_H
#define XBRANDNAME_H

#include <xtl.h>




//-----------------------------------------------------------------------------
// Name: XBRandName_GetRandomName()
// Desc: Generates a random and somewhat readable name. Useful for fake
//       player names, session names, etc.
//-----------------------------------------------------------------------------
VOID XBRandName_GetRandomName( WCHAR* strName, DWORD dwSize );




#endif // XBRANDNAME_H
