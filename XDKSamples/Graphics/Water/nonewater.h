//-----------------------------------------------------------------------------
// File: NoneWater.h
//
// Desc: This class contains all "non-water" objects
//
// Hist: 11.14.00 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#include "waterdefs.h"

class CModelList;
class CSky;
class CPackageManager;




//-----------------------------------------------------------------------------
// Name: class CNonWater
// Desc: This class include all the "non-water" objects: the sky and all the static 
//       models.
//-----------------------------------------------------------------------------
class CNonWater
{
protected:
    // Class to read models from package
    CPackageManager* m_pPackedModelMan;
    // The sky
    CSky*            m_pSky;
    // The models
    CModelList*      m_pModelList;

public:
    HRESULT Initialize();
    HRESULT Render( DWORD dwRenderFlag = RF_NONE );
    HRESULT Cleanup();

    CNonWater();
    virtual ~CNonWater();
};
