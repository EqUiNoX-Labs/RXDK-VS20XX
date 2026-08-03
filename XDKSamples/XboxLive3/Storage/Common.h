//-------------------------------------------------------------------------------------
// File: Common.h
//
// Desc: Holds common definitions used by the Storage sample.
//
// Hist: 08.10.04 - New for Sept release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef COMMON_H
#define COMMON_H

#include <XBOnlineTask.h>

// Common colors
static const D3DCOLOR COLOR_YELLOW    = 0xffffff00; // Yellow
static const D3DCOLOR COLOR_GREEN     = 0xff00ff00; // Green
static const D3DCOLOR COLOR_WHITE     = 0xffffffff; // White
static const D3DCOLOR COLOR_RED       = 0xffff0000; // Red
static const D3DCOLOR COLOR_BLUE      = 0xff0a0a6a; // Blue
static const D3DCOLOR COLOR_BLACK     = 0xff000000; // Black
static const D3DCOLOR COLOR_GREY      = 0xff999999; // Grey
static const D3DCOLOR COLOR_CLEAR     = 0x00000000; // Clear

const INT         MAX_SERVER_PATH_SIZE  = 128;      // Buffer size for server path
const INT         MAX_SAVE_PATH_SIZE    = 64;       // Buffer size for save game name
const CHAR* const SAVE_DRIVE            = "u:\\";   // Drive that saved games are on


// Constant names for the global content files used to 
// display messages of the day and to change UI settings
const WCHAR* const FILENAME_MOTD        = L"motd.txt";
const WCHAR* const FILENAME_COLORS      = L"colors.txt";
const WCHAR* const FILENAME_MOTD_ICON   = L"titleicon";


// XOnlineStorageDownloadToMemory requires the receive buffer to be at least
// 750 bytes
const DWORD MIN_XONLINE_DOWNLOAD_BUFFER_SIZE = 750;


// Add whatever services are appropriate for your title, but no
// more. Each service requires additional authentication time
// and network traffic.  For demonstration purposes, the
// matchmaking service is specified.  Additional services ids are
// specified in xonline.h.    
const DWORD SERVICES[]   = { XONLINE_STRING_SERVICE,
                             XONLINE_TEAM_SERVICE,
                             XONLINE_STORAGE_SERVICE,
                             XONLINE_SIGNATURE_SERVICE,
                             XONLINE_MESSAGING_SERVICE,
                             XONLINE_STATISTICS_SERVICE };

const DWORD NUM_SERVICES = sizeof( SERVICES ) / sizeof( SERVICES[0] );


//-------------------------------------------------------------------------------------
// Name: struct CUSTOMVERTEX
// Desc: For background vertex buffer
//-------------------------------------------------------------------------------------
struct CUSTOMVERTEX
{
    D3DXVECTOR4 p;
    D3DXVECTOR2 t;
};

const DWORD D3DFVF_CUSTOMVERTEX = D3DFVF_XYZRHW | D3DFVF_TEX1; // see CUSTOMVERTEX


//-------------------------------------------------------------------------------------
// General utility functions
//-------------------------------------------------------------------------------------

VOID BootToDash( DWORD dwReason );

BOOL WaitForTaskToComplete( CXBOnlineTask& Task, HRESULT* pHR );


#endif // COMMON_H