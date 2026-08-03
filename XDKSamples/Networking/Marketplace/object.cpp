//-----------------------------------------------------------------------------
// File: Object.cpp
//
// Desc: Object wraps the collision and sorting of the 2d sprites,
//       as well as encapsulates sound and icon data for a player or object
//       (fountain, tree, etc)
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "CommonInclude.h"

DisplayedObject g_IndexObject;

//------------------------------------------------------------------------------
// Name: DisplayedObject::DisplayedObject()
// Desc: Constructor
//------------------------------------------------------------------------------
DisplayedObject::DisplayedObject() 
{
    m_pPrev             = this;
    m_pNext             = this;
    m_iIdx              = -1;
    m_pLabel[0]         = 0;
    m_pSublabel[0]      = 0;
    m_pWavSound         = NULL;
    m_pVoiceSound       = NULL;
    m_dwObjectColor     = 0xffffffff;
    m_dwLabelColor      = 0xffffffff;
}





//------------------------------------------------------------------------------
// Name: DisplayedObject::~DisplayedObject()
// Desc: Destructor
//------------------------------------------------------------------------------
DisplayedObject::~DisplayedObject()
{
    m_pPrev->m_pNext = m_pNext;
    m_pNext->m_pPrev = m_pPrev;

    if (m_pWavSound) g_AudioMgr.FreeSound( m_pWavSound );
    if (m_pVoiceSound) g_AudioMgr.FreeSound( m_pVoiceSound );
}





//------------------------------------------------------------------------------
// Name: DisplayedObject::SetLabel()
// Desc: Sets text/icons above sprite
//------------------------------------------------------------------------------
void DisplayedObject::SetLabel( WCHAR *pLabel, DWORD dwLabelColor )
{
    if (pLabel)
        wcscpy(m_pLabel, pLabel);
    else
        m_pLabel[0] = 0;
    m_dwLabelColor = dwLabelColor;
}




//------------------------------------------------------------------------------
// Name: DisplayedObject::SetLabel()
// Desc: Sets text/icons next to sprite
//------------------------------------------------------------------------------
void DisplayedObject::SetSublabel( WCHAR *pLabel, DWORD dwLabelColor )
{
    if (pLabel)
        wcscpy(m_pSublabel, pLabel);
    else
        m_pSublabel[0] = 0;
    m_dwSublabelColor = dwLabelColor;
}




//------------------------------------------------------------------------------
// Name: DisplayedObject::Initialize()
// Desc: Set parameters for object
//------------------------------------------------------------------------------
void DisplayedObject::Initialize( const D3DXVECTOR3 &vPos, int iIndex, float fRadius )
{
    m_iIdx              = iIndex;
    m_fRadius           = fRadius;
    m_pPrev             = &g_IndexObject;
    m_pNext             = g_IndexObject.m_pNext;
    m_pPrev->m_pNext    = this;
    m_pNext->m_pPrev    = this;
    Move( vPos );
}




//------------------------------------------------------------------------------
// Name: DisplayedObject::SetImageIndex()
// Desc: Sets sprite index for the object
//------------------------------------------------------------------------------
void DisplayedObject::SetImageIndex( int iIdx )
{
    m_iIdx = iIdx;
}





//------------------------------------------------------------------------------
// Name: DisplayedObject::SetVoiceSound()
// Desc: Sets the voice sound to be used for the object
//------------------------------------------------------------------------------
void DisplayedObject::SetVoiceSound( PositionedVoiceSound *pSound )
{
    m_pVoiceSound = pSound;
}





//------------------------------------------------------------------------------
// Name: DisplayedObject::SetWavSound()
// Desc: Sets the 3d positioned sound to be used for the object
//------------------------------------------------------------------------------
void DisplayedObject::SetWavSound( PositionedWaveBankSound *pSound )
{
    m_pWavSound = pSound;
}






//------------------------------------------------------------------------------
// Name: DisplayedObject::SetDiffuseColor()
// Desc: Sets the diffuse lighting color for the sprite
//------------------------------------------------------------------------------
void DisplayedObject::SetDiffuseColor( DWORD dwColor )
{
    m_dwObjectColor = dwColor;
}





//------------------------------------------------------------------------------
// Name: DisplayedObject::Move()
// Desc: Moves the object and relinks into the back-to-front list
//------------------------------------------------------------------------------
void DisplayedObject::Move( const D3DXVECTOR3 &vPos )
{
    // unlink myself
    m_pPrev->m_pNext = m_pNext;
    m_pNext->m_pPrev = m_pPrev;
    m_vPos = vPos;

    DisplayedObject *pSrch;
    for ( pSrch=g_IndexObject.m_pNext; pSrch != &g_IndexObject; pSrch=pSrch->m_pNext )
        if (( vPos.y < pSrch->m_vPos.y ) || 
            (( vPos.y == pSrch->m_vPos.y ) && ( vPos.x < pSrch->m_vPos.x ) ) )
            break;


    // move the sound if there is one
    if ( m_pWavSound ) m_pWavSound->Move( vPos );
    if ( m_pVoiceSound ) m_pVoiceSound->Move( vPos );

    // relink
    m_pPrev = pSrch->m_pPrev ;
    m_pNext = pSrch;
    m_pPrev->m_pNext = this;
    m_pNext->m_pPrev = this;
}





//------------------------------------------------------------------------------
// Name: DisplayedObject::Render()
// Desc: Draws an object
//------------------------------------------------------------------------------
void DisplayedObject::Render()
{   
    if ( m_iIdx == -1 ) return;
    g_Marketplace.DrawScaledObject( m_vPos, m_dwObjectColor, m_iIdx );

    if ( m_pLabel && m_pLabel[0] )
        g_Marketplace.DrawScaledGlyph( m_vPos.x, m_vPos.y, -80.0f, 1.0f, m_pLabel, m_dwLabelColor );
    if ( m_pSublabel && m_pSublabel[0] )
    {
        g_Marketplace.DrawScaledGlyph( m_vPos.x + 2.8f, m_vPos.y, -50.0f, 1.0f, m_pSublabel, m_dwSublabelColor );
    }
}
