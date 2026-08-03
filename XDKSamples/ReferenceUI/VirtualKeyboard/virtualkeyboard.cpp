//-----------------------------------------------------------------------------
// File: VirtualKeyboard.cpp
//
// Desc: Virtual keyboard reference UI
//
// Note: This sample is intended to show appropriate functionality only.
//       Please do not lift the graphics for use in your game. A description
//       of the user research that went into the creation of this sample is
//       located in the XDK documentation at Developing for Xbox - Reference
//       User Interface.
//
// Hist: 02.13.01 - New for March XDK release 
//       03.07.01 - Localized for April XDK release
//       04.10.01 - Updated for May XDK with full translations
//       06.06.01 - Japanese keyboard added
//       07.22.02 - Japanese keyboard (keyboard) added
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "VirtualKeyboard.h"
#include <cassert>
#include <algorithm>

// The following header file is generated from "Resource.rdf" file using the
// Bundler tool. In addition to the header, the tool outputs a binary file
// (Resource.xpr) which contains compiled (i.e. bundled) resources and is
// loaded at runtime using the CXBPackedResource class.
#include "Resource.h"

// The following header file is generated from "ControllerS.rdf" file using the
// Bundler tool. In addition to the header, the tool outputs a binary file
// (ControllerS.xpr) which contains compiled (i.e. bundled) resources and is
// loaded at runtime using the CXBPackedResource class.
#include "ControllerS.h"




const UINT XC_LANGUAGE_MAX = XC_LANGUAGE_ITALIAN + 1;

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// These are the widths of the keys on the display. GAP2 is the distance
// between the mode keys on the left and the rest of the keyboard. The safe
// title area is 512 pixels across, so these values must meet the following
// guideline: MODEKEY_WIDTH + GAP2_WIDTH + (10*KEY_WIDTH) + (9*GAP_WIDTH) <=512
const DWORD GAP_WIDTH     = 0;
const DWORD GAP2_WIDTH    = 4;
const DWORD MODEKEY_WIDTH = 166;
const DWORD KEY_INSET     = 1;

const DWORD MAX_KEYS_PER_ROW = 14;

// Must be this far from center on 0.0 - 1.0 scale
const FLOAT JOY_THRESHOLD = 0.25f; 

// How often (per second) the caret blinks
const FLOAT fCARET_BLINK_RATE = 1.0f;

// During the blink period, the amount the caret is visible. 0.5 equals
// half the time, 0.75 equals 3/4ths of the time, etc.
const FLOAT fCARET_ON_RATIO = 0.75f;

// Text colors for keys
const D3DCOLOR COLOR_HIGHLIGHT     = 0xff00ff00;   // green
const D3DCOLOR COLOR_PRESSED       = 0xff808080;   // gray
const D3DCOLOR COLOR_NORMAL        = 0xff000000;   // black
const D3DCOLOR COLOR_DISABLED      = 0xffffffff;   // white
const D3DCOLOR COLOR_HELPTEXT      = 0xffffffff;   // white
const D3DCOLOR COLOR_FONT_DISABLED = 0xff808080;   // gray
const D3DCOLOR COLOR_INVISIBLE     = 0xff0000ff;   // blue
const D3DCOLOR COLOR_RED           = 0xffff0000;   // red

// Controller repeat values
const FLOAT fINITIAL_REPEAT = 0.333f; // 333 mS recommended for first repeat
const FLOAT fSTD_REPEAT     = 0.085f; // 85 mS recommended for repeat rate

// Maximum number of characters in string
const DWORD MAX_CHARS = 64;

// Width of text box
const FLOAT fTEXTBOX_WIDTH = 576.0f - 64.0f - 4.0f - 4.0f - 10.0f;

const FLOAT BUTTON_Y_POS    = 411.0f;      // button text line
const FLOAT BUTTON_X_OFFSET =  40.0f;      // space between button and text
const D3DCOLOR BUTTONTEXT_COLOR  = 0xffffffff;
const FLOAT FIXED_JSL_SIZE = 3.0f;

// Xboxdings font button mappings
const WCHAR* TEXT_A_BUTTON = L"A";
const WCHAR* TEXT_B_BUTTON = L"B";
const WCHAR* TEXT_X_BUTTON = L"C";
const WCHAR* TEXT_Y_BUTTON = L"D";





//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

// Localizable String Data
const WCHAR* g_strEnglish[] =
{
    L"English",
    L"Choose Keyboard",
    L"Sample graphics. Don't use in your game",
    L"Select",
    L"Back",
    L"Help",
    L"SPACE",
    L"BACKSPACE",
    L"SHIFT",
    L"CAPS LOCK",
    L"ALPHABET",
    L"SYMBOLS",
    L"ACEENTS",
    L"DONE",
    L"Select",
    L"Cancel",
    L"Toggle\nmode",
    L"Display help",
    L"Backspace",
    L"Space",
    L"Trigger buttons move cursor",
};

// Japanese - Controller Type (displayed if VKEY_JAPAN #defined)
const WCHAR* g_strJapanese1[] = 
{
    JH_NI JH_HO JH_N JH_GO L"\x30FB"
        JK_KO JK_N JK_TO JK_RO JK_DASH JK_RA JK_DASH, 
    JK_KI JK_DASH JK_BO JK_DASH JK_DO JH_WO JH_E JH_RA JH_N JH_DE JH_KU
    JH_DA JH_SA JH_I,
    JH_TO JH_KU JH_BE JH_TU JH_SI JH_YO JH_U JH_NO JK_KI JK_DASH JK_BO
    JK_DASH JK_DO,
    JH_KE JH_LTU JH_TE JH_I,
    JH_MO JH_DO JH_RU,
    JK_HE JK_RU JK_PU,
    // the followings are not used
    L"",
    L"",
    L"",
    L"",
    L"",
    L"",
    L"",
    L"",
    L"",
    L"",
    L"",
    L"",
    L"",
    L"",
    L"",
};

// Japanese - Keyboard Type (displayed if VKEY_JAPAN #defined)
const WCHAR* g_strJapanese2[] = 
{
    JH_NI JH_HO JH_N JH_GO L"\x30FB"
        JK_KI JK_DASH JK_BO JK_DASH JK_DO,
    JK_KI JK_DASH JK_BO JK_DASH JK_DO JH_WO JH_E JH_RA JH_N JH_DE JH_KU
    JH_DA JH_SA JH_I,
    JK_GU JK_RA JK_HU JK_LI JK_LTU JK_KU JH_HA JK_SA JK_N JK_PU JK_RU JH_YO JH_U,
    JH_KE JH_LTU JH_TE JH_I,
    JH_MO JH_DO JH_RU,
    JK_HE JK_RU JK_PU,
    L"SPACE",
    L"BACKSPACE",
    L"",    // not used
    L"",    // not used
    L"",    // not used
    L"",    // not used
    L"",    // not used
    JH_KA JH_N JH_RI JH_LYO JH_U,
    JH_SE JH_N JH_TA JH_KU,
    JK_KI JK_LYA JK_N JK_SE JK_RU,
    JK_MO JK_DASH JK_DO L"\n" JH_KI JH_RI JH_KA JH_E,
    JK_HE JK_RU JK_PU,
    JH_SA JH_KU JH_ZI JH_LYO,
    JK_SU JK_PE JK_DASH JK_SU,
    JK_TO JK_RI JK_GA JH_DE JK_KA JK_DASH JK_SO JK_RU JH_I JH_DO JH_U,
};

const WCHAR* g_strGerman[] =
{
    L"Deutsch",
    L"Tastatur ausw" L"\xE4" L"hlen",
    L"Grafiken nur zur Illustration",
    L"Ausw" L"\xE4" L"hlen",
    L"Zur" L"\xFC" L"ck",
    L"Hilfe",
    L"LEERTASTE",
    L"R" L"\xDC" L"CKTASTE",
    L"UMSCHALTTASTE",
    L"FESTSTELLTASTE",
    L"ALPHABET",
    L"SYMBOLE",
    L"AKZENTE",
    L"FERTIG",
    L"Ausw" L"\xE4" L"hlen",
    L"Abbrechen",
    L"Modus\nwechseln",
    L"Hilfe anzeigen",
    L"R" L"\xFC" L"cktaste",
    L"Leertaste",
    L"Schalter-Tasten bewegen den Cursor",
};

const WCHAR* g_strFrench[] =
{
    L"Fran" L"\xE7" L"ais",
    L"Choisir le clavier",
    L"Exemples de graphiques uniquement",
    L"S" L"\xE9" L"lectionner",
    L"Retour",
    L"Aide",
    L"ESPACE",
    L"RET. ARR",
    L"MAJ.",
    L"VERR. MAJ.",
    L"ALPHABET",
    L"SYMBOLE",
    L"ACCENTS",
    L"TERMIN" L"\xC9",
    L"S" L"\xE9" L"lectionner",
    L"Annuler",
    L"Basculer\nles modes",
    L"Afficher l'aide",
    L"Retour arri" L"\xE8" L"re",
    L"Espace",
    L"Les g" L"\xE2" L"chettes d" L"\xE9" L"placent le curseur",
};

const WCHAR* g_strSpanish[] =
{
    L"Espa" L"\xF1" L"ol",
    L"Elegir teclado",
    L"Gr" L"\xE1" L"ficos s" L"\xF3" L"lo de muestra",
    L"Seleccionar",
    L"Atr" L"\xE1" L"s",
    L"Ayuda",
    L"ESPACIO",
    L"RETROCESO",
    L"MAY" L"\xDA" L"S",
    L"BLOQ MAY" L"\xDA" L"S",
    L"ALFABETO",
    L"SIMBOLOS",
    L"ACCENTOS",
    L"HECHO",
    L"Seleccionar",
    L"Cancelar",
    L"Alternar\nmodo",
    L"Mostrar ayuda",
    L"Retroceso",
    L"Espacio",
    L"Los disparadores mueven el cursor",
};

const WCHAR* g_strItalian[] =
{
    L"Italiano",
    L"Scegli tastiera",
    L"Grafica solo dimostrativa",
    L"Seleziona",
    L"Indietro",
    L"Aiuto",
    L"BARRA SPAZIATRICE",
    L"BACKSPACE",
    L"MAIUSC",
    L"BLOC MAIUSC",
    L"ALFABETO",
    L"SIMBOLI",
    L"ACCENTI",
    L"CHIUDI",
    L"Seleziona",
    L"Annulla",
    L"Alterna\nmodalit" L"\xE0",
    L"Visualizza\nla Guida",
    L"Backspace",
    L"Spazio",
    L"I grilletti permettono di spostare il cursore",
};

const WCHAR** g_pStringTable = g_strEnglish;    // Current string table

// keyboard information
CXBVirtualKeyboard::KeyboardInfo g_aKeyboardInfo[] =
{
    CXBVirtualKeyboard::KeyboardInfo( XC_LANGUAGE_ENGLISH, 0, g_strEnglish ),
    CXBVirtualKeyboard::KeyboardInfo( XC_LANGUAGE_JAPANESE, 0, g_strJapanese1 ),
    CXBVirtualKeyboard::KeyboardInfo( XC_LANGUAGE_JAPANESE, 1, g_strJapanese2 ),
    CXBVirtualKeyboard::KeyboardInfo( XC_LANGUAGE_GERMAN, 0, g_strGerman ),
    CXBVirtualKeyboard::KeyboardInfo( XC_LANGUAGE_FRENCH, 0, g_strFrench ),
    CXBVirtualKeyboard::KeyboardInfo( XC_LANGUAGE_SPANISH, 0, g_strSpanish ),
    CXBVirtualKeyboard::KeyboardInfo( XC_LANGUAGE_ITALIAN, 0, g_strItalian ),
};
const UINT KEYBOARD_MAX = sizeof( g_aKeyboardInfo ) / sizeof( g_aKeyboardInfo[0] );




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program. Initializes everything, and goes into a
//       message-processing loop. Idle time is used to render the scene.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    CXBVirtualKeyboard xbApp;

    if( FAILED( xbApp.Create() ) )
        return;

    xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: CXBVirtualKeyboard::Key()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBVirtualKeyboard::Key::Key( Xkey xk, DWORD w )
:
    xKey( xk ),
    dwWidth( w ),
    strName()
{
    // Special keys get their own names
    switch( xKey )
    {
        case XK_SPACE:
            strName = GetString( STR_KEY_SPACE );
            break;
        case XK_BACKSPACE:
            strName = GetString( STR_KEY_BACKSPACE );
            break;
        case XK_SHIFT:
            strName = GetString( STR_KEY_SHIFT );
            break;
        case XK_CAPSLOCK:
            strName = GetString( STR_KEY_CAPSLOCK );
            break;
        case XK_ALPHABET:
            strName = GetString( STR_KEY_ALPHABET );
            break;
        case XK_SYMBOLS:
            strName = GetString( STR_KEY_SYMBOLS );
            break;
        case XK_ACCENTS:
            strName = GetString( STR_KEY_ACCENTS );
            break;
        case XK_OK:
            strName = GetString( STR_KEY_DONE );
            break;
        case XK_HIRAGANA:
            strName = JH_HI JH_RA JH_GA JH_NA;
            break;
        case XK_KATAKANA:
            strName = JK_KA JK_TA JK_KA JK_NA;
            break;
        case XK_ANS:
            strName = JH_E JH_I JH_SU JH_U JH_KI JH_GO JH_U;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: CXBVirtualKeyboard()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBVirtualKeyboard::CXBVirtualKeyboard()
{
    m_bIsCapsLockOn  = FALSE;
    m_bIsShiftOn     = FALSE;
    m_State          = STATE_STARTSCREEN;
    m_iPos           = 0;
    m_iCurrBoard     = TYPE_ALPHABET;
    m_iCurrRow       = 0;
    m_iCurrKey       = 0;
    m_iLastColumn    = 0;
    m_fRepeatDelay   = fINITIAL_REPEAT;
    m_pKeyTexture    = NULL;
    m_ptControllerS  = NULL;
    m_pCurrKeyboard  = g_aKeyboardInfo;
    m_fKeyHeight     = 42.0f;
    m_dwMaxRows      = 5;
    m_dwCurrCtlrState= 0;
    m_dwOldCtlrState = 0;
    m_xNextKeyJpn    = XK_NULL;
    m_bTrig          = FALSE;
    m_bKana          = FALSE;
    
    m_CaretTimer.Start();

    SelectKeyboard( 0 );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Sets up the virtual keyboard example
//-----------------------------------------------------------------------------
HRESULT CXBVirtualKeyboard::Initialize()
{
    // Create the resources
    if( FAILED( m_xprResource.Create( "Resource.xpr", resource_NUM_RESOURCES ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Set the matrices
    D3DXVECTOR3 vEye(-2.5f, 2.0f, -4.0f );
    D3DXVECTOR3 vAt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );

    D3DXMATRIX matWorld, matView, matProj;
    D3DXMatrixIdentity( &matWorld );
    D3DXMatrixLookAtLH( &matView, &vEye,&vAt, &vUp );
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 4.0f/3.0f, 1.0f, 100.0f );

    m_pd3dDevice->SetTransform( D3DTS_WORLD,      &matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW,       &matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Arial Unicode MS 18, regular, 32-376, for keys
    if( FAILED( m_Font18.Create( "Font18.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Arial 12, bold, 32-255, for capital words on keys
    if( FAILED( m_Font12.Create( "Font12.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Xbox dingbats (buttons) 24
    if( FAILED( m_FontButtons.Create( "Xboxdings_24.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // ControllerS gamepad
    if( FAILED( m_xprControllerS.Create( "ControllerS.xpr", ControllerS_NUM_RESOURCES ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    m_ptControllerS = m_xprControllerS.GetTexture( ControllerS_ControllerSTexture_OFFSET );

    // Load the click sound
    if( FAILED( m_ClickSnd.Create( "Sounds\\Click.wav" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Validate key sizes
    assert( MODEKEY_WIDTH + GAP2_WIDTH + (10 * KEY_WIDTH) + (9 * GAP_WIDTH) <= 512 );

    // Create the keyboard key texture
    m_pKeyTexture = m_xprResource.GetTexture( resource_KeyboardKey_OFFSET );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame; the entry point for animating the scene
//-----------------------------------------------------------------------------
HRESULT CXBVirtualKeyboard::FrameMove()
{
    ValidateState();

    // Poll the system for events
    Event ev = GetEvent();

    if( m_iKeyboard == KEYBOARD_JAPANESE1 && m_State == STATE_KEYBOARD )
    {
        if( ev == EV_BACK_BUTTON )
        {
            m_State = STATE_MENU;
        }
        else
        {
            // Must get additional information from the controller
            m_dwCurrCtlrState = GetEventJapan();
            UpdateStateJapan();
        }
    }
    else
    {
        // Normal state update
        UpdateState( ev );
    }
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d rendering.
//       This function sets up render states, clears the viewport, and renders
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBVirtualKeyboard::Render()
{
    // Clear the viewport, zbuffer, and stencil buffer
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | 
                         D3DCLEAR_STENCIL, 0xff1010c0, 1.0f, 0L );

    switch( m_State )
    {
        case STATE_STARTSCREEN: RenderStartScreen(); break;
        case STATE_MENU:        RenderMenu();        break;
        case STATE_KEYBOARD:    RenderKeyboard();    break;
        case STATE_HELP:        RenderHelp();        break;
        default:                assert( FALSE );     break;
    }
    
    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ValidateState()
// Desc: Check object invariants
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::ValidateState() const
{
    assert( m_State >= 0 );
    assert( m_State < STATE_MAX );
    assert( m_iLanguage > 0 );

    assert( m_iLanguage < XC_LANGUAGE_MAX );
    assert( m_iKeyboard < KEYBOARD_MAX );
    assert( m_iPos <= m_strData.length() );
    assert( m_iCurrBoard <= m_KeyboardList.size() );
    assert( m_iCurrRow < m_dwMaxRows );
    if( !m_KeyboardList.empty() )
        assert( m_iCurrKey < m_KeyboardList[ m_iCurrBoard ][ m_iCurrRow ].size() );
}




//-----------------------------------------------------------------------------
// Name: InitBoard()
// Desc: Sets up the virtual keyboard for the selected language
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::InitBoard()
{
    // Restore keyboard to default state
    m_iCurrRow = 1;
    m_iCurrKey = 1;
    m_iLastColumn = 1;
    m_iCurrBoard = TYPE_ALPHABET;
    m_bIsCapsLockOn = FALSE;
    m_bIsShiftOn = FALSE;
    m_strData.erase();
    m_iPos = 0;
    m_fKeyHeight    = 42.0f;
    m_dwMaxRows = 5;

    // Destroy old keyboard
    m_KeyboardList.clear();

    // Japanese doesn't use a Latin keyboard layout, so we can skip the
    // keyboard initialization
    if( m_iKeyboard == KEYBOARD_JAPANESE1 )
        return;
    if (m_iKeyboard == KEYBOARD_JAPANESE2 )
    {
        InitBoardJapanese2();
        return;
    }

    //-------------------------------------------------------------------------
    // Alpha keyboard
    //-------------------------------------------------------------------------

    Keyboard keyBoard;
    keyBoard.reserve( m_dwMaxRows );
    keyBoard.clear();

    KeyRow keyRow;
    keyRow.reserve( MAX_KEYS_PER_ROW );

    // First row is Done, 1-0
    keyRow.clear();
    keyRow.push_back( Key( XK_OK, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_1 ) );
    keyRow.push_back( Key( XK_2 ) );
    keyRow.push_back( Key( XK_3 ) );
    keyRow.push_back( Key( XK_4 ) );
    keyRow.push_back( Key( XK_5 ) );
    keyRow.push_back( Key( XK_6 ) );
    keyRow.push_back( Key( XK_7 ) );
    keyRow.push_back( Key( XK_8 ) );
    keyRow.push_back( Key( XK_9 ) );
    keyRow.push_back( Key( XK_0 ) );
    keyBoard.push_back( keyRow );

    // Second row is Shift, A-J
    keyRow.clear();
    keyRow.push_back( Key( XK_SHIFT, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_A ) );
    keyRow.push_back( Key( XK_B ) );
    keyRow.push_back( Key( XK_C ) );
    keyRow.push_back( Key( XK_D ) );
    keyRow.push_back( Key( XK_E ) );
    keyRow.push_back( Key( XK_F ) );
    keyRow.push_back( Key( XK_G ) );
    keyRow.push_back( Key( XK_H ) );
    keyRow.push_back( Key( XK_I ) );
    keyRow.push_back( Key( XK_J ) );
    keyBoard.push_back( keyRow );

    // Third row is Caps Lock, K-T
    keyRow.clear();
    keyRow.push_back( Key( XK_CAPSLOCK, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_K ) );
    keyRow.push_back( Key( XK_L ) );
    keyRow.push_back( Key( XK_M ) );
    keyRow.push_back( Key( XK_N ) );
    keyRow.push_back( Key( XK_O ) );
    keyRow.push_back( Key( XK_P ) );
    keyRow.push_back( Key( XK_Q ) );
    keyRow.push_back( Key( XK_R ) );
    keyRow.push_back( Key( XK_S ) );
    keyRow.push_back( Key( XK_T ) );
    keyBoard.push_back( keyRow );

    // Fourth row is Symbols, U-Z, Backspace
    keyRow.clear();
    keyRow.push_back( Key( XK_SYMBOLS, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_U ) );
    keyRow.push_back( Key( XK_V ) );
    keyRow.push_back( Key( XK_W ) );
    keyRow.push_back( Key( XK_X ) );
    keyRow.push_back( Key( XK_Y ) );
    keyRow.push_back( Key( XK_Z ) );
    keyRow.push_back( Key( XK_BACKSPACE, (KEY_WIDTH * 4) + (GAP_WIDTH * 3) ) );
    keyBoard.push_back( keyRow );

    // Fifth row is Accents, Space, Left, Right
    keyRow.clear();
    keyRow.push_back( Key( XK_ACCENTS, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_SPACE, (KEY_WIDTH * 6) + (GAP_WIDTH * 5) ) );
    keyRow.push_back( Key( XK_ARROWLEFT, (KEY_WIDTH * 2) + (GAP_WIDTH * 1) ) );
    keyRow.push_back( Key( XK_ARROWRIGHT, (KEY_WIDTH * 2) + (GAP_WIDTH * 1) ) );
    keyBoard.push_back( keyRow );

    // Add the alpha keyboard to the list
    m_KeyboardList.push_back( keyBoard );

    //-------------------------------------------------------------------------
    // Symbol keyboard
    //-------------------------------------------------------------------------

    keyBoard.clear();

    // First row
    keyRow.clear();
    keyRow.push_back( Key( XK_OK, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_LPAREN ) );
    keyRow.push_back( Key( XK_RPAREN ) );
    keyRow.push_back( Key( XK_AMPER ) );
    keyRow.push_back( Key( XK_UNDERS ) );
    keyRow.push_back( Key( XK_CARET ) );
    keyRow.push_back( Key( XK_PERCENT ) );
    keyRow.push_back( Key( XK_BSLASH ) );
    keyRow.push_back( Key( XK_FSLASH ) );
    keyRow.push_back( Key( XK_AT ) );
    keyRow.push_back( Key( XK_NSIGN ) );
    keyBoard.push_back( keyRow );

    // Second row
    keyRow.clear();
    keyRow.push_back( Key( XK_SHIFT, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_LBRACK ) );
    keyRow.push_back( Key( XK_RBRACK ) );
    keyRow.push_back( Key( XK_DOLLAR ) );
    keyRow.push_back( Key( XK_POUND_SIGN ) );
    keyRow.push_back( Key( XK_YEN_SIGN ) );
    keyRow.push_back( Key( XK_EURO_SIGN ) );
    keyRow.push_back( Key( XK_SEMI ) );
    keyRow.push_back( Key( XK_COLON ) );
    keyRow.push_back( Key( XK_QUOTE ) );
    keyRow.push_back( Key( XK_DQUOTE ) );
    keyBoard.push_back( keyRow );

    // Third row
    keyRow.clear();
    keyRow.push_back( Key( XK_CAPSLOCK, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_LT ) );
    keyRow.push_back( Key( XK_GT ) );
    keyRow.push_back( Key( XK_QMARK ) );
    keyRow.push_back( Key( XK_EXCL ) );
    keyRow.push_back( Key( XK_INVERTED_QMARK ) );
    keyRow.push_back( Key( XK_INVERTED_EXCL ) );
    keyRow.push_back( Key( XK_DASH ) );
    keyRow.push_back( Key( XK_STAR ) );
    keyRow.push_back( Key( XK_PLUS ) );
    keyRow.push_back( Key( XK_EQUAL ) );
    keyBoard.push_back( keyRow );

    // Fourth row
    keyRow.clear();
    keyRow.push_back( Key( XK_ALPHABET, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_LBRACE ) );
    keyRow.push_back( Key( XK_RBRACE ) );
    keyRow.push_back( Key( XK_LT_DBL_ANGLE_QUOTE ) );
    keyRow.push_back( Key( XK_RT_DBL_ANGLE_QUOTE ) );
    keyRow.push_back( Key( XK_COMMA ) );
    keyRow.push_back( Key( XK_PERIOD ) );
    keyRow.push_back( Key( XK_BACKSPACE, (KEY_WIDTH * 4) + (GAP_WIDTH * 3) ) );
    keyBoard.push_back( keyRow );

    // Fifth row is Accents, Space, Left, Right
    keyRow.clear();
    keyRow.push_back( Key( XK_ACCENTS, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_SPACE, (KEY_WIDTH * 6) + (GAP_WIDTH * 5) ) );
    keyRow.push_back( Key( XK_ARROWLEFT, (KEY_WIDTH * 2) + (GAP_WIDTH * 1) ) );
    keyRow.push_back( Key( XK_ARROWRIGHT, (KEY_WIDTH * 2) + (GAP_WIDTH * 1) ) );
    keyBoard.push_back( keyRow );

    // Add the symbol keyboard to the list
    m_KeyboardList.push_back( keyBoard );

    //-------------------------------------------------------------------------
    // Accents keyboard
    //-------------------------------------------------------------------------

    keyBoard.clear();

    // First row
    keyRow.clear();
    keyRow.push_back( Key( XK_OK, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_1 ) );
    keyRow.push_back( Key( XK_2 ) );
    keyRow.push_back( Key( XK_3 ) );
    keyRow.push_back( Key( XK_4 ) );
    keyRow.push_back( Key( XK_5 ) );
    keyRow.push_back( Key( XK_6 ) );
    keyRow.push_back( Key( XK_7 ) );
    keyRow.push_back( Key( XK_8 ) );
    keyRow.push_back( Key( XK_9 ) );
    keyRow.push_back( Key( XK_0 ) );
    keyBoard.push_back( keyRow );

    // Second row
    keyRow.clear();
    keyRow.push_back( Key( XK_SHIFT, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_CAP_A_GRAVE ) );
    keyRow.push_back( Key( XK_CAP_A_ACUTE ) );
    keyRow.push_back( Key( XK_CAP_A_CIRCUMFLEX ) );
    keyRow.push_back( Key( XK_CAP_A_DIAERESIS ) );
    keyRow.push_back( Key( XK_CAP_C_CEDILLA ) );
    keyRow.push_back( Key( XK_CAP_E_GRAVE ) );
    keyRow.push_back( Key( XK_CAP_E_ACUTE ) );
    keyRow.push_back( Key( XK_CAP_E_CIRCUMFLEX ) );
    keyRow.push_back( Key( XK_CAP_E_DIAERESIS ) );
    keyRow.push_back( Key( XK_CAP_I_GRAVE ) );
    keyBoard.push_back( keyRow );

    // Third row
    keyRow.clear();
    keyRow.push_back( Key( XK_CAPSLOCK, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_CAP_I_ACUTE ) );
    keyRow.push_back( Key( XK_CAP_I_CIRCUMFLEX ) );
    keyRow.push_back( Key( XK_CAP_I_DIAERESIS ) );
    keyRow.push_back( Key( XK_CAP_N_TILDE ) );
    keyRow.push_back( Key( XK_CAP_O_GRAVE ) );
    keyRow.push_back( Key( XK_CAP_O_ACUTE ) );
    keyRow.push_back( Key( XK_CAP_O_CIRCUMFLEX ) );
    keyRow.push_back( Key( XK_CAP_O_TILDE ) );
    keyRow.push_back( Key( XK_CAP_O_DIAERESIS ) );
    keyRow.push_back( Key( XK_SM_SHARP_S ) );
    keyBoard.push_back( keyRow );

    // Fourth row
    keyRow.clear();
    keyRow.push_back( Key( XK_ALPHABET, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_CAP_U_GRAVE ) );
    keyRow.push_back( Key( XK_CAP_U_ACUTE ) );
    keyRow.push_back( Key( XK_CAP_U_CIRCUMFLEX ) );
    keyRow.push_back( Key( XK_CAP_U_DIAERESIS ) );
    keyRow.push_back( Key( XK_CAP_Y_ACUTE ) );
    keyRow.push_back( Key( XK_CAP_Y_DIAERESIS ) );
    keyRow.push_back( Key( XK_BACKSPACE, (KEY_WIDTH * 4) + (GAP_WIDTH * 3) ) );
    keyBoard.push_back( keyRow );

    // Fifth row
    keyRow.clear();
    keyRow.push_back( Key( XK_ACCENTS, MODEKEY_WIDTH ) );
    keyRow.push_back( Key( XK_SPACE, (KEY_WIDTH * 6) + (GAP_WIDTH * 5) ) );
    keyRow.push_back( Key( XK_ARROWLEFT, (KEY_WIDTH * 2) + (GAP_WIDTH * 1) ) );
    keyRow.push_back( Key( XK_ARROWRIGHT, (KEY_WIDTH * 2) + (GAP_WIDTH * 1) ) );
    keyBoard.push_back( keyRow );

    // Add the accents keyboard to the list
    m_KeyboardList.push_back( keyBoard );

}




//-----------------------------------------------------------------------------
// Name: InitBoardJapanese2()
// Desc: Sets up the virtual keyboard for Japanese
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::InitBoardJapanese2()
{
    // Restore keyboard to default state
    m_iCurrRow = 0;
    m_iCurrKey = 0;
    m_iLastColumn = 0;
    m_iCurrBoard = TYPE_HIRAGANA;
    m_bIsCapsLockOn = FALSE;
    m_bIsShiftOn = FALSE;
    m_strData.erase();
    m_iPos = 0;
    m_fKeyHeight    = 32.0f;
    m_dwMaxRows = 9;

    // Destroy old keyboard
    m_KeyboardList.clear();

    //-------------------------------------------------------------------------
    // Hiragana keyboard
    //-------------------------------------------------------------------------

    Keyboard keyBoard;
    m_KeyboardList.push_back( keyBoard );
    m_KeyboardList.push_back( keyBoard );
    m_KeyboardList.push_back( keyBoard );

    keyBoard.reserve( m_dwMaxRows );
    keyBoard.clear();

    KeyRow keyRow;
    keyRow.reserve( MAX_KEYS_PER_ROW );

    // First row
    keyRow.clear();
    keyRow.push_back( Key( XK_HIRAGANA_A ) );
    keyRow.push_back( Key( XK_HIRAGANA_I ) );
    keyRow.push_back( Key( XK_HIRAGANA_U ) );
    keyRow.push_back( Key( XK_HIRAGANA_E ) );
    keyRow.push_back( Key( XK_HIRAGANA_O ) );
    keyRow.push_back( Key( XK_HIRAGANA_WA ) );
    keyRow.push_back( Key( XK_HIRAGANA_WO ) );
    keyRow.push_back( Key( XK_HIRAGANA_N ) );
    keyRow.push_back( Key( XK_NULL ) );
    keyRow.push_back( Key( XK_NULL ) );
    keyRow.push_back( Key( XK_HIRAGANA, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Second row
    keyRow.clear();
    keyRow.push_back( Key( XK_HIRAGANA_KA ) );
    keyRow.push_back( Key( XK_HIRAGANA_KI ) );
    keyRow.push_back( Key( XK_HIRAGANA_KU ) );
    keyRow.push_back( Key( XK_HIRAGANA_KE ) );
    keyRow.push_back( Key( XK_HIRAGANA_KO ) );
    keyRow.push_back( Key( XK_HIRAGANA_LA ) );
    keyRow.push_back( Key( XK_HIRAGANA_LI ) );
    keyRow.push_back( Key( XK_HIRAGANA_LU ) );
    keyRow.push_back( Key( XK_HIRAGANA_LE ) );
    keyRow.push_back( Key( XK_HIRAGANA_LO ) );
    keyRow.push_back( Key( XK_KATAKANA, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Third row
    keyRow.clear();
    keyRow.push_back( Key( XK_HIRAGANA_SA ) );
    keyRow.push_back( Key( XK_HIRAGANA_SI ) );
    keyRow.push_back( Key( XK_HIRAGANA_SU ) );
    keyRow.push_back( Key( XK_HIRAGANA_SE ) );
    keyRow.push_back( Key( XK_HIRAGANA_SO ) );
    keyRow.push_back( Key( XK_HIRAGANA_LTU ) );
    keyRow.push_back( Key( XK_HIRAGANA_LYA ) );
    keyRow.push_back( Key( XK_HIRAGANA_LYU ) );
    keyRow.push_back( Key( XK_HIRAGANA_LYO ) );
    keyRow.push_back( Key( XK_HIRAGANA_LWA ) );
    keyRow.push_back( Key( XK_ANS, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Fourth row
    keyRow.clear();
    keyRow.push_back( Key( XK_HIRAGANA_TA ) );
    keyRow.push_back( Key( XK_HIRAGANA_TI ) );
    keyRow.push_back( Key( XK_HIRAGANA_TU ) );
    keyRow.push_back( Key( XK_HIRAGANA_TE ) );
    keyRow.push_back( Key( XK_HIRAGANA_TO ) );
    keyRow.push_back( Key( XK_HIRAGANA_GA ) );
    keyRow.push_back( Key( XK_HIRAGANA_GI ) );
    keyRow.push_back( Key( XK_HIRAGANA_GU ) );
    keyRow.push_back( Key( XK_HIRAGANA_GE ) );
    keyRow.push_back( Key( XK_HIRAGANA_GO ) );
    keyRow.push_back( Key( XK_SPACE, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Fifth row
    keyRow.clear();
    keyRow.push_back( Key( XK_HIRAGANA_NA ) );
    keyRow.push_back( Key( XK_HIRAGANA_NI ) );
    keyRow.push_back( Key( XK_HIRAGANA_NU ) );
    keyRow.push_back( Key( XK_HIRAGANA_NE ) );
    keyRow.push_back( Key( XK_HIRAGANA_NO ) );
    keyRow.push_back( Key( XK_HIRAGANA_ZA ) );
    keyRow.push_back( Key( XK_HIRAGANA_ZI ) );
    keyRow.push_back( Key( XK_HIRAGANA_ZU ) );
    keyRow.push_back( Key( XK_HIRAGANA_ZE ) );
    keyRow.push_back( Key( XK_HIRAGANA_ZO ) );
    keyRow.push_back( Key( XK_BACKSPACE, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Sixth row
    keyRow.clear();
    keyRow.push_back( Key( XK_HIRAGANA_HA ) );
    keyRow.push_back( Key( XK_HIRAGANA_HI ) );
    keyRow.push_back( Key( XK_HIRAGANA_HU ) );
    keyRow.push_back( Key( XK_HIRAGANA_HE ) );
    keyRow.push_back( Key( XK_HIRAGANA_HO ) );
    keyRow.push_back( Key( XK_HIRAGANA_DA ) );
    keyRow.push_back( Key( XK_HIRAGANA_DI ) );
    keyRow.push_back( Key( XK_HIRAGANA_DU ) );
    keyRow.push_back( Key( XK_HIRAGANA_DE ) );
    keyRow.push_back( Key( XK_HIRAGANA_DO ) );
    keyRow.push_back( Key( XK_ARROWLEFT, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Seventh row
    keyRow.clear();
    keyRow.push_back( Key( XK_HIRAGANA_MA ) );
    keyRow.push_back( Key( XK_HIRAGANA_MI ) );
    keyRow.push_back( Key( XK_HIRAGANA_MU ) );
    keyRow.push_back( Key( XK_HIRAGANA_ME ) );
    keyRow.push_back( Key( XK_HIRAGANA_MO ) );
    keyRow.push_back( Key( XK_HIRAGANA_BA ) );
    keyRow.push_back( Key( XK_HIRAGANA_BI ) );
    keyRow.push_back( Key( XK_HIRAGANA_BU ) );
    keyRow.push_back( Key( XK_HIRAGANA_BE ) );
    keyRow.push_back( Key( XK_HIRAGANA_BO ) );
    keyRow.push_back( Key( XK_ARROWRIGHT, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Eighth row
    keyRow.clear();
    keyRow.push_back( Key( XK_HIRAGANA_YA ) );
    keyRow.push_back( Key( XK_NULL ) );
    keyRow.push_back( Key( XK_HIRAGANA_YU ) );
    keyRow.push_back( Key( XK_NULL ) );
    keyRow.push_back( Key( XK_HIRAGANA_YO ) );
    keyRow.push_back( Key( XK_HIRAGANA_PA ) );
    keyRow.push_back( Key( XK_HIRAGANA_PI ) );
    keyRow.push_back( Key( XK_HIRAGANA_PU ) );
    keyRow.push_back( Key( XK_HIRAGANA_PE ) );
    keyRow.push_back( Key( XK_HIRAGANA_PO ) );
    keyRow.push_back( Key( XK_NULL ) );
    keyBoard.push_back( keyRow );

    // Nineth row
    keyRow.clear();
    keyRow.push_back( Key( XK_HIRAGANA_RA ) );
    keyRow.push_back( Key( XK_HIRAGANA_RI ) );
    keyRow.push_back( Key( XK_HIRAGANA_RU ) );
    keyRow.push_back( Key( XK_HIRAGANA_RE ) );
    keyRow.push_back( Key( XK_HIRAGANA_RO ) );
    keyRow.push_back( Key( XK_KATAKANA_DASH ) );
    keyRow.push_back( Key( XK_ID_COMMA ) );
    keyRow.push_back( Key( XK_ID_PERIOD ) );
    keyRow.push_back( Key( XK_LCNER_BRAKET ) );
    keyRow.push_back( Key( XK_RCNER_BRAKET ) );
    keyRow.push_back( Key( XK_OK, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Add the alpha keyboard to the list
    m_KeyboardList.push_back( keyBoard );

    //-------------------------------------------------------------------------
    // Katakana keyboard
    //-------------------------------------------------------------------------

    keyBoard.clear();

    // First row
    keyRow.clear();
    keyRow.push_back( Key( XK_KATAKANA_A ) );
    keyRow.push_back( Key( XK_KATAKANA_I ) );
    keyRow.push_back( Key( XK_KATAKANA_U ) );
    keyRow.push_back( Key( XK_KATAKANA_E ) );
    keyRow.push_back( Key( XK_KATAKANA_O ) );
    keyRow.push_back( Key( XK_KATAKANA_WA ) );
    keyRow.push_back( Key( XK_KATAKANA_WO ) );
    keyRow.push_back( Key( XK_KATAKANA_N ) );
    keyRow.push_back( Key( XK_KATAKANA_VU ) );
    keyRow.push_back( Key( XK_NULL ) );
    keyRow.push_back( Key( XK_HIRAGANA, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Second row
    keyRow.clear();
    keyRow.push_back( Key( XK_KATAKANA_KA ) );
    keyRow.push_back( Key( XK_KATAKANA_KI ) );
    keyRow.push_back( Key( XK_KATAKANA_KU ) );
    keyRow.push_back( Key( XK_KATAKANA_KE ) );
    keyRow.push_back( Key( XK_KATAKANA_KO ) );
    keyRow.push_back( Key( XK_KATAKANA_LA ) );
    keyRow.push_back( Key( XK_KATAKANA_LI ) );
    keyRow.push_back( Key( XK_KATAKANA_LU ) );
    keyRow.push_back( Key( XK_KATAKANA_LE ) );
    keyRow.push_back( Key( XK_KATAKANA_LO ) );
    keyRow.push_back( Key( XK_KATAKANA, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Third row
    keyRow.clear();
    keyRow.push_back( Key( XK_KATAKANA_SA ) );
    keyRow.push_back( Key( XK_KATAKANA_SI ) );
    keyRow.push_back( Key( XK_KATAKANA_SU ) );
    keyRow.push_back( Key( XK_KATAKANA_SE ) );
    keyRow.push_back( Key( XK_KATAKANA_SO ) );
    keyRow.push_back( Key( XK_KATAKANA_LTU ) );
    keyRow.push_back( Key( XK_KATAKANA_LYA ) );
    keyRow.push_back( Key( XK_KATAKANA_LYU ) );
    keyRow.push_back( Key( XK_KATAKANA_LYO ) );
    keyRow.push_back( Key( XK_KATAKANA_LWA ) );
    keyRow.push_back( Key( XK_ANS, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Fourth row
    keyRow.clear();
    keyRow.push_back( Key( XK_KATAKANA_TA ) );
    keyRow.push_back( Key( XK_KATAKANA_TI ) );
    keyRow.push_back( Key( XK_KATAKANA_TU ) );
    keyRow.push_back( Key( XK_KATAKANA_TE ) );
    keyRow.push_back( Key( XK_KATAKANA_TO ) );
    keyRow.push_back( Key( XK_KATAKANA_GA ) );
    keyRow.push_back( Key( XK_KATAKANA_GI ) );
    keyRow.push_back( Key( XK_KATAKANA_GU ) );
    keyRow.push_back( Key( XK_KATAKANA_GE ) );
    keyRow.push_back( Key( XK_KATAKANA_GO ) );
    keyRow.push_back( Key( XK_SPACE, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Fifth row
    keyRow.clear();
    keyRow.push_back( Key( XK_KATAKANA_NA ) );
    keyRow.push_back( Key( XK_KATAKANA_NI ) );
    keyRow.push_back( Key( XK_KATAKANA_NU ) );
    keyRow.push_back( Key( XK_KATAKANA_NE ) );
    keyRow.push_back( Key( XK_KATAKANA_NO ) );
    keyRow.push_back( Key( XK_KATAKANA_ZA ) );
    keyRow.push_back( Key( XK_KATAKANA_ZI ) );
    keyRow.push_back( Key( XK_KATAKANA_ZU ) );
    keyRow.push_back( Key( XK_KATAKANA_ZE ) );
    keyRow.push_back( Key( XK_KATAKANA_ZO ) );
    keyRow.push_back( Key( XK_BACKSPACE, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Sixth row
    keyRow.clear();
    keyRow.push_back( Key( XK_KATAKANA_HA ) );
    keyRow.push_back( Key( XK_KATAKANA_HI ) );
    keyRow.push_back( Key( XK_KATAKANA_HU ) );
    keyRow.push_back( Key( XK_KATAKANA_HE ) );
    keyRow.push_back( Key( XK_KATAKANA_HO ) );
    keyRow.push_back( Key( XK_KATAKANA_DA ) );
    keyRow.push_back( Key( XK_KATAKANA_DI ) );
    keyRow.push_back( Key( XK_KATAKANA_DU ) );
    keyRow.push_back( Key( XK_KATAKANA_DE ) );
    keyRow.push_back( Key( XK_KATAKANA_DO ) );
    keyRow.push_back( Key( XK_ARROWLEFT, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Seventh row
    keyRow.clear();
    keyRow.push_back( Key( XK_KATAKANA_MA ) );
    keyRow.push_back( Key( XK_KATAKANA_MI ) );
    keyRow.push_back( Key( XK_KATAKANA_MU ) );
    keyRow.push_back( Key( XK_KATAKANA_ME ) );
    keyRow.push_back( Key( XK_KATAKANA_MO ) );
    keyRow.push_back( Key( XK_KATAKANA_BA ) );
    keyRow.push_back( Key( XK_KATAKANA_BI ) );
    keyRow.push_back( Key( XK_KATAKANA_BU ) );
    keyRow.push_back( Key( XK_KATAKANA_BE ) );
    keyRow.push_back( Key( XK_KATAKANA_BO ) );
    keyRow.push_back( Key( XK_ARROWRIGHT, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Eighth row
    keyRow.clear();
    keyRow.push_back( Key( XK_KATAKANA_YA ) );
    keyRow.push_back( Key( XK_NULL ) );
    keyRow.push_back( Key( XK_KATAKANA_YU ) );
    keyRow.push_back( Key( XK_NULL ) );
    keyRow.push_back( Key( XK_KATAKANA_YO ) );
    keyRow.push_back( Key( XK_KATAKANA_PA ) );
    keyRow.push_back( Key( XK_KATAKANA_PI ) );
    keyRow.push_back( Key( XK_KATAKANA_PU ) );
    keyRow.push_back( Key( XK_KATAKANA_PE ) );
    keyRow.push_back( Key( XK_KATAKANA_PO ) );
    keyRow.push_back( Key( XK_NULL ) );
    keyBoard.push_back( keyRow );

    // Nineth row
    keyRow.clear();
    keyRow.push_back( Key( XK_KATAKANA_RA ) );
    keyRow.push_back( Key( XK_KATAKANA_RI ) );
    keyRow.push_back( Key( XK_KATAKANA_RU ) );
    keyRow.push_back( Key( XK_KATAKANA_RE ) );
    keyRow.push_back( Key( XK_KATAKANA_RO ) );
    keyRow.push_back( Key( XK_KATAKANA_DASH ) );
    keyRow.push_back( Key( XK_ID_COMMA ) );
    keyRow.push_back( Key( XK_ID_PERIOD ) );
    keyRow.push_back( Key( XK_LCNER_BRAKET ) );
    keyRow.push_back( Key( XK_RCNER_BRAKET ) );
    keyRow.push_back( Key( XK_OK, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Add the symbol keyboard to the list
    m_KeyboardList.push_back( keyBoard );

    //-------------------------------------------------------------------------
    // Alphabet / Numeral / Symbol keyboard
    //-------------------------------------------------------------------------

    keyBoard.clear();

    // First row
    keyRow.clear();
    keyRow.push_back( Key( XK_A ) );
    keyRow.push_back( Key( XK_B ) );
    keyRow.push_back( Key( XK_C ) );
    keyRow.push_back( Key( XK_D ) );
    keyRow.push_back( Key( XK_E ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_A ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_B ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_C ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_D ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_E ) ) );
    keyRow.push_back( Key( XK_HIRAGANA, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Second row
    keyRow.clear();
    keyRow.push_back( Key( XK_F ) );
    keyRow.push_back( Key( XK_G ) );
    keyRow.push_back( Key( XK_H ) );
    keyRow.push_back( Key( XK_I ) );
    keyRow.push_back( Key( XK_J ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_F ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_G ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_H ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_I ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_J ) ) );
    keyRow.push_back( Key( XK_KATAKANA, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Third row
    keyRow.clear();
    keyRow.push_back( Key( XK_K ) );
    keyRow.push_back( Key( XK_L ) );
    keyRow.push_back( Key( XK_M ) );
    keyRow.push_back( Key( XK_N ) );
    keyRow.push_back( Key( XK_O ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_K ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_L ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_M ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_N ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_O ) ) );
    keyRow.push_back( Key( XK_ANS, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Fourth row
    keyRow.clear();
    keyRow.push_back( Key( XK_P ) );
    keyRow.push_back( Key( XK_Q ) );
    keyRow.push_back( Key( XK_R ) );
    keyRow.push_back( Key( XK_S ) );
    keyRow.push_back( Key( XK_T ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_P ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_Q ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_R ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_S ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_T ) ) );
    keyRow.push_back( Key( XK_SPACE, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Fifth row
    keyRow.clear();
    keyRow.push_back( Key( XK_U ) );
    keyRow.push_back( Key( XK_V ) );
    keyRow.push_back( Key( XK_W ) );
    keyRow.push_back( Key( XK_X ) );
    keyRow.push_back( Key( XK_Y ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_U ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_V ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_W ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_X ) ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_Y ) ) );
    keyRow.push_back( Key( XK_BACKSPACE, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Sixth row
    keyRow.clear();
    keyRow.push_back( Key( XK_Z ) );
    keyRow.push_back( Key( XK_DQUOTE ) );
    keyRow.push_back( Key( XK_QUOTE ) );
    keyRow.push_back( Key( XK_AT ) );
    keyRow.push_back( Key( XK_NSIGN ) );
    keyRow.push_back( Key( ( Xkey )ToLower( XK_Z ) ) );
    keyRow.push_back( Key( XK_LPAREN ) );
    keyRow.push_back( Key( XK_RPAREN ) );
    keyRow.push_back( Key( XK_LBRACE ) );
    keyRow.push_back( Key( XK_RBRACE ) );
    keyRow.push_back( Key( XK_ARROWLEFT, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Seventh row
    keyRow.clear();
    keyRow.push_back( Key( XK_AMPER ) );
    keyRow.push_back( Key( XK_CARET ) );
    keyRow.push_back( Key( XK_DOLLAR ) );
    keyRow.push_back( Key( XK_YEN_SIGN ) );
    keyRow.push_back( Key( XK_PERCENT ) );
    keyRow.push_back( Key( XK_DASH ) );
    keyRow.push_back( Key( XK_PLUS ) );
    keyRow.push_back( Key( XK_EQUAL ) );
    keyRow.push_back( Key( XK_STAR ) );
    keyRow.push_back( Key( XK_FSLASH ) );
    keyRow.push_back( Key( XK_ARROWRIGHT, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Eighth row
    keyRow.clear();
    keyRow.push_back( Key( XK_0 ) );
    keyRow.push_back( Key( XK_1 ) );
    keyRow.push_back( Key( XK_2 ) );
    keyRow.push_back( Key( XK_3 ) );
    keyRow.push_back( Key( XK_4 ) );
    keyRow.push_back( Key( XK_QMARK ) );
    keyRow.push_back( Key( XK_EXCL ) );
    keyRow.push_back( Key( XK_COLON ) );
    keyRow.push_back( Key( XK_SEMI ) );
    keyRow.push_back( Key( XK_BSLASH ) );
    keyRow.push_back( Key( XK_NULL ) );
    keyBoard.push_back( keyRow );

    // Nineth row
    keyRow.clear();
    keyRow.push_back( Key( XK_5 ) );
    keyRow.push_back( Key( XK_6 ) );
    keyRow.push_back( Key( XK_7 ) );
    keyRow.push_back( Key( XK_8 ) );
    keyRow.push_back( Key( XK_9 ) );
    keyRow.push_back( Key( XK_LT ) );
    keyRow.push_back( Key( XK_GT ) );
    keyRow.push_back( Key( XK_COMMA ) );
    keyRow.push_back( Key( XK_PERIOD ) );
    keyRow.push_back( Key( XK_UNDERS ) );
    keyRow.push_back( Key( XK_OK, MODEKEY_WIDTH ) );
    keyBoard.push_back( keyRow );

    // Add the accents keyboard to the list
    m_KeyboardList.push_back( keyBoard );

}




//-----------------------------------------------------------------------------
// Name: GetEvent()
// Desc: Polls the controller for events. Returns EV_NULL if no event
//-----------------------------------------------------------------------------
CXBVirtualKeyboard::Event CXBVirtualKeyboard::GetEvent()
{
    // Query the primary controller
    Event evControllerClick = GetControllerEvent();

    if( evControllerClick != EV_NULL && 
        m_iKeyboard != KEYBOARD_JAPANESE1 &&
        m_State == STATE_KEYBOARD )
    {
        PlayClick();
    }

    return evControllerClick;
}




//-----------------------------------------------------------------------------
// Name: IsAnyButtonActive()
// Desc: TRUE if any button depressed or any thumbstick offset on the given
//       controller.
//-----------------------------------------------------------------------------
BOOL IsAnyButtonActive( const XBGAMEPAD* pGamePad )
{
    // Check digital buttons
    if( pGamePad->wButtons )
        return TRUE;

    // Check analog buttons
    for( DWORD i = 0; i < 8; ++i )
    {
        if( pGamePad->bAnalogButtons[ i ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
            return TRUE;
    }

    // Check thumbsticks
    if( pGamePad->fX1 >  JOY_THRESHOLD ||
        pGamePad->fX1 < -JOY_THRESHOLD ||
        pGamePad->fY1 >  JOY_THRESHOLD ||
        pGamePad->fY1 < -JOY_THRESHOLD )
    {
        return TRUE;
    }

    if( pGamePad->fX2 >  JOY_THRESHOLD ||
        pGamePad->fX2 < -JOY_THRESHOLD ||
        pGamePad->fY2 >  JOY_THRESHOLD ||
        pGamePad->fY2 < -JOY_THRESHOLD )
    {
        return TRUE;
    }

    // Nothing active
    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: GetPrimaryController()
// Desc: The primary controller is the first controller used by a player.
//       If no controller has been used or the controller has been removed,
//       the primary controller is the controller inserted at the lowest 
//       port number. Function returns NULL if no controller is inserted.
//-----------------------------------------------------------------------------
const XBGAMEPAD* GetPrimaryController()
{
    static INT nPrimaryController = -1;

    // If primary controller has been set and hasn't been removed, use it
    const XBGAMEPAD* pGamePad = NULL;
    if( nPrimaryController != -1 )
    {
        pGamePad = &g_Gamepads[ nPrimaryController ];
        if( pGamePad->hDevice != NULL )
            return pGamePad;
    }

    // Primary controller hasn't been set or has been removed...

    // Examine each inserted controller to see if any is being used
    INT nFirst = -1;
    for( DWORD i=0; i < XGetPortCount(); ++i )
    {
        pGamePad = &g_Gamepads[i];
        if( pGamePad->hDevice != NULL )
        {
            // Remember the lowest inserted controller ID
            if( nFirst == -1 )
                nFirst = i;

            // If any button is active, we found the primary controller
            if( IsAnyButtonActive( pGamePad ) )
            {
                nPrimaryController = i;
                return pGamePad;
            }
        }
    }

    // No controllers are inserted
    if( nFirst == -1 )
        return NULL;

    // The primary controller hasn't been set and no controller has been
    // used yet, so return the controller on the lowest port number
    pGamePad = &g_Gamepads[ nFirst ];
    return pGamePad;
}




//-----------------------------------------------------------------------------
// Name: GetEventJapan()
// Desc: Returns the full ControllerS state packed into a DWORD.
//       Remembers the "old" state of the controller.
//-----------------------------------------------------------------------------
DWORD CXBVirtualKeyboard::GetEventJapan()
{
    assert( m_iKeyboard == KEYBOARD_JAPANESE1 );
    const XBGAMEPAD* pGamePad = GetPrimaryController();
    if( pGamePad == NULL )
        return 0;

    DWORD dwCurrCtlrState = 0;

    // Primary buttons
    if( pGamePad->wButtons & XINPUT_GAMEPAD_START )
    {
        if( !(m_dwOldCtlrState & XKJ_START) )
        {
            dwCurrCtlrState |= XKJ_START;
            m_dwOldCtlrState |= XKJ_START;
        }
    }
    else
        m_dwOldCtlrState &= ~XKJ_START;

    // "A"
    if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_A ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
    {
        if( !(m_dwOldCtlrState & XKJ_A) )
        {
            dwCurrCtlrState |= XKJ_A;
            m_dwOldCtlrState |= XKJ_A;
        }
    }
    else
        m_dwOldCtlrState &= ~XKJ_A;

    // "B"
    if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_B ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
    {
        if( !(m_dwOldCtlrState & XKJ_B) )
        {
            dwCurrCtlrState |= XKJ_B;
            m_dwOldCtlrState |= XKJ_B;
        }
    }
    else
        m_dwOldCtlrState &= ~XKJ_B;

    // "X"
    if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_X ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
    {
        if( !(m_dwOldCtlrState & XKJ_X) )
        {
            dwCurrCtlrState |= XKJ_X;
            m_dwOldCtlrState |= XKJ_X;
        }
    }
    else
        m_dwOldCtlrState &= ~XKJ_X;

    // "Y"
    if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_Y ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
    {
        if( !(m_dwOldCtlrState & XKJ_Y) )
        {
            dwCurrCtlrState |= XKJ_Y;
            m_dwOldCtlrState |= XKJ_Y;
        }
    }
    else
        m_dwOldCtlrState &= ~XKJ_Y;

    // black
    if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_BLACK ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
    {
        if( !(m_dwOldCtlrState & XKJ_BLACK) )
        {
            dwCurrCtlrState |= XKJ_BLACK;
            m_dwOldCtlrState |= XKJ_BLACK;
        }
    }
    else
        m_dwOldCtlrState &= ~XKJ_BLACK;

    // white
    if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_WHITE ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
    {
        if( !(m_dwOldCtlrState & XKJ_WHITE) )
        {
            dwCurrCtlrState |= XKJ_WHITE;
            m_dwOldCtlrState |= XKJ_WHITE;
        }
    }
    else
        m_dwOldCtlrState &= ~XKJ_WHITE;

    // left trigger
    if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_LEFT_TRIGGER ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
    {
        if( !(m_dwOldCtlrState & XKJ_LEFTTR) )
        {
            dwCurrCtlrState |= XKJ_LEFTTR;
            m_dwOldCtlrState |= XKJ_LEFTTR;
        }
    }
    else
        m_dwOldCtlrState &= ~XKJ_LEFTTR;

    // right trigger
    if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_RIGHT_TRIGGER ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
    {
        if( !(m_dwOldCtlrState & XKJ_RIGHTTR) )
        {
            dwCurrCtlrState |= XKJ_RIGHTTR;
            m_dwOldCtlrState |= XKJ_RIGHTTR;
        }
    }
    else
        m_dwOldCtlrState &= ~XKJ_RIGHTTR;

    // Cursor Movement
    if( pGamePad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        if( !(m_dwOldCtlrState & XKJ_DLEFT) )
        {
            dwCurrCtlrState |= XKJ_DLEFT;
            m_dwOldCtlrState |= XKJ_DLEFT;
        }
    }
    else
        m_dwOldCtlrState &= ~XKJ_DLEFT;

    if( pGamePad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        if( !(m_dwOldCtlrState & XKJ_DRIGHT) )
        {
            dwCurrCtlrState |= XKJ_DRIGHT;
            m_dwOldCtlrState |= XKJ_DRIGHT;
        }
    }
    else
        m_dwOldCtlrState &= ~XKJ_DRIGHT;

    if( pGamePad->fY1 > JOY_THRESHOLD )
        dwCurrCtlrState |= XKJ_UP;
    if( pGamePad->fY1 < -JOY_THRESHOLD )
        dwCurrCtlrState |= XKJ_DOWN;
    if( pGamePad->fX1 < -JOY_THRESHOLD )
        dwCurrCtlrState |= XKJ_LEFT;
    if( pGamePad->fX1 > JOY_THRESHOLD )
        dwCurrCtlrState |= XKJ_RIGHT;

    return dwCurrCtlrState;
}




//-----------------------------------------------------------------------------
// Name: GetControllerEvent()
// Desc: Polls the controller for events. Handles button repeats.
//-----------------------------------------------------------------------------
CXBVirtualKeyboard::Event CXBVirtualKeyboard::GetControllerEvent()
{
    const XBGAMEPAD* pGamePad = GetPrimaryController();
    if( pGamePad != NULL )
    {
        // Handle button press and joystick hold repeats
        BOOL bRepeat = FALSE;
        if( IsAnyButtonActive( pGamePad ) )
        {
            // If the timer is running, the button is being held. If it's
            // held long enough, it triggers a repeat. If the timer isn't
            // running, we start it.
            if( m_RepeatTimer.IsRunning() )
            {
                // If the timer is running but hasn't expired, bail out
                if( m_RepeatTimer.GetElapsedSeconds() < m_fRepeatDelay )
                    return EV_NULL;

                m_fRepeatDelay = fSTD_REPEAT;
                m_RepeatTimer.StartZero();
                bRepeat = TRUE;
            }
            else
            {
                m_fRepeatDelay = fINITIAL_REPEAT;
                m_RepeatTimer.StartZero();
            }
        }
        else
        {
            // No buttons or joysticks active; kill the repeat timer
            m_fRepeatDelay = fINITIAL_REPEAT;
            m_RepeatTimer.Stop();
        }

        // Only allow repeat for navigation or left/right arrows
        if( bRepeat )
        {
            if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_A ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
            {
                if( !m_KeyboardList.empty() )
                {
                    Key key = m_KeyboardList[ m_iCurrBoard ][ m_iCurrRow ][ m_iCurrKey ];
                    if( key.xKey == XK_ARROWLEFT ||
                        key.xKey == XK_ARROWRIGHT )
                    {
                        return EV_A_BUTTON;
                    }
                }
            }
        }
        else
        {
            // Primary buttons
            if( pGamePad->wButtons & XINPUT_GAMEPAD_START )
                return EV_START_BUTTON;
            if( pGamePad->wButtons & XINPUT_GAMEPAD_BACK )
                return EV_BACK_BUTTON;
            if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_A ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
                return EV_A_BUTTON;
            if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_B ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
                return EV_B_BUTTON;
            if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_X ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
                return EV_X_BUTTON;
            if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_Y ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
                return EV_Y_BUTTON;
            if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_BLACK ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
                return EV_BLACK_BUTTON;
            if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_WHITE ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
                return EV_WHITE_BUTTON;
        }

        // Cursor movement
        if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_LEFT_TRIGGER ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
            return EV_LEFT_BUTTON;
        if( pGamePad->bAnalogButtons[ XINPUT_GAMEPAD_RIGHT_TRIGGER ] > XINPUT_GAMEPAD_MAX_CROSSTALK )
            return EV_RIGHT_BUTTON;

        // Movement
        if( pGamePad->wButtons & XINPUT_GAMEPAD_DPAD_UP ||
            pGamePad->fY1 > JOY_THRESHOLD )
            return EV_UP;
        if( pGamePad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN ||
            pGamePad->fY1 < -JOY_THRESHOLD )
            return EV_DOWN;
        if( pGamePad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT ||
            pGamePad->fX1 < -JOY_THRESHOLD )
            return EV_LEFT;
        if( pGamePad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ||
            pGamePad->fX1 > JOY_THRESHOLD )
            return EV_RIGHT;
    }

    // No controllers inserted or no button presses or no repeat
    return EV_NULL;
}




//-----------------------------------------------------------------------------
// Name: UpdateState()
// Desc: State machine updates the current context based on the incoming event
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::UpdateState( Event ev )
{
    switch( m_State )
    {
        case STATE_STARTSCREEN:
            switch( ev )
            {
                case EV_A_BUTTON:           // Select current key
                case EV_START_BUTTON:
                    m_State = STATE_MENU;
                    break;
            }
            break;
        case STATE_MENU:
            switch( ev )
            {
                case EV_A_BUTTON:           // Select current key
                case EV_START_BUTTON:
                    InitBoard();
                    m_State = STATE_KEYBOARD;
                    break;

                // Navigation
                case EV_UP:
                    if( m_iKeyboard == 0 )
                        SelectKeyboard( KEYBOARD_MAX - 1 );
                    else
                        SelectKeyboard( m_iKeyboard - 1 );
                    break;

                case EV_DOWN:
                    if( m_iKeyboard == KEYBOARD_MAX - 1 )
                        SelectKeyboard( 0 );
                    else
                        SelectKeyboard( m_iKeyboard + 1 );
                    break;
            }
            break;
        case STATE_KEYBOARD:
            switch( ev )
            {
                case EV_A_BUTTON:           // Select current key
                case EV_START_BUTTON:
                    PressCurrent();
                    break;
                case EV_B_BUTTON:           // Shift mode
                case EV_BACK_BUTTON:        // Back
                    m_State = STATE_MENU;
                    break;
                case EV_X_BUTTON:           // Toggle keyboard
                    if( m_iKeyboard == KEYBOARD_ENGLISH )
                    {
                        Press( m_iCurrBoard == TYPE_SYMBOLS ?
                               XK_ALPHABET : XK_SYMBOLS );
                    }
                    else if ( m_iKeyboard == KEYBOARD_JAPANESE2 )
                    {
                        switch( m_iCurrBoard )
                        {
                            case TYPE_HIRAGANA:  Press( XK_KATAKANA ); break;
                            case TYPE_KATAKANA:  Press( XK_ANS      ); break;
                            case TYPE_ANS:       Press( XK_HIRAGANA ); break;
                        }
                    }
                    else
                    {
                        switch( m_iCurrBoard )
                        {
                            case TYPE_ALPHABET: Press( XK_SYMBOLS  ); break;
                            case TYPE_SYMBOLS:  Press( XK_ACCENTS  ); break;
                            case TYPE_ACCENTS:  Press( XK_ALPHABET ); break;
                        }
                    }
                    break;
                case EV_Y_BUTTON:           // Show help
                    m_State = STATE_HELP;
                    break;
                case EV_WHITE_BUTTON:       // Backspace
                    Press( XK_BACKSPACE );
                    break;
                case EV_BLACK_BUTTON:       // Space
                    Press( XK_SPACE );
                    break;
                case EV_LEFT_BUTTON:        // Left
                    Press( XK_ARROWLEFT );
                    break;
                case EV_RIGHT_BUTTON:       // Right
                    Press( XK_ARROWRIGHT );
                    break;

                // Navigation
                case EV_UP:     MoveUp();    break;
                case EV_DOWN:   MoveDown();  break;
                case EV_LEFT:   MoveLeft();  break;
                case EV_RIGHT:  MoveRight(); break;
            }
            break;
        case STATE_HELP:
            // Any key returns to keyboard
            if( ev != EV_NULL )
                m_State = STATE_KEYBOARD;
            break;
        default:
            assert( FALSE );
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateJapan()
// Desc: 日本語入力の子音、及びひらがな／カタカナ切替をここでする。
//       また押されたボタンによってはクリック音も出す。
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::UpdateStateJapan()
{
    assert( m_iKeyboard == KEYBOARD_JAPANESE1 );

    // See if right trigger is held
    if( m_dwCurrCtlrState & XKJ_RIGHTTR )
        m_bTrig ^= TRUE;

    // See if left trigger is held
    if( m_dwCurrCtlrState & XKJ_LEFTTR )
        m_bKana ^= TRUE;

    // Determine if we should play the click sound. The following
    // buttons give a click: A, B, X, Y, black, white, Dpad left/right
    const DWORD dwButtons = XKJ_A | XKJ_B | XKJ_X | XKJ_Y | XKJ_BLACK | 
                            XKJ_WHITE | XKJ_DLEFT | XKJ_DRIGHT;
    if( m_dwCurrCtlrState & dwButtons )
        PlayClick();

    // "Key press"
    if( m_xNextKeyJpn != NULL )
        Press( m_xNextKeyJpn );
}




//-----------------------------------------------------------------------------
// Name: PressCurrent()
// Desc: Press the current key on the keyboard
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::PressCurrent()
{
    // Determine the current key
    Key key = m_KeyboardList[ m_iCurrBoard ][ m_iCurrRow ][ m_iCurrKey ];

    // Press it
    Press( key.xKey );
}




//-----------------------------------------------------------------------------
// Name: Press()
// Desc: Press the given key on the keyboard
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::Press( Xkey xk )
{
    if ( xk == XK_NULL ) // happens in Japanese keyboard (keyboard type)
        xk = XK_SPACE;

    // If the key represents a character, add it to the word
    if( xk < 0x10000 && xk != XK_ARROWLEFT && xk != XK_ARROWRIGHT )
    {
        // Don't add more than the maximum characters, and don't allow 
        // text to exceed the width of the text entry field
        if( m_strData.length() < MAX_CHARS )
        {
            FLOAT fWidth, fHeight;
            m_Font18.GetTextExtent( m_strData.c_str(), &fWidth, &fHeight );

            if( fWidth < fTEXTBOX_WIDTH )
            {
                m_strData.insert( m_iPos, 1, GetChar( xk ) );
                ++m_iPos; // move the caret
            }
        }

        // Unstick the shift key
        m_bIsShiftOn = FALSE;
    }

    // Special cases
    else switch( xk )
    {
        case XK_BACKSPACE:
            if( m_iPos > 0 )
            {
                --m_iPos; // move the caret
                m_strData.erase( m_iPos, 1 );
            }
            break;
        case XK_DELETE: // Used for Japanese only
            if( m_strData.length() > 0 )
                m_strData.erase( m_iPos, 1 );
            break;
        case XK_SHIFT:
            m_bIsShiftOn = !m_bIsShiftOn;
            break;
        case XK_CAPSLOCK:
            m_bIsCapsLockOn = !m_bIsCapsLockOn;
            break;
        case XK_ALPHABET:
            m_iCurrBoard = TYPE_ALPHABET;

            // Adjust mode keys
            m_KeyboardList[m_iCurrBoard][3][0] = Key( XK_SYMBOLS, MODEKEY_WIDTH );
            m_KeyboardList[m_iCurrBoard][4][0] = Key( XK_ACCENTS, MODEKEY_WIDTH );

            break;
        case XK_SYMBOLS:
            m_iCurrBoard = TYPE_SYMBOLS;

            // Adjust mode keys
            m_KeyboardList[m_iCurrBoard][3][0] = Key( XK_ALPHABET, MODEKEY_WIDTH );
            m_KeyboardList[m_iCurrBoard][4][0] = Key( XK_ACCENTS, MODEKEY_WIDTH );

            break;
        case XK_ACCENTS:
            m_iCurrBoard = TYPE_ACCENTS;

            // Adjust mode keys
            m_KeyboardList[m_iCurrBoard][3][0] = Key( XK_ALPHABET, MODEKEY_WIDTH );
            m_KeyboardList[m_iCurrBoard][4][0] = Key( XK_SYMBOLS, MODEKEY_WIDTH );

            break;
        case XK_ARROWLEFT:
            if( m_iPos > 0 )
                --m_iPos;
            break;
        case XK_ARROWRIGHT:
            if( m_iPos < m_strData.length() )
                ++m_iPos;
            break;
        case XK_OK:
            m_iPos = 0;
            m_strData.erase();
            break;
        case XK_HIRAGANA:
            m_iCurrBoard = TYPE_HIRAGANA;
            break;
        case XK_KATAKANA:
            m_iCurrBoard = TYPE_KATAKANA;
            break;
        case XK_ANS:
            m_iCurrBoard = TYPE_ANS;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: MoveUp()
// Desc: Move the cursor up
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::MoveUp()
{
    if ( m_iKeyboard == KEYBOARD_JAPANESE2 )
    {
        do
        {
            m_iCurrRow = ( m_iCurrRow == 0 ) ? m_dwMaxRows - 1 : m_iCurrRow - 1;
        } while( IsKeyDisabled() );
        return;
    }

    do
    {
        // Update key index for special cases
        switch( m_iCurrRow )
        {
            case 0:
                if( 1 < m_iCurrKey && m_iCurrKey < 7 )      // 2 - 6
                {
                    m_iLastColumn = m_iCurrKey;             // remember column
                    m_iCurrKey = 1;                         // move to spacebar
                }
                else if( 6 < m_iCurrKey && m_iCurrKey < 9 ) // 7 - 8
                {
                    m_iLastColumn = m_iCurrKey;             // remember column
                    m_iCurrKey = 2;                         // move to left arrow
                }
                else if( m_iCurrKey > 8 )                   // 9 - 0
                {
                    m_iLastColumn = m_iCurrKey;             // remember column
                    m_iCurrKey = 3;                         // move to right arrow
                }
                break;
            case 3:
                if( m_iCurrKey == 7 )                       // backspace
                    m_iCurrKey = max( 7, m_iLastColumn );   // restore column
                break;
            case 4:
                if( m_iCurrKey == 1 )                       // spacebar
                    m_iCurrKey = min( 6, m_iLastColumn );   // restore column
                else if( m_iCurrKey > 1 )                   // left and right
                    m_iCurrKey = 7;                         // backspace
                break;
        }

        // Update row
        m_iCurrRow = ( m_iCurrRow == 0 ) ? m_dwMaxRows - 1 : m_iCurrRow - 1;

    } while( IsKeyDisabled() );
}




//-----------------------------------------------------------------------------
// Name: MoveDown()
// Desc: Move the cursor down
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::MoveDown()
{
    if ( m_iKeyboard == KEYBOARD_JAPANESE2 )
    {
        do
        {
            m_iCurrRow = ( m_iCurrRow == m_dwMaxRows - 1 ) ? 0 : m_iCurrRow + 1;
        } while( IsKeyDisabled() );
        return;
    }

    do
    {
        // Update key index for special cases
        switch( m_iCurrRow )
        {
            case 2:
                if( m_iCurrKey > 7 )                    // q - t
                {
                    m_iLastColumn = m_iCurrKey;         // remember column
                    m_iCurrKey = 7;                     // move to backspace
                }
                break;
            case 3:
                if( 0 < m_iCurrKey && m_iCurrKey < 7 )  // u - z
                {
                    m_iLastColumn = m_iCurrKey;         // remember column
                    m_iCurrKey = 1;                     // move to spacebar
                }
                else if( m_iCurrKey > 6 )               // backspace
                {
                    if( m_iLastColumn > 8 )
                        m_iCurrKey = 3;                 // move to right arrow
                    else
                        m_iCurrKey = 2;                 // move to left arrow
                }
                break;
            case 4:
                switch( m_iCurrKey )
                {
                    case 1:                             // spacebar
                        m_iCurrKey = min( 6, m_iLastColumn );
                        break;
                    case 2:                             // left arrow
                        m_iCurrKey = max( min( 8, m_iLastColumn ), 7 );
                        break;
                    case 3:                             // right arrow
                        m_iCurrKey = max( 9, m_iLastColumn );
                        break;
                }
                break;
        }

        // Update row
        m_iCurrRow = ( m_iCurrRow == m_dwMaxRows - 1 ) ? 0 : m_iCurrRow + 1;

    } while( IsKeyDisabled() );
}




//-----------------------------------------------------------------------------
// Name: MoveLeft()
// Desc: Move the cursor left
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::MoveLeft()
{
    do
    {
        if( m_iCurrKey == 0 )
            m_iCurrKey = m_KeyboardList[ m_iCurrBoard ][ m_iCurrRow ].size() - 1;
        else
            --m_iCurrKey;

    } while( IsKeyDisabled() );

    SetLastColumn();
}




//-----------------------------------------------------------------------------
// Name: MoveRight()
// Desc: Move the cursor right
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::MoveRight()
{
    do
    {
        if( m_iCurrKey == m_KeyboardList[ m_iCurrBoard ][ m_iCurrRow ].size() - 1 )
            m_iCurrKey = 0;
        else
            ++m_iCurrKey;

    } while( IsKeyDisabled() );

    SetLastColumn();
}




//-----------------------------------------------------------------------------
// Name: SetLastColumn()
// Desc: Remember the column position if we're on a single letter character
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::SetLastColumn()
{
    // If the new key is a single character, remember it for later
    Key key = m_KeyboardList[ m_iCurrBoard ][ m_iCurrRow ][ m_iCurrKey ];
    if( key.strName.empty() )
    {
        switch( key.xKey )
        {
            // Adjust the last column for the arrow keys to confine it
            // within the range of the key width
            case XK_ARROWLEFT:
                m_iLastColumn = ( m_iLastColumn <= 7 ) ? 7 : 8; break;
            case XK_ARROWRIGHT:
                m_iLastColumn = ( m_iLastColumn <= 9 ) ? 9 : 10; break;

            // Single char, non-arrow
            default:
                m_iLastColumn = m_iCurrKey; break;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: RenderKey()
// Desc: Render the key at the given position
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::RenderKey( FLOAT fX, FLOAT fY, const Key& key, 
                                    D3DCOLOR KeyColor, 
                                    D3DCOLOR TextColor ) const
{
    if( KeyColor == COLOR_INVISIBLE || key.xKey == XK_NULL )
        return;

    struct KEYVERTEX
    {
        D3DXVECTOR4 p;
        D3DXVECTOR2 t;
    };

    WCHAR strKey[2] = { GetChar( key.xKey ), 0 };
    const WCHAR* strName = key.strName.empty() ? strKey : key.strName.c_str();

    FLOAT x = fX + KEY_INSET;
    FLOAT y = fY + KEY_INSET;
    FLOAT z = fX + key.dwWidth - KEY_INSET + 2;
    FLOAT w = fY + m_fKeyHeight - KEY_INSET + 2;

    KEYVERTEX pVertices[16];
    pVertices[0].p  = D3DXVECTOR4( x-0.5f, y-0.5f, 1.0f, 1.0f );     pVertices[0].t  = D3DXVECTOR2( 0.0f, 0.0f );
    pVertices[1].p  = D3DXVECTOR4( x+17-0.5f, y-0.5f, 1.0f, 1.0f );  pVertices[1].t  = D3DXVECTOR2( 0.5f, 0.0f );
    pVertices[2].p  = D3DXVECTOR4( x+17-0.5f, w-0.5f, 1.0f, 1.0f );  pVertices[2].t  = D3DXVECTOR2( 0.5f, 1.0f );
    pVertices[3].p  = D3DXVECTOR4( x-0.5f, w-0.5f, 1.0f, 1.0f );     pVertices[3].t  = D3DXVECTOR2( 0.0f, 1.0f );

    pVertices[4].p  = D3DXVECTOR4( x+17-0.5f, y-0.5f, 1.0f, 1.0f );  pVertices[4].t  = D3DXVECTOR2( 0.5f, 0.0f );
    pVertices[5].p  = D3DXVECTOR4( z-17-0.5f, y-0.5f, 1.0f, 1.0f );  pVertices[5].t  = D3DXVECTOR2( 0.5f, 0.0f );
    pVertices[6].p  = D3DXVECTOR4( z-17-0.5f, w-0.5f, 1.0f, 1.0f );  pVertices[6].t  = D3DXVECTOR2( 0.5f, 1.0f );
    pVertices[7].p  = D3DXVECTOR4( x+17-0.5f, w-0.5f, 1.0f, 1.0f );  pVertices[7].t  = D3DXVECTOR2( 0.5f, 1.0f );

    pVertices[8].p  = D3DXVECTOR4( z-17-0.5f, y-0.5f, 1.0f, 1.0f );  pVertices[8].t  = D3DXVECTOR2( 0.5f, 0.0f );
    pVertices[9].p  = D3DXVECTOR4( z-0.5f, y-0.5f, 1.0f, 1.0f );     pVertices[9].t  = D3DXVECTOR2( 1.0f, 0.0f );
    pVertices[10].p = D3DXVECTOR4( z-0.5f, w-0.5f, 1.0f, 1.0f );     pVertices[10].t = D3DXVECTOR2( 1.0f, 1.0f );
    pVertices[11].p = D3DXVECTOR4( z-17-0.5f, w-0.5f, 1.0f, 1.0f );  pVertices[11].t = D3DXVECTOR2( 0.5f, 1.0f );

    pVertices[12].p  = D3DXVECTOR4( x-0.5f, y-0.5f, 1.0f, 1.0f );
    pVertices[13].p  = D3DXVECTOR4( z-0.5f, y-0.5f, 1.0f, 1.0f );
    pVertices[14].p  = D3DXVECTOR4( z-0.5f, w-0.5f, 1.0f, 1.0f );
    pVertices[15].p  = D3DXVECTOR4( x-0.5f, w-0.5f, 1.0f, 1.0f );


    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW | D3DFVF_TEX1 );

    // Draw the key background
    m_pd3dDevice->SetTexture( 0, m_pKeyTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR );
    
    if( KeyColor )
        m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, KeyColor );
    else
        m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xffffffff );

    m_pd3dDevice->DrawVerticesUP( D3DPT_QUADLIST, 12, pVertices, sizeof(KEYVERTEX) );

    // Draw the key text. If key name is, use a slightly smaller font.
    if( key.strName.length() > 1 && iswupper( key.strName[1] ) )
    {
        m_Font12.DrawText( ( x + z ) / 2.0f, ( y + w ) / 2.0f , TextColor, strName, 
                           XBFONT_CENTER_X | XBFONT_CENTER_Y );
    }
    else
    {
        m_Font18.DrawText( ( x + z ) / 2.0f, ( y + w ) / 2.0f , TextColor, strName, 
                           XBFONT_CENTER_X | XBFONT_CENTER_Y );
    }

}




//-----------------------------------------------------------------------------
// Name: DrawTextBox()
// Desc: Display box containing text input
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::DrawTextBox() const
{
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW );

    D3DXVECTOR4 avRect[4];
    avRect[0] = D3DXVECTOR4(  64 - 0.5f, 48 - 0.5f, 1.0f, 1.0f );
    avRect[1] = D3DXVECTOR4( 576 - 0.5f, 48 - 0.5f, 1.0f, 1.0f );
    avRect[2] = D3DXVECTOR4( 576 - 0.5f, 88 - 0.5f, 1.0f, 1.0f );
    avRect[3] = D3DXVECTOR4(  64 - 0.5f, 88 - 0.5f, 1.0f, 1.0f );

    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xffe0e0e0 );
    m_pd3dDevice->DrawVerticesUP( D3DPT_QUADLIST, 4, avRect, sizeof(D3DXVECTOR4) );
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xff101010 );
    m_pd3dDevice->DrawVerticesUP( D3DPT_LINELOOP, 4, avRect, sizeof(D3DXVECTOR4) );
}




//-----------------------------------------------------------------------------
// Name: RenderStartScreen()
// Desc: Startup screen
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::RenderStartScreen() const
{
    m_Font18.DrawText( 320.0f, 50.0f, 0xffffffff,
                       L"This sample is intended to show appropriate\n"
                       L"functionality only. Please do not lift the\n"
                       L"graphics for use in your game.\n"
                       L"Source code for this sample is located at\n"
                       L"Samples\\Xbox\\ReferenceUI\\VirtualKeyboard.\n"
                       L"A description of the user research that\n"
                       L"went into the creation of this sample is\n"
                       L"located in the XDK documentation at\n"
                       L"Developing for Xbox - Reference User Interface.",
                       XBFONT_CENTER_X );

    m_FontButtons.DrawText( 270.0f, BUTTON_Y_POS, 0xffffffff, TEXT_A_BUTTON );
    m_Font18.DrawText( 270.0f + BUTTON_X_OFFSET, BUTTON_Y_POS, BUTTONTEXT_COLOR, 
                       L"Continue" );
}




//-----------------------------------------------------------------------------
// Name: RenderMenu()
// Desc: Main menu
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::RenderMenu() const
{
    m_Font18.DrawText( 320.0f, 36.0f, 0xffeaeaea, 
                       L"Virtual Keyboard Sample", XBFONT_CENTER_X );

    m_Font18.DrawText( 320.0f, 90.0f, 0xffeaeaea, 
                       GetString(STR_MENU_CHOOSE_KEYBOARD), XBFONT_CENTER_X );
    for( UINT i = 0; i < KEYBOARD_MAX; ++i )
    {
        D3DCOLOR selColor = 0xffaaaaaa;
        if( m_iKeyboard == i )
            selColor = 0xff10ea10;

        m_Font18.DrawText( 320.0f, 120.0f + ( i * 25.0f ), selColor,
                           GetString( g_aKeyboardInfo[i].GetStringTable(), STR_MENU_KEYBOARD_NAME ),
                           XBFONT_CENTER_X );
    }

    m_Font12.DrawText( 320.0f, 340.0f, 0xffeaeaea, 
                       L"Localized terminology not final.\n"
                       L"See Xbox Guide for final terms.",
                       XBFONT_CENTER_X );

    m_Font12.DrawText( 320.0f, 378.0f, 0xffeaeaea, 
                       GetString( g_aKeyboardInfo[ m_iKeyboard ].GetStringTable(), STR_MENU_ILLUSTRATIVE_GRAPHICS ),
                       XBFONT_CENTER_X );

    DrawButton( 270.0f, TEXT_A_BUTTON );
}




//-----------------------------------------------------------------------------
// Name: RenderKeyboard()
// Desc: Display current keyboard
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::RenderKeyboard() const
{
    switch( m_iKeyboard )
    {
        case KEYBOARD_JAPANESE1:
            RenderKeyboardJapan();
            break;
        case KEYBOARD_JAPANESE2:
            RenderKeyboardJapanese2();
            break;
        default:
            RenderKeyboardLatin();
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: RenderKeyboardLatin()
// Desc: Display current latin keyboard
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::RenderKeyboardLatin() const
{
    // Show text and caret
    DrawTextBox();
    DrawText( 68.0f, 54.0f );

    // Draw each row
    FLOAT fY = 120.0f;
    const Keyboard& keyBoard = m_KeyboardList[ m_iCurrBoard ];
    for( DWORD row = 0; row < m_dwMaxRows; ++row, fY += m_fKeyHeight )
    {
        FLOAT fX = 64.0f;
        FLOAT fWidthSum = 0.0f;
        const KeyRow& keyRow = keyBoard[ row ];
        DWORD dwIndex = 0;
        for( KeyRow::const_iterator i = keyRow.begin(); i != keyRow.end(); ++i, ++dwIndex )
        {
            // Determine key name
            const Key& key = *i;
            D3DCOLOR selKeyColor = 0x00000000;
            D3DCOLOR selTextColor = COLOR_NORMAL;

            // Handle special key coloring
            switch( key.xKey )
            {
                case XK_SHIFT:
                    switch( m_iCurrBoard )
                    {
                        case TYPE_ALPHABET:
                        case TYPE_ACCENTS:
                            if( m_bIsShiftOn )
                                selKeyColor = COLOR_PRESSED;
                            break;
                        case TYPE_SYMBOLS:
                            selKeyColor = COLOR_DISABLED;
                            selTextColor = COLOR_FONT_DISABLED;
                            break;
                    }
                    break;
                case XK_CAPSLOCK:
                    switch( m_iCurrBoard )
                    {
                        case TYPE_ALPHABET:
                        case TYPE_ACCENTS:
                            if( m_bIsCapsLockOn )
                                selKeyColor = COLOR_PRESSED;
                            break;
                        case TYPE_SYMBOLS:
                            selKeyColor = COLOR_DISABLED;
                            selTextColor = COLOR_FONT_DISABLED;
                            break;
                    }
                    break;
                case XK_ACCENTS:
                    if( m_iKeyboard == KEYBOARD_ENGLISH )
                    {
                        selKeyColor = COLOR_INVISIBLE;
                        selTextColor = COLOR_INVISIBLE;
                    }
                    break;
            }

            // Highlight the current key
            if( row == m_iCurrRow && dwIndex == m_iCurrKey )
                selKeyColor |= COLOR_HIGHLIGHT;

            RenderKey( fX + fWidthSum, fY, key, selKeyColor, selTextColor );

            fWidthSum += key.dwWidth;

            // There's a slightly larger gap between the leftmost keys (mode
            // keys) and the main keyboard
            if( dwIndex == 0 )
                fWidthSum += GAP2_WIDTH;
            else
                fWidthSum += GAP_WIDTH;
        }
    }

    DrawButton(  80.0f, TEXT_A_BUTTON );
    DrawButton( 280.0f, TEXT_Y_BUTTON );
    DrawButton( 460.0f, TEXT_B_BUTTON );
}




//-----------------------------------------------------------------------------
// Name: RenderKeyboardJapan()
// Desc: Display current Japanese keyboard
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::RenderKeyboardJapan() const
{
    assert( m_iKeyboard == KEYBOARD_JAPANESE1 );

    // Show the controller
    DrawControllerS();

    // Show text and caret
    DrawTextBox();
    DrawText( 68.0f, 54.0f );

    const WCHAR *cChar = JH_A JH_KA JH_SA JH_TA JH_NA JH_HA JH_MA JH_YA
                         JH_LA JH_GA JH_ZA JH_DA JH_PA JH_BA JH_RA JH_WA
                         JK_A JK_KA JK_SA JK_TA JK_NA JK_HA JK_MA JK_YA
                         JK_LA JK_GA JK_ZA JK_DA JK_PA JK_BA JK_RA JK_WA;

    const WCHAR *cCharX = JH_A JH_I JH_U JH_E JH_O JH_NULL JH_KA JH_KI JH_KU 
                          JH_KE JH_KO JH_NULL JH_SA JH_SI JH_SU JH_SE JH_SO 
                          JH_NULL JH_TA JH_TI JH_TU JH_TE JH_TO JH_NULL JH_NA 
                          JH_NI JH_NU JH_NE JH_NO JH_NULL JH_HA JH_HI JH_HU 
                          JH_HE JH_HO JH_NULL JH_MA JH_MI JH_MU JH_ME JH_MO 
                          JH_NULL JH_YA JH_LYA JH_YU JH_LYU JH_YO JH_LYO JH_N 
                          JH_LTU JH_WO JH_NULL JH_NULL JH_NULL JH_LA JH_LI 
                          JH_LU JH_LE JH_LO JH_NULL JH_GA JH_GI JH_GU JH_GE 
                          JH_GO JH_NULL JH_ZA JH_ZI JH_ZU JH_ZE JH_ZO JH_NULL 
                          JH_DA JH_DI JH_DU JH_DE JH_DO JH_NULL JH_PA JH_PI 
                          JH_PU JH_PE JH_PO JH_NULL JH_BA JH_BI JH_BU JH_BE 
                          JH_BO JH_NULL JH_RA JH_RI JH_RU JH_RE JH_RO JH_NULL 
                          JH_WA JH_WI JH_LWA JH_WE JH_WO JH_NULL JH_N JH_LTU 
                          JH_WO JH_NULL JH_NULL JH_NULL JK_A JK_I JK_U JK_E 
                          JK_O JK_NULL JK_KA JK_KI JK_KU JK_KE JK_KO JK_NULL
                          JK_SA JK_SI JK_SU JK_SE JK_SO JK_NULL JK_TA JK_TI 
                          JK_TU JK_TE JK_TO JK_NULL JK_NA JK_NI JK_NU JK_NE 
                          JK_NO JK_NULL JK_HA JK_HI JK_HU JK_HE JK_HO JK_NULL
                          JK_MA JK_MI JK_MU JK_ME JK_MO JK_NULL JK_YA JK_LYA
                          JK_YU JK_LYU JK_YO JK_LYO JK_N JK_LTU JK_WO JK_DASH
                          JK_NULL JK_NULL JK_LA JK_LI JK_LU JK_LE JK_LO JK_NULL
                          JK_GA JK_GI JK_GU JK_GE JK_GO JK_NULL JK_ZA JK_ZI 
                          JK_ZU JK_ZE JK_ZO JK_NULL JK_DA JK_DI JK_DU JK_DE 
                          JK_DO JK_NULL JK_PA JK_PI JK_PU JK_PE JK_PO JK_NULL
                          JK_BA JK_BI JK_BU JK_BE JK_BO JK_NULL JK_RA JK_RI 
                          JK_RU JK_RE JK_RO JK_NULL JK_WA JK_WI JK_VU JK_WE
                          JK_WO JK_NULL JK_N JK_LTU JK_WO JK_DASH JK_NULL
                          JK_NULL;

    const WCHAR *strHiragana = JH_HI JH_RA JH_GA JH_NA;
    const WCHAR *strKatakana = JK_KA JK_TA JK_KA JK_NA;

    const WCHAR *strDESCRIPTION[8] =
    {
        { JK_DASH JH_SO JH_U JH_SA JH_SE JH_TU JH_ME JH_I JK_DASH },
        { JH_HI JH_DA JH_RI JH_NO JK_A JK_NA JK_RO JK_GU JK_PA JK_LTU JK_DO 
          JH_DE JH_SI JH_I JH_N JH_WO JH_KI JH_ME JH_TE L"\n"
          JH_MI JH_GI JH_NO JK_BO JK_TA JK_N JH_DE JH_MO JH_ZI JH_WO JH_U 
          JH_TI JH_MA JH_SU },
        { JH_U JH_RA JH_GA JH_WA JH_NI JH_A JH_RU JH_MI JH_GI JH_NO JK_TO JK_RI JK_GA JH_DE JH_SI JH_I 
          JH_N JH_WO }, 
        { JH_HI JH_DA JH_RI JH_NO JK_TO JK_RI JK_GA JH_DE JH_HI JH_RA JH_GA
          JH_NA JH_TO JK_KA JK_TA JK_KA JK_NA JH_WO L"\n"
          JH_KO JH_U JH_GO JH_NI JH_KI JH_RI JH_KA JH_E JH_RU JH_KO JH_TO 
          JH_GA JH_DE JH_KI JH_MA JH_SU },
        { JK_DE JK_ZI JK_TA JK_RU JK_PA JK_LTU JK_DO JH_NO JH_SA JH_YU JH_U 
          JH_NO JK_KI JK_DASH JH_DE L"\n"
          JK_KA JK_DASH JK_SO JK_RU JH_GA JH_I JH_DO JH_U JH_SI JH_MA JH_SU },
        { JH_MO JH_SI JH_U JH_TI JH_MA JH_TI JH_GA JH_E JH_TA JH_RA L"\n"
          JH_KU JH_RO JH_KA JH_SI JH_RO JH_NO JK_BO JK_TA JK_N JH_DE JH_MO 
          JH_ZI JH_WO JH_SA JH_KU JH_ZI JH_LYO JH_SI JH_TE JH_KU JH_DA JH_SA JH_I },
        { JK_BA JK_LTU JK_KU JK_BO JK_TA JK_N JH_WO JH_O JH_SU JH_TO L"\n"
          JK_KI JK_DASH JK_BO JK_DASH JK_DO JK_SE JK_RE JK_KU JK_TO JH_NO 
          JK_ME JK_NI JK_LYU JK_DASH JH_NI JH_MO JH_DO JH_RI JH_MA JH_SU },
        { L"" }
    };

    // Determine color and direction values
    DWORD dwColor[16];
    for( INT i = 0; i < 16; i++ )
        dwColor[i] = 0xffffffff;

    INT nDirection; // 0 - 8
    if( m_dwCurrCtlrState & XKJ_UP )
    {
        if( m_dwCurrCtlrState & XKJ_RIGHT )
        {
            nDirection = 1;
            dwColor[1] = 0xffffff00;
        }
        else if( m_dwCurrCtlrState & XKJ_LEFT )
        {
            nDirection = 7;
            dwColor[7] = 0xffffff00;
        }
        else
        {
            nDirection = 0;
            dwColor[0] = 0xffffff00;
        }
    }
    else if( m_dwCurrCtlrState & XKJ_DOWN )
    {
        if( m_dwCurrCtlrState & XKJ_RIGHT )
        {
            nDirection = 3;
            dwColor[3] = 0xffffff00;
        }
        else if( m_dwCurrCtlrState & XKJ_LEFT )
        {
            nDirection = 5;
            dwColor[5] = 0xffffff00;
        }
        else
        {
            nDirection = 4;
            dwColor[4] = 0xffffff00;
        }
    }
    else if( m_dwCurrCtlrState & XKJ_RIGHT )
    {
        nDirection = 2;
        dwColor[2] = 0xffffff00;
    }
    else if( m_dwCurrCtlrState & XKJ_LEFT )
    {
        nDirection = 6;
        dwColor[6] = 0xffffff00;
    }
    else 
    {
        nDirection = 8;
    }

    if( m_dwOldCtlrState & XKJ_X )
        dwColor[8] = 0xffffff00;
    if( m_dwOldCtlrState & XKJ_Y )
        dwColor[9] = 0xffffff00;
    if( m_dwOldCtlrState & XKJ_B )
        dwColor[10] = 0xffffff00;
    if( m_dwOldCtlrState & XKJ_A )
        dwColor[11] = 0xffffff00;
    if( m_dwOldCtlrState & XKJ_WHITE )
        dwColor[12] = 0xffffff00;
    if( m_dwOldCtlrState & XKJ_BLACK )
        dwColor[13] = 0xffffff00;
    if( m_bTrig )
        dwColor[14] = 0xffffff00;
    if( m_dwOldCtlrState & XKJ_LEFTTR )
        dwColor[15] = 0xffffff00;

    WCHAR wcChar[8][2];
    for( i = 0; i < 8; i++ )
    {
        INT nStp = m_bTrig ? 1 : 0;
        if( m_bKana )
            nStp += 2;

        wcChar[i][0] = cChar[i + 8 * nStp];
        wcChar[i][1] = 0;
    }

    WCHAR wcCharX[6][2];
    DWORD wcCharXIdx[6];
    for( i = 0; i < 6; i++ )
    {
        INT nStp = m_bTrig ? ( 6 * 9 ) : 0;
        if( m_bKana ) 
            nStp += 6 * 9 * 2;

        wcCharXIdx[i] = i + 6 * nDirection + nStp;
        wcCharX[i][0] = cCharX[wcCharXIdx[i]];
        wcCharX[i][1] = 0;
    }

    // Japanese characters
    const Xkey cJAPAN_KEY[] =
    {
        XK_HIRAGANA_A,XK_HIRAGANA_I,XK_HIRAGANA_U,XK_HIRAGANA_E,XK_HIRAGANA_O,XK_DELETE,
        XK_HIRAGANA_KA,XK_HIRAGANA_KI,XK_HIRAGANA_KU,XK_HIRAGANA_KE,XK_HIRAGANA_KO,XK_DELETE,
        XK_HIRAGANA_SA,XK_HIRAGANA_SI,XK_HIRAGANA_SU,XK_HIRAGANA_SE,XK_HIRAGANA_SO,XK_DELETE,
        XK_HIRAGANA_TA,XK_HIRAGANA_TI,XK_HIRAGANA_TU,XK_HIRAGANA_TE,XK_HIRAGANA_TO,XK_DELETE,
        XK_HIRAGANA_NA,XK_HIRAGANA_NI,XK_HIRAGANA_NU,XK_HIRAGANA_NE,XK_HIRAGANA_NO,XK_DELETE,
        XK_HIRAGANA_HA,XK_HIRAGANA_HI,XK_HIRAGANA_HU,XK_HIRAGANA_HE,XK_HIRAGANA_HO,XK_DELETE,
        XK_HIRAGANA_MA,XK_HIRAGANA_MI,XK_HIRAGANA_MU,XK_HIRAGANA_ME,XK_HIRAGANA_MO,XK_DELETE,
        XK_HIRAGANA_YA,XK_HIRAGANA_LYA,XK_HIRAGANA_YU,XK_HIRAGANA_LYU,XK_HIRAGANA_YO,XK_HIRAGANA_LYO,
        XK_HIRAGANA_N,XK_HIRAGANA_LTU,XK_HIRAGANA_WO,XK_SHIFT,XK_BACKSPACE,XK_DELETE,

        XK_HIRAGANA_LA,XK_HIRAGANA_LI,XK_HIRAGANA_LU,XK_HIRAGANA_LE,XK_HIRAGANA_LO,XK_DELETE,
        XK_HIRAGANA_GA,XK_HIRAGANA_GI,XK_HIRAGANA_GU,XK_HIRAGANA_GE,XK_HIRAGANA_GO,XK_DELETE,
        XK_HIRAGANA_ZA,XK_HIRAGANA_ZI,XK_HIRAGANA_ZU,XK_HIRAGANA_ZE,XK_HIRAGANA_ZO,XK_DELETE,
        XK_HIRAGANA_DA,XK_HIRAGANA_DI,XK_HIRAGANA_DU,XK_HIRAGANA_DE,XK_HIRAGANA_DO,XK_DELETE,
        XK_HIRAGANA_PA,XK_HIRAGANA_PI,XK_HIRAGANA_PU,XK_HIRAGANA_PE,XK_HIRAGANA_PO,XK_DELETE,
        XK_HIRAGANA_BA,XK_HIRAGANA_BI,XK_HIRAGANA_BU,XK_HIRAGANA_BE,XK_HIRAGANA_BO,XK_DELETE,
        XK_HIRAGANA_RA,XK_HIRAGANA_RI,XK_HIRAGANA_RU,XK_HIRAGANA_RE,XK_HIRAGANA_RO,XK_DELETE,
        XK_HIRAGANA_WA,XK_HIRAGANA_WI,XK_HIRAGANA_LWA,XK_HIRAGANA_WE,XK_HIRAGANA_WO,XK_DELETE,
        XK_HIRAGANA_N,XK_HIRAGANA_LTU,XK_HIRAGANA_WO,XK_SHIFT,XK_BACKSPACE,XK_DELETE,

        XK_KATAKANA_A,XK_KATAKANA_I,XK_KATAKANA_U,XK_KATAKANA_E,XK_KATAKANA_O,XK_DELETE,
        XK_KATAKANA_KA,XK_KATAKANA_KI,XK_KATAKANA_KU,XK_KATAKANA_KE,XK_KATAKANA_KO,XK_DELETE,
        XK_KATAKANA_SA,XK_KATAKANA_SI,XK_KATAKANA_SU,XK_KATAKANA_SE,XK_KATAKANA_SO,XK_DELETE,
        XK_KATAKANA_TA,XK_KATAKANA_TI,XK_KATAKANA_TU,XK_KATAKANA_TE,XK_KATAKANA_TO,XK_DELETE,
        XK_KATAKANA_NA,XK_KATAKANA_NI,XK_KATAKANA_NU,XK_KATAKANA_NE,XK_KATAKANA_NO,XK_DELETE,
        XK_KATAKANA_HA,XK_KATAKANA_HI,XK_KATAKANA_HU,XK_KATAKANA_HE,XK_KATAKANA_HO,XK_DELETE,
        XK_KATAKANA_MA,XK_KATAKANA_MI,XK_KATAKANA_MU,XK_KATAKANA_ME,XK_KATAKANA_MO,XK_DELETE,
        XK_KATAKANA_YA,XK_KATAKANA_LYA,XK_KATAKANA_YU,XK_KATAKANA_LYU,XK_KATAKANA_YO,XK_KATAKANA_LYO,
        XK_KATAKANA_N,XK_KATAKANA_LTU,XK_KATAKANA_WO,XK_KATAKANA_DASH,XK_BACKSPACE,XK_DELETE,

        XK_KATAKANA_LA,XK_KATAKANA_LI,XK_KATAKANA_LU,XK_KATAKANA_LE,XK_KATAKANA_LO,XK_DELETE,
        XK_KATAKANA_GA,XK_KATAKANA_GI,XK_KATAKANA_GU,XK_KATAKANA_GE,XK_KATAKANA_GO,XK_DELETE,
        XK_KATAKANA_ZA,XK_KATAKANA_ZI,XK_KATAKANA_ZU,XK_KATAKANA_ZE,XK_KATAKANA_ZO,XK_DELETE,
        XK_KATAKANA_DA,XK_KATAKANA_DI,XK_KATAKANA_DU,XK_KATAKANA_DE,XK_KATAKANA_DO,XK_DELETE,
        XK_KATAKANA_PA,XK_KATAKANA_PI,XK_KATAKANA_PU,XK_KATAKANA_PE,XK_KATAKANA_PO,XK_DELETE,
        XK_KATAKANA_BA,XK_KATAKANA_BI,XK_KATAKANA_BU,XK_KATAKANA_BE,XK_KATAKANA_BO,XK_DELETE,
        XK_KATAKANA_RA,XK_KATAKANA_RI,XK_KATAKANA_RU,XK_KATAKANA_RE,XK_KATAKANA_RO,XK_DELETE,
        XK_KATAKANA_WA,XK_KATAKANA_WI,XK_KATAKANA_VU,XK_KATAKANA_WE,XK_KATAKANA_WO,XK_DELETE,
        XK_KATAKANA_N,XK_KATAKANA_LTU,XK_KATAKANA_WO,XK_KATAKANA_DASH,XK_BACKSPACE,XK_DELETE,
    };

    // Determine the key to press
    m_xNextKeyJpn = XK_NULL;
    if( m_dwCurrCtlrState & XKJ_X )           m_xNextKeyJpn = cJAPAN_KEY[wcCharXIdx[0]];
    else if( m_dwCurrCtlrState & XKJ_Y )      m_xNextKeyJpn = cJAPAN_KEY[wcCharXIdx[1]];
    else if( m_dwCurrCtlrState & XKJ_B )      m_xNextKeyJpn = cJAPAN_KEY[wcCharXIdx[2]];
    else if( m_dwCurrCtlrState & XKJ_A )      m_xNextKeyJpn = cJAPAN_KEY[wcCharXIdx[3]];
    else if( m_dwCurrCtlrState & XKJ_WHITE )  m_xNextKeyJpn = cJAPAN_KEY[wcCharXIdx[4]];
    else if( m_dwCurrCtlrState & XKJ_BLACK )  m_xNextKeyJpn = cJAPAN_KEY[wcCharXIdx[5]];
    else if( m_dwCurrCtlrState & XKJ_DLEFT )  m_xNextKeyJpn = XK_ARROWLEFT;
    else if( m_dwCurrCtlrState & XKJ_DRIGHT ) m_xNextKeyJpn = XK_ARROWRIGHT;

    DWORD dwFontFlags = XBFONT_CENTER_X | XBFONT_CENTER_Y;
    DrawRedUnderlinedText( 230.0f, 142.0f, dwColor[0],  wcChar[0],  dwFontFlags );
    DrawRedUnderlinedText( 252.0f, 151.0f, dwColor[1],  wcChar[1],  dwFontFlags );
    DrawRedUnderlinedText( 268.0f, 169.0f, dwColor[2],  wcChar[2],  dwFontFlags );
    DrawRedUnderlinedText( 261.0f, 193.0f, dwColor[3],  wcChar[3],  dwFontFlags );
    DrawRedUnderlinedText( 239.0f, 206.0f, dwColor[4],  wcChar[4],  dwFontFlags );
    DrawRedUnderlinedText( 217.0f, 199.0f, dwColor[5],  wcChar[5],  dwFontFlags );
    DrawRedUnderlinedText( 200.0f, 178.0f, dwColor[6],  wcChar[6],  dwFontFlags );
    DrawRedUnderlinedText( 208.0f, 156.0f, dwColor[7],  wcChar[7],  dwFontFlags );
    DrawRedUnderlinedText( 406.0f, 194.0f, dwColor[8],  wcCharX[0], dwFontFlags );
    DrawRedUnderlinedText( 424.0f, 175.0f, dwColor[9],  wcCharX[1], dwFontFlags );
    DrawRedUnderlinedText( 454.0f, 205.0f, dwColor[10], wcCharX[2], dwFontFlags );
    DrawRedUnderlinedText( 435.0f, 226.0f, dwColor[11], wcCharX[3], dwFontFlags );
    DrawRedUnderlinedText( 449.0f, 272.0f, dwColor[12], wcCharX[4], dwFontFlags );
    DrawRedUnderlinedText( 469.0f, 254.0f, dwColor[13], wcCharX[5], dwFontFlags );
    DrawRedUnderlinedText( 140.0f, 238.0f, 0xff00ffff, JK_ME JK_NI JK_LYU JK_DASH, dwFontFlags );

    // Kana or Hiragana
    const WCHAR *strKanaGana = m_bKana ? strKatakana : strHiragana;
    DrawRedUnderlinedText( 135.0f, 145.0f, dwColor[15], strKanaGana, dwFontFlags );

    // Show description string
    static INT nCounter = 0;
    ++nCounter &= 0x00000fff;
    INT nDescript = (nCounter >> 9) & 7;
    DrawRedUnderlinedText( 320.0f, 410.0f, 0xffffffff, strDESCRIPTION[ nDescript ], 
                           dwFontFlags );
}




//-----------------------------------------------------------------------------
// Name: RenderKeyboardJapanese2()
// Desc: Display current Japanese keyboard (keyboard type)
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::RenderKeyboardJapanese2() const
{
    // Show text and caret
    DrawTextBox();
    DrawText( 68.0f, 54.0f );

    // Draw each row
    FLOAT fY = 100.0f;
    const Keyboard& keyBoard = m_KeyboardList[ m_iCurrBoard ];
    for( DWORD row = 0; row < keyBoard.size(); ++row, fY += m_fKeyHeight )
    {
        FLOAT fX = 64.0f;
        FLOAT fWidthSum = 0.0f;
        const KeyRow& keyRow = keyBoard[ row ];
        DWORD dwIndex = 0;
        for( KeyRow::const_iterator i = keyRow.begin(); 
             i != keyRow.end(); ++i, ++dwIndex )
        {
            // Determine key name
            const Key& key = *i;
            D3DCOLOR selKeyColor = 0x00000000;
            D3DCOLOR selTextColor = COLOR_NORMAL;

            // Highlight the current key
            if( row == m_iCurrRow && dwIndex == m_iCurrKey )
                selKeyColor |= COLOR_HIGHLIGHT;

            RenderKey( fX + fWidthSum, fY, key, selKeyColor, selTextColor );

            fWidthSum += key.dwWidth;

            // There are a slightly larger gaps
            if( dwIndex == 4 || dwIndex == 9 )
                fWidthSum += GAP2_WIDTH;
            else
                fWidthSum += GAP_WIDTH;
        }
    }

    DrawButton(  80.0f, TEXT_A_BUTTON );
    DrawButton( 280.0f, TEXT_Y_BUTTON );
    DrawButton( 460.0f, TEXT_B_BUTTON );
}




//-----------------------------------------------------------------------------
// Name: RenderHelp()
// Desc: Display controller mappings
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::RenderHelp() const
{
//    RenderKeyboard();

    const DWORD NUM_HELP_CALLOUTS = 9;
    const WORD PLC1 = XBHELP_PLACEMENT_1;
    const WORD PLC2 = XBHELP_PLACEMENT_2;
    XBHELP_CALLOUT HelpCallouts[] =
    {
        { XBHELP_A_BUTTON,     PLC2, (WCHAR*)GetString( STR_HELP_SELECT )    },
        { XBHELP_START_BUTTON, PLC2, (WCHAR*)GetString( STR_HELP_SELECT )    },
        { XBHELP_B_BUTTON,     PLC2, (WCHAR*)GetString( STR_HELP_CANCEL )    },
        { XBHELP_BACK_BUTTON,  PLC2, (WCHAR*)GetString( STR_HELP_CANCEL )    },
        { XBHELP_X_BUTTON,     PLC2, (WCHAR*)GetString( STR_HELP_TOGGLE )    },
        { XBHELP_Y_BUTTON,     PLC1, (WCHAR*)GetString( STR_HELP_HELP )      },
        { XBHELP_WHITE_BUTTON, PLC1, (WCHAR*)GetString( STR_HELP_BACKSPACE ) },
        { XBHELP_BLACK_BUTTON, PLC2, (WCHAR*)GetString( STR_HELP_SPACE )     },
        { XBHELP_MISC_CALLOUT, PLC2, (WCHAR*)GetString( STR_HELP_TRIGGER )   }
    };

    m_Help.Render( &m_Font18, HelpCallouts, NUM_HELP_CALLOUTS );
}




//-----------------------------------------------------------------------------
// Name: DrawText()
// Desc: Draw text in the text input area, accounting for special characters
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::DrawText( FLOAT x, FLOAT y ) const
{
    m_Font18.DrawText( x, y, 0xff10e010, m_strData.c_str() );

    // Draw blinking caret using line primitives.
    if( fmod( m_CaretTimer.GetElapsedSeconds(), fCARET_BLINK_RATE ) < fCARET_ON_RATIO )
    {
        if ( m_iPos )
        {
            String string;
            string.assign( m_strData.c_str(), m_iPos );

            FLOAT fCaretWidth = 0.0f;
            FLOAT fCaretHeight;
            m_Font18.GetTextExtent( string.c_str(), &fCaretWidth, &fCaretHeight );
            x += fCaretWidth;
        }

        m_pd3dDevice->SetTexture( 0, NULL );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
        m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xff101010 );

        m_pd3dDevice->Begin( D3DPT_LINELIST );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x-2, y+2,  1.0f, 1.0f );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x+2, y+2,  1.0f, 1.0f );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x-1, y+2,  1.0f, 1.0f );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x-1, y+25, 1.0f, 1.0f );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX,  x,  y+2,  1.0f, 1.0f );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX,  x,  y+25, 1.0f, 1.0f );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x-2, y+25, 1.0f, 1.0f );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, x+2, y+25, 1.0f, 1.0f );
        m_pd3dDevice->End();
    }
}




//-----------------------------------------------------------------------------
// Name: DrawRedUnderlinedText()
// Desc: Draw text with a red drop shadow
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::DrawRedUnderlinedText( FLOAT x, FLOAT y, DWORD dwColor, 
                                                const WCHAR* str, DWORD dwFlags ) const
{
    m_Font18.DrawText( x+2, y+2, COLOR_RED, str, dwFlags );
    m_Font18.DrawText( x+0, y+0,   dwColor, str, dwFlags );
}




//-----------------------------------------------------------------------------
// Name: DrawButton()
// Desc: Display button image and descriptive text
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::DrawButton( FLOAT fX, const WCHAR* strButton ) const
{
    m_FontButtons.DrawText( fX, BUTTON_Y_POS, 0xffffffff, strButton );
    
    if( !_wcsicmp( TEXT_A_BUTTON, strButton ) )  
        m_Font18.DrawText( fX + BUTTON_X_OFFSET, BUTTON_Y_POS, BUTTONTEXT_COLOR, GetString( STR_MENU_A_SELECT ) );
    if( !_wcsicmp( TEXT_B_BUTTON, strButton ) )  
        m_Font18.DrawText( fX + BUTTON_X_OFFSET, BUTTON_Y_POS, BUTTONTEXT_COLOR, GetString( STR_MENU_B_BACK ) );
    if( !_wcsicmp( TEXT_Y_BUTTON, strButton ) )  
        m_Font18.DrawText( fX + BUTTON_X_OFFSET, BUTTON_Y_POS, BUTTONTEXT_COLOR, GetString( STR_MENU_Y_HELP ) );
}




//-----------------------------------------------------------------------------
// Name: DrawControllerS()
// Desc: Render an image of the Xbox Controller S that ships in Japan
//-----------------------------------------------------------------------------
HRESULT CXBVirtualKeyboard::DrawControllerS() const
{
    // Setup vertices for a background-covering quad
    struct BACKGROUNDVERTEX
    { 
        FLOAT x,y,z,w;    // x,y,z,w form D3DXVECTOR4
        FLOAT u, v;
    } v[4] =
    {
        {   0+120 - 0.5f,   0+120 - 0.5f, 1.0f, 1.0f,    0,   0 },
        { 400+120 - 0.5f,   0+120 - 0.5f, 1.0f, 1.0f,  400,   0 },
        { 400+120 - 0.5f, 320+120 - 0.5f, 1.0f, 1.0f,  400, 320 },
        {   0+120 - 0.5f, 320+120 - 0.5f, 1.0f, 1.0f,    0, 320 },
    };

    // Set states
    m_pd3dDevice->SetTexture( 0, m_ptControllerS );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );

    // Render the quad
    m_pd3dDevice->DrawVerticesUP( D3DPT_QUADLIST, 4, v, sizeof(BACKGROUNDVERTEX) );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: IsDisabled()
// Desc: TRUE if the current key (m_iCurrBoard, m_iCurrRow, m_iCurrKey) is
//       disabled.
//-----------------------------------------------------------------------------
BOOL CXBVirtualKeyboard::IsKeyDisabled() const
{
    Key key = m_KeyboardList[ m_iCurrBoard ][ m_iCurrRow ][ m_iCurrKey ];

    if ( m_iKeyboard == KEYBOARD_JAPANESE2 )
    {
        return key.xKey == XK_NULL;
    }

    // On the symbols keyboard, Shift and Caps Lock are disabled
    if( m_iCurrBoard == TYPE_SYMBOLS )
    {
        if( key.xKey == XK_SHIFT || key.xKey == XK_CAPSLOCK )
            return TRUE;
    }

    // On the English keyboard, the Accents key is disabled
    if( m_iKeyboard == KEYBOARD_ENGLISH )
    {
        if( key.xKey == XK_ACCENTS )
            return TRUE;
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: PlayClick()
// Desc: Produces an audible click to meet certification requirements
//-----------------------------------------------------------------------------
VOID CXBVirtualKeyboard::PlayClick() const
{
    m_ClickSnd.Play();
}




//-----------------------------------------------------------------------------
// Name: GetChar()
// Desc: Convert Xkey value to WCHAR given current capitalization settings
//-----------------------------------------------------------------------------
WCHAR CXBVirtualKeyboard::GetChar( Xkey xk ) const
{
    if( m_iKeyboard == KEYBOARD_JAPANESE2 )
        return WCHAR( xk );
    // Handle case conversion
    WCHAR wc = WCHAR( xk );

    if( ( m_bIsCapsLockOn && !m_bIsShiftOn ) || ( !m_bIsCapsLockOn && m_bIsShiftOn ) )
        wc = ToUpper( wc );
    else
        wc = ToLower( wc );

    return wc;
}




//-----------------------------------------------------------------------------
// Name: ToUpper()
// Desc: Convert WCHAR to upper case. Handles accented characters properly.
//-----------------------------------------------------------------------------
WCHAR CXBVirtualKeyboard::ToUpper( WCHAR c ) // static
{
#ifdef USE_CONVERSION_TABLE

    // The table-based solution is faster, but requires 512 bytes of space
    static const WCHAR arrToUpper[] =
    {
        0,   1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
        64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
        80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96

        // alpha mapping here
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        
        123, 124, 125, 126, 127,
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
        0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
        0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,

        // accented character mapping here
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
        0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xFE, 0x0178,
    };

    if( c > 0xFF )
        return c;
    return arrToUpper[ c ];

#else

    // The code solution is slower but smaller
    if( c >= 'a' && c <= 'z' )
        return c - ('a' - 'A');
    if( c >= 0xE0 && c <= 0xFD )
        return c - (0xE0 - 0xC0);
    if( c == XK_SM_Y_DIAERESIS )    // 0x00FF
        return XK_CAP_Y_DIAERESIS;  // 0x0178
    return c;

#endif
}




//-----------------------------------------------------------------------------
// Name: ToLower()
// Desc: Convert WCHAR to lower case. Handles accented characters properly.
//-----------------------------------------------------------------------------
WCHAR CXBVirtualKeyboard::ToLower( WCHAR c ) // static
{
#ifdef USE_CONVERSION_TABLE
   
    // The table-based solution is faster, but requires 512 bytes of space
    static const WCHAR arrToLower[] =
    {
        0,   1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64

        // alpha mapping here
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',

        91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104,
        105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
        118, 119, 120, 121, 122, 123, 124, 125, 126, 127,

        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
        0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,

        // accented character mapping here
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
        0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
        0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 
        
        0xDE, 0xDF,
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
        0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
        0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
    };

    if( c == XK_CAP_Y_DIAERESIS ) // 0x0178
        return XK_SM_Y_DIAERESIS;
    if( c > 0xFF )
        return c;
    return arrToLower[ c ];

#else
    
    // The code solution is slower but smaller
    if( c >= 'A' && c <= 'Z' )
        return c + ( 'a' - 'A' );
    if( c >= 0xC0 && c <= 0xDD )
        return c + ( 0xE0 - 0xC0 );
    if( c == XK_CAP_Y_DIAERESIS ) // 0x0178
        return XK_SM_Y_DIAERESIS;
    return c;

#endif
}




//-----------------------------------------------------------------------------
// Name: SelectKeyboard()
// Desc: Select Keyboard and update related information
//-----------------------------------------------------------------------------
void CXBVirtualKeyboard::SelectKeyboard( UINT iKeyboard )
{
    if( iKeyboard > KEYBOARD_MAX )
        return;
    m_iKeyboard     = iKeyboard;
    m_pCurrKeyboard = g_aKeyboardInfo + m_iKeyboard;
    m_iLanguage     = m_pCurrKeyboard->GetLanguage();
    g_pStringTable  = m_pCurrKeyboard->GetStringTable();
}




//-----------------------------------------------------------------------------
// Name: SelectKeyboard()
// Desc: Select Keyboard and update related information
//-----------------------------------------------------------------------------
const WCHAR* GetString( const WCHAR* strTable[], UINT dwStringID )
{
    if( strTable && dwStringID < STR_MAX )
        return strTable[ dwStringID ];
    
    return L"";
};




//-----------------------------------------------------------------------------
// Name: SelectKeyboard()
// Desc: Select Keyboard and update related information
//-----------------------------------------------------------------------------
const WCHAR* GetString( UINT dwStringID )
{
    return GetString( g_pStringTable, dwStringID );
};
