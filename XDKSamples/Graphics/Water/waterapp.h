//-----------------------------------------------------------------------------
// File: Waterapp.h
//
// Desc: Defines the main sample application class.
//
//       The workflow of the water application is:
//       1) Initialize
//          Create device, buffers; Load all meshes, textures, shaders, effects 
//          and other resource into memory.
//       2) FrameMove
//          Get gamepad input and change camera's position and direction. If the 
//          camera changed, update the reflection and refraction texture by 
//          rendering the non-water scene (static models and sky) into atexture.
//          Change the bumpmap for water and its related matrices.
//       3) Render
//          Render the sky(CSky), static models( managed by CModelList ) and
//          water(CWater).
//       
//
// Hist: 11.14.00 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#include "waterdefs.h"
#include <xbfont.h>
#include <xbhelp.h>

const INT c_nMaxNumHotspots = 100;

class CResourceManager;
class CReflection;
class CRefraction;
class CNonWater;
class CWater;


//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: The application class of this sample.
//-----------------------------------------------------------------------------
class CXBoxSample :  public CXBApplication, 
                     public ICullFrustum
{
    friend class CWater;

protected:
    CXBFont          m_Font;
    CXBHelp          m_Help;
    BOOL             m_bShowHelp;
    BOOL             m_bShowInfo;
    BOOL             m_bWireFrame;

protected:
    FLOAT            m_fFieldOfView;
    FLOAT            m_fAspect;
    // Avoid turning over when looking at top or bottom
    FLOAT            m_fMaxLookDownAngle;
    
    // Camera
    D3DXVECTOR3      m_vViewPos;
    D3DXVECTOR3      m_vViewDir; 
    D3DXVECTOR3      m_vViewLeftDir;
    BOOL             m_bCameraChanged;

    ICullFrustum*    m_pCurrentCullFrustum;
    D3DXMATRIX       m_matView;
    D3DXMATRIX       m_matProj;
    D3DXMATRIX       m_matViewProj;

    INT              m_nNumHotspots;
    struct HOTSpots
    {
        D3DXVECTOR3 pos;
        D3DXVECTOR3 dir;
    } m_rgHotSpots[c_nMaxNumHotspots];

protected:
    HRESULT Initialize();
    void CameraMove();
    INT GoToHotspot ( INT nID );
    HRESULT FrameMove();
    HRESULT Render();
    HRESULT Cleanup();

public:
    CResourceManager*      m_pResMan;
    CReflection*           m_pReflection;
    CRefraction*           m_pRefraction;
    CNonWater*             m_pNonWater;
    CWater*                m_pWater;

    CXBoxSample();
    virtual ~CXBoxSample(); 
    HRESULT Create();


    const FLOAT & GetElapsedTime() const
    {
        return m_fElapsedTime;
    }

    // From interface ICullFrustum
    D3DXMATRIX* GetViewMatrix() 
    {      
        return &m_matView;  
    } 

    D3DXMATRIX* GetProjMatrix() 
    {       
        return &m_matProj;    
    }

    D3DXMATRIX* GetViewProjMultiMatrix()
    {      
        return &m_matViewProj;  
    }
    

    inline D3DXVECTOR3 & GetViewDirection()
    {       
        return m_vViewDir; 
    }

    inline D3DXVECTOR3 & GetViewPosition()
    {     
        return m_vViewPos; 
    }

    inline FLOAT GetFieldOfView()
    {      
        return m_fFieldOfView;
    }

    inline FLOAT GetAspect()
    {      
        return m_fAspect;  
    }

    // Used in CReflection and CRefraction
    // The reflection/refraction texture will be updated
    // when the camera is changed.
    inline BOOL IsCameraChanged()
    {      
        return m_bCameraChanged; 
    }


    ICullFrustum* GetCullFrustumObject();
    void SetCullFrustumObject( ICullFrustum* pCullObject );
};

extern CXBoxSample*  g_pApp;
