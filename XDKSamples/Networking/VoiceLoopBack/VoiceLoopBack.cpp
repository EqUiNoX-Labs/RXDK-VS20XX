//-----------------------------------------------------------------------------
// File: VoiceLoopBack.cpp
//
// Desc: Demonstrates basic usage of the Xbox communicator.
//       This sample just monitors the microphone of each connected
//       communicator and routes the data directly to that communicator's
//       headphone
//
// Hist: 08.14.01 - New for August M1 Online XDK release
//       02.18.02 - Cleaned up for March XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XButil.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <cassert>
#include <algorithm>
#include <xvoice.h>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Display help" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Select voice\nmask option" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Change voice\nmask value" },
    { XBHELP_MISC_CALLOUT, XBHELP_PLACEMENT_2, L"Triggers:\nSelect preset" },
};

const DWORD NUM_HELP_CALLOUTS = ( sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] ) );

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD COLOR_HIGHLIGHT     = 0xffffff00;     // Yellow
const DWORD COLOR_GREEN         = 0xff00ff00;     // Green
const DWORD COLOR_NORMAL        = 0xffffffff;     // White
const DWORD MAX_ERROR_STR       = 64;
const DWORD MAX_STATUS_STR      = 128;


// Voice processing: 8kHz, 16 bit samples, 4 packets, 40ms each 
const DWORD VOICE_SAMPLE_RATE   = 8000;
const DWORD BYTES_PER_SAMPLE    = 2;
const DWORD NUM_PACKETS         = 4;
const DWORD PACKET_SIZE         = VOICE_SAMPLE_RATE * BYTES_PER_SAMPLE / 25;
const DWORD COMPRESSED_SIZE     = 18;



// This struct is used to help display UI 
// for manipulating voice masks.
typedef struct
{
    DWORD   dwOffset;
    FLOAT   fXPosition;
    FLOAT   fYPosition;
    WCHAR*  strLabel;
} VOICE_MASK_OPTION;

VOICE_MASK_OPTION g_VoiceMaskOptions[] =
{
    { offsetof( XVOICE_MASK, fSpecEnergyWeight ), 100.0f, 200.0f, L"SpecEnergy" },
    { offsetof( XVOICE_MASK, fPitchScale ), 100.0f, 225.0f, L"PitchScale" },
    { offsetof( XVOICE_MASK, fWhisperValue ), 100.0f, 250.0f, L"Whisper" },
    { offsetof( XVOICE_MASK, fRoboticValue ), 100.0f, 275.0f, L"Robotic" },
};
static const DWORD NUM_VOICEMASKOPTIONS = sizeof( g_VoiceMaskOptions ) / sizeof( g_VoiceMaskOptions[0] );

typedef struct
{
    WCHAR*      strName;
    XVOICE_MASK voiceMask;
} VOICE_MASK_PRESET;


/*

  Voice Masks from Whacked!
    Courtesy of Presto Studios, Inc.
    Created by John Schultz
    7/17/2002

*/

// Presto predefined voice masks                  fSpecEnergyWeight fPitchScale                 fWhisperValue               fRoboticValue
#define XVOICE_MASK_PRESTO_DEFAULT              { 1.00f,            0.18f,                      XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_GENERIC_SANS         { 0.70f,            0.18f,                      XVOICE_MASK_PARAM_DISABLED, 0.17f                       }
#define XVOICE_MASK_PRESTO_CARTOON              { 0.10f,            XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_SARCASTIC_LITTLE_BOY { 0.03f,            0.05f,                      1.00f,                      0.00f                       }
#define XVOICE_MASK_PRESTO_CHAIN_SMOKER_MALE    { 0.03f,            0.05f,                      XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_CLEAN_DEEP_MALE      { 0.03f,            0.47f,                      1.00f,                      XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_HOARSE_MALE          { 0.05f,            XVOICE_MASK_PARAM_DISABLED, 0.10f,                      XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_NASAL_CLEAN_MALE     { 0.17f,            0.44f,                      1.00f,                      XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_FLUTTER              { 0.03f,            0.47f,                      XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_CHILD                { 0.10f,            0.70f,                      XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_TEENAGE_GIRL         { 0.31f,            0.65f,                      XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_SOMEONES_MOM         { 0.42f,            0.37f,                      XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_MEDIUM_HOARSE_FEMALE { 0.42f,            0.37f,                      0.32f,                      XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_TEENAGE_BOY          { 0.42f,            0.19f,                      XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_TEENAGE_GEEK_BOY     { 0.39f,            0.36f,                      1.00f,                      XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_MEDIUM_MALE          { 0.63f,            0.22f,                      XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_DEEP_CLEAN           { 1.00f,            XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_FULL_MEDIUM_DEEP     { 0.52f,            0.01f,                      XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_FULL_DEEP_CLEAN      { 0.90f,            0.05f,                      XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_FULL_DEEP_CLEAN_2    { 1.00f,            0.00f,                      XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_OLDER_MALE           { 0.83f,            0.01f,                      XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED  }
#define XVOICE_MASK_PRESTO_LITTLE_GIRL_ROBOT    { 0.10f,            0.70f,                      XVOICE_MASK_PARAM_DISABLED, 0.41f                       }
#define XVOICE_MASK_PRESTO_MEDIUM_FEMALE_SANS   { 0.39f,            XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED, 0.44f                       }
#define XVOICE_MASK_PRESTO_OLDER_MALE_SANS      { 0.82f,            XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED, 0.01f                       }
#define XVOICE_MASK_PRESTO_CHILD_ROBOT          { 0.1f,             XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED, 0.87f                       }
#define XVOICE_MASK_PRESTO_ROBOT_CHIC           { 0.3f,             XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED, 0.58f                       }
#define XVOICE_MASK_PRESTO_MEDIUM_MALE_ROBOT    { 0.5f,             XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED, 0.05f                       }
#define XVOICE_MASK_PRESTO_DARTH_ROBOT          { 1.00f,            XVOICE_MASK_PARAM_DISABLED, XVOICE_MASK_PARAM_DISABLED, 0.01f                       }


// Voice mask global presets
VOICE_MASK_PRESET g_VoiceMaskPresets[] =
{
    { L"None",                          XVOICE_MASK_NONE                        },
    { L"Anonymous",                     XVOICE_MASK_ANONYMOUS                   },
    { L"Cartoon",                       XVOICE_MASK_CARTOON                     },
    { L"Big Guy",                       XVOICE_MASK_BIGGUY                      },
    { L"Child",                         XVOICE_MASK_CHILD                       },
    { L"Robot",                         XVOICE_MASK_ROBOT                       },
    { L"Dark Master",                   XVOICE_MASK_DARKMASTER                  },
    { L"Whisper",                       XVOICE_MASK_WHISPER                     },
    { L"Presto Default",                XVOICE_MASK_PRESTO_DEFAULT              },
    { L"Presto Generic\nSane",          XVOICE_MASK_PRESTO_GENERIC_SANS         },
    { L"Presto Cartoon",                XVOICE_MASK_PRESTO_CARTOON              },
    { L"Presto Sarcastic\nLittle Boy",  XVOICE_MASK_PRESTO_SARCASTIC_LITTLE_BOY },
    { L"Presto Chain\nSmoker Male",     XVOICE_MASK_PRESTO_CHAIN_SMOKER_MALE    },
    { L"Presto Clean\nDeep Male",       XVOICE_MASK_PRESTO_CLEAN_DEEP_MALE      },
    { L"Presto Hoarse Male",            XVOICE_MASK_PRESTO_HOARSE_MALE          },
    { L"Presto Nasal\nClean Male",      XVOICE_MASK_PRESTO_NASAL_CLEAN_MALE     },
    { L"Presto Flutter",                XVOICE_MASK_PRESTO_FLUTTER              },
    { L"Presto Child",                  XVOICE_MASK_PRESTO_CHILD                },
    { L"Presto Teenage\nGirl",          XVOICE_MASK_PRESTO_TEENAGE_GIRL         },
    { L"Presto Someone's\nMom",         XVOICE_MASK_PRESTO_SOMEONES_MOM         },
    { L"Presto Medium\nHoarse Female",  XVOICE_MASK_PRESTO_MEDIUM_HOARSE_FEMALE },
    { L"Presto Teenage\nBoy",           XVOICE_MASK_PRESTO_TEENAGE_BOY          },
    { L"Presto Teenage\nGeek Boy",      XVOICE_MASK_PRESTO_TEENAGE_GEEK_BOY     },
    { L"Presto Medium Male",            XVOICE_MASK_PRESTO_MEDIUM_MALE          },
    { L"Presto Deep\nClean",            XVOICE_MASK_PRESTO_DEEP_CLEAN           },
    { L"Presto Full\nMedium Deep",      XVOICE_MASK_PRESTO_FULL_MEDIUM_DEEP     },
    { L"Presto Full\nDeep Clean",       XVOICE_MASK_PRESTO_FULL_DEEP_CLEAN      },
    { L"Presto Full\nDeep Clean 2",     XVOICE_MASK_PRESTO_FULL_DEEP_CLEAN_2    },
    { L"Presto Older\nMale",            XVOICE_MASK_PRESTO_OLDER_MALE           },
    { L"Presto Little\nGirl Robot",     XVOICE_MASK_PRESTO_LITTLE_GIRL_ROBOT    },
    { L"Presto Medium\nFemale Sans",    XVOICE_MASK_PRESTO_MEDIUM_FEMALE_SANS   },
    { L"Presto Older\nMale Sans",       XVOICE_MASK_PRESTO_OLDER_MALE_SANS      },
    { L"Presto Child\nRobot",           XVOICE_MASK_PRESTO_CHILD_ROBOT          },
    { L"Presto Robot\nChic",            XVOICE_MASK_PRESTO_ROBOT_CHIC           },
    { L"Presto Medium\nMale Robot",     XVOICE_MASK_PRESTO_MEDIUM_MALE_ROBOT    },
    { L"Presto Darth\nRobot",           XVOICE_MASK_PRESTO_DARTH_ROBOT          },
};

static const DWORD NUM_VOICEMASKPRESETS = sizeof( g_VoiceMaskPresets ) / sizeof( g_VoiceMaskPresets[0] );


//-----------------------------------------------------------------------------
// Name: class CLoopbackCommunicator
// Desc: This class represents one instance of a Xbox Communicator performing
//          basic loopback from the microphone to the headphone
//-----------------------------------------------------------------------------
class CLoopbackCommunicator
{
public:
    CLoopbackCommunicator();
    ~CLoopbackCommunicator();

    HRESULT Initialize( DWORD dwPort );
    HRESULT Inserted();
    HRESULT Removed();
    HRESULT Process();

private:
    DWORD           m_dwControllerPort;

    // Microphone-related data
    BYTE*           m_pMicrophoneBuffer;
    DWORD           m_adwMicrophoneStatus[NUM_PACKETS];
    DWORD           m_dwMicrophonePacket;
    XMediaObject*   m_pMicrophoneXMO;
    
    // Headphone-related data
    BYTE*           m_pHeadphoneBuffer;
    DWORD           m_adwHeadphoneStatus[NUM_PACKETS];
    DWORD           m_dwHeadphonePacket;
    XMediaObject*   m_pHeadphoneXMO;

public:
    LPXVOICEENCODER m_pEncoderXMO;
    LPXVOICEDECODER m_pDecoderXMO;
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Font and help
    BOOL                m_bDrawHelp;
    CXBFont             m_Font;
    CXBHelp             m_Help;

    CLoopbackCommunicator m_aCommunicators[ XGetPortCount() ];
    DWORD               m_dwMicrophoneState;
    DWORD               m_dwHeadphoneState;
    DWORD               m_dwConnectedCommunicators;
   
    DWORD               m_dwCurrentPreset;
    XVOICE_MASK         m_VoiceMask;
    DWORD               m_dwCurrentOption;

public:
    CXBoxSample();

    virtual HRESULT Initialize();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();

    HRESULT CheckCommunicatorStatus();  // Handle device insertion/removal
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
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
{
    m_bDrawHelp       = FALSE;
    m_dwCurrentOption = 0;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // In order to handle an Xbox Communicator being inserted between the two
    // calls to XGetDevices (or XGetDeviceChanges), we track the state of
    // microphone devices and headphone devices separately.
    m_dwConnectedCommunicators = 0;
    m_dwMicrophoneState = XGetDevices( XDEVICE_TYPE_VOICE_MICROPHONE );
    m_dwHeadphoneState  = XGetDevices( XDEVICE_TYPE_VOICE_HEADPHONE );

    // Setting our default voice mask
    m_dwCurrentPreset = 1;
    m_VoiceMask = g_VoiceMaskPresets[m_dwCurrentPreset].voiceMask;

    for( int i = 0; i < XGetPortCount(); i++ )
    {
        // Tell the CLoopbackCommunicator which port it owns.  This doesn't
        // mean that a Communicator is inserted there - we'll call Inserted() 
        // when that happens in CheckCommunicatorStatus
        m_aCommunicators[i].Initialize( i );
    }

    return S_OK;
}




static const SHORT XINPUT_DEADZONE = (SHORT)( 0.24f * FLOAT(0x7FFF) );
//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Check for insertion and removal of communicators
    CheckCommunicatorStatus();

    // Toggle help
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_WHITE ] )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Set to true if any voice masking settings change
    BOOL bResetVoiceMask = FALSE;

    // Change to a different preset voice mask configuration
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_LEFT_TRIGGER ] )
    {
        m_dwCurrentPreset = ( m_dwCurrentPreset + NUM_VOICEMASKPRESETS - 1 ) % NUM_VOICEMASKPRESETS;
        m_VoiceMask = g_VoiceMaskPresets[ m_dwCurrentPreset ].voiceMask;
        bResetVoiceMask = TRUE;
    }
    else if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_RIGHT_TRIGGER ] )
    {
        m_dwCurrentPreset = ( m_dwCurrentPreset + 1 ) % NUM_VOICEMASKPRESETS;
        m_VoiceMask = g_VoiceMaskPresets[ m_dwCurrentPreset ].voiceMask;
        bResetVoiceMask = TRUE;
    }

    // Select a different voice mask option
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_dwCurrentOption = ( m_dwCurrentOption + NUM_VOICEMASKOPTIONS - 1 ) % NUM_VOICEMASKOPTIONS;
    }
    else if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_dwCurrentOption = ( m_dwCurrentOption + 1 ) % NUM_VOICEMASKOPTIONS;
    }

    // Change the value of the current option
    // For simplicity, we're using the same voice mask for all 4 
    // communicators.  You could easily have 4 different masks, 
    // 1 for each communicator.
    FLOAT* pfValue = (FLOAT *)( ( (BYTE *)&m_VoiceMask ) + g_VoiceMaskOptions[ m_dwCurrentOption ].dwOffset );
    if( m_DefaultGamepad.sThumbRX != 0 )
    {
        // In order to make XVOICE_MASK_PARAM_DISABLED look like part of
        // the 0.0-1.0 spectrum, we'll jump from XVOICE_MASK_PARAM_DISABLED
        // to 0.0 if we're increasing the value.
        if( *pfValue == XVOICE_MASK_PARAM_DISABLED && m_DefaultGamepad.sThumbRX > 0 )
            *pfValue = 0.0f;

        *pfValue += m_DefaultGamepad.sThumbRX * m_fElapsedTime / 32768;
        if( *pfValue > 1.0f )
            *pfValue = 1.0f;
        if( *pfValue < 0.0f )
            *pfValue = XVOICE_MASK_PARAM_DISABLED;
    
        bResetVoiceMask = TRUE;
    }

    // For each active communicator, process I/O
    for( int i = 0; i < XGetPortCount(); i++ )
    {
        if( m_dwConnectedCommunicators & ( 1 << i ) )
        {
            if( bResetVoiceMask )
                m_aCommunicators[i].m_pEncoderXMO->SetVoiceMask( 0, &m_VoiceMask );

            m_aCommunicators[i].Process();
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 
                         0x000A0A6A, 1.0f, 0L );

    m_Font.DrawText(  64, 50, 0xffffffff, L"VoiceLoopBack" );

    // Display help screen
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        // If there are no communicators plugged in, display a message
        // stating that the user should plug one in.
        if( m_dwConnectedCommunicators == 0 )
        {
            // Display a status message
            m_Font.DrawText( 100, 110, COLOR_GREEN, L"Please insert an Xbox Communicator\ninto one or more controllers." );
        }
        else
        {
            // Display a status message
            m_Font.DrawText( 100, 110, COLOR_GREEN, L"Speak into the microphone of your Xbox\nCommunicator, and you should hear your\nvoice through the headphone." );

            // If we're still on a preset, display that preset
            if( !memcmp( &m_VoiceMask, &g_VoiceMaskPresets[ m_dwCurrentPreset ].voiceMask, sizeof( XVOICE_MASK ) ) )
            {
                WCHAR str[100];
                swprintf( str, L"Current Voice Mask:\n%s", g_VoiceMaskPresets[ m_dwCurrentPreset ].strName );
                m_Font.DrawText( 350, 220, COLOR_NORMAL, str );
            }

            // Display the different voice mask options
            for( DWORD i = 0; i < NUM_VOICEMASKOPTIONS; i++ )
            {
                WCHAR str[100];
                FLOAT* pfValue = (FLOAT *)( ( (BYTE *)&m_VoiceMask ) + g_VoiceMaskOptions[i].dwOffset );
                if( *pfValue < 0.0f )
                {
                    swprintf( str, L"%s: None", g_VoiceMaskOptions[i].strLabel );
                }
                else
                {
                    swprintf( str, L"%s: %0.2f", g_VoiceMaskOptions[i].strLabel, *pfValue );
                }

                m_Font.DrawText( g_VoiceMaskOptions[i].fXPosition, 
                                 g_VoiceMaskOptions[i].fYPosition,
                                 ( i == m_dwCurrentOption ) ? COLOR_HIGHLIGHT : COLOR_GREEN,
                                 str );
            }

            // Display which communicators are connected
            for( int i = 0; i < XGetPortCount(); i++ )
            {
                if( m_dwConnectedCommunicators & ( 1 << i ) )
                {
                    WCHAR str[100];
                    swprintf( str, L"Communicator connected to port %d", i );
                    m_Font.DrawText( 100.0f, 310.0f + 30 * i, COLOR_GREEN, str );
                }
            }
        }
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CheckCommunicatorStatus()
// Desc: Handles any changes in the status of Xbox Communicators.  In order
//          to handle the possibility that a device could be inserted 
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::CheckCommunicatorStatus()
{
    // Check the microphones
    DWORD dwMicrophoneInsertions;
    DWORD dwMicrophoneRemovals;
    XGetDeviceChanges( XDEVICE_TYPE_VOICE_MICROPHONE, 
                       &dwMicrophoneInsertions,
                       &dwMicrophoneRemovals );

    // Check the headphones
    DWORD dwHeadphoneInsertions;
    DWORD dwHeadphoneRemovals;
    XGetDeviceChanges( XDEVICE_TYPE_VOICE_HEADPHONE, 
                       &dwHeadphoneInsertions,
                       &dwHeadphoneRemovals );

    // Update our internal state for removals
    m_dwMicrophoneState &= ~( dwMicrophoneRemovals );
    m_dwHeadphoneState  &= ~( dwHeadphoneRemovals );

    // Update state for new insertions
    m_dwMicrophoneState |= dwMicrophoneInsertions;
    m_dwHeadphoneState  |= dwHeadphoneInsertions;

    for( int i = 0; i < XGetPortCount(); i++ )
    {
        // If either the microphone or the headphone was
        // removed since last call, remove the communicator
        if( ( m_dwConnectedCommunicators & ( 1 << i ) ) &&
            ( ( dwMicrophoneRemovals & ( 1 << i ) ) ||
              ( dwHeadphoneRemovals  & ( 1 << i ) ) ) )
        {
            // Remove the Communicator
            m_aCommunicators[i].Removed();
            m_dwConnectedCommunicators &= ~( 1 << i );
        }

        // If both microphone and headphone are present, and
        // we didn't have a communicator here last frame,
        // register the insertion
        if( ( m_dwMicrophoneState & ( 1 << i ) ) &&
            ( m_dwHeadphoneState  & ( 1 << i ) ) &&
            !( m_dwConnectedCommunicators & ( 1 << i ) ) )
        {
            // Insert the headset
            if( SUCCEEDED( m_aCommunicators[i].Inserted() ) )
            {
                // For simplicity, we're using the same voice mask
                // for all 4 communicators.  You could easily have
                // 4 different masks, 1 for each communicator.
                m_aCommunicators[i].m_pEncoderXMO->SetVoiceMask( 0, &m_VoiceMask );

                m_dwConnectedCommunicators |= ( 1 << i );
            }
            else
                m_aCommunicators[i].Removed();
        }
    }

    return S_OK;
}





//-----------------------------------------------------------------------------
// Name: CLoopbackCommunicator (ctor)
// Desc: Initializes member variables
//-----------------------------------------------------------------------------
CLoopbackCommunicator::CLoopbackCommunicator()
{
    m_dwControllerPort = 0xFFFFFFFF;
    m_pMicrophoneBuffer= NULL;
    m_pHeadphoneBuffer = NULL;
    m_pMicrophoneXMO   = NULL;
    m_pHeadphoneXMO    = NULL;
    m_pEncoderXMO      = NULL;
    m_pDecoderXMO      = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CLoopbackCommunicator (Dtor)
// Desc: Frees up any resources
//-----------------------------------------------------------------------------
CLoopbackCommunicator::~CLoopbackCommunicator()
{
    Removed();
}



//-----------------------------------------------------------------------------
// Name: Initialize
// Desc: Initializes the communicator to a specific port
//-----------------------------------------------------------------------------
HRESULT CLoopbackCommunicator::Initialize( DWORD dwPort )
{
    m_dwControllerPort = dwPort;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: Inserted
// Desc: Handles insertion of a communicator
//-----------------------------------------------------------------------------
HRESULT CLoopbackCommunicator::Inserted()
{
    HRESULT hr;

    OUTPUT_DEBUG_STRING( "Detected communicator insertion\n" );

    // Allocate a buffer for PCM sample data
    m_pMicrophoneBuffer = new BYTE[ PACKET_SIZE * NUM_PACKETS ];
    if( !m_pMicrophoneBuffer )
    {
        Removed();
        return E_OUTOFMEMORY;
    }

    // Allocate a buffer for PCM sample data
    m_pHeadphoneBuffer = new BYTE[ PACKET_SIZE * NUM_PACKETS ];
    if( !m_pHeadphoneBuffer )
    {
        Removed();
        return E_OUTOFMEMORY;
    }

    // Fill out a waveformat structure
    WAVEFORMATEX wfx;
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.cbSize          = 0;
    wfx.nChannels       = 1;
    wfx.nSamplesPerSec  = VOICE_SAMPLE_RATE;
    wfx.wBitsPerSample  = BYTES_PER_SAMPLE * 8;
    wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

    //  Create the microphone device
    hr = XVoiceCreateMediaObject( XDEVICE_TYPE_VOICE_MICROPHONE, 
                                  m_dwControllerPort, 
                                  NUM_PACKETS,
                                  &wfx, 
                                  &m_pMicrophoneXMO );
    if( FAILED( hr ) )
    {
        OUTPUT_DEBUG_STRING( "Couldn't create microphone device\n" );
        Removed();
        return hr;
    }

    // Create the headphone device
    hr = XVoiceCreateMediaObject( XDEVICE_TYPE_VOICE_HEADPHONE, 
                                  m_dwControllerPort, 
                                  NUM_PACKETS,
                                  &wfx, 
                                  &m_pHeadphoneXMO );
    if( FAILED( hr ) )
    {
        OUTPUT_DEBUG_STRING( "Couldn't create headphone device\n" );
        Removed();
        return hr;
    }

    for( int i = 0; i < NUM_PACKETS; i++ )
    {
        // Seed the microphone device with all our media packets
        XMEDIAPACKET xmp;
        xmp.dwMaxSize           = PACKET_SIZE;
        xmp.pdwCompletedSize    = NULL;
        xmp.pContext            = NULL;
        xmp.prtTimestamp        = NULL;
        xmp.pvBuffer            = m_pMicrophoneBuffer + i * PACKET_SIZE;
        xmp.pdwStatus           = &m_adwMicrophoneStatus[i];

        m_pMicrophoneXMO->Process( NULL, &xmp );

        // Initialize all the headphone packets to be available
        m_adwHeadphoneStatus[i] = XMEDIAPACKET_STATUS_SUCCESS;
    }

    m_dwMicrophonePacket = 0;
    m_dwHeadphonePacket = 0;

    // Create encoder/decoder XMOs
    hr = XVoiceCreateOneToOneEncoder( &m_pEncoderXMO );
    if( FAILED( hr ) )
        return hr;
    hr = XVoiceCreateOneToOneDecoder( &m_pDecoderXMO );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Removed
// Desc: Handles removal (or failed insertion) of a communicator
//-----------------------------------------------------------------------------
HRESULT CLoopbackCommunicator::Removed()
{
    OUTPUT_DEBUG_STRING( "Detected communicator removal or failed insertion.\n" );

    delete[] m_pMicrophoneBuffer;
    m_pMicrophoneBuffer = NULL;

    delete[] m_pHeadphoneBuffer;
    m_pHeadphoneBuffer = NULL;

    if( m_pMicrophoneXMO )
    {
        m_pMicrophoneXMO->Release();
        m_pMicrophoneXMO = NULL;
    }

    if( m_pHeadphoneXMO )
    {
        m_pHeadphoneXMO->Release();
        m_pHeadphoneXMO = NULL;
    }
    
    if( m_pEncoderXMO )
    {
        m_pEncoderXMO->Release();
        m_pEncoderXMO = NULL;
    }

    if( m_pDecoderXMO )
    {
        m_pDecoderXMO->Release();
        m_pDecoderXMO = NULL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Process
// Desc: Processes the Microphone and Headphone XMOs to handle loopback voice
//          communication
//-----------------------------------------------------------------------------
HRESULT CLoopbackCommunicator::Process()
{
    while( m_adwMicrophoneStatus[ m_dwMicrophonePacket ] != XMEDIAPACKET_STATUS_PENDING &&
           m_adwHeadphoneStatus[ m_dwHeadphonePacket ] != XMEDIAPACKET_STATUS_PENDING )
    {
        // Set the basic packet fields
        XMEDIAPACKET xmp = {0};
        xmp.dwMaxSize           = PACKET_SIZE;
        xmp.pvBuffer            = m_pMicrophoneBuffer + m_dwMicrophonePacket * PACKET_SIZE;

        // Compress and decompress
        BYTE         Buffer[COMPRESSED_SIZE];
        DWORD        dwCompressedSize;
        XMEDIAPACKET xmpTemp = {0};
        xmpTemp.dwMaxSize       = COMPRESSED_SIZE;
        xmpTemp.pvBuffer        = Buffer;
        xmpTemp.pdwCompletedSize = &dwCompressedSize;
        m_pEncoderXMO->ProcessMultiple( 1, &xmp, 1, &xmpTemp );

        // If dwCompressedSize == 0, then the packet was silence
        if( dwCompressedSize )
            m_pDecoderXMO->ProcessMultiple( 1, &xmpTemp, 1, &xmp );
        else
            ZeroMemory( xmp.pvBuffer, PACKET_SIZE );

        // Copy the data into the headphone buffer
        memcpy( m_pHeadphoneBuffer + m_dwHeadphonePacket * PACKET_SIZE,
                m_pMicrophoneBuffer + m_dwMicrophonePacket * PACKET_SIZE,
                PACKET_SIZE );

        // Resubmit the microphone packet
        xmp.pvBuffer            = m_pMicrophoneBuffer + m_dwMicrophonePacket * PACKET_SIZE;
        xmp.pdwStatus           = &m_adwMicrophoneStatus[ m_dwMicrophonePacket ];
        if( FAILED( m_pMicrophoneXMO->Process( NULL, &xmp ) ) )
            break;

        // Submit the copied data to the headphone
        xmp.pvBuffer            = m_pHeadphoneBuffer + m_dwHeadphonePacket * PACKET_SIZE;
        xmp.pdwStatus           = &m_adwHeadphoneStatus[ m_dwHeadphonePacket ];
        if( FAILED( m_pHeadphoneXMO->Process( &xmp, NULL ) ) )
            break;

        m_dwMicrophonePacket = ( m_dwMicrophonePacket + 1 ) % NUM_PACKETS;
        m_dwHeadphonePacket  = ( m_dwHeadphonePacket + 1 ) % NUM_PACKETS;
    }

    return S_OK;
}




