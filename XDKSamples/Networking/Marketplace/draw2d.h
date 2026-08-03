//-----------------------------------------------------------------------------
// File: Draw2D.h
//
// Desc: CDraw2D deals with the display of the sprite graphics.
//       the sprites[] array has the center and size of each graphic hardcoded, 
//       so we can display and collide appropriately
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#include "XBResource.h"
#include "XBUtil.h"

//-----------------------------------------------------------------------------
// Name: class CDraw2D
// Desc: Draws 2d sprites
//-----------------------------------------------------------------------------

class CDraw2D
{
public:
    // Saved state for rendering (if not using a pure device)
    BOOL          m_bSaveState;
    DWORD         m_dwSavedState[16];

    // D3D rendering objects
    CXBPackedResource m_xprResource;
    
    // Internal creation calls
    HRESULT     CreateShaders();       

    static DWORD m_dwVertexShader;
    static DWORD m_dwPixelShader;

    int m_dwNestedBeginCount;

public:
    // Constructor/destructor
    CDraw2D();
    ~CDraw2D();

    // Functions to create and destroy the internal objects
    HRESULT Create( const CHAR* strResourceFileName );
    HRESULT Destroy();

    HRESULT Begin();
    HRESULT DrawSprite( FLOAT sx, FLOAT sy, FLOAT scale, DWORD dwColor, DWORD idx );
    HRESULT End();
};
