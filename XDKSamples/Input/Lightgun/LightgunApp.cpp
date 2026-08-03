//-----------------------------------------------------------------------------
// File: LightgunApp.cpp
//
// Desc: Tool to experiment with all things related to an Xbox lightgun.
//
// Hist: 10.08.02 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBMesh.h>
#include <XBUtil.h>
#include <XBResource.h>
#include <xgraphics.h>
#include "LightgunMesh.h"
#include "Lightgun.h"




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Application class. The base class provides just about all the
//       functionality we want, so we're just supplying stubs to interface with
//       the non-C++ functions of the app.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Valid app states
    enum APPSTATE 
    { 
        APPSTATE_CONTROLTEST=0, 
        APPSTATE_BULLETHOLESTEST,
        APPSTATE_CROSSHAIRSTEST,
        APPSTATE_CALIBRATION,
        APPSTATE_MAX,
    };

    // General application members
    APPSTATE           m_AppState;           // State of the app
    APPSTATE           m_OldAppState;        // Previous app state
    
    CXBPackedResource  m_xprResource;        // Packed resources for the app
    CXBFont            m_Font16;             // 16-point font class
    CXBFont            m_Font12;             // 12-point font class
    
    D3DDISPLAYMODE     m_DisplayMode;        // Current display mode
    WCHAR              m_strDisplayMode[80]; // Desc of display mode

    // Geometry
    CLightgunMesh      m_LightGunMesh;       // Geometry for the lightgun

    // Active gamepad/lightgun
    DWORD              m_dwNumInsertedLightguns;
    CLightgun          m_Lightgun;
    
    // Vibration motor values
    WORD               m_wLeftMotorSpeed;
    WORD               m_wRightMotorSpeed;

    // Calibration data
    BOOL               m_bCalibratingCenter;
    SHORT              m_CenterCalibrationPointX;
    SHORT              m_CenterCalibrationPointY;
    SHORT              m_UpperLeftCalibrationPointX;
    SHORT              m_UpperLeftCalibrationPointY;

    // Calibration target and gun testing
    D3DTexture*        m_pTargetTexture;
    D3DTexture*        m_pBulletHolesTexture;
    D3DXVECTOR2        m_vBulletHoleList[100];
    DWORD              m_dwNumBulletHoles;

    BOOL               m_bVideoModeChanged;

    // Changes the video mode to the next supported mode
    HRESULT SelectNextVideoMode();

    // Drawing helper functions
    HRESULT RenderTarget( FLOAT fTargetX, FLOAT fTargetY );
    
    // The page that says to insert or remove controllers
    HRESULT RenderInsertRemoveControllerPage();

    // The page for testing the lightgun's controls
    HRESULT InitControlTestPage();
    HRESULT FrameMoveControlTestPage();
    HRESULT RenderControlTestPage();

    // The page for test firing the lightgun
    HRESULT InitBulletHolesTestPage();
    HRESULT FrameMoveBulletHolesTestPage();
    HRESULT RenderBulletHolesTestPage();

    // The page for showing cross hairs on a light background
    HRESULT InitCrosshairsTestPage();
    HRESULT FrameMoveCrosshairsTestPage();
    HRESULT RenderCrosshairsTestPage();

    // The page for calibrating the lightgun
    HRESULT InitCalibrationPage();
    HRESULT FrameMoveCalibrationPage();
    HRESULT RenderCalibrationPage();

protected:
    HRESULT Initialize();
    HRESULT FrameMove();
    HRESULT Render();

public:
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
    // Initialize state
    m_AppState               = APPSTATE_CONTROLTEST;
    m_dwNumInsertedLightguns = 0L;
    m_wLeftMotorSpeed        = 0;
    m_wRightMotorSpeed       = 0;

    // Make sure lightguns use the SET REPORT method for USB communication. 
    g_PollingParameters.fInterruptOut = FALSE;

    // Select an initial video mode
    SelectNextVideoMode();
}




//-----------------------------------------------------------------------------
// Name: SelectNextVideoMode()
// Desc: Changes the current video mode to the next available mode
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SelectNextVideoMode()
{
    DWORD dwNumModes = Direct3D::GetAdapterModeCount( 0 );

    static DWORD dwCurrentMode = dwNumModes-1;

    for( DWORD i=0; i<dwNumModes; i++ )
    {
        // Check the next available mode
        DWORD dwMode = (dwCurrentMode + i + 1 ) % dwNumModes;
        Direct3D::EnumAdapterModes( 0, dwMode, &m_DisplayMode );

        // Skip modes we don't care about
        if( m_DisplayMode.Format != D3DFMT_LIN_A8R8G8B8 )
            continue;
        if( m_DisplayMode.Flags & D3DPRESENTFLAG_FIELD )
            continue;
        if( m_DisplayMode.Flags & D3DPRESENTFLAG_10X11PIXELASPECTRATIO )
            continue;
        if( m_DisplayMode.Flags & D3DPRESENTFLAG_EMULATE_REFRESH_RATE )
            continue;

        // If we get here, we found an acceptable mode
        dwCurrentMode = dwMode;
        break;
    }

    // Build a display mode string
    WCHAR* strMode;
    if( m_DisplayMode.Height == 1080 )
        strMode = L"1080i";
    else if( m_DisplayMode.Flags & D3DPRESENTFLAG_PROGRESSIVE )
    {
        if( m_DisplayMode.Height == 480 )
            strMode = L"480p";
        else
            strMode = L"720p";
    }
    else if( XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I )
        strMode = L"PAL";
    else
        strMode = L"NTSC";

    swprintf( m_strDisplayMode, L"Mode = %d x %d %s @%dHz%s%s", 
                                m_DisplayMode.Width, m_DisplayMode.Height, 
                                strMode, m_DisplayMode.RefreshRate,
                                m_DisplayMode.Flags & D3DPRESENTFLAG_WIDESCREEN ? L" Widescreen" : L"" );

    // Set the new presentation parameters
    m_d3dpp.BackBufferWidth            = m_DisplayMode.Width;
    m_d3dpp.BackBufferHeight           = m_DisplayMode.Height;
    m_d3dpp.Flags                      = m_DisplayMode.Flags;
    m_d3dpp.FullScreen_RefreshRateInHz = m_DisplayMode.RefreshRate;

    // If a device already exists, apply the new presentation parameters
    if( m_pd3dDevice )
    {
        // Reset the device with the new parameters
        m_pd3dDevice->Reset( &m_d3dpp );

        // Clear both buffers to avoid flicker artifacts
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L );
        m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L );
        m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    }

    // Scale fonts to look right in the various display modes
    if( m_DisplayMode.Flags & D3DPRESENTFLAG_WIDESCREEN )
    {
        m_Font16.SetScaleFactors( 0.75f, 1.0f );
        m_Font12.SetScaleFactors( 0.75f, 1.0f );
    }
    else
    {
        m_Font16.SetScaleFactors( 1.0f, 1.0f );
        m_Font12.SetScaleFactors( 1.0f, 1.0f );
    }

    // Set a flag that the video mode changed
    m_bVideoModeChanged = TRUE;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create the fonts
    if( FAILED( m_Font16.Create( "Font16.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;
    if( FAILED( m_Font12.Create( "Font12.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    m_pTargetTexture      = m_xprResource.GetTexture( "Target" );
    m_pBulletHolesTexture = m_xprResource.GetTexture( "BulletHole" );

    // Load the lightgun object
    if( FAILED( m_LightGunMesh.Create( "Models\\LightGun.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Start off in the control test page
    m_AppState = APPSTATE_CONTROLTEST;
    InitControlTestPage();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Copy data for the active lightgun. This is done in a way that works even
    // if the lightgun is removed and inserted into a different port.
    m_dwNumInsertedLightguns = 0;

    // Check all ports for a lightgun
    for( DWORD i=0; i<4; i++ )
    {
        if( m_Gamepad[i].hDevice && m_Gamepad[i].caps.SubType == XINPUT_DEVSUBTYPE_GC_LIGHTGUN )
        {
            // Copy the gamepad input to the lightgun structure.
            // Note: This is just for convenience so we can refer to a 
            // "lightgun" instead of a "gamepad".
            m_Lightgun.CopyInput( &m_Gamepad[i] );

            // Record the number of lightguns that are inserted
            m_dwNumInsertedLightguns++;
        }
    }

    // Ensure that one and only one lightgun is inserted.
    if( m_dwNumInsertedLightguns != 1 )
        return S_FALSE;

    // Make sure the gun is properly calibrated. Note that this is called every
    // frame in case the state of the display changes
    m_Lightgun.VerifyCalibrationState( m_bVideoModeChanged );

    m_bVideoModeChanged = FALSE;

    // Set the vibration motors
    m_Lightgun.SetVibrationMotors( m_wLeftMotorSpeed, m_wRightMotorSpeed );

    // If the user presses B + BACK, then change the video mode
    if( ( ( m_Lightgun.wButtons & XINPUT_GAMEPAD_BACK ) && ( m_Lightgun.bPressedAnalogButtons[XINPUT_GAMEPAD_B] ) ) ||
        ( ( m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_B] ) && ( m_Lightgun.wPressedButtons & XINPUT_GAMEPAD_BACK ) ) )
    {
        // Change the video mode
        SelectNextVideoMode();
    }

    // Move to next app state when user presses BACK and START together
    if( ( ( m_Lightgun.wButtons & XINPUT_GAMEPAD_START ) && ( m_Lightgun.bPressedAnalogButtons[XINPUT_GAMEPAD_B] ) ) ||
        ( ( m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_B] ) && ( m_Lightgun.wPressedButtons & XINPUT_GAMEPAD_START ) ) )
    {
        // Advance the state
        switch( m_AppState )
        {
            case APPSTATE_CONTROLTEST:     InitCrosshairsTestPage(); 
                                           break;
            case APPSTATE_CROSSHAIRSTEST:  InitBulletHolesTestPage(); 
                                           break;
            case APPSTATE_BULLETHOLESTEST: InitControlTestPage(); 
                                           break;
        }
    }

    // Go to app-state specific control handling
    switch( m_AppState )
    {
        case APPSTATE_CONTROLTEST:     FrameMoveControlTestPage();
                                       break;
        case APPSTATE_CROSSHAIRSTEST:  FrameMoveCrosshairsTestPage();
                                       break;
        case APPSTATE_BULLETHOLESTEST: FrameMoveBulletHolesTestPage();
                                       break;
        case APPSTATE_CALIBRATION:     FrameMoveCalibrationPage();
                                       break;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderInsertRemoveControllerPage()
// Desc: Inform the user to please insert or remove a controller
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RenderInsertRemoveControllerPage()
{
    FLOAT fWidth  = (FLOAT)m_d3dpp.BackBufferWidth;
    FLOAT fHeight = (FLOAT)m_d3dpp.BackBufferHeight;

    // Draw a gradient filled background
    RenderGradientBackground( 0xff000000, 0xff0000ff );

    // Set some default state
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT,          0x00ffffff );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,         TRUE );

    // Draw header
    XBUtil_DrawRect( 48, fHeight-80, fWidth-48, fHeight-36, 0x40000000, 0xff000000 );
    m_Font16.DrawText( 53, fHeight-80, 0xffffffff, L"Xbox Lightgun Tool", XBFONT_LEFT );

    // Display a message requesting the user to insert a gamepad controller. Since
    // the Xbox input API take a second or so to detect a controller, let's delay
    // this message a tiny bit
    if( m_fAppTime > 2.0f )
    {
        XBUtil_DrawRect( 48, 36, fWidth-48, fHeight-88, 0x40000000, 0xff000000 );

        if( m_dwNumInsertedLightguns == 0 )
            m_Font16.DrawText( fWidth/2, (fHeight-52)/2, 0xffffffff, L"Please insert a lightgun controller", XBFONT_CENTER_X|XBFONT_CENTER_Y );
        else
            m_Font16.DrawText( fWidth/2, (fHeight-52)/2, 0xffffffff, L"Please remove all but one lightgun controller", XBFONT_CENTER_X|XBFONT_CENTER_Y );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InitControlTestPage()
// Desc: Initializes the page for testing the controls
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitControlTestPage()
{
    m_AppState = APPSTATE_CONTROLTEST; 

    D3DXMatrixIdentity( &m_LightGunMesh.m_matWorld );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMoveControlTestPage()
// Desc: Handles input for the page for testing the controls
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMoveControlTestPage()
{
    // Set up world matrix
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_LightGunMesh.m_matWorld );

    // Set up view matrix
    static D3DXVECTOR3 vEyePt    = D3DXVECTOR3( 0.0f, 0.0f, 50.0f );
    static D3DXVECTOR3 vLookatPt = D3DXVECTOR3( 0.0f, 0.0f,  0.0f );
    static D3DXVECTOR3 vUpVec    = D3DXVECTOR3( 0.0f, 1.0f,  0.0f );
    D3DXMATRIX matView;
    D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    // Set up proj matrix
    FLOAT      fAspectRatio = ( m_DisplayMode.Flags & D3DPRESENTFLAG_WIDESCREEN ) ? 16.0f/9.0f : 4.0f/3.0f;
    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, fAspectRatio, 1.0f, 1000.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Setup a base material
    D3DMATERIAL8 mtrl;
    XBUtil_InitMaterial( mtrl, 1.0f, 1.0f, 1.0f, 1.0f );
    m_pd3dDevice->SetMaterial( &mtrl );

    // Perform object rotation
    D3DXMATRIX matRotate;
    FLOAT fXRotate = 0.0f;
    FLOAT fYRotate = 0.0f;
    fXRotate += m_Lightgun.wButtons & XINPUT_GAMEPAD_DPAD_LEFT  ? +0.1f : 0.0f;
    fXRotate += m_Lightgun.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ? -0.1f : 0.0f;
    fYRotate += m_Lightgun.wButtons & XINPUT_GAMEPAD_DPAD_UP    ? +0.1f : 0.0f;
    fYRotate += m_Lightgun.wButtons & XINPUT_GAMEPAD_DPAD_DOWN  ? -0.1f : 0.0f;
    D3DXMatrixRotationYawPitchRoll( &matRotate, -fXRotate, -fYRotate, 0.0f );
    D3DXMatrixMultiply( &m_LightGunMesh.m_matWorld, &m_LightGunMesh.m_matWorld, &matRotate );

    // Animate buttons
    D3DXMatrixTranslation( m_LightGunMesh.m_pmatWhiteButtonMatrix, 0.0f, 0.0f,-0.3f * m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_WHITE] / 255.0f );
    D3DXMatrixTranslation( m_LightGunMesh.m_pmatBlackButtonMatrix, 0.0f, 0.0f,-0.3f * m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_BLACK] / 255.0f );
    D3DXMatrixTranslation( m_LightGunMesh.m_pmatXButtonMatrix,     0.0f, 0.0f, 0.3f * m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_X] / 255.0f );
    D3DXMatrixTranslation( m_LightGunMesh.m_pmatYButtonMatrix,     0.0f, 0.0f, 0.3f * m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_Y] / 255.0f );

    // Animate trigger
    const D3DXVECTOR3 vAxis( 0.0f, 0.0f, -1.0f );
    D3DXMatrixRotationAxis( &matRotate, &vAxis, (D3DX_PI/12) * m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_A] / 255.0f );
    (*m_LightGunMesh.m_pmatTriggerMatrix) = matRotate;

    return S_OK;
}



    
//-----------------------------------------------------------------------------
// Name: RenderControlTestPage()
// Desc: Renders the page for testing the controls
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RenderControlTestPage()
{
    // Draw a gradient filled background
    RenderGradientBackground( 0xff0000ff, 0xff000000 );

    // Set some default state
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT,          0x00ffffff );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,         TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_NORMALIZENORMALS, TRUE );

    // Generate spheremap texture coords from the camera space normal. This has
    // two steps. First, tell D3D to use the vertex normal (in camera space) as
    // texture coordinates. Then, we setup a texture matrix to transform these
    // texcoords from (-1,+1) view space to (0,1) texture space. This way,
    // the normal can be used to look up a texel in the spheremap.
    D3DXMATRIX mat;
    mat._11 = 0.5f; mat._12 = 0.0f;
    mat._21 = 0.0f; mat._22 =-0.5f;
    mat._31 = 0.0f; mat._32 = 0.0f;
    mat._41 = 0.5f; mat._42 = 0.5f;
    m_pd3dDevice->SetTransform( D3DTS_TEXTURE0, &mat );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACENORMAL );

    // Finally, draw the object
    m_LightGunMesh.Render( XBMESH_NOTEXTURES|XBMESH_NOMATERIALS );

    // Restore render states
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,     TRUE );

    FLOAT fWidth  = (FLOAT)m_d3dpp.BackBufferWidth;
    FLOAT fHeight = (FLOAT)m_d3dpp.BackBufferHeight;

    // Draw header
    XBUtil_DrawRect( 48, 36, fWidth-48, 81, 0x40000000, 0xff000000 );
    m_Font16.DrawText(  53, 36, 0xffffffff, L"Xbox Lightgun Tool" );
    m_Font12.DrawText(  53, 61, 0xffa0a0a0, L"Press B+BACK to change video mode" );
    m_Font16.DrawText( fWidth-53, 36, 0xffffffff, L"Control Test Page", XBFONT_RIGHT );
    m_Font12.DrawText( fWidth-53, 61, 0xffa0a0a0, L"Press B+START for next page", XBFONT_RIGHT );

    // Draw the labels for the buttons
    m_Font16.Begin();
    m_Font16.DrawText( fWidth-250,  85, m_Lightgun.wButtons & XINPUT_GAMEPAD_BACK  ? 0xffffff00: 0x80ffffff, GLYPH_BACK1_BUTTON GLYPH_BACK2_BUTTON, XBFONT_LEFT );
    m_Font16.DrawText( fWidth-150,  85, m_Lightgun.wButtons & XINPUT_GAMEPAD_START ? 0xffffff00: 0x80ffffff, GLYPH_START1_BUTTON GLYPH_START2_BUTTON, XBFONT_LEFT );
    m_Font16.DrawText( fWidth-250, 112, m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_Y]     ? 0xffffffff: 0x80ffffff, GLYPH_Y_BUTTON );
    m_Font16.DrawText( fWidth-250, 137, m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_X]     ? 0xffffffff: 0x80ffffff, GLYPH_X_BUTTON );
    m_Font16.DrawText( fWidth-250, 162, m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_WHITE] ? 0xffffffff: 0x80ffffff, GLYPH_WHITE_BUTTON );
    m_Font16.DrawText( fWidth-150, 112, m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_B]     ? 0xffffffff: 0x80ffffff, GLYPH_B_BUTTON );
    m_Font16.DrawText( fWidth-150, 137, m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_A]     ? 0xffffffff: 0x80ffffff, GLYPH_A_BUTTON );
    m_Font16.DrawText( fWidth-150, 162, m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_BLACK] ? 0xffffffff: 0x80ffffff, GLYPH_BLACK_BUTTON );
    m_Font16.End();
    
    // Draw status of the lightgun controls
    WCHAR strBuffer[100];
    m_Font12.Begin();

    // Show the current display mode
    m_Font12.DrawText( 48, fHeight-108, 0x80ffffff, m_strDisplayMode );

    // Show the calibration status
    if( m_Lightgun.IsUserCalibrated() )
        m_Font12.DrawText( 48, fHeight-88, 0xff80ff80, L"Lightgun is calibrated\nfor this mode" );
    else
        m_Font12.DrawText( 48, fHeight-88, 0xffff8080, L"Lightgun is not calibrated\n for this mode" );

    swprintf( strBuffer, L"LeftStick.x = %d", m_Lightgun.sThumbLX );
    m_Font12.DrawText(  48, 82, m_Lightgun.fX1!=0.0f ? 0xffffff00: 0x80ffffff, strBuffer );
    swprintf( strBuffer, L"LeftStick.y = %d", m_Lightgun.sThumbLY );
    m_Font12.DrawText(  48, 100, m_Lightgun.fY1!=0.0f ? 0xffffff00: 0x80ffffff, strBuffer );

    m_Font12.DrawText( 114, 128, m_Lightgun.wButtons & XINPUT_GAMEPAD_DPAD_UP    ? 0xffffff00: 0x80ffffff, L"Up",    XBFONT_CENTER_X );
    m_Font12.DrawText(  89, 146, m_Lightgun.wButtons & XINPUT_GAMEPAD_DPAD_LEFT  ? 0xffffff00: 0x80ffffff, L"Left",  XBFONT_CENTER_X );
    m_Font12.DrawText( 139, 146, m_Lightgun.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ? 0xffffff00: 0x80ffffff, L"Right", XBFONT_CENTER_X );
    m_Font12.DrawText( 114, 164, m_Lightgun.wButtons & XINPUT_GAMEPAD_DPAD_DOWN  ? 0xffffff00: 0x80ffffff, L"Down",  XBFONT_CENTER_X );

    swprintf( strBuffer, L" = %d", m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_Y] );
    m_Font12.DrawText( fWidth-250+28, 116, m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_Y] ? 0xffffff00: 0x80ffffff, strBuffer );
    swprintf( strBuffer, L" = %d", m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_X] );
    m_Font12.DrawText( fWidth-250+28, 141, m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_X] ? 0xffffff00: 0x80ffffff, strBuffer );
    swprintf( strBuffer, L" = %d", m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_WHITE] );
    m_Font12.DrawText( fWidth-250+28, 166, m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_WHITE] ? 0xffffff00: 0x80ffffff, strBuffer );

    swprintf( strBuffer, L" = %d", m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_B] );
    m_Font12.DrawText( fWidth-150+28, 116, m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_B] ? 0xffffff00: 0x80ffffff, strBuffer );
    swprintf( strBuffer, L" = %d", m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_A] );
    m_Font12.DrawText( fWidth-150+28, 141, m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_A] ? 0xffffff00: 0x80ffffff, strBuffer );
    swprintf( strBuffer, L" = %d", m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_BLACK] );
    m_Font12.DrawText( fWidth-150+28, 166, m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_BLACK] ? 0xffffff00: 0x80ffffff, strBuffer );

    m_Font12.DrawText( fWidth-180, fHeight-108, m_Lightgun.wButtons & XINPUT_LIGHTGUN_ONSCREEN      ? 0xffffff00: 0x80ffffff, L"On Screen" );
    m_Font12.DrawText( fWidth-180, fHeight-88,  m_Lightgun.wButtons & XINPUT_LIGHTGUN_FRAME_DOUBLER ? 0xffffff00: 0x80ffffff, L"Frame Doubler" );
    m_Font12.DrawText( fWidth-180, fHeight-68,  m_Lightgun.wButtons & XINPUT_LIGHTGUN_LINE_DOUBLER  ? 0xffffff00: 0x80ffffff, L"Line Doubler" );

    m_Font12.End();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InitCrosshairsTestPage()
// Desc: Initializes the page for calibrating the lightgun.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitCrosshairsTestPage()
{
    m_AppState = APPSTATE_CROSSHAIRSTEST; 

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMoveCrosshairsTestPage()
// Desc: Handle input and animations for the page for calibrating the lightgun.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMoveCrosshairsTestPage()
{
    // Handle input options: calibrate the lightgun
    if( m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_A] && m_Lightgun.bPressedAnalogButtons[XINPUT_GAMEPAD_B] ||
        m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_B] && m_Lightgun.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        // Start the calibration process
        InitCalibrationPage();
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderCrosshairsTestPage()
// Desc: Renders the page for calibrating the lightgun.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RenderCrosshairsTestPage()
{
    FLOAT fWidth  = (FLOAT)m_d3dpp.BackBufferWidth;
    FLOAT fHeight = (FLOAT)m_d3dpp.BackBufferHeight;

    RenderGradientBackground( 0xff00ffff, 0xff00ffff );

    // Draw header
    XBUtil_DrawRect( 48, 36, fWidth-48, 81, 0x40000000, 0xff000000 );
    m_Font16.DrawText(  53, 36, 0xffffffff, L"Xbox Lightgun Tool" );
    m_Font12.DrawText(  53, 61, 0xffa0a0a0, L"Press B+BACK to change video mode" );
    m_Font16.DrawText( fWidth-53, 36, 0xffffffff, L"Crosshairs Test Page", XBFONT_RIGHT );
    m_Font12.DrawText( fWidth-53, 61, 0xffa0a0a0, L"Press B+START for next page", XBFONT_RIGHT );

    // Show the current display mode
    m_Font12.DrawText( 48, fHeight-108, 0x80ffffff, m_strDisplayMode );

    // Show the calibration status
    if( m_Lightgun.IsUserCalibrated() )
        m_Font12.DrawText( 48, fHeight-88, 0xff80ff80, L"Lightgun is calibrated\nfor this mode" );
    else
        m_Font12.DrawText( 48, fHeight-88, 0xffff8080, L"Lightgun is not calibrated\n for this mode" );

    // Draw instructions
    XBUtil_DrawRect( fWidth-286, 90, fWidth-48, 188, 0x40000000, 0xff000000 );
    m_Font12.DrawText( fWidth-281, 90, 0xffffffff, L"Press A and B together to\n"
                                                   L"calibrate the lightgun.\n"
                                                   L"\n" );

    // Draw crosshairs in red
    if( m_Lightgun.wButtons & XINPUT_LIGHTGUN_ONSCREEN )
    {
        FLOAT fThumbLX = (fWidth/2) + (fWidth/2)*(m_Lightgun.sThumbLX+0.5f)/32767.5f;
        FLOAT fThumbLY = (fHeight/2) - (fHeight/2)*(m_Lightgun.sThumbLY+0.5f)/32767.5f;
        XBUtil_DrawRect( fThumbLX-1, fThumbLY-8, fThumbLX+1, fThumbLY+8, 0xffff0000, 0xffff0000 );
        XBUtil_DrawRect( fThumbLX-8, fThumbLY-1, fThumbLX+8, fThumbLY+1, 0xffff0000, 0xffff0000 );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InitBulletHolesTestPage()
// Desc: Initializes the page for calibrating the lightgun.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitBulletHolesTestPage()
{
    m_AppState = APPSTATE_BULLETHOLESTEST; 

    m_dwNumBulletHoles = 0L;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameBulletHolesTestPage()
// Desc: Handle input and animations for the page for calibrating the lightgun.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMoveBulletHolesTestPage()
{
    // This code serves to calibrate the lightgun. The lightgun needs to be
    // calibrated because people aim differently. Calibration simply requires
    // the user to aim and click at two pre-defined points. The points are
    // passed down to the hardware so that, from then on, the hardware only
    // reports adjusted points back to the app.

    // After receiving calibration data, the app should save the information
    // so the user does not need to go through calibration every time they
    // play the game.

    // See if we received a shot
    BOOL bShotFired;
    BOOL bShotHitScreen;
    BOOL bShotMissedScreen;
    m_Lightgun.Update( &bShotFired, &bShotHitScreen, &bShotMissedScreen );

    // If the screen was hit, add a bullet mark to the list
    if( bShotHitScreen )
    {
        FLOAT fWidth  = (FLOAT)m_d3dpp.BackBufferWidth;
        FLOAT fHeight = (FLOAT)m_d3dpp.BackBufferHeight;
        
        DWORD dwBullet = (m_dwNumBulletHoles++) % 100;
        m_vBulletHoleList[dwBullet].x = (fWidth/2) + (fWidth/2)*(m_Lightgun.sThumbLX+0.5f)/32767.5f;
        m_vBulletHoleList[dwBullet].y = (fHeight/2) - (fHeight/2)*(m_Lightgun.sThumbLY+0.5f)/32767.5f;
    }

    // Handle input options: reset bullet holes
    if( m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_A] && ( m_Lightgun.wPressedButtons & XINPUT_GAMEPAD_START ) )
        m_dwNumBulletHoles = 0;

    // Handle input options: calibrate the lightgun
    if( m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_A] && m_Lightgun.bPressedAnalogButtons[XINPUT_GAMEPAD_B] ||
        m_Lightgun.bAnalogButtons[XINPUT_GAMEPAD_B] && m_Lightgun.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        // Start the calibration process
        InitCalibrationPage();
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderBulletHolesTestPage()
// Desc: Renders the page for calibrating the lightgun.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RenderBulletHolesTestPage()
{
    FLOAT fWidth  = (FLOAT)m_d3dpp.BackBufferWidth;
    FLOAT fHeight = (FLOAT)m_d3dpp.BackBufferHeight;

    RenderGradientBackground( 0xff0000ff, 0xff000000 );

    // Draw header
    XBUtil_DrawRect( 48, 36, fWidth-48, 81, 0x40000000, 0xff000000 );
    m_Font16.DrawText(  53, 36, 0xffffffff, L"Xbox Lightgun Tool" );
    m_Font12.DrawText(  53, 61, 0xffa0a0a0, L"Press B+BACK to change video mode" );
    m_Font16.DrawText( fWidth-53, 36, 0xffffffff, L"Bullet Holes Test Page", XBFONT_RIGHT );
    m_Font12.DrawText( fWidth-53, 61, 0xffa0a0a0, L"Press B+START for next page", XBFONT_RIGHT );

    // Show the current display mode
    m_Font12.DrawText( 48, fHeight-108, 0x80ffffff, m_strDisplayMode );

    // Show the calibration status
    if( m_Lightgun.IsUserCalibrated() )
        m_Font12.DrawText( 48, fHeight-88, 0xff80ff80, L"Lightgun is calibrated\nfor this mode" );
    else
        m_Font12.DrawText( 48, fHeight-88, 0xffff8080, L"Lightgun is not calibrated\n for this mode" );

    XBUtil_DrawRect( fWidth-286, 90, fWidth-48, 242, 0x40000000, 0xff000000 );
    m_Font12.DrawText( fWidth-281, 90, 0xffffffff, L"Pull the trigger to test the\n"
                                                   L"lightgun.\n"
                                                   L"\n"
                                                   L"Press A and B together to\n"
                                                   L"calibrate the lightgun.\n"
                                                   L"\n"
                                                   L"Hold A and press START\n"
                                                   L"to clear the screen.\n" );

    // Draw some targets
    RenderTarget( fWidth*0.250f, fHeight*0.250f );
    RenderTarget( fWidth*0.375f, fHeight*0.375f );
    RenderTarget( fWidth*0.500f, fHeight*0.500f );
    RenderTarget( fWidth*0.625f, fHeight*0.625f );
    RenderTarget( fWidth*0.750f, fHeight*0.750f );

    // Draw bullet holes
    m_pd3dDevice->SetTexture( 3, m_pBulletHolesTexture );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_POINTSCALEENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_POINTSIZE,         FtoDW(16.0f) );

    // Turn on alphablending
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE ); 
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA ); 
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW );

    // Render particles
    D3DDevice::Begin( D3DPT_POINTLIST );
    for( DWORD i=0; i<min(100,m_dwNumBulletHoles); i++ )
    {
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, m_vBulletHoleList[i].x, 
                                                    m_vBulletHoleList[i].y,
                                                    1.0f, 1.0f );
    }
    D3DDevice::End();

    // Reset render states
    m_pd3dDevice->SetTexture( 3, NULL );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_POINTSCALEENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE,  FALSE );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InitCalibrationPage()
// Desc: Initializes the page for calibrating the lightgun.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitCalibrationPage()
{
    m_OldAppState = m_AppState;
    m_AppState    = APPSTATE_CALIBRATION;

    // Reset values
    m_CenterCalibrationPointX    = 0;
    m_CenterCalibrationPointY    = 0;
    m_UpperLeftCalibrationPointX = 0;
    m_UpperLeftCalibrationPointY = 0;

    // Reset the calibration offsets
    m_Lightgun.ResetCalibrationOffsets();

    // Calibrate the center point first
    m_bCalibratingCenter = TRUE;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMoveCalibrationPage()
// Desc: Handle input and animations for the page for calibrating the lightgun.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMoveCalibrationPage()
{
    // This code serves to calibrate the lightgun. The lightgun needs to be
    // calibrated because people aim differently. Calibration simply requires
    // the user to aim and click at two pre-defined points. The points are
    // passed down to the hardware so that, from then on, the hardware only
    // reports adjusted points back to the app.

    // After receiving calibration data, the app should save the information
    // so the user does not need to go through calibration every time they
    // play the game.

    // See if we received a shot
    BOOL bShotFired;
    BOOL bShotHitScreen;
    BOOL bShotMissedScreen;
    m_Lightgun.Update( &bShotFired, &bShotHitScreen, &bShotMissedScreen );

    // Check if the lightgun is onscreen and the trigger was pulled
    if( bShotHitScreen )
    {
        // Calibrate the center point
        if( m_bCalibratingCenter )
        {
            m_CenterCalibrationPointX = m_Lightgun.sThumbLX;
            m_CenterCalibrationPointY = m_Lightgun.sThumbLY;

            // Set the new calibration offsets
            m_Lightgun.SetCalibrationOffsets( m_CenterCalibrationPointX, 
                                              m_CenterCalibrationPointY,
                                              XINPUT_LIGHTGUN_CALIBRATION_UPPERLEFT_X,
                                              XINPUT_LIGHTGUN_CALIBRATION_UPPERLEFT_Y );

            m_bCalibratingCenter = FALSE;
        }
        else // Calibrate the upperleft point
        {
            m_UpperLeftCalibrationPointX = m_Lightgun.sThumbLX;
            m_UpperLeftCalibrationPointY = m_Lightgun.sThumbLY;

            // Set the new calibration offsets and save them for future retrieval
            m_Lightgun.SetCalibrationOffsets( m_CenterCalibrationPointX, 
                                              m_CenterCalibrationPointY,
                                              m_UpperLeftCalibrationPointX,
                                              m_UpperLeftCalibrationPointY );

            // Go back to whereever we came from
            m_AppState = m_OldAppState;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderTarget()
// Desc: Draws a target for the user to aim at
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RenderTarget( FLOAT fTargetX, FLOAT fTargetY )
{
    // Adjust coordinates for screen space rendering
    fTargetX = floorf( fTargetX ) - 0.5f;
    fTargetY = floorf( fTargetY ) - 0.5f;

    // Set render states
    m_pd3dDevice->SetTexture( 0, m_pTargetTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xffffff00 ); 
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE ); 
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA ); 
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE ); 
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1|D3DFVF_TEXCOORDSIZE4(0) );
    
    // Set the width and height for the display mode
    FLOAT fWidth  = 32.0f;
    FLOAT fHeight = 32.0f;
    if( m_DisplayMode.Flags & D3DPRESENTFLAG_WIDESCREEN )
        fWidth *= 0.75f;

    // Draw the target
    m_pd3dDevice->Begin( D3DPT_QUADLIST );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 0.0f, 0.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, fTargetX-fWidth, fTargetY-fHeight, 1.0f, 1.0f );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 1.0f, 0.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, fTargetX+fWidth, fTargetY-fHeight, 1.0f, 1.0f );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 1.0f, 1.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, fTargetX+fWidth, fTargetY+fHeight, 1.0f, 1.0f );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 0.0f, 1.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, fTargetX-fWidth, fTargetY+fHeight, 1.0f, 1.0f );
    m_pd3dDevice->End();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderCalibrationPage()
// Desc: Renders the page for calibrating the lightgun.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RenderCalibrationPage()
{
    FLOAT fWidth  = (FLOAT)m_d3dpp.BackBufferWidth;
    FLOAT fHeight = (FLOAT)m_d3dpp.BackBufferHeight;

    // Draw a gradient filled background
    RenderGradientBackground( 0xff0000ff, 0xff0000ff );

    // Draw footer
    XBUtil_DrawRect( 48, fHeight-80, fWidth-48, fHeight-36, 0x40000000, 0xff000000 );
    m_Font16.DrawText( 53, fHeight-80, 0xffffffff, L"Lightgun Calibration" );

    // Draw instructions
    XBUtil_DrawRect( fWidth-286, fHeight-194, fWidth-48, fHeight-88, 0x40000000, 0xff000000 );
    if( m_bCalibratingCenter )
    {
        m_Font12.DrawText( fWidth-281, fHeight-194, 0xffffffff, 
                           L"Step 1: Point the lightgun\n"
                           L"at the target in the center\n"
                           L"of the screen and pull the\n"
                           L"trigger." );
    }
    else
    {
        m_Font12.DrawText( fWidth-281, fHeight-194, 0xffffffff, 
                           L"Step 2: Point the lightgun\n"
                           L"at the target in the upper\n"
                           L"left corner of the screen and\n"
                           L"pull the trigger." );
    }

    // Get the coordinates for the target
    FLOAT fTargetX, fTargetY;
    if( m_bCalibratingCenter )
        m_Lightgun.GetCalibrationTargetCoords( &fTargetX, &fTargetY, NULL, NULL );
    else
        m_Lightgun.GetCalibrationTargetCoords( NULL, NULL, &fTargetX, &fTargetY );

    // Draw the target
    RenderTarget( fTargetX, fTargetY );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    if( m_dwNumInsertedLightguns != 1 )
    {
        RenderInsertRemoveControllerPage();
    }
    else
    {
        switch( m_AppState )
        {
            case APPSTATE_CONTROLTEST:     RenderControlTestPage();
                                           break;
            case APPSTATE_CROSSHAIRSTEST:  RenderCrosshairsTestPage();
                                           break;
            case APPSTATE_BULLETHOLESTEST: RenderBulletHolesTestPage();
                                           break;
            case APPSTATE_CALIBRATION:     RenderCalibrationPage();
                                           break;
        }
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}



