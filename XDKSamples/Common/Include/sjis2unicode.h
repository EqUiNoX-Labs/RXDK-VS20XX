//-----------------------------------------------------------------------------
// File: Sjis2unicode.h
//
// Desc: Unicode string engine header
//
// Hist: 5.05.02 - New for June release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef __SJIS2UNICODE_H__
#define __SJIS2UNICODE_H__

#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif

__declspec(dllexport) size_t __stdcall Sjis2Unicode(wchar_t*, const char*, const size_t);

#endif
