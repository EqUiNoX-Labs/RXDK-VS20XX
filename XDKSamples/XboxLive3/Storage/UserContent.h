//-------------------------------------------------------------------------------------
// File: UserContent.h
//
// Desc: Holds definition for a user content object used to demonstrate
//       Xbox Live storage functionality
//
// Hist: 08.10.04 - New for Sept release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef USERCONTENT_H
#define USERCONTENT_H

#include <xtl.h>
#include <xonline.h>


const INT         LIVE_SIG_BUFFER_SIZE  = 100;   // Size of live signature buffer
const INT         SAVE_SIG_BUFFER_SIZE  = 20;    // Size of save signature buffer
const INT         DIGEST_BUFFER_SIZE    = 20;    // Size of data digest buffer

const FLOAT       ICON_SIZE             = 32.0f; // Size of the icon to display (X & Y)


// Content Signing helper functions

DWORD   CalculateSignature( PBYTE rwSignature,
                            DWORD dwMaxSigSize,
                            const PBYTE pUserData,
                            const DWORD dwUserDataSize,
                            const DWORD dwSigType );

VOID    CalculateDigest( BYTE *pbData, DWORD dwSize,
                         BYTE *pDigest, DWORD dwDigestSize );

BOOL    IsValidLiveSignature( DWORD dwSigSize, PBYTE pSignature,
                              DWORD dwDigestSize, PBYTE pDigest );


// User creatable content object
// Made a class to help support ease
// of content signing
class CUserContent
{
public:

    enum
    {
        // Size of the bitmap in both directions ( 16x16 )
        BITMAP_SIZE = 16,
        // Size of memory for the linear bitmap
        DATA_SIZE   = ( sizeof( DWORD ) * BITMAP_SIZE * BITMAP_SIZE ) + sizeof( BOOL )
    };

protected:

    DWORD m_rwBitmap[ BITMAP_SIZE * BITMAP_SIZE ];
    BOOL  m_bDirty;
    BOOL  m_bContentDead;

public:

    BOOL    IsDirty() const { return m_bDirty; }
    VOID    SetDirty( const BOOL bDirty ) { m_bDirty = bDirty; }

    BOOL    IsDead() const { return m_bContentDead; }
    VOID    SetDead( const BOOL bDead ) { m_bContentDead = bDead; }

    INT     GetSize() const { return BITMAP_SIZE; }
    DWORD*  GetBitmap() { return m_rwBitmap; }

    DWORD   GetColor( INT iX, INT iY );
    VOID    SetColor( INT iX, INT iY, DWORD dwColor );

    
    // Rendering helper functions
    LPDIRECT3DTEXTURE8  CreateTexture( LPDIRECT3DDEVICE8  lpD3dDevice );
    VOID                UpdateTexture( LPDIRECT3DTEXTURE8 lpTexture );


    // Save/Load helper functions
    BOOL    Save( BOOL bUserSignedIn, const WCHAR* wszFilename );
    BOOL    Load( BOOL bUserSignedIn,
                  const CHAR*  szPath,
                  const WCHAR* wszFilename );


    // Content Transfer functions
    BOOL    Upload( const DWORD dwControllingUserPort,
                    const ULONGLONG qwUserID,
                    const ULONGLONG qwTeamID,
                    WCHAR* wszFilename = NULL );
    HRESULT Download( const DWORD dwControllingUserPort,
                      const ULONGLONG qwUserID,
                      const ULONGLONG qwTeamID,
                      WCHAR* wszFilename = NULL );

    CUserContent& operator=( const CUserContent& );

    // Intialization functions
    VOID    Clear();

    CUserContent();
    ~CUserContent() {}
};


// Maximum buffer size to user when reading the file
const DWORD MAX_FILE_SIZE  = CUserContent::DATA_SIZE + ( 2 * LIVE_SIG_BUFFER_SIZE );

#endif // USERCONTENT_H