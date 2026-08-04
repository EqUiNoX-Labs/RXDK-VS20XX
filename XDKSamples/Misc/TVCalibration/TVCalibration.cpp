//-----------------------------------------------------------------------------
// File: TVCalibration.cpp
//
// Desc: This sample is a standalone program used to do basic calibration on a
//       TV. It supplies a set of on-screen instructions to ensure that the
//       TV is set to optimimum output settings.
//
// Hist: 12.02.02 - Ported from some E3 TV calibration code
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <dsound.h>
#include <dsstdfx.h>
#include <XBApp.h>
#include <XBFont.h>
#include <XBUtil.h>
#include <XBSound.h>




//-----------------------------------------------------------------------------
// Structures and Macros
//-----------------------------------------------------------------------------
struct SCREENVERTEX
{
    D3DXVECTOR4 pos;   // The transformed position for the vertex point.
    DWORD       color; // The vertex color. 
};
#define D3DFVF_SCREENVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)


struct D3DVERTEX
{
    D3DXVECTOR3 p;           // position
    D3DCOLOR    c;           // color
};
#define D3DFVF_D3DVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE)


struct D3DVERTEX_TEX
{
    D3DXVECTOR3 p;           // position
    D3DCOLOR    c;           // color
    FLOAT       tu, tv;     // texture
};
#define D3DFVF_D3DVERTEX_TEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)


enum State
{
   START,
   STAGE1,
   STAGE2,
   STAGE3,
   STAGE4,
   ENDSTAGE
};

// Constants to define our world space
#define XMIN -10
#define XMAX 10
#define ZMIN -10
#define ZMAX 10
#define YMIN 0
#define YMAX 5


// Some colors
#define SEMITRANS_BLACK 0x40000000
#define BLACK           0xff000000
#define WHITE           0xffffffff
#define YELLOW          0xffffff00
#define RED             0xffff0000
#define DARK_RED        0xff500000
#define DARK_GREEN      0xff008000




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Application class. The base class provides just about all the
//       functionality we want, so we're just supplying stubs to interface with
//       the non-C++ functions of the app.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Packed resources
    CXBPackedResource       m_xprResource;

    // Font
    CXBFont                 m_Font;

    // Current display state
    enum State              m_eCurrentState;

    // Track last button pressed
    INT                     m_iLastButton;

    // Geometry
    LPDIRECT3DTEXTURE8      m_pTestPatternTexture;
    
    // Geometry for floor, source, and listener
    D3DVERTEX               m_pFloorVertices[4];    // Quad for the floor
    D3DVERTEX_TEX           m_pSourceVertices[4];   // Quad for the source
    D3DVERTEX               m_pListenerVertices[4]; // Quad for the listener
    LPDIRECT3DVERTEXBUFFER8 m_pGridVB;              // Lines to grid the floor
    LPDIRECT3DTEXTURE8      m_pSpeakerTexture;

    // Sound variables
    CWaveFile               m_wfSound;              // Wave file
    LONG                    m_lVolumePercent;       // Current volume
    LPDIRECTSOUND8          m_pDSound;              // DirectSound object
    LPDIRECTSOUNDBUFFER8    m_pDSBuffer;            // DirectSoundBuffer
    BYTE*                   m_pSampleData;          // Sample data from wav

    // Sound source and listener positions
    D3DXVECTOR3             m_vSourcePosition;      // Source position vector
    D3DXVECTOR3             m_vListenerPosition;    // Listener position vector

    // Transform matrices
    D3DXMATRIX              m_matWorld;             // World transform
    D3DXMATRIX              m_matView;              // View transform
    D3DXMATRIX              m_matProj;              // Projection transform

    // Drawing functions
    VOID    DrawLine( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, DWORD dwStartColor, DWORD dwEndColor );
    VOID    DrawThickLine( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, DWORD dwColor, FLOAT fWidth );
    VOID    DrawRectOutline( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, DWORD dwColor );
    VOID    DrawRect( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, DWORD dwStartColor, DWORD dwEndColor );
    VOID    DrawTextBox( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, WCHAR* strText );
    VOID    DrawTestPattern();
    VOID    DrawStartScreen();
    VOID    DrawStage1Screen();
    VOID    DrawStage2Screen();
    VOID    DrawStage3Screen();
    VOID    DrawStage4Screen();

public:
    HRESULT Initialize();
    HRESULT Render();
    HRESULT FrameMove();

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
// Desc: Application constructor. Sets attributes for the app.
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
    m_iLastButton       = XINPUT_GAMEPAD_A;
    m_eCurrentState     = START;

    m_pTestPatternTexture = NULL;

    m_pGridVB           = NULL;
    m_pSpeakerTexture   = NULL;

    m_lVolumePercent    = 100;
    m_pSampleData       = NULL;
    m_vSourcePosition   = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    m_vListenerPosition = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize scene objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return E_FAIL;

    // Load the packed resources
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return E_FAIL;
    m_pTestPatternTexture = m_xprResource.GetTexture( "TestPattern" );
    m_pSpeakerTexture     = m_xprResource.GetTexture( "Speaker" );

    // Set the transform matrices
    D3DXVECTOR3 vEyePt      = D3DXVECTOR3( XMIN, 45.0f,  ZMAX / 2.0f );
    D3DXVECTOR3 vLookatPt   = D3DXVECTOR3( XMIN,  0.0f,  ZMAX / 2.0f );
    D3DXVECTOR3 vUpVec      = D3DXVECTOR3( 0.0f,  0.0f,  1.0f );
    D3DXMatrixIdentity( &m_matWorld );
    D3DXMatrixLookAtLH( &m_matView, &vEyePt, &vLookatPt, &vUpVec );
    D3DXMatrixPerspectiveFovLH( &m_matProj, D3DX_PI/4, 4.0f/3.0f, 1.0f, 10000.0f );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProj );

    // Create our vertex buffers
    m_pd3dDevice->CreateVertexBuffer( 2 * ( (ZMAX-ZMIN+1) + (XMAX-XMIN+1) ) * sizeof(D3DVERTEX), 0, 0, 0, &m_pGridVB );
    
    // Fill the VB for the floor
    m_pFloorVertices[0].p = D3DXVECTOR3( XMIN, 0.0f, ZMIN ); m_pFloorVertices[0].c = 0xff101010;
    m_pFloorVertices[1].p = D3DXVECTOR3( XMAX, 0.0f, ZMIN ); m_pFloorVertices[1].c = 0xff101010;
    m_pFloorVertices[2].p = D3DXVECTOR3( XMAX, 0.0f, ZMAX ); m_pFloorVertices[2].c = 0xff101010;
    m_pFloorVertices[3].p = D3DXVECTOR3( XMIN, 0.0f, ZMAX ); m_pFloorVertices[3].c = 0xff101010;

    // Fill the VB for the grid
    D3DVERTEX* pGridVertices;
    m_pGridVB->Lock( 0, 0, (BYTE **)&pGridVertices, 0 );
    // RXDK: hoisted out of the for-init (MSVC's old for-scope leaked it past the loop)
    int i, j;
    for( i = ZMIN, j = 0; i <= ZMAX; i++, j++ )
    {
        pGridVertices[j*2+0].p = D3DXVECTOR3( XMIN, 0, (FLOAT)i ); pGridVertices[j*2+0].c = 0xff00a000;
        pGridVertices[j*2+1].p = D3DXVECTOR3( XMAX, 0, (FLOAT)i ); pGridVertices[j*2+1].c = 0xff00a000;
    }
    for( i = XMIN; i <= XMAX; i++, j++ )
    {
        pGridVertices[j*2+0].p = D3DXVECTOR3( (FLOAT)i, 0, ZMIN ); pGridVertices[j*2+0].c = 0xff00a000;
        pGridVertices[j*2+1].p = D3DXVECTOR3( (FLOAT)i, 0, ZMAX ); pGridVertices[j*2+1].c = 0xff00a000;
    }
    m_pGridVB->Unlock();

    //------------------------  
    // Sound stuff
    //------------------------  
    
    // Create DirectSound
    if( FAILED( DirectSoundCreate( NULL, &m_pDSound, NULL ) ) )
        return E_FAIL;

    // Note: If the application doesn't care about vertical HRTF positioning,
    //       calling DirectSoundUseLightHRTF() can save about 60k of memory.
    DirectSoundUseLightHRTF();

    // Download the standard DirectSound effects image
    DSEFFECTIMAGELOC EffectLoc;
    EffectLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    EffectLoc.dwCrosstalkIndex   = GraphXTalk_XTalk;
    LPDSEFFECTIMAGEDESC pDesc;
    if( FAILED( XAudioDownloadEffectsImage( "d:\\Media\\dsstdfx.bin", &EffectLoc,
                                            XAUDIO_DOWNLOADFX_EXTERNFILE, &pDesc ) ) )
        return E_FAIL;

    // Load up a wave file
    if( FAILED( m_wfSound.Open( "D:\\Media\\Sounds\\TestTone.wav" ) ) )
        return E_FAIL;

    // Check that it is a mono wav as we are going to use it for 3D positioning
    WAVEFORMATEXTENSIBLE wfSoundFormat;
    if( FAILED( m_wfSound.GetFormat( &wfSoundFormat ) ) )
        return E_FAIL;

    if( wfSoundFormat.Format.nChannels != 1 )
    {
        OutputDebugString( _T(".wav file must be mono \n") );
        return E_FAIL;
    }

    // Create a sound buffer, we will be handling the memory for the buffer
    // ourselves in the sample, not via DSound
    DSBUFFERDESC dsbdesc;
    ZeroMemory( &dsbdesc, sizeof(DSBUFFERDESC) );
    dsbdesc.dwSize        = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags       = DSBCAPS_CTRL3D | DSBCAPS_LOCDEFER;
    dsbdesc.dwBufferBytes = 0;
    dsbdesc.lpwfxFormat   = (WAVEFORMATEX*)&wfSoundFormat;

    if( FAILED( m_pDSound->CreateSoundBuffer( &dsbdesc, &m_pDSBuffer, NULL ) ) )
        return E_FAIL;

    // Allocate memory for the sound
    DWORD dwSoundSampleSize;
    m_wfSound.GetDuration( &dwSoundSampleSize );
    VOID* pvBuffer = malloc( dwSoundSampleSize );
    if( !pvBuffer )
        return E_FAIL;
    m_pSampleData = (BYTE*)pvBuffer;

    // Read sample data from the file
    m_wfSound.ReadSample( 0, m_pSampleData, dwSoundSampleSize, &dwSoundSampleSize );

    // Set up values for the new buffer
    m_pDSBuffer->SetBufferData( m_pSampleData, dwSoundSampleSize );
    m_pDSBuffer->SetLoopRegion( 0, dwSoundSampleSize );
    m_pDSBuffer->SetCurrentPosition( 0 );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene. As this code only changes text, there is no real animation
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    D3DXVECTOR3    vListenerOld = m_vListenerPosition;
    D3DXVECTOR3    vSourceOld   = m_vSourcePosition;

    if( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_A] > 0 )
    {
        if( m_iLastButton != XINPUT_GAMEPAD_A )
        {
            m_iLastButton = XINPUT_GAMEPAD_A;

            switch(m_eCurrentState)
            {
                case START:
                    m_eCurrentState = STAGE1;
                    break;

                case STAGE1:
                    m_eCurrentState = STAGE2;
                    break;

                case STAGE2:
                    m_eCurrentState = STAGE3;
                    break;

                case STAGE3:
                    m_eCurrentState = STAGE4;
                    break;

                case STAGE4:
                    m_eCurrentState = START;
                    break;
                default:
                    m_eCurrentState = START;
            }
        }
    }
    else
    {
        m_iLastButton = !XINPUT_GAMEPAD_A;
    }

    // Increase/Decrease volume
    m_lVolumePercent += ( ( m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP   ) - 
                          ( m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN ) );

    // Make sure volume is in the appropriate range
    if( m_lVolumePercent <   0 )  m_lVolumePercent =   0;
    if( m_lVolumePercent > 100 )  m_lVolumePercent = 100;

    // Set volume
    FLOAT fVolume = powf( m_lVolumePercent/100.0f, 0.2f );
    m_pDSBuffer->SetVolume( DSBVOLUME_MIN + (LONG)(fVolume*(DSBVOLUME_MAX-DSBVOLUME_MIN)) );

    // Handle movement of sound
    m_vSourcePosition.x += m_DefaultGamepad.fX1 * 0.2f;
    if( m_vSourcePosition.x < XMIN )
        m_vSourcePosition.x = XMIN;
    else if( m_vSourcePosition.x > XMAX )
        m_vSourcePosition.x = XMAX;

    m_vSourcePosition.z += m_DefaultGamepad.fY1 * 0.2f;
    if( m_vSourcePosition.z < ZMIN )
        m_vSourcePosition.z = ZMIN;
    else if( m_vSourcePosition.z > ZMAX )
        m_vSourcePosition.z = ZMAX;

    // Update source/listener vertex buffers
    m_pSourceVertices[0].p = m_vSourcePosition + D3DXVECTOR3(-1.0f, 0.0f, -1.0f ); m_pSourceVertices[0].c = 0xffff0000; m_pSourceVertices[0].tu = 0.0f; m_pSourceVertices[0].tv = 0.0f; 
    m_pSourceVertices[1].p = m_vSourcePosition + D3DXVECTOR3( 1.0f, 0.0f, -1.0f ); m_pSourceVertices[1].c = 0xffff0000; m_pSourceVertices[1].tu = 1.0f; m_pSourceVertices[1].tv = 0.0f; 
    m_pSourceVertices[2].p = m_vSourcePosition + D3DXVECTOR3( 1.0f, 0.0f,  1.0f ); m_pSourceVertices[2].c = 0xffff0000; m_pSourceVertices[2].tu = 1.0f; m_pSourceVertices[2].tv = 1.0f; 
    m_pSourceVertices[3].p = m_vSourcePosition + D3DXVECTOR3(-1.0f, 0.0f,  1.0f ); m_pSourceVertices[3].c = 0xffff0000; m_pSourceVertices[3].tu = 0.0f; m_pSourceVertices[3].tv = 1.0f; 

    m_pListenerVertices[0].p = m_vListenerPosition + D3DXVECTOR3(-0.5f, 0.0f, -0.5f ); m_pListenerVertices[0].c = 0xffffff00;
    m_pListenerVertices[1].p = m_vListenerPosition + D3DXVECTOR3( 0.5f, 0.0f, -0.5f ); m_pListenerVertices[1].c = 0xffffff00;
    m_pListenerVertices[2].p = m_vListenerPosition + D3DXVECTOR3( 0.5f, 0.0f,  0.5f ); m_pListenerVertices[2].c = 0xffffff00;
    m_pListenerVertices[3].p = m_vListenerPosition + D3DXVECTOR3(-0.5f, 0.0f,  0.5f ); m_pListenerVertices[3].c = 0xffffff00;

    // Position the sound and listener in 3D. 
    // We use DS3D_DEFERRED so that all the changes will 
    // be commited at once.
    D3DXVECTOR3 vListenerVelocity = ( m_vListenerPosition - vListenerOld ) / m_fElapsedTime;
    D3DXVECTOR3 vSoundVelocity = ( m_vSourcePosition - vSourceOld ) / m_fElapsedTime;

    // Source position/velocity/volume
    m_pDSBuffer->SetPosition( m_vSourcePosition.x, m_vSourcePosition.y, m_vSourcePosition.z, DS3D_DEFERRED );
    m_pDSBuffer->SetVelocity( vSoundVelocity.x, vSoundVelocity.y, vSoundVelocity.z, DS3D_DEFERRED );

    // Listener position/velocity
    m_pDSound->SetPosition( m_vListenerPosition.x, m_vListenerPosition.y, m_vListenerPosition.z, DS3D_DEFERRED  );
    m_pDSound->SetVelocity( vListenerVelocity.x, vListenerVelocity.y, vListenerVelocity.z, DS3D_DEFERRED );

    // Commit position/velocity changes
    m_pDSound->CommitDeferredSettings();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawLine()
// Desc: 
//-----------------------------------------------------------------------------
VOID CXBoxSample::DrawLine( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2,
                            DWORD dwStartColor, DWORD dwEndColor )
{
    SCREENVERTEX v[2];
    v[0].pos = D3DXVECTOR4( x1, y1, 0.5f, 1.0f );   v[0].color = dwStartColor;
    v[1].pos = D3DXVECTOR4( x2, y2, 0.5f, 1.0f );   v[1].color = dwEndColor;
    
    // Render the line
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );

    m_pd3dDevice->SetVertexShader( D3DFVF_SCREENVERTEX) ;
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 1, v, sizeof(SCREENVERTEX) );
}




//-----------------------------------------------------------------------------
// Name: DrawThickLine()
// Desc: 
//-----------------------------------------------------------------------------
VOID CXBoxSample::DrawThickLine( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2,
                                 DWORD dwColor, FLOAT fWidth )
{
    // Determine offsets for the black line
    D3DXVECTOR3 vc( y2-y1, -x2+x1, 0.0f );
    D3DXVec3Normalize( &vc, &vc );
    D3DXVECTOR3 vc1 = vc * (fWidth/2+1.0f);
    D3DXVECTOR3 vc2 = vc * (fWidth/2);
    
    // Draw the line
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_DIFFUSE ) ;
    m_pd3dDevice->Begin( D3DPT_QUADLIST );

    m_pd3dDevice->SetVertexDataColor( D3DVSDE_DIFFUSE, BLACK );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x1-vc1.x, y1-vc1.y, 0.0f, 0.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x1+vc1.x, y1+vc1.y, 0.0f, 0.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x2+vc1.x, y2+vc1.y, 0.0f, 0.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x2-vc1.x, y2-vc1.y, 0.0f, 0.0f );

    m_pd3dDevice->SetVertexDataColor( D3DVSDE_DIFFUSE, dwColor );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x1-vc2.x, y1-vc2.y, 0.0f, 0.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x1+vc2.x, y1+vc2.y, 0.0f, 0.0f );
    m_pd3dDevice->SetVertexDataColor( D3DVSDE_DIFFUSE, dwColor );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x2+vc2.x, y2+vc2.y, 0.0f, 0.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x2-vc2.x, y2-vc2.y, 0.0f, 0.0f );
    
    m_pd3dDevice->End();
}




//-----------------------------------------------------------------------------
// Name: DrawRectOutline()
// Desc: 
//-----------------------------------------------------------------------------
VOID CXBoxSample::DrawRectOutline( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, 
                                   DWORD dwColor )
{
    DrawLine( x1, y1, x1, y2, dwColor, dwColor );
    DrawLine( x1, y1, x2, y1, dwColor, dwColor );
    DrawLine( x2, y1, x2, y2, dwColor, dwColor );
    DrawLine( x1, y2, x2, y2, dwColor, dwColor );
}




//-----------------------------------------------------------------------------
// Name: DrawRect()
// Desc: 
//-----------------------------------------------------------------------------
VOID CXBoxSample::DrawRect( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, 
                            DWORD dwStartColor, DWORD dwEndColor )
{
    SCREENVERTEX v[4];
    v[0].pos = D3DXVECTOR4( x1-0.5f, y1-0.5f, 1.0f, 1.0f );  v[0].color = dwStartColor;
    v[1].pos = D3DXVECTOR4( x2-0.5f, y1-0.5f, 1.0f, 1.0f );  v[1].color = dwStartColor;
    v[2].pos = D3DXVECTOR4( x1-0.5f, y2-0.5f, 1.0f, 1.0f );  v[2].color = dwEndColor;
    v[3].pos = D3DXVECTOR4( x2-0.5f, y2-0.5f, 1.0f, 1.0f );  v[3].color = dwEndColor;
    
    // Render the rectangle
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE,   TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    m_pd3dDevice->SetVertexShader( D3DFVF_SCREENVERTEX );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof(SCREENVERTEX) );
}




//-----------------------------------------------------------------------------
// Name: DrawTextBox()
// Desc: Renders text in an outlined, semi-transparent box
//-----------------------------------------------------------------------------
VOID CXBoxSample::DrawTextBox( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, 
                               WCHAR* strText )
{
    DrawRect( x1, y1, x2, y2, SEMITRANS_BLACK, SEMITRANS_BLACK );
    DrawRectOutline( x1, y1, x2, y2, BLACK );
    m_Font.DrawText( x1+5, y1+5, WHITE, strText );
}




//-----------------------------------------------------------------------------
// Name: DrawTestPattern()
// Desc: 
//-----------------------------------------------------------------------------
VOID CXBoxSample::DrawTestPattern()
{
    // Set state for rendering the quad
    m_pd3dDevice->SetTexture( 0, m_pTestPatternTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
    m_pd3dDevice->Begin( D3DPT_QUADLIST );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 0.0f, 0.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX,   0.0f-0.5f,   0.0f-0.5f, 0.0f, 1.0f );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 640.0f, 0.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, 640.0f-0.5f,   0.0f-0.5f, 0.0f, 1.0f );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 640.0f, 480.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, 640.0f-0.5f, 480.0f-0.5f, 0.0f, 1.0f );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 0.0f, 480.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX,   0.0f-0.5f, 480.0f-0.5f, 0.0f, 1.0f );
    m_pd3dDevice->End();
}




//-----------------------------------------------------------------------------
// Name: DrawStartScreen()
// Desc: 
//-----------------------------------------------------------------------------
VOID CXBoxSample::DrawStartScreen()
{
    // Draw the test pattern
    DrawTestPattern();

    // Stop the sound if it is playing
    m_pDSBuffer->Stop();

    // Render instructions in an outlined, semi-transparent box
    DrawTextBox( 64, 50, 576, 135, L"Turn up the CONTRAST to full, then turn up the\n"
                                   L"BRIGHTNESS until you can see five separate\n"
                                   L"black regions between these lines." );

    // Draw lines
    DrawThickLine( 468, 136, 468, 340, WHITE, 2.0f );
    DrawThickLine( 336, 340, 600, 340, WHITE, 2.0f );
    DrawThickLine( 336, 340, 336, 360, WHITE, 2.0f );
    DrawThickLine( 600, 340, 600, 360, WHITE, 2.0f );
}




//-----------------------------------------------------------------------------
// Name: DrawStage1Screen()
// Desc: 
//-----------------------------------------------------------------------------
VOID CXBoxSample::DrawStage1Screen()
{
    // Draw the test pattern
    DrawTestPattern();

    // Render instructions in an outlined, semi-transparent box
    DrawTextBox( 64, 50, 576, 135, L"Now reduce the BRIGHTNESS until the gray bar\n"
                                   L"disappears, then turn the BRIGHTNESS back up\n"
                                   L"until it becomes just visible." );

    // Draw lines
    DrawThickLine( 536, 136, 536, 340, WHITE, 2.0f );
    DrawThickLine( 520, 340, 552, 340, WHITE, 2.0f );
    DrawThickLine( 520, 340, 520, 360, WHITE, 2.0f );
    DrawThickLine( 552, 340, 552, 360, WHITE, 2.0f );
}




//-----------------------------------------------------------------------------
// Name: DrawStage2Screen()
// Desc: 
//-----------------------------------------------------------------------------
VOID CXBoxSample::DrawStage2Screen()
{
    // Draw the test pattern
    DrawTestPattern();

    // Render instructions in an outlined, semi-transparent box
    DrawTextBox( 64, 50, 576, 160, L"Set the CONTRAST by lowering it until the edges\n"
                                   L"of the white bar look sharp, continue lowering\n"
                                   L"until white bar begins to look not-white, then turn\n"
                                   L"back up until white bar looks white again." );

    // Draw lines
    DrawThickLine( 173, 161, 173, 340, WHITE, 2.0f );
    DrawThickLine( 113, 340, 223, 340, WHITE, 2.0f );
    DrawThickLine( 113, 340, 113, 360, WHITE, 2.0f );
    DrawThickLine( 223, 340, 223, 360, WHITE, 2.0f );
}




//-----------------------------------------------------------------------------
// Name: DrawStage3Screen()
// Desc: 
//-----------------------------------------------------------------------------
VOID CXBoxSample::DrawStage3Screen()
{
    // Draw the test pattern
    DrawTestPattern();

    // Render instructions in an outlined, semi-transparent box
    DrawTextBox( 64, 50, 576, 110, L"Now adjust COLOR to minimize bleeding, using\n"
                                   L"the red bar as your main reference." );
}




//-----------------------------------------------------------------------------
// Name: DrawStage4Screen()
// Desc: 
//-----------------------------------------------------------------------------
VOID CXBoxSample::DrawStage4Screen()
{
    // Start Sound
    m_pDSBuffer->Play( 0, 0, DSBPLAY_LOOPING );

    // Render a background
    RenderGradientBackground( DARK_GREEN, BLACK );

    // Render instructions in an outlined, semi-transparent box
    DrawTextBox( 64, 50, 576, 160, L"Adjust TV sound to desired level. Use UP/DOWN\n"
                                   L"to control program volume for testing and use\n"
                                   L"the left thumbstick to move the direction of the\n"
                                   L"sound to test speakers." );

    // Render the volume bar
    DrawRect( 64, 390-(FLOAT)m_lVolumePercent, 84, 390, RED, DARK_RED );
    DrawRectOutline( 64, 290, 84, 390, YELLOW );

    // Render the volume text
    WCHAR strVolumeText[40];
    swprintf( strVolumeText, L"Volume: %d%%", m_lVolumePercent );
    m_Font.DrawText( 64, 390, 0xffffff00, strVolumeText);

    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,         FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_NONE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetVertexShader( D3DFVF_D3DVERTEX );

    // Draw the floor
    m_pd3dDevice->DrawVerticesUP( D3DPT_QUADLIST, 4, m_pFloorVertices, sizeof(D3DVERTEX) );

    // Draw the grid
    m_pd3dDevice->SetStreamSource( 0, m_pGridVB, sizeof(D3DVERTEX) );
    m_pd3dDevice->DrawVertices( D3DPT_LINELIST, 0, 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ) );

    // Draw the listener
    m_pd3dDevice->DrawVerticesUP( D3DPT_QUADLIST, 4, m_pListenerVertices, sizeof(D3DVERTEX) );

    // Draw the source
    m_pd3dDevice->SetTexture( 0, m_pSpeakerTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    m_pd3dDevice->SetVertexShader( D3DFVF_D3DVERTEX_TEX );
    m_pd3dDevice->DrawVerticesUP( D3DPT_QUADLIST, 4, m_pSourceVertices, sizeof(D3DVERTEX_TEX) );
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Depending on the app state, call one of the following methods
    switch( m_eCurrentState )
    {
        case START:
            DrawStartScreen();
            break;

        case STAGE1:
            DrawStage1Screen();
            break;
        
        case STAGE2:
            DrawStage2Screen();
            break;
        
        case STAGE3:
            DrawStage3Screen();
            break;
        
        case STAGE4:
            DrawStage4Screen();
            break;
        
        case ENDSTAGE:
            break;
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




