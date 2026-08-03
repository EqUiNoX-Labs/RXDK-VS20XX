//-----------------------------------------------------------------------------
// File: Lightgun.h
//
// Desc: Code to handle various aspects of a lightgun
//
// Hist: 10.22.02 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef LIGHTGUN_H
#define LIGHTGUN_H

#include <xtl.h>
#include <XBInput.h>




//-----------------------------------------------------------------------------
// Name: enum LIGHTGUN_CALIBRATION_STATE
// Desc: Whether or not a lightgun is calibrated
//-----------------------------------------------------------------------------
enum LIGHTGUN_CALIBRATION_STATE
{
    LIGHTGUNCALIBRATIONSTATE_CALIBRATION_UNKNOWN,
    LIGHTGUNCALIBRATIONSTATE_NOT_CALIBRATED,
    LIGHTGUNCALIBRATIONSTATE_CALIBRATING_CENTER,
    LIGHTGUNCALIBRATIONSTATE_CALIBRATING_UPPERLEFT,
    LIGHTGUNCALIBRATIONSTATE_USER_CALIBRATED
};




//-----------------------------------------------------------------------------
// Name: struct LIGHTGUN_CALIBRATION_POINTS
// Desc: Calibration points for the lightgun, depending on screen resolution
//-----------------------------------------------------------------------------
struct LIGHTGUN_CALIBRATION_POINTS
{
    WORD  wScreenWidth,  wScreenHeight;
    XINPUT_LIGHTGUN_CALIBRATION_OFFSETS offsets;
    BOOL  bUserCalibrated;
};




//-----------------------------------------------------------------------------
// Name: struct LIGHTGUN_CALIBRATION_DATA
// Desc: Calibration data for the lightgun for all display modes
//-----------------------------------------------------------------------------
#define NUM_SUPPORTED_LIGHTGUN_DISPLAY_MODES 6


struct LIGHTGUN_CALIBRATION_DATA
{
    XINPUT_DEVICE_DESCRIPTION   DeviceDesc;
    
    BOOL                        bFrameDoubler;
    BOOL                        bLineDoubler;

    LIGHTGUN_CALIBRATION_POINTS CalibrationPts[NUM_SUPPORTED_LIGHTGUN_DISPLAY_MODES];

    WORD                        wCurrentDisplayModeIndex;
};




//-----------------------------------------------------------------------------
// Name: class CLightgun
// Desc: 
//-----------------------------------------------------------------------------
class CLightgun : public XBGAMEPAD
{
public:
    LIGHTGUN_CALIBRATION_DATA  m_CalibrationData;
    LIGHTGUN_CALIBRATION_STATE m_CalibrationState;

    BOOL                       m_bTrackDisplayChanges;
    BOOL                       m_bFrameDoubler;
    BOOL                       m_bLineDoubler;

private:
    CHAR*   GetCalibrationDataFilename( LIGHTGUN_CALIBRATION_DATA* pCalibrationData );
    HRESULT SaveCalibrationOffsets( XINPUT_LIGHTGUN_CALIBRATION_OFFSETS* pCalibrationOffsets );
    HRESULT LoadCalibrationData();
    HRESULT InitCalibrationData();

public:
    VOID    CopyInput( XBGAMEPAD* pLightgun );

    BOOL    IsOnCorrectField();
    VOID    DisplayWhiteField();

    HRESULT ResetCalibrationOffsets();
    HRESULT SetCalibrationOffsets( WORD wCenterX, WORD wCenterY, WORD wUpperLeftX, WORD wUpperLeftY );
    BOOL    IsUserCalibrated();

    VOID    GetCalibrationTargetCoords( FLOAT* pfCenterTargetX, FLOAT* pfCenterTargetY,
                                        FLOAT* pfUpperLeftTargetX, FLOAT* pfUpperLeftTargetY );

    VOID    Update( BOOL* pbShotFired, BOOL* pbShotHitScreen, BOOL* pbShotMissedScreen );

    VOID    VerifyCalibrationState( BOOL bModeChange );
    VOID    SetVibrationMotors( WORD wLeftMotorSpeed, WORD wRightMotorSpeed );

    CLightgun();
};




#endif // LIGHTGUN_H
