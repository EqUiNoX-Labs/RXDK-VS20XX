//-----------------------------------------------------------------------------
// File: XBKeyboard.cpp
//
// Desc: Implements a virtual keyboard class. See the header file for details.
//
// Hist: 11.24.03 - Heavily revised for the December 2003 XDK release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "XBKeyboard.h"




//-----------------------------------------------------------------------------
// Keyboard layouts
//-----------------------------------------------------------------------------
DWORD CXBKeyboard::m_pAlphabetKeyboard[MAX_ROWS_PER_KEYBOARD][MAX_KEYS_PER_ROW] = 
{
    { XK_OK,       L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L'0' },
    { XK_SHIFT,    L'A', L'B', L'C', L'D', L'E', L'F', L'G', L'H', L'I', L'J' },
    { XK_CAPSLOCK, L'K', L'L', L'M', L'N', L'O', L'P', L'Q', L'R', L'S', L'T' },
    { XK_SYMBOLS,  L'U', L'V', L'W', L'X', L'Y', L'Z', XK_BACKSPACE },
    { XK_ACCENTS,  XK_SPACE, XK_ARROWLEFT, XK_ARROWRIGHT },
};

DWORD CXBKeyboard::m_pSymbolsKeyboard[MAX_ROWS_PER_KEYBOARD][MAX_KEYS_PER_ROW] =
{
    { XK_OK,       L'(', L')', L'$', L'_', L'^', L'%', L'\\', L'/', L'@', L'#' },
    { XK_SHIFT,    L'[', L']', L'$', XK_POUND_SIGN, XK_YEN_SIGN, XK_EURO_SIGN, L';', L':', L'\'', L'\"' },
    { XK_CAPSLOCK, L'<', L'>', L'?', L'!', XK_INVERTED_QMARK, XK_INVERTED_EXCL, L'-', L'*', L'+', L'=' },
    { XK_ALPHABET, L'{', L'}', XK_LT_DBL_ANGLE_QUOTE, XK_RT_DBL_ANGLE_QUOTE, L',', L'.', XK_BACKSPACE },
    { XK_ACCENTS,  XK_SPACE, XK_ARROWLEFT, XK_ARROWRIGHT },
};

DWORD CXBKeyboard::m_pAccentsKeyboard[MAX_ROWS_PER_KEYBOARD][MAX_KEYS_PER_ROW] =
{
    { XK_OK,       L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L'0' },
    { XK_SHIFT,    XK_CAP_A_GRAVE, XK_CAP_A_ACUTE, XK_CAP_A_CIRCUMFLEX, XK_CAP_A_DIAERESIS, XK_CAP_C_CEDILLA, XK_CAP_E_GRAVE, XK_CAP_E_ACUTE, XK_CAP_E_CIRCUMFLEX, XK_CAP_E_DIAERESIS, XK_CAP_I_GRAVE },
    { XK_CAPSLOCK, XK_CAP_I_ACUTE, XK_CAP_I_CIRCUMFLEX, XK_CAP_I_DIAERESIS, XK_CAP_N_TILDE, XK_CAP_O_GRAVE, XK_CAP_O_ACUTE, XK_CAP_O_CIRCUMFLEX, XK_CAP_O_TILDE, XK_CAP_O_DIAERESIS, XK_SM_SHARP_S },
    { XK_ALPHABET, XK_CAP_U_GRAVE, XK_CAP_U_ACUTE, XK_CAP_U_CIRCUMFLEX, XK_CAP_U_DIAERESIS, XK_CAP_Y_ACUTE, XK_CAP_Y_DIAERESIS, XK_BACKSPACE },
    { XK_ACCENTS,  XK_SPACE, XK_ARROWLEFT, XK_ARROWRIGHT },
};

DWORD CXBKeyboard::m_pHiraganaKeyboard[MAX_ROWS_PER_KEYBOARD][MAX_KEYS_PER_ROW] = 
{
    { XK_HIRAGANA_A,   XK_HIRAGANA_I,   XK_HIRAGANA_U,   XK_HIRAGANA_E,   XK_HIRAGANA_O,   XK_HIRAGANA_WA,  XK_HIRAGANA_WO,  XK_HIRAGANA_N,   XK_NULL,         XK_NULL,         XK_HIRAGANA, },
    { XK_HIRAGANA_KA,  XK_HIRAGANA_KI,  XK_HIRAGANA_KU,  XK_HIRAGANA_KE,  XK_HIRAGANA_KO,  XK_HIRAGANA_LA,  XK_HIRAGANA_LI,  XK_HIRAGANA_LI,  XK_HIRAGANA_LE,  XK_HIRAGANA_LO,  XK_KATAKANA, },
    { XK_HIRAGANA_SA,  XK_HIRAGANA_SI,  XK_HIRAGANA_SU,  XK_HIRAGANA_SE,  XK_HIRAGANA_SO,  XK_HIRAGANA_LTU, XK_HIRAGANA_LYA, XK_HIRAGANA_LYU, XK_HIRAGANA_LYO, XK_HIRAGANA_LWA, XK_EISUUKIGOU, },
    { XK_HIRAGANA_TA,  XK_HIRAGANA_TI,  XK_HIRAGANA_TU,  XK_HIRAGANA_TE,  XK_HIRAGANA_TO,  XK_HIRAGANA_GA,  XK_HIRAGANA_GI,  XK_HIRAGANA_GU,  XK_HIRAGANA_GE,  XK_HIRAGANA_GO,  XK_SPACE, },
    { XK_HIRAGANA_NA,  XK_HIRAGANA_NI,  XK_HIRAGANA_NU,  XK_HIRAGANA_NE,  XK_HIRAGANA_NO,  XK_HIRAGANA_ZA,  XK_HIRAGANA_ZI,  XK_HIRAGANA_ZU,  XK_HIRAGANA_ZE,  XK_HIRAGANA_ZO,  XK_BACKSPACE, },
    { XK_HIRAGANA_HA,  XK_HIRAGANA_HI,  XK_HIRAGANA_HU,  XK_HIRAGANA_HE,  XK_HIRAGANA_HO,  XK_HIRAGANA_DA,  XK_HIRAGANA_DI,  XK_HIRAGANA_DU,  XK_HIRAGANA_DE,  XK_HIRAGANA_DO,  XK_ARROWLEFT, },
    { XK_HIRAGANA_MA,  XK_HIRAGANA_MI,  XK_HIRAGANA_MU,  XK_HIRAGANA_ME,  XK_HIRAGANA_MO,  XK_HIRAGANA_BA,  XK_HIRAGANA_BI,  XK_HIRAGANA_BU,  XK_HIRAGANA_BE,  XK_HIRAGANA_BO,  XK_ARROWRIGHT, },
    { XK_HIRAGANA_YA,  XK_NULL,         XK_HIRAGANA_YU,  XK_NULL,         XK_HIRAGANA_YO,  XK_HIRAGANA_PA,  XK_HIRAGANA_PI,  XK_HIRAGANA_PU,  XK_HIRAGANA_PE,  XK_HIRAGANA_PO,  XK_NULL, },
    { XK_HIRAGANA_RA,  XK_HIRAGANA_RI,  XK_HIRAGANA_RU,  XK_HIRAGANA_RE,  XK_HIRAGANA_RO,  XK_KATAKANA_DASH,XK_ID_COMMA,     XK_ID_PERIOD,    XK_LCNER_BRAKET, XK_RCNER_BRAKET, XK_OK, },
};

DWORD CXBKeyboard::m_pKatakanaKeyboard[MAX_ROWS_PER_KEYBOARD][MAX_KEYS_PER_ROW] = 
{
    { XK_KATAKANA_A,   XK_KATAKANA_I,   XK_KATAKANA_U,   XK_KATAKANA_E,   XK_KATAKANA_O,   XK_KATAKANA_WA,  XK_KATAKANA_WO,  XK_KATAKANA_N,   XK_KATAKANA_VU,  XK_NULL,         XK_HIRAGANA, },
    { XK_KATAKANA_KA,  XK_KATAKANA_KI,  XK_KATAKANA_KU,  XK_KATAKANA_KE,  XK_KATAKANA_KO,  XK_KATAKANA_LA,  XK_KATAKANA_LI,  XK_KATAKANA_LI,  XK_KATAKANA_LE,  XK_KATAKANA_LO,  XK_KATAKANA, },
    { XK_KATAKANA_SA,  XK_KATAKANA_SI,  XK_KATAKANA_SU,  XK_KATAKANA_SE,  XK_KATAKANA_SO,  XK_KATAKANA_LTU, XK_KATAKANA_LYA, XK_KATAKANA_LYU, XK_KATAKANA_LYO, XK_KATAKANA_LWA, XK_EISUUKIGOU, },
    { XK_KATAKANA_TA,  XK_KATAKANA_TI,  XK_KATAKANA_TU,  XK_KATAKANA_TE,  XK_KATAKANA_TO,  XK_KATAKANA_GA,  XK_KATAKANA_GI,  XK_KATAKANA_GU,  XK_KATAKANA_GE,  XK_KATAKANA_GO,  XK_SPACE, },
    { XK_KATAKANA_NA,  XK_KATAKANA_NI,  XK_KATAKANA_NU,  XK_KATAKANA_NE,  XK_KATAKANA_NO,  XK_KATAKANA_ZA,  XK_KATAKANA_ZI,  XK_KATAKANA_ZU,  XK_KATAKANA_ZE,  XK_KATAKANA_ZO,  XK_BACKSPACE, },
    { XK_KATAKANA_HA,  XK_KATAKANA_HI,  XK_KATAKANA_HU,  XK_KATAKANA_HE,  XK_KATAKANA_HO,  XK_KATAKANA_DA,  XK_KATAKANA_DI,  XK_KATAKANA_DU,  XK_KATAKANA_DE,  XK_KATAKANA_DO,  XK_ARROWLEFT, },
    { XK_KATAKANA_MA,  XK_KATAKANA_MI,  XK_KATAKANA_MU,  XK_KATAKANA_ME,  XK_KATAKANA_MO,  XK_KATAKANA_BA,  XK_KATAKANA_BI,  XK_KATAKANA_BU,  XK_KATAKANA_BE,  XK_KATAKANA_BO,  XK_ARROWRIGHT, },
    { XK_KATAKANA_YA,  XK_NULL,         XK_KATAKANA_YU,  XK_NULL,         XK_KATAKANA_YO,  XK_KATAKANA_PA,  XK_KATAKANA_PI,  XK_KATAKANA_PU,  XK_KATAKANA_PE,  XK_KATAKANA_PO,  XK_NULL, },
    { XK_KATAKANA_RA,  XK_KATAKANA_RI,  XK_KATAKANA_RU,  XK_KATAKANA_RE,  XK_KATAKANA_RO,  XK_KATAKANA_DASH,XK_ID_COMMA,     XK_ID_PERIOD,    XK_LCNER_BRAKET, XK_RCNER_BRAKET, XK_OK, },
};

DWORD CXBKeyboard::m_pEisuukigouKeyboard[MAX_ROWS_PER_KEYBOARD][MAX_KEYS_PER_ROW] = 
{
    { L'A', L'B', L'C', L'D', L'E', L'a', L'b', L'c', L'd', L'e', XK_HIRAGANA, },
    { L'F', L'G', L'H', L'I', L'J', L'f', L'g', L'h', L'i', L'j', XK_KATAKANA, },
    { L'K', L'L', L'M', L'N', L'O', L'k', L'l', L'm', L'n', L'o', XK_EISUUKIGOU, },
    { L'P', L'Q', L'R', L'S', L'T', L'p', L'q', L'r', L's', L't', XK_SPACE, },
    { L'U', L'V', L'W', L'X', L'Y', L'u', L'v', L'w', L'x', L'y', XK_BACKSPACE, },
    { L'U', L'V', L'W', L'X', L'Y', L'u', L'v', L'w', L'x', L'y', XK_BACKSPACE, },
    { L'Z', L'\'',L'\"',L'@', L'#', L'z', L'(', L')', L'{', L'}', XK_ARROWLEFT, },
    { L'&', L'^', L'$', XK_YEN_SIGN, L'%', L'-', L'+', L'=', L'*', L'/', XK_ARROWRIGHT, },
    { L'0', L'1', L'2', L'3', L'4', L'?', L'!', L':', L';', L'\\',XK_NULL, },
    { L'5', L'6', L'7', L'8', L'9', L'<', L'>', L',', L'.', L'_', XK_OK, },
};




//-----------------------------------------------------------------------------
// Name: CXBKeyboard()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBKeyboard::CXBKeyboard( int iKeyboardType, 
                          CXBKeyboardFont* pFont, D3DTexture* pKeyTexture,
                          WCHAR* strStringTable[XBKEYBOARD_STR_MAX],
                          DWORD dwColorTable[XBKEYBOARD_COLOR_MAX] )
{
    m_iKeyboardType  = iKeyboardType;
    m_pFont          = pFont;
    m_pKeyTexture    = pKeyTexture;
    memcpy( m_pStringTable, strStringTable, sizeof(m_pStringTable) );
    memcpy( m_dwColorTable, dwColorTable, sizeof(m_dwColorTable) );

    m_bIsCapsLockOn  = FALSE;
    m_bIsShiftOn     = FALSE;
    m_iCurrBoard     = XBKEYBOARD_LAYOUT_ALPHABET;
    m_iCurrRow       = 0;
    m_iCurrKey       = 0;
    m_iLastColumn    = 0;

    // Not all keybaords have accents strings
    if( m_pStringTable[XBKEYBOARD_STR_KEY_ACCENTS] &&
        m_pStringTable[XBKEYBOARD_STR_KEY_ACCENTS][0] )
        m_bHasAccentsKey = TRUE;
    else
        m_bHasAccentsKey = FALSE;

    if( iKeyboardType == XBKEYBOARD_TYPE_LATIN )
        InitLatinBoard();
    else // XBKEYBOARD_TYPE_JAPANESE:
        InitJapaneseBoard();
}




//-----------------------------------------------------------------------------
// Name: GetString()
// Desc: Returns a localized string
//-----------------------------------------------------------------------------
WCHAR* CXBKeyboard::GetString( DWORD StringID )
{
    return (WCHAR*)m_pStringTable[StringID];
}




//-----------------------------------------------------------------------------
// Name: GetChar()
// Desc: Convert Xkey value to WCHAR given current capitalization settings
//-----------------------------------------------------------------------------
WCHAR CXBKeyboard::GetChar( DWORD xKey )
{
    if( m_iKeyboardType == XBKEYBOARD_TYPE_JAPANESE )
        return (WCHAR)xKey;

    // Handle case conversion
    WCHAR c = (WCHAR)xKey;

    if( ( m_bIsCapsLockOn && !m_bIsShiftOn ) || ( !m_bIsCapsLockOn && m_bIsShiftOn ) )
    {
        // Return the upper-case equivalent
        if( c >= 'a' && c <= 'z' )
            return c - ('a' - 'A');
        if( c >= 0xE0 && c <= 0xFD )
            return c - (0xE0 - 0xC0);
        if( c == XK_SM_Y_DIAERESIS )    // 0x00FF
            return XK_CAP_Y_DIAERESIS;  // 0x0178
    }
    else
    {
        // Return the lower-case equivalent
        if( c >= 'A' && c <= 'Z' )
            return c + ( 'a' - 'A' );
        if( c >= 0xC0 && c <= 0xDD )
            return c + ( 0xE0 - 0xC0 );
        if( c == XK_CAP_Y_DIAERESIS ) // 0x0178
            return XK_SM_Y_DIAERESIS; // 0x00FF
    }

    // The space bar should retuan a space character
    if( xKey == XK_SPACE )
        return L' ';

    return c;
}




//-----------------------------------------------------------------------------
// Name: GetKeyName()
// Desc: Returns the key name of a special key
//-----------------------------------------------------------------------------
WCHAR* CXBKeyboard::GetKeyName( DWORD xKey )
{
    if( xKey > XK_SPECIAL )
    {
        switch( xKey )
        {
            case XK_SPACE:      return GetString(XBKEYBOARD_STR_KEY_SPACE);
            case XK_BACKSPACE:  return GetString(XBKEYBOARD_STR_KEY_BACKSPACE);
            case XK_ARROWLEFT:  return GetString(XBKEYBOARD_STR_KEY_ARROWLEFT);
            case XK_ARROWRIGHT: return GetString(XBKEYBOARD_STR_KEY_ARROWRIGHT);
            case XK_SHIFT:      return GetString(XBKEYBOARD_STR_KEY_SHIFT);
            case XK_CAPSLOCK:   return GetString(XBKEYBOARD_STR_KEY_CAPSLOCK);
            case XK_ALPHABET:   return GetString(XBKEYBOARD_STR_KEY_ALPHABET);
            case XK_SYMBOLS:    return GetString(XBKEYBOARD_STR_KEY_SYMBOLS);
            case XK_ACCENTS:    return GetString(XBKEYBOARD_STR_KEY_ACCENTS);
            case XK_OK:         return GetString(XBKEYBOARD_STR_KEY_DONE);
            case XK_HIRAGANA:   return JH_HI JH_RA JH_GA JH_NA;
            case XK_KATAKANA:   return JK_KA JK_TA JK_KA JK_NA;
            case XK_EISUUKIGOU: return JH_E JH_I JH_SU JH_U JH_KI JH_GO JH_U;
        }
    }

    // For all other keys, return the single-character string
    static WCHAR strKey[2] = L"?";
    strKey[0] = GetChar(xKey);
    return strKey;
}




//-----------------------------------------------------------------------------
// Name: Press()
// Desc: Press the given key on the keyboard
//-----------------------------------------------------------------------------
DWORD CXBKeyboard::Press( DWORD xk )
{
    switch( xk )
    {
        case XK_ARROWLEFT:
        case XK_ARROWRIGHT:
        case XK_OK:
        case XK_BACKSPACE:
        case XK_DELETE:
            return xk;

        case XK_SPACE:
            return L' ';

        case XK_NULL:
            return L' ';

        case XK_SHIFT:
            m_bIsShiftOn = !m_bIsShiftOn;
            break;

        case XK_CAPSLOCK:
            m_bIsCapsLockOn = !m_bIsCapsLockOn;
            break;

        case XK_ALPHABET:
            m_iCurrBoard = XBKEYBOARD_LAYOUT_ALPHABET;
            memcpy( m_pKeyboard, m_pAlphabetKeyboard, sizeof(m_pKeyboard) );
            m_pKeyboard[3][0] = XK_SYMBOLS;
            m_pKeyboard[4][0] = XK_ACCENTS;
            break;

        case XK_SYMBOLS:
            m_iCurrBoard = XBKEYBOARD_LAYOUT_SYMBOLS;
            memcpy( m_pKeyboard, m_pSymbolsKeyboard, sizeof(m_pKeyboard) );
            m_pKeyboard[3][0] = XK_ALPHABET;
            m_pKeyboard[4][0] = XK_ACCENTS;
            break;

        case XK_ACCENTS:
            m_iCurrBoard = XBKEYBOARD_LAYOUT_ACCENTS;
            memcpy( m_pKeyboard, m_pAccentsKeyboard, sizeof(m_pKeyboard) );
            m_pKeyboard[3][0] = XK_ALPHABET;
            m_pKeyboard[4][0] = XK_SYMBOLS;
            break;

        case XK_HIRAGANA:
            m_iCurrBoard = XBKEYBOARD_LAYOUT_HIRAGANA;
            memcpy( m_pKeyboard, m_pHiraganaKeyboard, sizeof(m_pKeyboard) );
            break;

        case XK_KATAKANA:
            m_iCurrBoard = XBKEYBOARD_LAYOUT_KATAKANA;
            memcpy( m_pKeyboard, m_pKatakanaKeyboard, sizeof(m_pKeyboard) );
            break;

        case XK_EISUUKIGOU:
            m_iCurrBoard = XBKEYBOARD_LAYOUT_EISUUKIGOU;
            memcpy( m_pKeyboard, m_pEisuukigouKeyboard, sizeof(m_pKeyboard) );
            break;

        default:
            // Get the corresponding character
            DWORD c = GetChar(xk);

            // Unstick the shift key
            m_bIsShiftOn = FALSE;

            // Return the character
            return c;
    }

    return XK_NULL;
}




//-----------------------------------------------------------------------------
// Name: RenderKey()
// Desc: Render the key at the given position
//-----------------------------------------------------------------------------
VOID CXBKeyboard::RenderKey( FLOAT fX, FLOAT fY, DWORD xKey, 
                             DWORD dwKeyColor, DWORD dwTextColor )
{
    if( xKey == XK_NULL )
        return;

    // Dimensions for the key
    FLOAT x1 = fX + KEY_INSET;
    FLOAT y1 = fY + KEY_INSET;
    FLOAT x2 = fX + GetKeyWidth( xKey ) - KEY_INSET + 2;
    FLOAT y2 = fY + m_fKeyHeight - KEY_INSET + 2;

    // Vertex data to render a key
    FLOAT pVertices[8][6] = 
    {
        //    X        Y      Z     W       TU    TV
        { x1   -0.5f, y2-0.5f, 1.0f, 1.0f,   0.0f, 1.0f },
        { x1   -0.5f, y1-0.5f, 1.0f, 1.0f,   0.0f, 0.0f },
        { x1+17-0.5f, y2-0.5f, 1.0f, 1.0f,   0.5f, 1.0f },
        { x1+17-0.5f, y1-0.5f, 1.0f, 1.0f,   0.5f, 0.0f },
        { x2-17-0.5f, y2-0.5f, 1.0f, 1.0f,   0.5f, 1.0f },
        { x2-17-0.5f, y1-0.5f, 1.0f, 1.0f,   0.5f, 0.0f },
        { x2   -0.5f, y2-0.5f, 1.0f, 1.0f,   1.0f, 1.0f },
        { x2   -0.5f, y1-0.5f, 1.0f, 1.0f,   1.0f, 0.0f },
    };

    // Draw the key background
    D3DDevice::SetTexture( 0, m_pKeyTexture );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR );
    D3DDevice::SetRenderState( D3DRS_TEXTUREFACTOR, dwKeyColor );
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW | D3DFVF_TEX1 );

    D3DDevice::DrawVerticesUP( D3DPT_QUADSTRIP, 8, pVertices, sizeof(pVertices[0]) );

    // Draw the key text using the callbacks. If key name is, use a slightly smaller font.
    m_pFont->DrawCenteredText( (DWORD)((x1+x2)/2), (DWORD)((y1+y2)/2), 
                               ( xKey > XK_SPECIAL ) ? 22 : 29, 
                               dwTextColor, GetKeyName( xKey ) );
}




//-----------------------------------------------------------------------------
// Name: IsCurrentKeyDisabled()
// Desc: TRUE if the current key is disabled.
//-----------------------------------------------------------------------------
BOOL CXBKeyboard::IsCurrentKeyDisabled()
{
    DWORD xKey = GetCurrentKey();

    if( xKey == XK_NULL )
        return TRUE;

    if( m_iKeyboardType == XBKEYBOARD_TYPE_JAPANESE )
        return FALSE;

    // On the symbols keyboard, Shift and Caps Lock are disabled
    if( m_iCurrBoard == XBKEYBOARD_LAYOUT_SYMBOLS )
    {
        if( xKey == XK_SHIFT || xKey == XK_CAPSLOCK )
            return TRUE;
    }

    // On the English keyboard, the Accents key is disabled
    if( m_bHasAccentsKey == FALSE )
    {
        if( xKey == XK_ACCENTS )
            return TRUE;
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: 
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CXBKeyboard::HandleEvent( XBKEYBOARD_EVENT ev, DWORD* pdwOutput )
{
    (*pdwOutput) = 0;

    switch( ev )
    {
        case XBKEYBOARD_EVENT_SELECT:           // Select current key
            (*pdwOutput) = Press( GetCurrentKey() );
            break;

        case XBKEYBOARD_EVENT_TOGGLEKEYS:           // Toggle keyboard
            if( m_iKeyboardType == XBKEYBOARD_TYPE_JAPANESE )
            {
                switch( m_iCurrBoard )
                {
                    case XBKEYBOARD_LAYOUT_HIRAGANA:   (*pdwOutput) = Press( XK_KATAKANA );   break;
                    case XBKEYBOARD_LAYOUT_KATAKANA:   (*pdwOutput) = Press( XK_EISUUKIGOU ); break;
                    case XBKEYBOARD_LAYOUT_EISUUKIGOU: (*pdwOutput) = Press( XK_HIRAGANA );   break;
                }
            }
            else
            {
                switch( m_iCurrBoard )
                {
                    case XBKEYBOARD_LAYOUT_ALPHABET: (*pdwOutput) = Press( XK_SYMBOLS );  break;
                    case XBKEYBOARD_LAYOUT_SYMBOLS:  (*pdwOutput) = Press( XK_ACCENTS );  break;
                    case XBKEYBOARD_LAYOUT_ACCENTS:  (*pdwOutput) = Press( XK_ALPHABET ); break;
                }

                if( m_iCurrBoard == XBKEYBOARD_LAYOUT_ACCENTS && m_bHasAccentsKey == FALSE )
                    (*pdwOutput) = Press( XK_ALPHABET );
            }
            break;                  
        
        case XBKEYBOARD_EVENT_SPACE:
            (*pdwOutput) = Press( XK_SPACE );
            break;
        case XBKEYBOARD_EVENT_BACKSPACE:
            (*pdwOutput) = Press( XK_BACKSPACE );
            break;
        case XBKEYBOARD_EVENT_CURSORLEFT:
            (*pdwOutput) = Press( XK_ARROWLEFT );
            break;
        case XBKEYBOARD_EVENT_CURSORRIGHT:
            (*pdwOutput) = Press( XK_ARROWRIGHT );
            break;

        // Navigation
        case XBKEYBOARD_EVENT_UP:     MoveUp();    (*pdwOutput) = XK_UP;    return S_OK;
        case XBKEYBOARD_EVENT_DOWN:   MoveDown();  (*pdwOutput) = XK_DOWN;  return S_OK;
        case XBKEYBOARD_EVENT_LEFT:   MoveLeft();  (*pdwOutput) = XK_LEFT;  return S_OK;
        case XBKEYBOARD_EVENT_RIGHT:  MoveRight(); (*pdwOutput) = XK_RIGHT; return S_OK;

        default:
            return S_FALSE;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetCurrentKey()
// Desc: Move the cursor left
//-----------------------------------------------------------------------------
DWORD CXBKeyboard::GetCurrentKey()
{
    return m_pKeyboard[m_iCurrRow][m_iCurrKey];
}




//-----------------------------------------------------------------------------
// Name: MoveUp()
// Desc: Move the cursor up
//-----------------------------------------------------------------------------
VOID CXBKeyboard::MoveUp()
{
    if( m_iKeyboardType == XBKEYBOARD_TYPE_JAPANESE )
    {
        do
        {
            m_iCurrRow = ( m_iCurrRow == 0 ) ? m_dwMaxRows - 1 : m_iCurrRow - 1;
        } while( IsCurrentKeyDisabled() );
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

    } while( IsCurrentKeyDisabled() );
}




//-----------------------------------------------------------------------------
// Name: MoveDown()
// Desc: Move the cursor down
//-----------------------------------------------------------------------------
VOID CXBKeyboard::MoveDown()
{
    if ( m_iKeyboardType == XBKEYBOARD_TYPE_JAPANESE )
    {
        do
        {
            m_iCurrRow = ( m_iCurrRow == (int)m_dwMaxRows - 1 ) ? 0 : m_iCurrRow + 1;
        } while( IsCurrentKeyDisabled() );
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
        m_iCurrRow = ( m_iCurrRow == (int)m_dwMaxRows - 1 ) ? 0 : m_iCurrRow + 1;

    } while( IsCurrentKeyDisabled() );
}




//-----------------------------------------------------------------------------
// Name: MoveLeft()
// Desc: Move the cursor left
//-----------------------------------------------------------------------------
VOID CXBKeyboard::MoveLeft()
{
    do
    {
        if( m_iCurrKey == 0 )
            m_iCurrKey = MAX_KEYS_PER_ROW-1;
        else
            m_iCurrKey--;

    } while( IsCurrentKeyDisabled() );

    SetLastColumn();
}




//-----------------------------------------------------------------------------
// Name: MoveRight()
// Desc: Move the cursor right
//-----------------------------------------------------------------------------
VOID CXBKeyboard::MoveRight()
{
    do
    {
        if( m_iCurrKey >= MAX_KEYS_PER_ROW-1 )
            m_iCurrKey = 0;
        else
            m_iCurrKey++;

    } while( IsCurrentKeyDisabled() );

    SetLastColumn();
}




//-----------------------------------------------------------------------------
// Name: SetLastColumn()
// Desc: Remember the column position if we're on a single letter character
//-----------------------------------------------------------------------------
VOID CXBKeyboard::SetLastColumn()
{
    // If the new key is a single character, remember it for later
    DWORD xKey = GetCurrentKey();
    
    switch( xKey )
    {
        // Adjust the last column for the arrow keys to confine it
        // within the range of the key width
        case XK_ARROWLEFT:
            m_iLastColumn = ( m_iLastColumn <= 7 ) ? 7 : 8; 
            break;
        
        case XK_ARROWRIGHT:
            m_iLastColumn = ( m_iLastColumn <= 9 ) ? 9 : 10; 
            break;

        // Single char, non-arrow
        default:
            if( xKey < XK_SPECIAL )
                m_iLastColumn = m_iCurrKey; 
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: GetKeyWidth()
// Desc: 
//-----------------------------------------------------------------------------
DWORD CXBKeyboard::GetKeyWidth( DWORD xKey )
{
    if( m_iKeyboardType == XBKEYBOARD_TYPE_JAPANESE )
    {
        switch( xKey )
        {
            case XK_HIRAGANA:
            case XK_KATAKANA:
            case XK_EISUUKIGOU:
            case XK_SPACE:
            case XK_BACKSPACE:
            case XK_ARROWLEFT:
            case XK_ARROWRIGHT:
            case XK_OK:
                return MODEKEY_WIDTH;
        }
    }
    else
    {
        switch( xKey )
        {
            case XK_SYMBOLS:
            case XK_ACCENTS:
            case XK_ALPHABET:
            case XK_CAPSLOCK:
            case XK_SHIFT:
            case XK_OK:
                return MODEKEY_WIDTH;

            case XK_BACKSPACE:
                return (KEY_WIDTH * 4) + (GAP_WIDTH * 3);
            
            case XK_SPACE:
                return (KEY_WIDTH * 6) + (GAP_WIDTH * 5);
            
            case XK_ARROWLEFT:
            case XK_ARROWRIGHT:
                return (KEY_WIDTH * 2) + (GAP_WIDTH * 1);
        }
    }

    return KEY_WIDTH;
}




//-----------------------------------------------------------------------------
// Name: InitLatinBoard()
// Desc: Sets up the virtual keyboard for the selected language
//-----------------------------------------------------------------------------
VOID CXBKeyboard::InitLatinBoard()
{
    // Restore keyboard to default state
    m_iCurrRow      = 1;
    m_iCurrKey      = 1;
    m_iLastColumn   = 1;
    m_iCurrBoard    = XBKEYBOARD_LAYOUT_ALPHABET;
    m_bIsCapsLockOn = FALSE;
    m_bIsShiftOn    = FALSE;
    m_fKeyHeight    = 42.0f;
    m_dwMaxRows     = 5;

    memcpy( m_pKeyboard, m_pAlphabetKeyboard, sizeof(m_pKeyboard) );
}




//-----------------------------------------------------------------------------
// Name: InitJapaneseBoard()
// Desc: Sets up the virtual keyboard for the selected language
//-----------------------------------------------------------------------------
VOID CXBKeyboard::InitJapaneseBoard()
{
    m_iCurrRow      = 0;
    m_iCurrKey      = 0;
    m_iLastColumn   = 0;
    m_iCurrBoard    = XBKEYBOARD_LAYOUT_HIRAGANA;
    m_bIsCapsLockOn = FALSE;
    m_bIsShiftOn    = FALSE;
    m_fKeyHeight    = 32.0f;
    m_dwMaxRows     = 9;

    memcpy( m_pKeyboard, m_pHiraganaKeyboard, sizeof(m_pKeyboard) );
}




//-----------------------------------------------------------------------------
// Name: RenderKeyboard()
// Desc: Display current keyboard
//-----------------------------------------------------------------------------
VOID CXBKeyboard::RenderKeyboard( FLOAT fStartX, FLOAT fStartY )
{
    if( m_iKeyboardType == XBKEYBOARD_TYPE_JAPANESE )
        RenderJapaneseKeyboard( fStartX, fStartY );
    else
        RenderLatinKeyboard( fStartX, fStartY );
}




//-----------------------------------------------------------------------------
// Name: RenderLatinKeyboard()
// Desc: Display current latin keyboard
//-----------------------------------------------------------------------------
VOID CXBKeyboard::RenderLatinKeyboard( FLOAT fStartX, FLOAT fStartY )
{
    // Draw each row
    FLOAT fY = fStartY;

    for( int row = 0; row < MAX_ROWS_PER_KEYBOARD; row++ )
    {
        FLOAT fWidthSum = 0.0f;
        FLOAT fX        = fStartX;

        for( int i = 0; i < MAX_KEYS_PER_ROW; i++ )
        {
            // Determine key name
            DWORD xKey = m_pKeyboard[row][i];
            BOOL     bVisible     = TRUE;
            BOOL     bPressed     = FALSE;
            BOOL     bDisabled    = FALSE;

            // Handle special key coloring
            switch( xKey )
            {
                case XK_SHIFT:
                    switch( m_iCurrBoard )
                    {
                        case XBKEYBOARD_LAYOUT_ALPHABET:
                        case XBKEYBOARD_LAYOUT_ACCENTS:
                            if( m_bIsShiftOn )
                                bPressed = TRUE;
                            break;
                        case XBKEYBOARD_LAYOUT_SYMBOLS:
                            bDisabled = TRUE;
                            break;
                    }
                    break;
                case XK_CAPSLOCK:
                    switch( m_iCurrBoard )
                    {
                        case XBKEYBOARD_LAYOUT_ALPHABET:
                        case XBKEYBOARD_LAYOUT_ACCENTS:
                            if( m_bIsCapsLockOn )
                                bPressed = TRUE;
                            break;
                        case XBKEYBOARD_LAYOUT_SYMBOLS:
                            bDisabled = TRUE;
                            break;
                    }
                    break;
                case XK_ACCENTS:
                    if( m_bHasAccentsKey == FALSE )
                        bVisible = FALSE;
                    break;

                case XK_NULL:
                    bVisible = FALSE;
                    false;
            }

            if( bVisible )
            {
                // Highlight the current key
                BOOL  bHighlighted = ( row == m_iCurrRow && i == m_iCurrKey ) ? TRUE : FALSE;
                DWORD dwKeyColorID;
                DWORD dwTextColorID;

                if( bDisabled )
                {
                    dwKeyColorID  = bHighlighted ? XBKEYBOARD_COLOR_DISABLED_KEY_SELECTED  : XBKEYBOARD_COLOR_DISABLED_KEY;
                    dwTextColorID = bHighlighted ? XBKEYBOARD_COLOR_DISABLED_TEXT_SELECTED : XBKEYBOARD_COLOR_DISABLED_TEXT;
                }
                else if( bPressed )
                {
                    dwKeyColorID  = bHighlighted ? XBKEYBOARD_COLOR_PRESSED_KEY_SELECTED  : XBKEYBOARD_COLOR_PRESSED_KEY;
                    dwTextColorID = bHighlighted ? XBKEYBOARD_COLOR_PRESSED_TEXT_SELECTED : XBKEYBOARD_COLOR_PRESSED_TEXT;
                }
                else
                {
                    dwKeyColorID  = bHighlighted ? XBKEYBOARD_COLOR_NORMAL_KEY_SELECTED  : XBKEYBOARD_COLOR_NORMAL_KEY;
                    dwTextColorID = bHighlighted ? XBKEYBOARD_COLOR_NORMAL_TEXT_SELECTED : XBKEYBOARD_COLOR_NORMAL_TEXT;
                }

                RenderKey( fX + fWidthSum, fY, xKey, m_dwColorTable[dwKeyColorID], m_dwColorTable[dwTextColorID] );
            }

            fWidthSum += GetKeyWidth( xKey );

            // There's a slightly larger gap between the leftmost keys (mode
            // keys) and the main keyboard
            if( i == 0 )
                fWidthSum += GAP2_WIDTH;
            else
                fWidthSum += GAP_WIDTH;
        }

        // Advance to next row
        fY += m_fKeyHeight;
    }
}




//-----------------------------------------------------------------------------
// Name: RenderJapaneseKeyboard()
// Desc: Display current Japanese keyboard (keyboard type)
//-----------------------------------------------------------------------------
VOID CXBKeyboard::RenderJapaneseKeyboard( FLOAT fStartX, FLOAT fStartY )
{
    // Draw each row
    FLOAT fY = fStartY;

    for( int row = 0; row < MAX_ROWS_PER_KEYBOARD; row++ )
    {
        FLOAT fWidthSum = 0.0f;
        FLOAT fX        = fStartX;

        for( int i = 0; i < MAX_KEYS_PER_ROW; i++ )
        {
            // Determine key name
            DWORD xKey = m_pKeyboard[row][i];
            DWORD dwKeyColorID  = XBKEYBOARD_COLOR_NORMAL_KEY;
            DWORD dwTextColorID = XBKEYBOARD_COLOR_NORMAL_TEXT;

            // Highlight the current key
            if( row == m_iCurrRow && i == m_iCurrKey )
            {
                dwKeyColorID  = XBKEYBOARD_COLOR_NORMAL_KEY_SELECTED;
                dwTextColorID = XBKEYBOARD_COLOR_NORMAL_TEXT_SELECTED;
            }

            RenderKey( fX + fWidthSum, fY, xKey, m_dwColorTable[dwKeyColorID], m_dwColorTable[dwTextColorID] );

            fWidthSum += GetKeyWidth( xKey );

            // There are a slightly larger gaps
            if( i == 4 || i == 9 )
                fWidthSum += GAP2_WIDTH;
            else
                fWidthSum += GAP_WIDTH;
        }

        // Advance to the next row
        fY += m_fKeyHeight;
    }
}




