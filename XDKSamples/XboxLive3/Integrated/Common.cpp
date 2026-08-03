//-------------------------------------------------------------------------------------
// File: Common.cpp
//
// Desc: Holds common utility code.
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include <assert.h>
#include <xtl.h>

#include <XBStopWatch.h>
#include "common.h"


// Make the font global so we don't have to pass
// it as a parameter for simple rendering functions
CXBFont* g_pFont = NULL;


//-------------------------------------------------------------------------------------
// Name: RenderWorkingScreen()
// Desc: Renders a screen that indicates the system is working.
//       This function clears AND swaps the buffer so it can be called
//       in "tight" loops. This should not be called by any other rendering
//       function.
//-------------------------------------------------------------------------------------
VOID RenderWorkingScreen()
{
    // Constants for the animation
    const FLOAT PROGRESS_RATE       = 0.10f; // Change the spinner every 1/4 second
    const FLOAT FONT_SCALE          = 3.0f;
    const WCHAR* const rwSpinners[] = { L".  ", L"o ", L"O",
                                        L"O", L" o", L"  ." };
    const WORD NUM_SPINNER_STATES   = sizeof( rwSpinners ) / sizeof( rwSpinners[0] );


    // Keep an index to the "animation frame"
    // and a timer to progress
    static CXBStopWatch progressTimer;
    static WORD         wSpinnerState;

    // Clear the framebuffer
    g_pd3dDevice->Clear( 0L, NULL,
                         D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         COLOR_BLUE, 1.0f, 0L );


    // If the timer is not running ( the first time the
    // function is called ), start it
    if( !progressTimer.IsRunning() )
    {
        wSpinnerState = 0;
        progressTimer.StartZero();
    }

    // If the current frame has been up for long enough
    // progress to the next frame
    if( progressTimer.GetElapsedSeconds() > PROGRESS_RATE )
    {
        ++wSpinnerState;
        wSpinnerState = wSpinnerState >= NUM_SPINNER_STATES ? 0 : wSpinnerState;

        progressTimer.StartZero();
    }


    // Render the text
    g_pFont->SetScaleFactors( FONT_SCALE, FONT_SCALE );
    g_pFont->DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y,
                       COLOR_NORMAL,
                       rwSpinners[wSpinnerState],
                       XBFONT_CENTER_X | XBFONT_CENTER_Y );
    g_pFont->SetScaleFactors( 1.0f, 1.0f );

    g_pFont->DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y * 0.75f,
                       COLOR_NORMAL,
                       L"WORKING",
                       XBFONT_CENTER_X );


    // Present the scene
    g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
}

//-------------------------------------------------------------------------------------
// Name: BootToDash()
// Desc: Boot back into either the main or online dashes
//-------------------------------------------------------------------------------------
VOID BootToDash( DWORD dwReason )
{
    LD_LAUNCH_DASHBOARD ld;
    ZeroMemory( &ld, sizeof(ld) );
    ld.dwReason = dwReason;
    XLaunchNewImage( NULL, PLAUNCH_DATA( &ld ) );

    // XLaunchNewImage should never return
    assert( FALSE );
}

//-------------------------------------------------------------------------------------
// Name: WaitForTaskToComplete()
// Desc: Helper function for waiting for a task to complete
//       In a real game, this would be part of your game loop
//       Returns TRUE for success, FALSE for failure.
//       Optionally returns the results code in pHR
//-------------------------------------------------------------------------------------
BOOL WaitForTaskToComplete( CXBOnlineTask& Task, HRESULT* pHR, BOOL bRenderWorking )
{
    assert( pHR );
    HRESULT hrTask = XONLINETASK_S_RUNNING;

    while( hrTask == XONLINETASK_S_RUNNING )
    {
        // In a real game, this would be part of your game loop-
        // you wouldn't block on this.
        // The logon task should also be pumped inside this loop.

        hrTask = Task.Continue();
        if( FAILED( hrTask ))
        {
            *pHR = hrTask;
            return FALSE;
        }

        // put in a delay of about 1 frame so as not to
        // spam with MessageSendGetProgress
        if( bRenderWorking )
            RenderWorkingScreen();

        Sleep( 15 );
    }

    if( hrTask != XONLINETASK_S_SUCCESS )
    {
        *pHR = hrTask;
        return FALSE;
    }

    *pHR = S_OK;
    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: AddSeconds()
// Desc: Add the specified number of seconds to the FILETIME passed in,
//       and return the result.
//-----------------------------------------------------------------------------
FILETIME AddSeconds( FILETIME startTime, ULONGLONG qwDeltaSeconds )
{
    // FILETIME structures count 100-nanosecond intervals, therefore there
    // are 10,000,000 ticks per second in a FILETIME structure.
    const int TICKSPERSECOND = 10000000;

    // Copy StartTime to a ULARGE_INTEGER
    ULARGE_INTEGER data;
    assert( sizeof( data ) == sizeof( startTime ) );
    memcpy( &data, &startTime, sizeof( data ) );

    // Do the math
    data.QuadPart += qwDeltaSeconds * TICKSPERSECOND;

    FILETIME Result;
    memcpy( &Result, &data, sizeof( data ) );
    return Result;
}

//-----------------------------------------------------------------------------
// Name: GetUserStruct()
// Desc: This function returns an XONLINE_USER struct for the specified player
//       ID, to faciliate logging them in. Note that this function currently
//       only searches the list of gamertags on the local machine, which is
//       sufficient for this sample but might be
//       inadequate in a real competition.
//-----------------------------------------------------------------------------
XONLINE_USER GetUserStruct( ULONGLONG playerID,
                            XONLINE_USER* rwUserList,
                            DWORD dwUserCount )
{
    for( DWORD i = 0; i < dwUserCount; ++i )
    {
        if( rwUserList[i].xuid.qwUserID == playerID )
            return rwUserList[i];
    }

    // If a user has been deleted from the machine while they are signed up
    // for a tournament then this function will not be able to find their
    // user struct.
    // The sample will not be able to play matches for deleted users.
    XONLINE_USER unknown = { 0 };

    return unknown;
}

//-------------------------------------------------------------------------------------
// Name: CreateFace()
// Desc: Returns a pointer to an array of four vertices to draw our texture
//       onto. The memory is allocated by the GPU so we do not need to free it.
//       The width and height of the QUAD are the sizes given.
//-------------------------------------------------------------------------------------
LPDIRECT3DVERTEXBUFFER8 CreateFace( LPDIRECT3DDEVICE8 pd3dDevice,
                                    FLOAT fX,
                                    FLOAT fY )
{
    // Step 1
    //
    // Create our background vertex buffer

    LPDIRECT3DVERTEXBUFFER8 pQuadVertices = NULL;

    pd3dDevice->CreateVertexBuffer(
                    4 * sizeof(CUSTOMVERTEX), // Size of buffer
                    0,                        // Usage: ignored
                    0,                        // FVF: ignored
                    NULL,                     // Pool: ignored
                    &pQuadVertices            // Output pointer to the vertex buffer
                );

    assert( pQuadVertices );


    // Step 2
    //
    // Lock the verex buffer so we can write the vertice's coordinates into memory

    CUSTOMVERTEX* pVertices = NULL;
    pQuadVertices->Lock( 0, 0, (BYTE **)&pVertices, 0L );


    // This offset's our coordintates so
    // the function's input parameters
    // refer to the upper left corner
    FLOAT fMagicX      = ICON_SIZE - 1.0f;
    FLOAT fMagicY      = ICON_SIZE - 1.0f;


    // Step 3
    //
    // Write the coordinates for each of the four points
    // of the quad into memory

    // Lower Left
    pVertices[0].p = D3DXVECTOR4( fX + fMagicX,
                                  fY + ICON_SIZE + fMagicY,
                                  1.0f, 1.0f );
    pVertices[0].t = D3DXVECTOR2( 0.0f, BITMAP_SIZE );

    // Upper Left
    pVertices[1].p = D3DXVECTOR4( fX + fMagicX,
                                  fY + fMagicY,
                                  1.0f, 1.0f );
    pVertices[1].t = D3DXVECTOR2( 0.0f, 0.0f );

    // Upper Right
    pVertices[2].p = D3DXVECTOR4( fX + ICON_SIZE + fMagicX,
                                  fY + fMagicY,
                                  1.0f, 1.0f );
    pVertices[2].t = D3DXVECTOR2( BITMAP_SIZE, 0.0f ); // Upper right

    // Lower Right
    pVertices[3].p = D3DXVECTOR4( fX + ICON_SIZE + fMagicX,
                                  fY + ICON_SIZE + fMagicY,
                                  1.0f, 1.0f );
    pVertices[3].t = D3DXVECTOR2( BITMAP_SIZE, BITMAP_SIZE );


    // Step 4
    //
    // Unlock the memory to commit our changes,
    // return our pointer

    pQuadVertices->Unlock();

    return pQuadVertices;
}

//-------------------------------------------------------------------------------------
// Name: SetFacePos()
// Desc: Sets the position of the QUAD in 2D space
//-------------------------------------------------------------------------------------
VOID SetFacePos( LPDIRECT3DVERTEXBUFFER8 pVerts,
                 FLOAT fX,
                 FLOAT fY )
{
    assert( pVerts );

    CUSTOMVERTEX* pData  = NULL;
    UINT          uiSize = 4 * sizeof(CUSTOMVERTEX);

    // Step 1
    //
    // Lock the memory so we can make changes to
    // the vertex buffer

    pVerts->Lock( 0,              // offset to the data
                  uiSize,         // Data size we want
                  (PBYTE*)&pData, // Pointer to the data we will modify
                  0               // Flags
                );

    assert( pData );


    // Step 2
    //
    // Write the new coordinates to the
    // vertex buffer

    pData[0].p.y = fY + ICON_SIZE;
    pData[1].p.y = fY;
    pData[2].p.y = fY;
    pData[3].p.y = fY + ICON_SIZE;

    pData[0].p.x = fX;
    pData[1].p.x = fX;
    pData[2].p.x = fX + ICON_SIZE;
    pData[3].p.x = fX + ICON_SIZE;


    // Step 3
    //
    // Unlock to commit our changes

    pVerts->Unlock();
}

//-------------------------------------------------------------------------------------
// Name: TranslateFace()
// Desc: Moves the given Quad to given amount in 2D space
//-------------------------------------------------------------------------------------
VOID TranslateFace( LPDIRECT3DVERTEXBUFFER8 pVerts,
                    FLOAT fX,
                    FLOAT fY )
{
    assert( pVerts );

    CUSTOMVERTEX* pData  = NULL;
    UINT          uiSize = 4 * sizeof(CUSTOMVERTEX);


    // Step 1
    //
    // Lock the vertices and get a pointer to the vertex buffer

    pVerts->Lock( 0,              // offset to the data we want to modify
                  uiSize,         // Data size
                  (PBYTE*)&pData, // Output pointer to the verticies
                  0 );            // Flags

    assert( pData );


    // Step 2
    //
    // Add the translation to the verts

    pData[0].p.y += fY;
    pData[1].p.y += fY;
    pData[2].p.y += fY;
    pData[3].p.y += fY;

    pData[0].p.x += fX;
    pData[1].p.x += fX;
    pData[2].p.x += fX;
    pData[3].p.x += fX;


    // Step 3
    //
    // Unlock the memory to commit our changes

    pVerts->Unlock();
}

//-------------------------------------------------------------------------------------
// Name: RenderSprite()
// Desc: Renders the texture over the given Quad
//-------------------------------------------------------------------------------------
VOID RenderSprite ( LPDIRECT3DDEVICE8 pd3dDevice,
                    LPDIRECT3DVERTEXBUFFER8 pVerts,
                    LPDIRECT3DTEXTURE8 pTexture )
{
    assert( pVerts );
    assert( pTexture );

    // Step 1
    //
    // Set our render state
    // It is import to use clamped UVs because we are using a
    // linear texture

    pd3dDevice->SetTexture( 0, pTexture );
    pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );


    // Step 2
    //
    // Draw the Quad

    pd3dDevice->SetStreamSource( 0, pVerts, sizeof( CUSTOMVERTEX ) );
    pd3dDevice->SetVertexShader( D3DFVF_CUSTOMVERTEX );
    pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );
    pd3dDevice->SetTexture( 0, NULL );
}
