//-----------------------------------------------------------------------------
// File: NoneWater.cpp
//
// Desc: This class include all the models except the water
//
// Hist: 11.14.00 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "waterdefs.h"
#include "waterapp.h"
#include "nonewater.h"
#include "sky.h"
#include "model.h"
#include "packman.h"
#include "resman.h"




//-----------------------------------------------------------------------------
// Name: CNonWater
// Desc: Constructor
//-----------------------------------------------------------------------------
CNonWater::CNonWater()
{
    m_pSky            = NULL;
    m_pModelList      = NULL;
    m_pPackedModelMan = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CNonWater
// Desc: Destructor
//-----------------------------------------------------------------------------
CNonWater::~CNonWater( void )
{
}




//-----------------------------------------------------------------------------
// Name: Initialize
// Desc: Initializes the class
//-----------------------------------------------------------------------------
HRESULT CNonWater::Initialize()
{
    HRESULT hr;
    
    m_pSky = new CSky();
    assert( m_pSky );
    if ( FAILED( hr = m_pSky->Initialize() ) )
        return hr;

    m_pModelList = new CModelList();
    assert( m_pModelList );

    m_pPackedModelMan = new CPackageManager( m_pModelList );
    assert( m_pPackedModelMan );
    if ( FAILED( hr = m_pPackedModelMan->Initialize() ) )
        return hr;

    m_pModelList->SortRenderOrder();
    return hr;
}




//-----------------------------------------------------------------------------
// Name: Render
// Desc: dwRenderFlag could be : RF_NORMAL, RF_ABOVEWATER, EF_BELOWWATER
//-----------------------------------------------------------------------------
HRESULT CNonWater::Render( DWORD dwRenderFlag )
{
    // Render the sky (as long as we're not under water)
    if( 0 == ( dwRenderFlag & RF_BELOW_WATER ) )
        m_pSky->Render( g_pApp->GetCullFrustumObject() );
    
    // Render the model list
    m_pModelList->Render( dwRenderFlag );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Cleanup
// Desc: Frees resoures
//-----------------------------------------------------------------------------
HRESULT CNonWater::Cleanup()
{
    SAFE_DELETE( m_pSky );

    m_pPackedModelMan->Cleanup();
    SAFE_DELETE( m_pPackedModelMan );

    SAFE_DELETE( m_pModelList );

    return S_OK;
}

