//-----------------------------------------------------------------------------
// File: XMVCompare.h
//
// Desc: Declarations and Definitions for XMVCompare sample.
//
//
// Hist:    11.20.03 - Added Remote Connection stuff for Win32 tool
//          07.30.03 - New for November 2003 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


#pragma once


#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include <XBResource.h>
#include <xmv.h>
#include <xbdm.h>
#include "XMVHelper.h"
#include "RemoteConnection.h"

#define MAX_FILES               20
#define MAX_FILENAME_LENGTH     128
#define VIDEO_PLAYBACK_A        1
#define VIDEO_PLAYBACK_B        2
#define VIDEO_PLAYBACK_BOTH     3


//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont             m_Font;
    CXBPackedResource   m_xprResource;
    CXBHelp             m_Help;
    CXMVPlayer          m_PlayerA;
    CXMVPlayer          m_PlayerB;

    BOOL                m_bDrawHelp;
    BOOL                m_bPause;
    BOOL                m_bVideoChanged;
    BOOL                m_bDisplayText;

    DWORD               m_dwVideoFlags;
    DWORD               m_dwCurrentMode;
    FLOAT               m_fSplitterPosition;
    FLOAT               m_fSplitterScrollRate;

    CHAR                m_szVideoFileNames[MAX_FILES][MAX_FILENAME_LENGTH];
    INT                 m_iCurrentAVideo;
    INT                 m_iCurrentBVideo;
    INT                 m_iTempASelection;
    INT                 m_iTempBSelection;
    WORD                m_wMaxFilesFound;
    INT                 m_iSplitterColorIndex;

    LPDIRECT3DTEXTURE8  m_pTextureA;
    LPDIRECT3DTEXTURE8  m_pTextureB;

public:
    CXBoxSample();
    HRESULT Initialize();
    HRESULT FrameMove();
    HRESULT Render();

    WORD EnumVideoFiles();
    INT IndexFromVideoFilename( const CHAR* strFileName);
    BOOL SetLeftVideo( const CHAR* strFileName);
    BOOL SetRightVideo( const CHAR* strFileName);
    HRESULT RenderSplitter();
    HRESULT PlayVideoWithGetNextFrame( const CHAR* strFilename, const CHAR* strFilenameB );
    BOOL PlayVideoFrame( WORD wFlag );
    BOOL RenderVideoFrame();
    VOID Play();

};

// external pointer to the sample class, also declared in XMVCompare.cpp.
// This is so we can access the class from outside its file-scope
extern CXBoxSample* g_pxbApp;