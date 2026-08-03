//-----------------------------------------------------------------------------
// File: CreatePushBufferOnPC.cpp
//
// Desc: Shows how to create static push buffers on the PC.
//
// Hist: 01.23.03 - New for February XDK
//       02.06.03 - Code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
#endif
#include <stdio.h>
#include <windows.h>
#include <d3d8-xbox.h>

#include "PushBufferModel.h"




//-----------------------------------------------------------------------------
// Push-buffer objects
//-----------------------------------------------------------------------------

// The static push buffer
D3DPushBuffer*   g_pPushBuffer = NULL;

// Some geometry to draw
D3DVertexBuffer* g_pVertexBuffer = NULL;
DWORD            g_dwVertexBufferSize;

// Fixup offsets
DWORD            g_dwSetVertexShaderInputFixupOffset;
DWORD            g_dwSetVertexShaderConstantFixupOffset;
DWORD            g_dwSetModelViewFixupOffset;




//-----------------------------------------------------------------------------
// Direct3D objects
//-----------------------------------------------------------------------------
LPDIRECT3D8       g_pD3D;              // The D3D enumerator object
LPDIRECT3DDEVICE8 g_pd3dDevice;        // The D3D rendering device




//-----------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initializes Direct3D objects
//-----------------------------------------------------------------------------
HRESULT InitD3D()
{
    // Init D3D
    g_pD3D = Direct3DCreate8( D3D_SDK_VERSION );
    if( NULL == g_pD3D )
        return E_FAIL;

    // Create the D3D device
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.BackBufferWidth        = 640;
    d3dpp.BackBufferHeight       = 480;
    d3dpp.BackBufferFormat       = D3DFMT_A8R8G8B8;
    d3dpp.BackBufferCount        = 1;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;

    if( FAILED( g_pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL, 
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING, 
                                      &d3dpp, &g_pd3dDevice ) ) )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FreeD3D()
// Desc: 
//-----------------------------------------------------------------------------
VOID FreeD3D()
{
    if( g_pD3D )       g_pD3D->Release();
    if( g_pd3dDevice ) g_pd3dDevice->Release();
}




//-----------------------------------------------------------------------------
// Name: RecordPushBuffer()
// Desc: Records the push buffer. At any point in recording the push buffer,
//       you can call GetPushBufferOffset() to save offsets where you can later
//       fix up the data there.
//       Note that this function can also be used to calculate the size needed
//       to create the push buffer.
//-----------------------------------------------------------------------------
DWORD RecordPushBuffer()
{
    // Create a vertex shader
    DWORD dwVertexShader;
    g_pd3dDevice->CreateVertexShader( g_dwVertexShaderDecl, g_dwVertexShaderProgram,
                                      &dwVertexShader, 0 );

    // Create a pixel shader
    DWORD dwPixelShader;
    g_pd3dDevice->CreatePixelShader( (D3DPIXELSHADERDEF*)g_dwPixelShaderProgram,                                                
                                     &dwPixelShader );
    
    // Note that this matrix isn't initialized, which is okay because we'll
    // always fix it (the push buffer) up before playback.
    D3DMATRIX mat;

    // Start recording the push buffer. 
    g_pd3dDevice->BeginPushBuffer( g_pPushBuffer );

    // Set some state
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_NONE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );

    // Draw some vertices using the programable pipeline and vertices in the
    // push buffer

    // Save the push buffer offset so we know where to fixup data
    g_pd3dDevice->GetPushBufferOffset( &g_dwSetVertexShaderConstantFixupOffset );
    g_pd3dDevice->SetVertexShaderConstant( 0, &mat, 4 );
    g_pd3dDevice->SetVertexShader( dwVertexShader );
    g_pd3dDevice->SetPixelShader( dwPixelShader );
    g_pd3dDevice->DrawVerticesUP( D3DPT_TRIANGLELIST, 3, g_Vertices, sizeof(VERTEX) );
    
    // Draw some vertices using the fixed-function pipeline and vertices in a VB

    // Save the push buffer offset so we know where to fixup data
    g_pd3dDevice->GetPushBufferOffset( &g_dwSetModelViewFixupOffset );
    g_pd3dDevice->SetModelView( &mat, &mat, &mat );
    g_pd3dDevice->SetVertexShader( D3DFVF_XYZ|D3DFVF_DIFFUSE );
    g_pd3dDevice->SetPixelShader( NULL );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

    // Save the push buffer offset so we know where to fixup data
    g_pd3dDevice->GetPushBufferOffset( &g_dwSetVertexShaderInputFixupOffset );
    D3DSTREAM_INPUT Input = { g_pVertexBuffer, sizeof(VERTEX), 0 };
    g_pd3dDevice->SetVertexShaderInputDirect( &g_FixedFunctionShaderInputs, 1, &Input );
    g_pd3dDevice->DrawVertices( D3DPT_TRIANGLELIST, 0, 3 );

    // Restore state
    g_pd3dDevice->SetModelView( NULL, NULL, NULL );
    g_pd3dDevice->SetVertexShaderInput( NULL, 0, NULL );

    // Stop recording the push buffer
    g_pd3dDevice->EndPushBuffer();

    g_pd3dDevice->DeletePixelShader( dwPixelShader );
    g_pd3dDevice->DeleteVertexShader( dwVertexShader );

    // For convenience, return the size of the push buffer
    DWORD dwSize;
    g_pPushBuffer->GetSize( &dwSize );
    return dwSize;
}




//-----------------------------------------------------------------------------
// Name: InitializePushBufferData()
// Desc: Creates the push buffer from the data in the header file
//-----------------------------------------------------------------------------
HRESULT InitializePushBufferData()
{
    // Free resources
    if( g_pVertexBuffer ) g_pVertexBuffer->Release();
    if( g_pPushBuffer )   g_pPushBuffer->Release();

    // Create the vertex buffer
    g_dwVertexBufferSize = sizeof(g_Vertices);
    g_pd3dDevice->CreateVertexBuffer( g_dwVertexBufferSize , 0, 0, 0, &g_pVertexBuffer );
    
    // Fill vertex buffer
    VERTEX* pVertices;
    g_pVertexBuffer->Lock( 0, 0, (BYTE**)&pVertices, 0 );
    memcpy( pVertices, g_Vertices, g_dwVertexBufferSize );
    g_pVertexBuffer->Unlock();

    // Get the correct size for the push buffer

    // To create a push buffer, first create a dummy push buffer of minimum size
    // (4096 bytes) that we will use to query actual size.
    g_pd3dDevice->CreatePushBuffer( 4096, TRUE, &g_pPushBuffer );

    // Now record the push buffer to retrieve the size requirements, making sure
    // the size is at least 4096 bytes.
    DWORD dwPushBufferSize = max( 4096, RecordPushBuffer() );

    // Now release and recreate a push buffer of the req'd size. Note that the
    // bRunUsingCPUCopy flag does not matter, since the produced data is
    // exactly the same. When we load this data on the Xbox, we can decide then
    // whether we'll create the push buffer as RunUsingCPUCopy or not.
    g_pPushBuffer->Release();
    g_pd3dDevice->CreatePushBuffer( dwPushBufferSize, TRUE, &g_pPushBuffer );

    // And record the push-buffer for real
    RecordPushBuffer();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SavePushBuffer()
// Desc: Saves the push buffer and vertex buffer
//-----------------------------------------------------------------------------
HRESULT SavePushBuffer( const CHAR* strFilename )
{ 
    // Write out vertex and push buffer data
    HANDLE hFile = CreateFile( strFilename, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0 );
    if( INVALID_HANDLE_VALUE == hFile )
        return E_FAIL;

    DWORD dwNumWritten = 0;

    // Write out push buffer offsets
    {
        WriteFile( hFile, &g_dwSetVertexShaderInputFixupOffset,    sizeof(DWORD), &dwNumWritten, NULL );
        WriteFile( hFile, &g_dwSetVertexShaderConstantFixupOffset, sizeof(DWORD), &dwNumWritten, NULL );
        WriteFile( hFile, &g_dwSetModelViewFixupOffset,            sizeof(DWORD), &dwNumWritten, NULL );
    }

    // Write out the push buffer
    {
        WriteFile( hFile, &g_pPushBuffer->Size,           sizeof(DWORD), &dwNumWritten, NULL );
        WriteFile( hFile, &g_pPushBuffer->AllocationSize, sizeof(DWORD), &dwNumWritten, NULL );

        // Write out the push buffer data
        BYTE* pPushBufferData = (BYTE*)g_pPushBuffer->Data;
        WriteFile( hFile, pPushBufferData, g_pPushBuffer->Size, &dwNumWritten, NULL );
    }

    // Write out the vertex buffer
    {
        WriteFile( hFile, &g_dwVertexBufferSize, sizeof(DWORD), &dwNumWritten, NULL );

        // Write out the vertex buffer data
        BYTE* pVertexBufferData;
        g_pVertexBuffer->Lock( 0, 0, &pVertexBufferData, 0 );
        WriteFile( hFile, pVertexBufferData, g_dwVertexBufferSize, &dwNumWritten, NULL );
        g_pVertexBuffer->Unlock();
    }

    // Close and return
    CloseHandle( hFile );
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FreePushBufferData()
// Desc: Frees resources
//-----------------------------------------------------------------------------
VOID FreePushBufferData()
{
    if( g_pVertexBuffer ) g_pVertexBuffer->Release();
    if( g_pPushBuffer )   g_pPushBuffer->Release();
}




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Application entry point
//-----------------------------------------------------------------------------
int __cdecl main( int argc, char* argv[] )
{
    // Initialize Direct3D objects
    if( FAILED( InitD3D() ) )
    {
        printf( "ERROR: Could not create Direct3D!\n" );
        return -1;
    }

    // Create the push-buffer. Note that pushbuffers can be one of two types.
    // If bRunUsingCPUCopy is TRUE, then the push buffer is a system memory
    // object which gets memcopied onto the real push buffer. The other type of
    // push buffer is a video memory object that gets directly executed by the
    // GPU. To do so uses interrupts, though, so make this choice carefully.
    if( FAILED( InitializePushBufferData() ) )
    {
        printf( "ERROR: Could not create push-buffer!\n" );
        return -1;
    }

    // Save the push-buffer data into the project's media directory
    if( FAILED( SavePushBuffer( "Media\\PushBuffer.dat" ) ) )
    {
        printf( "ERROR: Could not save push-buffer!\n" );
        return -1;
    }

    printf( "Push-buffer data was successfully saved.\n" );

    // Cleanup and return
    FreePushBufferData();
    FreeD3D();
    return 0;
};


