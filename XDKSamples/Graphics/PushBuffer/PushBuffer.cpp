//-----------------------------------------------------------------------------
// File: PushBuffer.cpp
//
// Desc: Demonstrates using static pushbuffers. Pushbuffers can be thought of
//       as display lists or instruction buffers to the GPU. Rather than
//       rendering a scene via a myriad of D3D calls each frame, the calls can
//       be recorded into a static pushpuffer. Any dynamic data within a
//       push buffer, such as vertex shader constants that control the rotation
//       of an object, can be modified on the fly via "fixup" objects.
//
//       There are two types of static push buffers: CPU-copy pushbuffers and
//       directly executable pushbuffers. CPU-copy pushbuffers would be
//       recommended for such a simple sample, since directly executable
//       pushbuffers are implemented with interrupts. However, this sample can
//       use directly executable push buffers in order to show off how to use
//       the IDirect3DFixup objects.
//
// Hist: 03.20.01 - New for April XDK
//       11.26.02 - Modified to use SetModelView()
//       01.23.03 - Added support for loading push buffers created on the PC
//       02.06.03 - Code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include <XBUtil.h>

#include "PushBufferModel.h"




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle push\nbuffer model" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle RunUsing-\nCPUCopy flag" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_2, L"Display\nhelp" },
    { XBHELP_START_BUTTON, XBHELP_PLACEMENT_1, L"Pause" },
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]))




//-----------------------------------------------------------------------------
// Name: class CPushBufferModel
// Desc: Defines a model encapsulated in a push buffer
//-----------------------------------------------------------------------------
class CPushBufferModel
{
public:
    BOOL             m_bRunUsingCPUCopy; // Push buffer creation flag
    D3DPushBuffer*   m_pPushBuffer;      // The static push buffer
    D3DVertexBuffer* m_pVertexBuffer;    // Some geometry to draw
    
    // Fixup offsets
    DWORD            m_dwSetVertexShaderInputFixupOffset;
    DWORD            m_dwSetVertexShaderConstantFixupOffset;
    DWORD            m_dwSetModelViewFixupOffset;

public:
    // Access to the push buffer object
    D3DPushBuffer* GetPushBuffer()         { return m_pPushBuffer; }
    BOOL           IsTypeRunUsingCPUCopy() { return m_bRunUsingCPUCopy; }

    // Function to apply the fixups to the push buffer.
    VOID           ApplyFixups( LPDIRECT3DFIXUP8 pFixup,
                                D3DMATRIX* pMatWVP, D3DMATRIX* pMatModelView,
                                D3DMATRIX* pMaxInvModelView, 
                                D3DMATRIX* pMatComposite );

    // Constructor
    CPushBufferModel()
    {
        m_pVertexBuffer = NULL;
        m_pPushBuffer   = NULL;
    }

    // Destructor
    ~CPushBufferModel()
    {
        if( m_pVertexBuffer ) m_pVertexBuffer->Release();
        if( m_pPushBuffer )   m_pPushBuffer->Release();
    }
};




//-----------------------------------------------------------------------------
// Name: ApplyFixups()
// Desc: Fixes up the push buffer.
//-----------------------------------------------------------------------------
VOID CPushBufferModel::ApplyFixups( LPDIRECT3DFIXUP8 pFixup,
                                    D3DMATRIX* pMatWVP, D3DMATRIX* pMatModelView,
                                    D3DMATRIX* pMatInvModelView, 
                                    D3DMATRIX* pMatComposite )
{
    // Begin the push buffer fixup (Note that for RunUsingCPUCopy type pushbuffers,
    // the pFixup value should be NULL.)
    m_pPushBuffer->BeginFixup( pFixup, 0 );

    // Fixup the transforms passed in via SetVertexShaderConstant()
    m_pPushBuffer->SetVertexShaderConstant( m_dwSetVertexShaderConstantFixupOffset,
                                            0, pMatWVP, 4 );

    // Fixup the transforms passed in via SetModelView()
    m_pPushBuffer->SetModelView( m_dwSetModelViewFixupOffset,
                                 pMatModelView, pMatInvModelView, pMatComposite );

    // Fixup the vertex shader input
    D3DSTREAM_INPUT Input = { m_pVertexBuffer, sizeof(VERTEX), 0 };
    m_pPushBuffer->SetVertexShaderInputDirect( m_dwSetVertexShaderInputFixupOffset,
                                               &g_FixedFunctionShaderInputs, 1, &Input );

    // End the push buffer fixup
    m_pPushBuffer->EndFixup();
}




//-----------------------------------------------------------------------------
// Name: LoadPushBufferFromFile()
// Desc: Loads the push buffer and vertex buffer from a file
//-----------------------------------------------------------------------------
HRESULT LoadPushBufferFromFile( const CHAR* strFilename,
                                CPushBufferModel** ppPushBufferModel,
                                BOOL bRunUsingCPUCopy )
{
    if( NULL == strFilename || NULL == ppPushBufferModel )
        return E_INVALIDARG;

    // Read in vertex and push buffer data
    HANDLE hFile = CreateFile( strFilename, GENERIC_READ, 0, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
    if( INVALID_HANDLE_VALUE == hFile )
        return E_FAIL;

    DWORD dwNumRead = 0;

    // Read push buffer offsets
    DWORD dwSetVertexShaderInputFixupOffset;
    DWORD dwSetVertexShaderConstantFixupOffset;
    DWORD dwSetModelViewFixupOffset;
    ReadFile( hFile, &dwSetVertexShaderInputFixupOffset,    sizeof(DWORD), &dwNumRead, NULL );
    ReadFile( hFile, &dwSetVertexShaderConstantFixupOffset, sizeof(DWORD), &dwNumRead, NULL );
    ReadFile( hFile, &dwSetModelViewFixupOffset,            sizeof(DWORD), &dwNumRead, NULL );

    // Read push buffer size
    DWORD dwPushBufferAllocationSize;
    DWORD dwPushBufferSize;
    ReadFile( hFile, &dwPushBufferSize,           sizeof(DWORD), &dwNumRead, NULL );
    ReadFile( hFile, &dwPushBufferAllocationSize, sizeof(DWORD), &dwNumRead, NULL );
    
    // Create a push buffer of the req'd size
    D3DPushBuffer* pPushBuffer;
    g_pd3dDevice->CreatePushBuffer( dwPushBufferSize, bRunUsingCPUCopy, &pPushBuffer );

    // Read push buffer data
    ReadFile( hFile, (BYTE*)pPushBuffer->Data, dwPushBufferSize, &dwNumRead, NULL );

    // Normally the Size of a push buffer is updated as data is recorded into it,
    // but since we're putting our own data in there, don't forget to update the
    // size.
    pPushBuffer->Size = dwPushBufferSize;

    // Read vertex buffer size
    DWORD dwVBSize;
    ReadFile( hFile, &dwVBSize, sizeof(DWORD), &dwNumRead, NULL );

    // Create the vertex buffer
    D3DVertexBuffer* pVertexBuffer;
    g_pd3dDevice->CreateVertexBuffer( dwVBSize, 0, 0, 0, &pVertexBuffer );
    
    // Read vertex buffer data
    BYTE* pVertexBufferData;
    pVertexBuffer->Lock( 0, 0, &pVertexBufferData, 0 );
    ReadFile( hFile, pVertexBufferData, dwVBSize, &dwNumRead, NULL );
    pVertexBuffer->Unlock();

    // Close the file
    CloseHandle( hFile );

    // Create a new push buffer model from the objects loaded from the file
    (*ppPushBufferModel) = new CPushBufferModel();
    (*ppPushBufferModel)->m_dwSetVertexShaderInputFixupOffset    = dwSetVertexShaderInputFixupOffset;
    (*ppPushBufferModel)->m_dwSetVertexShaderConstantFixupOffset = dwSetVertexShaderConstantFixupOffset;
    (*ppPushBufferModel)->m_dwSetModelViewFixupOffset            = dwSetModelViewFixupOffset;
    (*ppPushBufferModel)->m_bRunUsingCPUCopy = bRunUsingCPUCopy;
    (*ppPushBufferModel)->m_pPushBuffer      = pPushBuffer;
    (*ppPushBufferModel)->m_pVertexBuffer    = pVertexBuffer;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RecordPushBuffer()
// Desc: Records the push buffer. At any point in recording the push buffer,
//       you can call GetPushBufferOffset() to save offsets where you can later
//       fix up the data there.
//-----------------------------------------------------------------------------
HRESULT RecordPushBuffer( D3DPushBuffer* pPushBuffer,
                          DWORD* pdwSetVertexShaderConstantFixupOffset = NULL,
                          DWORD* pdwSetModelViewFixupOffset = NULL,
                          DWORD* pdwSetVertexShaderInputFixupOffset = NULL )
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
    D3DMATRIX matDummy;
    D3DVertexBuffer vbDummy;
    XGSetVertexBufferHeader( 0, 0, 0, 0, &vbDummy, 0 );
    D3DSTREAM_INPUT StreamInput = { &vbDummy, sizeof(VERTEX), 0 };

    // Start recording the push buffer. 
    g_pd3dDevice->BeginPushBuffer( pPushBuffer );

    // Set some state
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_NONE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_LIGHTING,         FALSE );

    // Draw some vertices using the programable pipeline and vertices in the
    // push buffer, saving offsets so we know where to fixup data
    if( pdwSetVertexShaderConstantFixupOffset )
        g_pd3dDevice->GetPushBufferOffset( pdwSetVertexShaderConstantFixupOffset );
    g_pd3dDevice->SetVertexShaderConstant( 0, &matDummy, 4 );
    g_pd3dDevice->SetVertexShader( dwVertexShader );
    g_pd3dDevice->SetPixelShader( dwPixelShader );
    g_pd3dDevice->DrawVerticesUP( D3DPT_TRIANGLELIST, 3, g_Vertices, sizeof(VERTEX) );

    // Draw some vertices using the fixed-function pipeline and vertices in
    // a VB, saving offsets so we know where to fixup data
    if( pdwSetModelViewFixupOffset )
        g_pd3dDevice->GetPushBufferOffset( pdwSetModelViewFixupOffset );
    g_pd3dDevice->SetModelView( &matDummy, &matDummy, &matDummy );
    g_pd3dDevice->SetVertexShader( D3DFVF_XYZ|D3DFVF_DIFFUSE );
    g_pd3dDevice->SetPixelShader( NULL );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    if( pdwSetVertexShaderInputFixupOffset )
        g_pd3dDevice->GetPushBufferOffset( pdwSetVertexShaderInputFixupOffset );
    g_pd3dDevice->SetVertexShaderInputDirect( &g_FixedFunctionShaderInputs, 1, &StreamInput );
    g_pd3dDevice->DrawVertices( D3DPT_TRIANGLELIST, 0, 3 );

    // Restore state
    g_pd3dDevice->SetModelView( NULL, NULL, NULL );
    g_pd3dDevice->SetVertexShaderInput( NULL, 0, NULL );

    // Stop recording the push buffer
    g_pd3dDevice->EndPushBuffer();

    g_pd3dDevice->DeletePixelShader( dwPixelShader );
    g_pd3dDevice->DeleteVertexShader( dwVertexShader );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CreatePushBuffer()
// Desc: Creates the push buffer from data
//-----------------------------------------------------------------------------
HRESULT CreatePushBuffer( CPushBufferModel** ppPushBufferModel,
                          BOOL bRunUsingCPUCopy )
{
    // Create the vertex buffer
    D3DVertexBuffer* pVertexBuffer;
    g_pd3dDevice->CreateVertexBuffer( sizeof(g_Vertices) , 0, 0, 0, &pVertexBuffer );
    
    // Fill vertex buffer
    BYTE* pVertexBufferData;
    pVertexBuffer->Lock( 0, 0, &pVertexBufferData, 0 );
    memcpy( pVertexBufferData, g_Vertices, sizeof(g_Vertices) );
    pVertexBuffer->Unlock();

    // Create a dummy push buffer of default size that we can use before we
    // know what the actual size requirements will be.
    D3DPushBuffer* pTempPushBuffer = NULL;
    g_pd3dDevice->CreatePushBuffer( 4096, TRUE, &pTempPushBuffer );
    
    // Record the push buffer and get any fixup offsets that we'll need later
    DWORD dwSetVertexShaderConstantFixupOffset = 0;
    DWORD dwSetModelViewFixupOffset            = 0;
    DWORD dwSetVertexShaderInputFixupOffset    = 0;

    RecordPushBuffer( pTempPushBuffer, &dwSetVertexShaderConstantFixupOffset,
                                       &dwSetModelViewFixupOffset,
                                       &dwSetVertexShaderInputFixupOffset );

    // Get the actual size of the push buffer
    DWORD dwPushBufferSize;
    pTempPushBuffer->GetSize( &dwPushBufferSize );

    // Create a push buffer of the req'd size
    D3DPushBuffer* pPushBuffer;
    g_pd3dDevice->CreatePushBuffer( dwPushBufferSize, bRunUsingCPUCopy, &pPushBuffer );
    
    // Copy the push buffer data into the newly created push buffer
    memcpy( (VOID*)pPushBuffer->Data, (VOID*)pTempPushBuffer->Data, dwPushBufferSize );
    pPushBuffer->Size = dwPushBufferSize;

    // Free the temporary push buffer
    pTempPushBuffer->Release();

    // Create a new push buffer model from the objects created above
    (*ppPushBufferModel) = new CPushBufferModel();
    (*ppPushBufferModel)->m_dwSetVertexShaderInputFixupOffset    = dwSetVertexShaderInputFixupOffset;
    (*ppPushBufferModel)->m_dwSetVertexShaderConstantFixupOffset = dwSetVertexShaderConstantFixupOffset;
    (*ppPushBufferModel)->m_dwSetModelViewFixupOffset            = dwSetModelViewFixupOffset;
    (*ppPushBufferModel)->m_bRunUsingCPUCopy = bRunUsingCPUCopy;
    (*ppPushBufferModel)->m_pPushBuffer      = pPushBuffer;
    (*ppPushBufferModel)->m_pVertexBuffer    = pVertexBuffer;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont           m_Font;             // Font for text display
    CXBHelp           m_Help;             // Help class
    BOOL              m_bDrawHelp;        // Whether to draw help

    CPushBufferModel* m_pModelFromPC;     // Push buffer loaded from PC
    CPushBufferModel* m_pModelFromXbox;   // Push buffer created on Xbox
    BOOL              m_bUseModelFromPC;  // Model selection flag
    CPushBufferModel* m_pActiveModel;     // Active objects used for rendering

    BOOL              m_bRunUsingCPUCopy; // Push buffer creation flag
    D3DPushBuffer*    m_pActivePushBuffer;// Ptr to active push buffer 

    D3DFixup*         m_pFixups[2];       // Buffer of fixup objects
    DWORD             m_dwFixupSize;      // Size of the fixup object
    DWORD             m_dwActiveFixup;
    D3DFixup*         m_pActiveFixup;
    
    // Transforms for the vertex shader pipeline
    D3DXMATRIX        m_matWVP;
        
    // Transforms for the fixed-function pipeline
    D3DXMATRIX        m_matModelView, m_matInvModelView, m_matComposite;
        
    HRESULT CreatePushBufferModel();
public:
    HRESULT Initialize();                // Initialize the sample
    HRESULT Render();                    // Render the scene
    HRESULT FrameMove();                 // Perform per-frame updates

    CXBoxSample();
};




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    CXBoxSample xbApp;
    if( FAILED( xbApp.Create() ) )
        return;
    xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample() 
            :CXBApplication()
{
    // Allow unlimited frame rate
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    m_bDrawHelp        = FALSE;

    m_bRunUsingCPUCopy = TRUE;
    m_bUseModelFromPC  = FALSE;
    m_pActiveModel     = NULL;

    m_pFixups[0]       = NULL;
    m_pFixups[1]       = NULL;
    m_dwActiveFixup    = 0;
}




//-----------------------------------------------------------------------------
// Name: CreatePushBufferModel()
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::CreatePushBufferModel()
{
    // Delete any existing model.
    if( m_pActiveModel )
    {
        // Make sure GPU is not using our push buffer
        g_pd3dDevice->BlockUntilIdle();
        g_pd3dDevice->BlockUntilVerticalBlank();
        delete m_pActiveModel;
    }

    // Create or load a push buffer model
    if( m_bUseModelFromPC )
    {
        // Load pre-created push buffer data from a file. Note that this data is
        // created with the separate "CreatePushBufferOnPC" project.
        if( FAILED( LoadPushBufferFromFile( "d:\\Media\\PushBuffer.dat",
                                            &m_pActiveModel, m_bRunUsingCPUCopy ) ) )
            return XBAPPERR_MEDIANOTFOUND;
    }
    else
    {
        // Create a push buffer from scratch
        if( FAILED( CreatePushBuffer( &m_pActiveModel, m_bRunUsingCPUCopy ) ) )
            return E_FAIL;
    }

    // For convenience, get a ptr to the active push buffer
    m_pActivePushBuffer = m_pActiveModel->GetPushBuffer();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create a push buffer model
    if( FAILED( CreatePushBufferModel() ) )
        return E_FAIL;

    // Create the fixup objects that we'll need for directly executable
    // type push buffers
    {
        // Create an empty fixup object, which will be used to determine the size
        // needed for fixup objects
        LPDIRECT3DFIXUP8 pEmptyFixup;
        m_pd3dDevice->CreateFixup( 0, &pEmptyFixup );

        // Apply the fixup (note, we are only looking for fixup size, so
        // these variables don't need to be initialized
        m_pActiveModel->ApplyFixups( pEmptyFixup, &m_matWVP,
                                    &m_matModelView, &m_matInvModelView,
                                    &m_matComposite );

        // Finally, get the size of the fixup, and release it
        pEmptyFixup->GetSize( &m_dwFixupSize );
        pEmptyFixup->Release();
        
        // Create two (for double-buffering) fixup objects. These objects will be
        // filled by the app-dependent ApplyFixups() function, and then used when
        // push buffer is rendered.
        m_pd3dDevice->CreateFixup( m_dwFixupSize, &m_pFixups[0] );
        m_pd3dDevice->CreateFixup( m_dwFixupSize, &m_pFixups[1] );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle the push buffer model
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        m_bUseModelFromPC = !m_bUseModelFromPC;

        if( FAILED( CreatePushBufferModel() ) )
            assert(0);
    }

    // Toggle the bRunUsingCPUCopy flag
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
    {
        m_bRunUsingCPUCopy = !m_bRunUsingCPUCopy;

        if( FAILED( CreatePushBufferModel() ) )
            assert(0);
    }

    // Update the matrices for the scene
    {
        D3DXMATRIX matScale, matRotate, matWorld, matView, matProj;
        
        // Rotate around the Y axis
        D3DXMatrixScaling( &matScale, 0.5f, 1.0f, 1.0f );
        D3DXMatrixRotationY( &matRotate, m_fAppTime );
        D3DXMatrixMultiply( &matWorld, &matScale, &matRotate );

        // Set up our view matrix.
        const D3DXVECTOR3 vEyePos( 0.0f, 0.0f, -3.0f );
        const D3DXVECTOR3 vLookAt( 0.0f, 0.0f,  0.0f );
        const D3DXVECTOR3 vUp    ( 0.0f, 1.0f,  0.0f );
        D3DXMatrixLookAtLH( &matView, &vEyePos, &vLookAt, &vUp );

        // Set up our projection matrix.
        D3DXMATRIX matProjViewport;
        D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4,
                                    480.0f/640.0f, 1.0f, 800.0f );
        m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );
        m_pd3dDevice->GetProjectionViewportMatrix( &matProjViewport );

        // Calculate the model view transforms, for the SetModelView() API
        matWorld._41 = -0.5f;
        D3DXMatrixMultiply( &m_matModelView, &matWorld, &matView );
        D3DXMatrixInverse( &m_matModelView, NULL, &m_matInvModelView );
        D3DXMatrixMultiply( &m_matComposite, &m_matModelView, &matProjViewport );

        // Save the transpose of the World x View x Proj for the vertex shaders
        matWorld._41 = +0.5f;
        D3DXMatrixMultiply( &m_matWVP, &matWorld, &matView );
        D3DXMatrixMultiply( &m_matWVP, &m_matWVP, &matProj );
        D3DXMatrixTranspose( &m_matWVP, &m_matWVP );
    }

    // Select a fixup to use
    if( m_pActiveModel->IsTypeRunUsingCPUCopy() )
    {
        // CPU copy push buffers do not use fixup objects
        m_pActiveFixup = NULL;
    }
    else
    {
        // Check the space of the current fixup object
        DWORD dwSpace;
        m_pActiveFixup = m_pFixups[m_dwActiveFixup];
        m_pActiveFixup->GetSpace( &dwSpace );

        // If we overflowed the current fixup buffer, then use another fixup
        if( dwSpace < m_dwFixupSize )
        {
            // Note: If we tried to reset the same buffer we were just using, D3D
            // would have to sit and spin until the GPU is idle, since we just
            // used that buffer.
            m_dwActiveFixup = (m_dwActiveFixup+1) % 2;
            m_pActiveFixup  = m_pFixups[m_dwActiveFixup];
            m_pActiveFixup->Reset();
        }
    }

    // Okay, do the fixup for real
    m_pActiveModel->ApplyFixups( m_pActiveFixup, &m_matWVP, &m_matModelView,
                                 &m_matInvModelView, &m_matComposite );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the render target and z-buffer
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         0xff0000ff, 1.0f, 0L );

    // Run the static push buffer, which renders the objects in our scene
    m_pd3dDevice->RunPushBuffer( m_pActivePushBuffer, m_pActiveFixup );

    // Whack state back to reality after running a static push buffer
    m_pd3dDevice->SetVertexShader( NULL );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"PushBuffer" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        m_Font.DrawText( 64, 75, 0xffffffff, L"Push buffer created on: " );
        m_Font.DrawText( 0xffffff00, m_bUseModelFromPC ? L"PC" : L"Xbox" );
        m_Font.DrawText( 64, 100, 0xffffffff, L"bRunUsingCPUCopy flag: " );
        m_Font.DrawText( 0xffffff00, m_bRunUsingCPUCopy ? L"TRUE" : L"FALSE" );
        
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




