//-------------------------------------------------------------------------------------
// File: Common.h
//
// Desc: Holds common definitions used by all the sample's modules.
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef COMMON_H
#define COMMON_H

#include <xtl.h>
#include <XBFont.h>
#include <xonline.h>

#include "XBOnlineTask.h"

// Common colors
static const D3DCOLOR COLOR_YELLOW    = 0xffffff00; // Yellow
static const D3DCOLOR COLOR_GREEN     = 0xff00ff00; // Green
static const D3DCOLOR COLOR_WHITE     = 0xffffffff; // White
static const D3DCOLOR COLOR_RED       = 0xffff0000; // Red
static const D3DCOLOR COLOR_BLUE      = 0xff0a0a6a; // Blue
static const D3DCOLOR COLOR_BLACK     = 0xff000000; // Black
static const D3DCOLOR COLOR_GREY      = 0xff999999; // Grey
static const D3DCOLOR COLOR_CLEAR     = 0x00000000; // Clear
static const D3DCOLOR COLOR_BROWN     = 0xFF4F3F0F; // Brown

// Default UI colors

static const D3DCOLOR COLOR_NORMAL    = COLOR_WHITE;
static const D3DCOLOR COLOR_HIGHLIGHT = COLOR_YELLOW;
static const D3DCOLOR COLOR_POINTER   = COLOR_GREEN;


// Time constants

const DWORD SECONDS_PER_MINUTE        = 60;
const DWORD SECONDS_PER_HOUR          = 60 * SECONDS_PER_MINUTE;
const DWORD SECONDS_PER_DAY           = SECONDS_PER_HOUR * 24;


// UI size constants

const FLOAT SCREEN_SIZE_X          = 640.0f;
const FLOAT SCREEN_SIZE_Y          = 480.0f;
const FLOAT SCREEN_CENTER_X        = ( SCREEN_SIZE_X * 0.5f );
const FLOAT SCREEN_CENTER_Y        = ( SCREEN_SIZE_Y * 0.5f );


// Save game constants

const INT         MAX_SERVER_PATH_SIZE = 128;
const INT         MAX_SAVE_PATH_SIZE   = 64;
const FLOAT       ICON_SIZE            = 32.0f;
const FLOAT       BITMAP_SIZE          = 16.0f;
const CHAR* const SAVE_DRIVE           = "u:\\";
static WCHAR*     TEAM_LOGO_FILENAME   = L"logo";



// Matchmaking constants

const INT MAX_USERS                = 4;
const INT MAX_XBOXES               = 4;
const INT MAX_MATCHERS             = MAX_USERS * MAX_XBOXES;

// XOnlineStorageDownloadToMemory requires the receive buffer to be at least
// 750 bytes
const DWORD MIN_XONLINE_DOWNLOAD_BUFFER_SIZE = 750;

// Now, add whatever services are appropriate for your title, but no
// more. Each service requires additional authentication time
// and network traffic.  For demonstration purposes, the
// matchmaking service is specified.  Additional services ids are
// specified in xonline.h.

const DWORD SERVICES[]   = { XONLINE_STRING_SERVICE,
                             XONLINE_MATCHMAKING_SERVICE,
                             XONLINE_ARBITRATION_SERVICE,
                             XONLINE_QUERY_SERVICE,
                             XONLINE_TEAM_SERVICE,
                             XONLINE_STORAGE_SERVICE,
                             XONLINE_MESSAGING_SERVICE,
                             XONLINE_STATISTICS_SERVICE };

const DWORD NUM_SERVICES = sizeof( SERVICES ) / sizeof( SERVICES[0] );


extern LPDIRECT3DDEVICE8 g_pd3dDevice;
extern CXBFont*          g_pFont;

VOID RenderWorkingScreen();


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


VOID BootToDash( DWORD dwReason );

BOOL WaitForTaskToComplete( CXBOnlineTask& Task,
                            HRESULT* pHR,
                            BOOL bRenderWorking = FALSE );

FILETIME AddSeconds( FILETIME startTime, ULONGLONG qwDeltaSeconds );

// Get the local XONLINE_USER struct for the specified player
// id. This function doesn't handle remote users.
XONLINE_USER GetUserStruct( ULONGLONG playerID,
                            XONLINE_USER* rwUserList,
                            DWORD dwUserCount );


// Icon/sprite helper functions

LPDIRECT3DVERTEXBUFFER8 CreateFace( LPDIRECT3DDEVICE8 pd3dDevice,
                                    FLOAT fX,
                                    FLOAT fY );

VOID SetFacePos( LPDIRECT3DVERTEXBUFFER8 pVerts,
                 FLOAT fX,
                 FLOAT fY );

VOID TranslateFace( LPDIRECT3DVERTEXBUFFER8 pVerts,
                    FLOAT fX,
                    FLOAT fY );

VOID RenderSprite ( LPDIRECT3DDEVICE8 pd3dDevice,
                    LPDIRECT3DVERTEXBUFFER8 pVerts,
                    LPDIRECT3DTEXTURE8 pTexture );

#endif // COMMON_H