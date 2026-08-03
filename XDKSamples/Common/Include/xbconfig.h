//-----------------------------------------------------------------------------
// File: XbConfig.h
//
// Desc: Config object wraps XQueryValue
//
// Hist: 03.02.01 - New for April XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef XBCONFIG_H
#define XBCONFIG_H

#include <xtl.h>




//-----------------------------------------------------------------------------
// Name: class CXBConfig
// Desc: Xbox configuration parameters
//-----------------------------------------------------------------------------
class CXBConfig
{
public:

    // Localized dates, times and numbers
    static VOID FormatDateTime( const FILETIME&, WCHAR* strDate, WCHAR* strTime,
                                BOOL bIncludeSeconds = FALSE );
    static VOID FormatInt( INT, WCHAR* strNumber );
    static VOID FormatFloat( DOUBLE, WCHAR* strNumber, INT nCount = 2,
                             BOOL bThousandsSep = TRUE );

    // Symbols based on current language
    static WCHAR GetDecimalSymbol();
    static WCHAR GetThousandsSeparator();
};




#endif // XBCONFIG_H
