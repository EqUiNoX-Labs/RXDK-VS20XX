//-----------------------------------------------------------------------------
// File: Text.cpp
//
// Desc: All text in single place to simplify localization
//
// Hist: 04.10.01 - New for May XDK release 
//       09.24.02 - Modified to use file-based localized strings
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <stdio.h>
#include "Text.h"




//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
WCHAR* g_StringArray[TEXT_NUMSTRINGS];




//-----------------------------------------------------------------------------
// Name: LoadStrings()
// Desc: Loads localized strings for the app
//-----------------------------------------------------------------------------
HRESULT LoadStrings( DWORD dwLanguage )
{
    CHAR* strFilename = "d:\\Media\\Strings\\English.txt";
    switch( dwLanguage )
    {
        case XC_LANGUAGE_FRENCH:   strFilename = "d:\\Media\\Strings\\French.txt";   break;
        case XC_LANGUAGE_GERMAN:   strFilename = "d:\\Media\\Strings\\German.txt";   break;
        case XC_LANGUAGE_ITALIAN:  strFilename = "d:\\Media\\Strings\\Italian.txt";  break;
        case XC_LANGUAGE_JAPANESE: strFilename = "d:\\Media\\Strings\\Japanese.txt"; break;
        case XC_LANGUAGE_SPANISH:  strFilename = "d:\\Media\\Strings\\Spanish.txt";  break;
    }

    // Read in the strings file
    FILE* file = fopen( strFilename, "rb" );
    if( NULL == file )
        return E_FAIL;
    
    // Get file size (minus 2 bytes for the unicode marker)
    fseek( file, -2, SEEK_END );
    DWORD dwFileSize = ftell( file );
    fseek( file, 2, SEEK_SET );

    static WCHAR* g_pStrings = NULL;
    if( g_pStrings )
        delete [] g_pStrings;
    g_pStrings = (WCHAR*)new BYTE[dwFileSize];
    fread( g_pStrings, 1, dwFileSize, file );
    fclose( file );

    // Assign the strings
    WCHAR* pSrc = g_pStrings;

    for( DWORD i=0; i<TEXT_NUMSTRINGS; i++ )
    {
        g_StringArray[i] = pSrc;

        // Skip to the next string
        WCHAR* pDst = pSrc;
        for(;;)
        {
            if( pSrc[0] == L'\r' && pSrc[1] == L'\n' )
            {
                *pDst++ = 0;
                pSrc += 2;
                break;
            }
            if( pSrc[0] == L'\\' && pSrc[1] == L'n' )
            {
                *pDst++ = L'\n';
                pSrc += 2;
            }
            else
            {
                *pDst++ = *pSrc++;
            }
        }
    }
    return S_OK;
}




