//-----------------------------------------------------------------------------
// File: Common.h
//
// Desc: Include files, pragmas, macros, types and constants used across all 
//       source files
//
// Hist: 04.10.01 - New for May XDK release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef TECHCERTGAME_COMMON_H
#define TECHCERTGAME_COMMON_H
#include <xtl.h>
#include <XBUtil.h>
#include <cassert>




//-----------------------------------------------------------------------------
// #pragmas
//-----------------------------------------------------------------------------
#pragma warning( disable: 4714 )    // ignore function not inlined
#pragma warning( disable: 4786 )    // ignore warning about identifier truncation




//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------
#define USED( x )   static_cast<VOID>( x )  // avoid warning about unused var




#endif // TECHCERTGAME_COMMON_H
