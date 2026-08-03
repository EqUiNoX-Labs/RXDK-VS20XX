//----------------------------------------------------------------------------
// File: Waterdefs.h
//
// Desc: Defines global constants, defines, utils, and structures.
//
// Hist: 11.14.00 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include <D3D8.h>
#include <d3dx8.h>
#include <D3DX8Math.h> 
#include <xbapp.h>
#include <xgraphics.h>
#include <crtdbg.h>
#include <stdio.h>
#include <assert.h>




//----------------------------------------------------------------------------
// The global defines 
//----------------------------------------------------------------------------
#define RTEXTURE_WIDTH  512         // The width of render target texture
#define RTEXTURE_HEIGHT 512         // The height of render target texture




//-----------------------------------------------------------------------------
//  Constants 
//-----------------------------------------------------------------------------

// The fog constants
const FLOAT                         c_fFogFarPlane      = 1024;
const FLOAT                         c_FogNearPlane      = 128;
const DWORD                         c_dwFogColor        = 0xffffffff;
extern D3DXVECTOR4                  g_vFog;
//The far plane distance for project matrix
const FLOAT                         c_fFarPlane         = c_fFogFarPlane * 1.01f;
const FLOAT                         c_fNearPlane        = 0.25f;

//Maximum constant
const INT                           c_nMaxTexture       = 64;
const INT                           c_nMaxVShader       = 64;
const INT                           c_nMaxPShader       = 64;
const INT                           c_nMaxEffect        = 64;
const INT                           c_nMaxModelNumber   = 256;
const INT                           c_nMaxTextureStage  = 4;
const INT                           c_nMaxPathLength    = 256;

// The package file directory
extern const CHAR* c_strPackDir; 

//Render fags
enum RENDER_FLAG
{
   RF_NONE = 0,
   RF_NORMAL = 1,        //Normal render
   RF_ABOVE_WATER = 2,   //Only render the "above water" part, for reflection
   RF_BELOW_WATER = 4,   //Only render the "below water" part, for refraction
};




//-----------------------------------------------------------------------------
//  The direction of the sun. This MUST be the same as the direction of the 
//  sun in the sky texture.
//  The direction affects the position of the highlighted part in the water.
//  The value is in WaterApp.cpp
//-----------------------------------------------------------------------------
extern D3DXVECTOR4 g_SunLightDir;




//-----------------------------------------------------------------------------
// Utility functions
//-----------------------------------------------------------------------------
// This function change the name of a parameter into FCC.
DWORD GetFCCFromString( const CHAR* str );

// Some string are more than 4 chars. We need change it to 4 chars, 
// e.g "normal' -> 'NORM'
DWORD ConvertStringToFCC( const CHAR* str );

HRESULT CreateEffectFromFile( LPDIRECT3DDEVICE8 pDevice,
                              const CHAR* pSrcFile,
                              LPD3DXEFFECT* ppEffect );




//-----------------------------------------------------------------------------
// FourCC macros
//-----------------------------------------------------------------------------
#define FCC_MTOT ( GetFCCFromString( "mTot" ) )
#define FCC_WFGC ( GetFCCFromString( "WFgC" ) )
#define FCC_FOGT ( GetFCCFromString( "FogT" ) )
#define FCC_FOGS ( GetFCCFromString( "FogS" ) )
#define FCC_LTDR ( GetFCCFromString( "LtDr" ) )
#define FCC_REFF ( GetFCCFromString( "REFF" ) )
#define FCC_BSTX ( GetFCCFromString( "BsTx" ) )
#define FCC_FGHP ( GetFCCFromString( "FgHp" ) )
#define FCC_FGCR ( GetFCCFromString( "FgCr" ) )
#define FCC_CLMD ( GetFCCFromString( "ClMd" ) )
#define FCC_TREL ( GetFCCFromString( "tREL" ) )
#define FCC_TRER ( GetFCCFromString( "tRER" ) )
#define FCC_TBUP ( GetFCCFromString( "tBUP" ) )
#define FCC_TFRS ( GetFCCFromString( "tFRS" ) )
#define FCC_VTCS ( GetFCCFromString( "vTCS" ) )
#define FCC_VFOG ( GetFCCFromString( "vFog" ) )
#define FCC_LRED ( GetFCCFromString( "LReD" ) )
#define FCC_EYPS ( GetFCCFromString( "EyPs" ) )
#define FCC_WDTT ( GetFCCFromString( "WdTt" ) )




//-----------------------------------------------------------------------------
// Name: class ICullFrustum
// Desc: ICullFrustum is the base class/interface of the CScene, CRelection and
//       CRefraction.
//       The normal, reflection, and refraction views have this common
//       interface to give do view projection matrix calculations and view
//       frustum culling. View fustum culling has not been implemented yet.
//-----------------------------------------------------------------------------
class ICullFrustum
{
public: 
    virtual BOOL IsReflected() 
    {
        return FALSE;
    }

    virtual D3DXMATRIX* GetViewMatrix() = 0;
    virtual D3DXMATRIX* GetProjMatrix() = 0;
    virtual D3DXMATRIX* GetViewProjMultiMatrix() = 0;  // matView * matProj
};


