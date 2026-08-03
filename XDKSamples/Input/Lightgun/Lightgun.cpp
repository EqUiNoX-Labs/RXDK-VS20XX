//-----------------------------------------------------------------------------
// File: Lightgun.cpp
//
// Desc: Code to handle various aspects of a lightgun
//
// Hist: 10.22.02 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "Lightgun.h"
#include <stdio.h>




//-----------------------------------------------------------------------------
// Calibration Process
//-----------------------------------------------------------------------------
// These are the steps taken to calibrate a lightgun:
// 
//  1. Draw a light background (white or yellow) for the calibration screen.
//
//  2. Wait for the XINPUT_LIGHTGUN_ONSCREEN bit to be set. It takes approximately 20
//     frames for the hardware to determine whether the TV is using line or frame
//     doubling, after which the XINPUT_LIGHTGUN_ONSCREEN bit will be turned on.
//
//  3. Draw a first target at the center calibration position and prompt the user
//     to aim at the target and pull the trigger. Note: the center calibration
//     location depends on the resolution of the screen.
//
//  4. Wait for a trigger pull from the gun (with the ONSCREEN bit set) and then read
//     the X,Y values returned by the gun when it is detected. Compute offsets that
//     get from that X,Y point to the actual calibration point.
//
//  5. If the game decides that the calibration values are too far off to be acceptable,
//     prompt user to try again.
//
//  6. Send the computed calibration offsets for the center point down to the gun using
//     the XInputSetLightgunCalibration() call.
//
//  7. Repeat steps 3 through 6 for the upper left calibration point.
//
//  8. The game can now place a crosshair on the screen that follows where the gun is
//     pointed on screen to show the user where they are pointing.
//
//  9. Allow the user to recalibrate either calibration point until they are satisfied
//     with the results. As each new calibration setting is measured, the total
//     cumulative offset for that point should be sent to the gun.
//
// 10. Outside of the calibration screen, the game must monitor the
//     XINPUT_LIGHTGUN_FRAME_DOUBLER flag to ensure that its state does not change.
//     There are TV sets that allow this mode to be changed dynamically. If the flag
//     changes state, the game should lead the user to calibrate the gun or apply
//     appropriate calibration settings measured and stored previously.
//
// 11. The game should save calibration data for this lightgun, resolution, and
//     frame/line doubler state




// Default calibration data for lightguns with no saved data
static LIGHTGUN_CALIBRATION_POINTS g_LightGunDefaultCalibrationPoints[] = 
{
    // Screen res   Center pt   Upperleft pt   User calibrated?
    {  640,  480,   { 0, 0,       0,  0   },   FALSE },
    {  720,  480,   { 0, 0,       0,  0   },   FALSE },
    { 1280,  720,   { 0, 0,       0,  0   },   FALSE },
    { 1920, 1080,   { 0, 0,       0,  0   },   FALSE },
    {  640,  576,   { 0, 0,       0,  0   },   FALSE },
    {  720,  576,   { 0, 0,       0,  0   },   FALSE },
};




//-----------------------------------------------------------------------------
// Name: CLightgun()
// Desc: Constructor
//-----------------------------------------------------------------------------
CLightgun::CLightgun()
{
    m_CalibrationState     = LIGHTGUNCALIBRATIONSTATE_CALIBRATION_UNKNOWN;
    m_bTrackDisplayChanges = FALSE;
    m_bFrameDoubler        = FALSE;
    m_bLineDoubler         = FALSE;
}




//-----------------------------------------------------------------------------
// Name: CopyInput()
// Desc: Updates the lightgun with recently polled gamepad input data.
//-----------------------------------------------------------------------------
VOID CLightgun::CopyInput( XBGAMEPAD* pLightgun )
{
    memcpy( this, pLightgun, sizeof(XBGAMEPAD) );
}




//-----------------------------------------------------------------------------
// Name: InitCalibrationData()
// Desc: Initialize the calibration data structure for a lightgun
//-----------------------------------------------------------------------------
HRESULT CLightgun::InitCalibrationData()
{
    // Make sure the device is a lightgun
    XINPUT_CAPABILITIES caps;
    XInputGetCapabilities( hDevice, &caps );
    if( caps.SubType != XINPUT_DEVSUBTYPE_GC_LIGHTGUN )
        return E_INVALIDARG;

    // Get the current state of the lightgun
    XINPUT_STATE state;
    XInputGetState( hDevice, &state );
    
    // Make sure the lightgun is onscreen...a necessary condition for the
    // frame and line doubler flags to be valid
    if( 0 == ( state.Gamepad.wButtons & XINPUT_LIGHTGUN_ONSCREEN ) )
        return E_FAIL;

    // Record whether frame or line doubling is in effect
    m_CalibrationData.bFrameDoubler = state.Gamepad.wButtons & XINPUT_LIGHTGUN_FRAME_DOUBLER;
    m_CalibrationData.bLineDoubler  = state.Gamepad.wButtons & XINPUT_LIGHTGUN_LINE_DOUBLER;

    // Get the desription of the lightgun
    XInputGetDeviceDescription( hDevice, &m_CalibrationData.DeviceDesc );

    // Initialize the calibration points for the lightgun
    memcpy( m_CalibrationData.CalibrationPts, &g_LightGunDefaultCalibrationPoints,
            sizeof(g_LightGunDefaultCalibrationPoints) );

    // Set the display mode index
    D3DDISPLAYMODE mode;
    D3DDevice::GetDisplayMode( &mode );

    for( WORD i=0; i<NUM_SUPPORTED_LIGHTGUN_DISPLAY_MODES; i++ )
    {
        if( m_CalibrationData.CalibrationPts[i].wScreenWidth  == mode.Width &&
            m_CalibrationData.CalibrationPts[i].wScreenHeight == mode.Height )
        {
            m_CalibrationData.wCurrentDisplayModeIndex = i;
            break;
        }
    }

    // Double check that we are in a supported display mode
    if( i == NUM_SUPPORTED_LIGHTGUN_DISPLAY_MODES )
        return E_FAIL;

    // Put the newly initialized calibration offsets into effect
    XInputSetLightgunCalibration( hDevice, 
                                  &m_CalibrationData.CalibrationPts[m_CalibrationData.wCurrentDisplayModeIndex].offsets );

    // Calibration state is initially unknown
    m_CalibrationState = LIGHTGUNCALIBRATIONSTATE_CALIBRATION_UNKNOWN;

    // Load the calibration data for the lightgun
    if( FAILED( LoadCalibrationData() ) ) 
        return E_FAIL;

    // Update calibration status
    if( m_CalibrationData.CalibrationPts[m_CalibrationData.wCurrentDisplayModeIndex].bUserCalibrated )
        m_CalibrationState = LIGHTGUNCALIBRATIONSTATE_USER_CALIBRATED;
    else
        m_CalibrationState = LIGHTGUNCALIBRATIONSTATE_NOT_CALIBRATED;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetCalibrationDataFilename()
// Desc: Returns a unique filename for this lightgun (based on vendor
//       information and display settings)
//-----------------------------------------------------------------------------
CHAR* CLightgun::GetCalibrationDataFilename( LIGHTGUN_CALIBRATION_DATA* pCalibrationData )
{
    // Generate a filename
    static CHAR strFilename[100];
    sprintf( strFilename, "t:\\Lightgun_%04lx%04lx%04lx%s%s.dat",
                          pCalibrationData->DeviceDesc.wProductID,
                          pCalibrationData->DeviceDesc.wVendorID,
                          pCalibrationData->DeviceDesc.wVersion,
                          pCalibrationData->bFrameDoubler ? "_fd" : "",
                          pCalibrationData->bLineDoubler  ? "_ld" : "" );

    return strFilename;
}




//-----------------------------------------------------------------------------
// Name: LoadCalibrationData()
// Desc: Loads calibration data for a lightgun
//-----------------------------------------------------------------------------
HRESULT CLightgun::LoadCalibrationData()
{
    // Get the unique filename for this lightgun
    CHAR* strFilename = GetCalibrationDataFilename( &m_CalibrationData );
    
    // Open the file, if it exists
    FILE* file = fopen( strFilename, "rb" );
    if( file )
    {
        // Read the calibration data from the file
        LIGHTGUN_CALIBRATION_POINTS CalibrationPtsFromFile[NUM_SUPPORTED_LIGHTGUN_DISPLAY_MODES];
        int count = fread( &CalibrationPtsFromFile, sizeof(LIGHTGUN_CALIBRATION_POINTS), 
                           NUM_SUPPORTED_LIGHTGUN_DISPLAY_MODES, file );

        fclose( file );

        if( NUM_SUPPORTED_LIGHTGUN_DISPLAY_MODES == count )
        {
            // If the read was successful, use that data
            memcpy( m_CalibrationData.CalibrationPts, &CalibrationPtsFromFile,
                    NUM_SUPPORTED_LIGHTGUN_DISPLAY_MODES * sizeof(LIGHTGUN_CALIBRATION_POINTS) );

            // Put the newly loaded calibration offsets into effect
            XInputSetLightgunCalibration( hDevice, &m_CalibrationData.CalibrationPts[m_CalibrationData.wCurrentDisplayModeIndex].offsets );

            return S_OK;
        }
    }

    return S_FALSE;
}




//-----------------------------------------------------------------------------
// Name: ResetCalibrationOffsets()
// Desc: Sets and saves calibration offsets for a lightgun
//-----------------------------------------------------------------------------
HRESULT CLightgun::ResetCalibrationOffsets()
{
    XINPUT_LIGHTGUN_CALIBRATION_OFFSETS offsets;
    offsets.wCenterX    = 0;
    offsets.wCenterY    = 0;
    offsets.wUpperLeftX = 0;
    offsets.wUpperLeftY = 0;
    XInputSetLightgunCalibration( hDevice, &offsets );

    // Copy the new calibration offsets to the data structure
    memcpy( &m_CalibrationData.CalibrationPts[m_CalibrationData.wCurrentDisplayModeIndex].offsets,
            &offsets, sizeof(XINPUT_LIGHTGUN_CALIBRATION_OFFSETS) );

    m_CalibrationData.CalibrationPts[m_CalibrationData.wCurrentDisplayModeIndex].bUserCalibrated = FALSE;
    m_CalibrationState = LIGHTGUNCALIBRATIONSTATE_NOT_CALIBRATED;

    // Save the new offsets for future retrieval
    return SaveCalibrationOffsets( &offsets );
}




//-----------------------------------------------------------------------------
// Name: SetCalibrationOffsets()
// Desc: Sets and saves calibration offsets for a lightgun
//-----------------------------------------------------------------------------
HRESULT CLightgun::SetCalibrationOffsets( WORD wCenterX, WORD wCenterY,
                                          WORD wUpperLeftX, WORD wUpperLeftY )
{
    XINPUT_LIGHTGUN_CALIBRATION_OFFSETS offsets;
    offsets.wCenterX    = XINPUT_LIGHTGUN_CALIBRATION_CENTER_X    - wCenterX;
    offsets.wCenterY    = XINPUT_LIGHTGUN_CALIBRATION_CENTER_Y    - wCenterY;
    offsets.wUpperLeftX = XINPUT_LIGHTGUN_CALIBRATION_UPPERLEFT_X - wUpperLeftX;
    offsets.wUpperLeftY = XINPUT_LIGHTGUN_CALIBRATION_UPPERLEFT_Y - wUpperLeftY;
    XInputSetLightgunCalibration( hDevice, &offsets );

    // Copy the new calibration offsets to the data structure
    memcpy( &m_CalibrationData.CalibrationPts[m_CalibrationData.wCurrentDisplayModeIndex].offsets,
            &offsets, sizeof(XINPUT_LIGHTGUN_CALIBRATION_OFFSETS) );

    m_CalibrationData.CalibrationPts[m_CalibrationData.wCurrentDisplayModeIndex].bUserCalibrated = TRUE;
    m_CalibrationState = LIGHTGUNCALIBRATIONSTATE_USER_CALIBRATED;

    // Save the new offsets for future retrieval
    return SaveCalibrationOffsets( &offsets );
}




//-----------------------------------------------------------------------------
// Name: SaveCalibrationOffsets()
// Desc: Saves calibration offsets for a lightgun
//-----------------------------------------------------------------------------
HRESULT CLightgun::SaveCalibrationOffsets( XINPUT_LIGHTGUN_CALIBRATION_OFFSETS* pCalibrationOffsets )
{
    if( NULL == pCalibrationOffsets )
        return E_INVALIDARG;

    // Get the unique filename for this lightgun
    CHAR* strFilename = GetCalibrationDataFilename( &m_CalibrationData );
    
    // Create the file
    FILE* file = fopen( strFilename, "wb" );
    if( NULL == file )
        return E_FAIL;

    // Write the calibration data to the file
    int count = fwrite( m_CalibrationData.CalibrationPts, sizeof(LIGHTGUN_CALIBRATION_POINTS),
                        NUM_SUPPORTED_LIGHTGUN_DISPLAY_MODES, file );

    fclose( file );

    // Check whether the write was successful
    if( NUM_SUPPORTED_LIGHTGUN_DISPLAY_MODES != count )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: IsUserCalibrated()
// Desc: Returns whether the lightgun has been calibrated by the user for this
//       display mode
//-----------------------------------------------------------------------------
BOOL CLightgun::IsUserCalibrated()
{
    return ( LIGHTGUNCALIBRATIONSTATE_USER_CALIBRATED == m_CalibrationState );
}




//-----------------------------------------------------------------------------
// Name: GetCalibrationTargetCoords()
// Desc: Returns screen coords for the targets that an app draws during
//       calibration of the lightgun. The coords depend on the display mode, as
//       in the following table:
//
//           Display mode:    Center pt:     Upperleft pt:
//          -------------   ------------    ------------
//            640 x  480:   ( 320, 240 )    (  76,  57 )
//            720 x  480:   ( 360, 240 )    (  85,  57 )
//           1280 x  720:   ( 640, 480 )    ( 152,  85 )
//           1920 x 1080:   ( 960, 540 )    ( 227, 128 )
//            640 x  576:   ( 320, 288 )    (  76,  68 )
//            720 x  576:   ( 360, 288 )    (  85,  68 )
//-----------------------------------------------------------------------------
VOID CLightgun::GetCalibrationTargetCoords( FLOAT* pfCenterTargetX,
                                            FLOAT* pfCenterTargetY,
                                            FLOAT* pfUpperLeftTargetX,
                                            FLOAT* pfUpperLeftTargetY )
{
    // Get the display mode
    D3DDISPLAYMODE mode;
    D3DDevice::GetDisplayMode( &mode );

    // Return the coords for the targets
    if( pfCenterTargetX )    (*pfCenterTargetX )    = floorf( (320.0f/640.0f) * mode.Width  );
    if( pfCenterTargetY )    (*pfCenterTargetY )    = floorf( (240.0f/480.0f) * mode.Height );
    if( pfUpperLeftTargetX ) (*pfUpperLeftTargetX ) = floorf( ( 76.0f/640.0f) * mode.Width  );
    if( pfUpperLeftTargetY ) (*pfUpperLeftTargetY ) = floorf( ( 57.0f/480.0f) * mode.Height );
}




//-----------------------------------------------------------------------------
// Name: IsOnCorrectField()
// Desc: Returns TRUE if display is on correct field for reading lightgun data.
//-----------------------------------------------------------------------------
BOOL CLightgun::IsOnCorrectField()
{
    // Get the current display mode
    D3DDISPLAYMODE DisplayMode;
    D3DDevice::GetDisplayMode( &DisplayMode );
    
    // Get the current display field status
    D3DFIELD_STATUS DisplayFieldStatus;
    D3DDevice::GetDisplayFieldStatus( &DisplayFieldStatus );

    // On interlaced NTSC and PAL-60 systems, lightguns can only be read
    // during odd fields. On PAL-50 systems, lightguns can only be read
    // during even fields. So, here we check the current display field
    // and return the results.
    if( DisplayMode.RefreshRate == 50 )
    {
        if( DisplayFieldStatus.Field == D3DFIELD_EVEN )
            return TRUE;
    }
    else // DisplayMode.RefreshRate == 60
    {   
        if( DisplayFieldStatus.Field == D3DFIELD_ODD )
            return TRUE;
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: DisplayWhiteField()
// Desc: Displays a white screen for one field.
//-----------------------------------------------------------------------------
VOID CLightgun::DisplayWhiteField()
{
    static D3DGAMMARAMP WhiteGammaRamp = {0};

    // On first entry, initialize the white gamma ramp
    if( WhiteGammaRamp.red[0] == 0 )
    {
        for( DWORD i=0; i<256; i++ )
        {
            WhiteGammaRamp.red[i]   = 255;
            WhiteGammaRamp.green[i] = 255;
            WhiteGammaRamp.blue[i]  = 255;
        }
    }

    // Save the current gamma ramp
    D3DGAMMARAMP SavedGammaRamp;
    D3DDevice::GetGammaRamp( &SavedGammaRamp );
    
    // Set the white gamma ramp and wait for the field to draw
    D3DDevice::SetGammaRamp( D3DSGR_IMMEDIATE, &WhiteGammaRamp );
    D3DDevice::BlockUntilVerticalBlank();
    
    // Restore the gamma ramp
    D3DDevice::SetGammaRamp( D3DSGR_IMMEDIATE, &SavedGammaRamp );
}




//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Get lightgun shot information. Since determining this information
//       involves many different states over many different frames, this
//       function needs to be called every frame.
//-----------------------------------------------------------------------------
VOID CLightgun::Update( BOOL* pbShotFired, BOOL* pbShotHitScreen,
                        BOOL* pbShotMissedScreen )
{
    // Variables for whether shots hit or missed. These are the return values
    // for the function
    BOOL bShotFired        = FALSE;
    BOOL bShotHitScreen    = FALSE;
    BOOL bShotMissedScreen = FALSE;

    // Static state for reading the lightgun data
    enum LIGHTGUNSTATE
    {
        LIGHTGUNSTATE_READY,
        LIGHTGUNSTATE_WAITUNTILCORRECTFIELD,
        LIGHTGUNSTATE_WAITONEFRAME,
        LIGHTGUNSTATE_READINPUT,
    };
    static LIGHTGUNSTATE LightgunState = LIGHTGUNSTATE_READY;

    // To read the lightgun, a number of steps over a few frames needs to be
    // taken. This is due to the fact that the lightgun hardware only works
    // when a white frame is rendered in the odd-display field and then needs
    // time to process data.
    //
    // So, we implement a mini-state engine to track the following:
    // (1) Acknowledge that the lightgun trigger was pulled. Then draw a white
    //     screen in the appropriate display field
    // (2) Wait for the next vblank, so the lightgun hardware gets a signal
    // (3) Poll and read the lightgun's gamepad structure
    switch( LightgunState )
    {
        // If the lightgun state is ready, check for trigger pulls
        case LIGHTGUNSTATE_READY:
            // Don't do anything until the trigger is pulled
            if( FALSE == bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
                break;

            // After the trigger is pulled, fall through to the next state
            bShotFired    = TRUE;
            LightgunState = LIGHTGUNSTATE_WAITUNTILCORRECTFIELD;

        // Wait until the correct field and then draw one field worth of
        // a white screen
        case LIGHTGUNSTATE_WAITUNTILCORRECTFIELD:
            // If we're on the wrong field, wait until the correct one
            if( FALSE == IsOnCorrectField() )
                break;

            // Draw one field worth of a white screen. The lightgun needs this
            // in order to reliably read valid data.
            DisplayWhiteField();

            // Adavance the state for the next time around
            LightgunState = LIGHTGUNSTATE_WAITONEFRAME;
            break;

        // If the trigger was pulled in a previous frame, we would have drawn a
        // white flash, and must still wait an additional frame before reading
        // input so that the lightgun hardware has time to process data.
        case LIGHTGUNSTATE_WAITONEFRAME:
            // On the frame following this one, we will be ready to read the
            // lightgun data
            LightgunState = LIGHTGUNSTATE_READINPUT;
            break;

        // After flashing the screen white and giving the hardware time to
        // process data, the lightgun data will now be ready to be read.
        case LIGHTGUNSTATE_READINPUT:
            // Record whether the screen was hit
            if( wButtons & XINPUT_LIGHTGUN_ONSCREEN )
                bShotHitScreen = TRUE;
            else
                bShotMissedScreen = TRUE;

            // Now we're ready to record another trigger pull again
            LightgunState = LIGHTGUNSTATE_READY;
            break;
    }

    // Return whether a shot was received this frame
    if( pbShotFired )        (*pbShotFired)        = bShotFired;
    if( pbShotHitScreen )    (*pbShotHitScreen)    = bShotHitScreen;
    if( pbShotMissedScreen ) (*pbShotMissedScreen) = bShotMissedScreen;
}




//-----------------------------------------------------------------------------
// Name: VerifyCalibrationState()
// Desc: Verifies whether the lightgun is calibrated for the current display
//       mode.
//-----------------------------------------------------------------------------
VOID CLightgun::VerifyCalibrationState( BOOL bModeChange )
{
    // If the device was just inserted or the display mode was changed, re-init
    // (or reload) the calibration data
    if( bModeChange || bInserted == TRUE )
    {
        // Set a flag so we know to reload the lightgun calibration data
        m_CalibrationState = LIGHTGUNCALIBRATIONSTATE_CALIBRATION_UNKNOWN;
        m_bTrackDisplayChanges = FALSE;
    }

    // Check if we need to load the calibration data.
    if( LIGHTGUNCALIBRATIONSTATE_CALIBRATION_UNKNOWN == m_CalibrationState )
    {
        // The lightgun must be onscreen before we can know if frame
        // doubling or line doubling is in effect. So wait for that bit
        // so we know to load the matching calibration data.
        if( wButtons & XINPUT_LIGHTGUN_ONSCREEN )
        {
            // Initialize the calibration data structure for this mode
            if( SUCCEEDED( InitCalibrationData() ) )
            {
                // Starting now, let's track the state of the display. If the user
                // turns on/off frame or line doubling, we will need to
                // recalibrate the lightgun
                m_bTrackDisplayChanges = TRUE;
                m_bFrameDoubler = wButtons & XINPUT_LIGHTGUN_FRAME_DOUBLER;
                m_bLineDoubler  = wButtons & XINPUT_LIGHTGUN_LINE_DOUBLER;
            }
        }
    }
    
    // If we are tracking display changes (for when the user changes modes on
    // his TV) then do that here. Note: in the middle of a game, we might not
    // want to interrupt the user.
    if( m_bTrackDisplayChanges  )
    {
        if( m_bFrameDoubler != ( wButtons & XINPUT_LIGHTGUN_FRAME_DOUBLER ) ||
            m_bLineDoubler  != ( wButtons & XINPUT_LIGHTGUN_LINE_DOUBLER ) )
        {
            // Settings have changed, so set a flag to get updated calibration info
            m_CalibrationState = LIGHTGUNCALIBRATIONSTATE_CALIBRATION_UNKNOWN;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: SetVibrationMotors()
// Desc: Set the vibration motors, but only if they changed
//-----------------------------------------------------------------------------
VOID CLightgun::SetVibrationMotors( WORD wLeftMotorSpeed, WORD wRightMotorSpeed )
{
    if( Feedback.Rumble.wLeftMotorSpeed  != wLeftMotorSpeed ||
        Feedback.Rumble.wRightMotorSpeed != wRightMotorSpeed )
    {
        // Check to see if we are done transferring any previous motor values
        if( Feedback.Header.dwStatus != ERROR_IO_PENDING )
        {
            // Set the new vibration motor values
            Feedback.Rumble.wLeftMotorSpeed  = wLeftMotorSpeed;
            Feedback.Rumble.wRightMotorSpeed = wRightMotorSpeed;
            XInputSetState( hDevice, &Feedback );
        }
    }
}



