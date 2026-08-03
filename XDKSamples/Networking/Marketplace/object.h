//-----------------------------------------------------------------------------
// File: Object.h
//
// Desc: DisplayedObject wraps the collision and sorting of the 2d sprites,
//       as well as encapsulates sound and icon data for a player or object
//       (fountain, tree, etc)
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once


class DisplayedObject
{
public:
    DisplayedObject();
    ~DisplayedObject();

    void Initialize( const D3DXVECTOR3 &vPos, int iIndex, float fRadius );
    void Move( const D3DXVECTOR3 &vNewPos );

    void SetVoiceSound( PositionedVoiceSound *pSound );     // set the voice sound associated with this object
    void SetWavSound( PositionedWaveBankSound *pSound );    // set the wav sound associated with this object
    
    PositionedVoiceSound *GetVoiceSound();
    PositionedWaveBankSound *GetWavSound();

    void SetImageIndex( int iIdx );                         // set which image is currently being shown
                                                            // from the resource file
    
    void Render();                                                  // draw the object
    void SetLabel( WCHAR *pLabel, DWORD dwLabelColor );             // set the text above the object
    void SetSublabel( WCHAR *pSublabel, DWORD dwSublabelColor );    // set the text to the right of the object
    
    void SetDiffuseColor( DWORD dwColor );                  // set the objects color

    const FLOAT        GetRadius();
    const D3DXVECTOR3 &GetPosition();

    DisplayedObject *Next();
private:
    D3DXVECTOR3 m_vPos;
    int         m_iIdx; // graphic index; -1 = start of list
    float       m_fRadius;
    
    WCHAR       m_pLabel[30];
    WCHAR       m_pSublabel[30];
    DWORD       m_dwLabelColor;
    DWORD       m_dwSublabelColor;
    DWORD       m_dwObjectColor;

    PositionedWaveBankSound *m_pWavSound;
    PositionedVoiceSound *m_pVoiceSound;

    DisplayedObject *m_pPrev;
    DisplayedObject *m_pNext;
};

inline DisplayedObject *DisplayedObject::Next()
{
    return m_pNext;
}

inline const D3DXVECTOR3 & DisplayedObject::GetPosition() 
{
    return m_vPos;
}

inline const FLOAT DisplayedObject::GetRadius()
{
    return m_fRadius;
}

inline PositionedWaveBankSound* DisplayedObject::GetWavSound()
{
    return m_pWavSound;
}

inline PositionedVoiceSound* DisplayedObject::GetVoiceSound()
{
    return m_pVoiceSound;
}

extern DisplayedObject g_IndexObject;


