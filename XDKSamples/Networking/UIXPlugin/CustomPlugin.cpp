//-----------------------------------------------------------------------------
// File: CustomPlugin.cpp
//
// Desc: A custom UI plugin for UIX. Apps can use this code as a starting point
//       for writing their own custom UI for UIX..
//
// Hist: 07.14.03 - New for August release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xonline.h>
#include <uix.h>
#include "XBFont.h"
#include "XBOnline.h"
#include "CustomPlugin.h"




//-----------------------------------------------------------------------------
// Name: Font_GetTextExtent()
// Desc: Wrapper for dealing with text with a char count
//-----------------------------------------------------------------------------
HRESULT Font_GetTextExtent( CXBFont* pFont, const WCHAR* strText, DWORD CharCount,
                            FLOAT* pWidth, FLOAT* pHeight )
{
    if( CharCount == (DWORD)-1 )
        return pFont->GetTextExtent( strText, pWidth, pHeight );

    DWORD  dwText  = (DWORD)strText;
    WCHAR* strTemp = (WCHAR*)dwText;
    WCHAR  cSave   = strTemp[CharCount];
    strTemp[CharCount] = L'\0';

    pFont->GetTextExtent( strText, pWidth, pHeight );

    strTemp[CharCount] = cSave;
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Create a background UI object
//-----------------------------------------------------------------------------
HRESULT CUIBackgroundObject::Create()
{
    m_ObjectType = UIX_OBJECT_BACKGROUND;

    // Get the texture resource
    m_pTexture = NULL;
    if( m_pLayout->ImageOffset != (DWORD)-1 )
    {
        if( FAILED( m_pPlugin->m_pPluginSupport->GetScreenImage( (DWORD)m_ScreenResID,
                                                                 m_pLayout->ImageOffset,
                                                                 &m_pTexture ) ) )
            return E_FAIL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders a background UI object
//-----------------------------------------------------------------------------
HRESULT CUIBackgroundObject::Render( FLOAT fX, FLOAT fY )
{
    FLOAT fWidth  = (FLOAT)m_pLayout->Width;
    FLOAT fHeight = (FLOAT)m_pLayout->Height;

    if( m_pTexture )
    {
        // If there is a background image, render it
        DrawTexture( m_pTexture, fX, fY, fWidth, fHeight );
    }
    else if( m_pLayout->BackColor & 0xff000000 )
    {
        // Else, draw the background color
        D3DDevice::SetVertexShader( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );
        D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2 );
        D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
        D3DDevice::Begin( D3DPT_TRIANGLEFAN );
        D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, m_pLayout->BackColor );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX+0,      fY+0,       0.1f, 0.9f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX+fWidth, fY+0,       0.1f, 0.9f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX+fWidth, fY+fHeight, 0.1f, 0.9f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX+0,      fY+fHeight, 0.1f, 0.9f );
        D3DDevice::End();
    }
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Creates a textbox UI object
//-----------------------------------------------------------------------------
HRESULT CUITextBoxObject::Create()
{
    m_ObjectType = UIX_OBJECT_TEXTBOX;
    m_bIsGreyed  = FALSE;
    m_bWrapText  = FALSE;
    m_Icons      = NULL;
    m_IconCount  = 0;
    m_Text       = NULL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Destroy()
// Desc: Destroys a textbox UI object
//-----------------------------------------------------------------------------
VOID CUITextBoxObject::Destroy()
{
    if( m_Text )  delete[] m_Text;
    if( m_Icons ) delete[] m_Icons;
    m_Text      = NULL;
    m_Icons     = NULL;
    m_IconCount = 0L;
}




//-----------------------------------------------------------------------------
// Name: SetState()
// Desc: Handler for setting state for a textbox UI object
//-----------------------------------------------------------------------------
HRESULT CUITextBoxObject::SetState( DWORD ItemIndex, UIX_OBJSTATE_TYPE State,
                                    DWORD Value )
{
    switch( State )
    {
        // Set word-wrapping state
        case UIX_OBJSTATE_WORD_WRAP:
        {
            m_bWrapText = TRUE;
            break;
        }

        default:
            return E_NOTIMPL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetText()
// Desc: Handler for setting text for a textbox UI object
//-----------------------------------------------------------------------------
HRESULT CUITextBoxObject::SetText( DWORD ItemIndex, const WCHAR* strText,
                                   DWORD dwIconCount,
                                   const UIX_SKIN_ICON_INFO* pIcons )
{
    // Cleanup any previous stuff
    Destroy();

    // Allocate space for and copy the icons
    m_IconCount = dwIconCount;
    if( m_IconCount )
    {
        m_Icons = new TEXTBOX_ICON[m_IconCount];

        for( DWORD i=0; i<m_IconCount; i++ )
        {
            if( FAILED( m_pPlugin->m_pPluginSupport->GetImage( pIcons[i].IconResID,
                                                               &m_Icons[i].pTexture ) ) )
                OutputDebugStringA( "CUITextBoxObject: Failed to get icon texture\n" );

            D3DSURFACE_DESC IconDesc;
            m_Icons[i].pTexture->GetLevelDesc( 0, &IconDesc );
            m_Icons[i].fWidth  = (FLOAT)IconDesc.Width;
            m_Icons[i].fHeight = (FLOAT)IconDesc.Width;

            m_Icons[i].dwInsertPosInText = pIcons[i].InsertPosInText;
            m_Icons[i].dwFlags           = pIcons[i].Flags;

//          FLOAT fWhitespaceAdvance = (FLOAT)m_pPlugin->m_pFont->m_Glyphs[m_pPlugin->m_pFont->m_TranslatorTable[L' ']].wAdvance;
//          dwExtraWhitespace += (DWORD)ceilf( fIconWidth/fWhitespaceAdvance ) + 1;
        }
    }

    // Allocate space for and copy the text
    if( strText && strText[0] )
    {
        m_Text = new WCHAR[wcslen(strText)+1];
        wcscpy( m_Text, strText );

        // If the text needs to be wrapped, do that now
        if( m_bWrapText )
            WrapText();
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders a textbox UI object
//-----------------------------------------------------------------------------
HRESULT CUITextBoxObject::Render( FLOAT fStartX, FLOAT fStartY )
{
    CXBFont*        pFont = m_pPlugin->m_pFont;
    FLOAT           fTextWidth;
    FLOAT           fTextHeight;
    ULONG           CharCount;
    const WCHAR*    pStrSegment;
    DWORD           dwLastPosInText;
    FLOAT           fMarginSpace = 4;

    // Make sure there is some text to draw
    if( NULL == m_Text )
        return S_FALSE;

    // Select the text color
    DWORD dwTextColor = m_bIsGreyed ? m_pLayout->DisabledTextColor : m_pLayout->TextColor;

    // Set font height
    FLOAT fScale = ((FLOAT)m_pLayout->FontHeight/pFont->m_fFontYAdvance);
    pFont->SetScaleFactors( fScale, fScale );

    // Set the starting position for text
    FLOAT fX = fStartX + fMarginSpace;
    FLOAT fY = fStartY;

    // Special case: If there are no icons in this string, we simply do a draw text
    if( 0 == m_IconCount )
    {
        // Make adjustments for alignment
        DWORD dwDrawTextFlags = 0L;
        if( m_pLayout->Flags & UIX_LAYOUT_FLAG_RIGHT_ALIGN )
        {
            fX = fStartX - (FLOAT)m_pLayout->XOffset + (FLOAT)m_pLayout->Width - (FLOAT)m_pLayout->XOffset;
            dwDrawTextFlags = XBFONT_RIGHT;
        }
        if( m_pLayout->Flags & UIX_LAYOUT_FLAG_CENTER_ALIGN )
        {
            fX = fStartX - (FLOAT)m_pLayout->XOffset + (FLOAT)m_pLayout->Width/2;
            dwDrawTextFlags = XBFONT_CENTER_X;
        }

        pFont->DrawText( fX, fY, dwTextColor, m_Text, dwDrawTextFlags );
    }
    else // m_IconCount > 0
    {
        // Make a copy for the string that inserts enough extra whitespace to allow
        // room for the icons
        FLOAT fWhitespaceAdvance = pFont->GetTextWidth( L"  " ) - pFont->GetTextWidth( L" " );

        DWORD dwNumSpaces = 0;
        for( DWORD i = 0; i < m_IconCount; i++ )
        {
            if( UIX_ICON_RIGHT_JUSTIFIED != m_Icons[i].dwFlags )
                dwNumSpaces += (DWORD)ceilf( m_Icons[i].fWidth/fWhitespaceAdvance );
        }

        WCHAR* strText = new WCHAR[ wcslen(m_Text) + dwNumSpaces + 1 ];

        DWORD dwDstPos = 0;
        DWORD dwSrcPos = 0;
        for( DWORD i = 0; i < m_IconCount; i++ )
        {
            if( UIX_ICON_RIGHT_JUSTIFIED != m_Icons[i].dwFlags )
            {
                DWORD dwNumSpaces = (DWORD)ceilf( m_Icons[i].fWidth/fWhitespaceAdvance );
                DWORD dwLen = m_Icons[i].dwInsertPosInText - dwSrcPos;
                wcsncpy( &strText[dwDstPos], &m_Text[dwSrcPos], dwLen );
                dwDstPos += dwLen;
                dwSrcPos += dwLen;
                for( DWORD j=0; j<dwNumSpaces; j++ )
                    strText[dwDstPos++] = L' ';
                strText[dwDstPos] = 0;
            }
        }
        wcscpy( &strText[dwDstPos], &m_Text[dwSrcPos] );

        // Compute the offset to align this text
        if( m_pLayout->Flags & UIX_LAYOUT_FLAG_CENTER_ALIGN )
            fX = fStartX + ( m_pLayout->Width - pFont->GetTextWidth( strText ) ) / 2;
        else if( m_pLayout->Flags & UIX_LAYOUT_FLAG_RIGHT_ALIGN )
            fX = fStartX + m_pLayout->Width - m_pLayout->XOffset - pFont->GetTextWidth( strText );

        // Now draw the text fragments
        pFont->DrawText( fX, fY, dwTextColor, strText );
        delete[] strText;

        // Draw embedded icons
        pStrSegment = m_Text;
        dwLastPosInText = 0;
        for( DWORD i = 0; i < m_IconCount; i++ )
        {
            if( UIX_ICON_INSIDE_TEXT == m_Icons[i].dwFlags )
            {
                CharCount = m_Icons[i].dwInsertPosInText - dwLastPosInText;

                if( CharCount )
                    Font_GetTextExtent( pFont, pStrSegment, CharCount, &fTextWidth, &fTextHeight );
                else
                {
                    fTextWidth  = 0.0f;
                    fTextHeight = 0.0f;
                }

                pStrSegment += CharCount;

                // Render the icon texture
                fX += fTextWidth;

                FLOAT fYAdjust = floorf( ( pFont->m_fFontYAdvance - m_Icons[i].fHeight ) / 2 );

                DrawTexture( m_Icons[i].pTexture, fX, fY + fYAdjust, m_Icons[i].fWidth, m_Icons[i].fHeight );

                fX += m_Icons[i].fWidth;
            }
        }
    }

    // Restore the font size
    pFont->SetScaleFactors( 1.0f, 1.0f );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Create a listbox UI object
//-----------------------------------------------------------------------------
HRESULT CUIListBoxObject::Create()
{
    m_ObjectType        = UIX_OBJECT_LISTBOX;
    m_CurrentIndex      = 0;
    m_TopIndex          = 0;
    m_dwMaxVisibleCount = m_pLayout->CustomParam ? m_pLayout->CustomParam :
                                                   DEFAULT_LB_VISIBLE_ITEMS;
    m_dwNumChildObjects = 0;
    ZeroMemory( &m_ChildObjectArray, sizeof(m_ChildObjectArray) );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Destroy()
// Desc: Destroys a listbox UI object
//-----------------------------------------------------------------------------
VOID CUIListBoxObject::Destroy()
{
    // Destroy child objects
    if( m_dwNumChildObjects > 0 )
        DestroyChildObjects();
}




//-----------------------------------------------------------------------------
// Name: DestroyChildObjects()
// Desc: Destroys all child objects (list items) owned by a listbox UI object
//-----------------------------------------------------------------------------
VOID CUIListBoxObject::DestroyChildObjects()
{
    // Go through all the child objects and release the memory
    for( DWORD i = 0; i < m_dwNumChildObjects; i++ )
    {
        m_ChildObjectArray[i]->Destroy();
        delete m_ChildObjectArray[i];
    }

    // Free array memory
    ZeroMemory( m_ChildObjectArray, sizeof(m_ChildObjectArray) );
    m_dwNumChildObjects = 0;
}




//-----------------------------------------------------------------------------
// Name: InsertItem()
// Desc: Inserts a listbox item into a listbox UI object
//-----------------------------------------------------------------------------
HRESULT CUIListBoxObject::InsertItem( ULONG ItemIndex, ULONG* pReturnIndex )
{
    HRESULT hr;

    // Add an entry into the list box item array at the right index.  If
    // the index >= number of items in the array or index=-1, we append
    // the value at the end of the array.  Otherwise, we insert it
    if ((ItemIndex >= m_dwNumChildObjects) || ItemIndex == (DWORD)-1)
        ItemIndex = m_dwNumChildObjects;

    m_ChildObjectArray[ItemIndex] = new CUITextBoxObject;
    m_ChildObjectArray[ItemIndex]->m_pPlugin = m_pPlugin;
    m_ChildObjectArray[ItemIndex]->m_pLayout = m_pLayout;

    // Init the list box item object
    hr = m_ChildObjectArray[ItemIndex]->Create();
    if( FAILED(hr) )
    {
        m_ChildObjectArray[ItemIndex]->Destroy();
        delete m_ChildObjectArray[ItemIndex];
        m_ChildObjectArray[ItemIndex] = NULL;
        return hr;
    }

    m_dwNumChildObjects++;

    if( pReturnIndex != NULL )
        (*pReturnIndex) = ItemIndex;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Clear()
// Desc: Clears the list of items owned by a listbox UI object
//-----------------------------------------------------------------------------
HRESULT CUIListBoxObject::Clear( BOOL bResetSelectionIndex )
{
    DestroyChildObjects();

    if( bResetSelectionIndex )
    {
        m_CurrentIndex = 0;
        m_TopIndex = 0;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetState()
// Desc: Handler for setting state for a listbox UI object
//-----------------------------------------------------------------------------
HRESULT CUIListBoxObject::SetState( DWORD ItemIndex, UIX_OBJSTATE_TYPE State,
                                    DWORD Value )
{
    switch( State )
    {
        // Set list selection index
        case UIX_OBJSTATE_LIST_SELECTION_INDEX:
        {
            if( Value != 0 && Value >= m_dwNumChildObjects )
                return E_INVALIDARG;

            m_CurrentIndex = Value;

            if( m_CurrentIndex < m_TopIndex )
                m_TopIndex = m_CurrentIndex;

            if( m_CurrentIndex >= m_TopIndex + m_dwMaxVisibleCount )
                m_TopIndex = m_CurrentIndex - (m_dwMaxVisibleCount - 1);

            break;
        }

        // Set grey out list item flag
        case UIX_OBJSTATE_LIST_ITEM_GREYED:
        {
            if( ItemIndex >= m_dwNumChildObjects )
                return E_INVALIDARG;

            m_ChildObjectArray[ItemIndex]->m_bIsGreyed = (BOOL)Value;
            break;
        }

        default:
            return E_NOTIMPL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetState()
// Desc: Handler for getting state for a listbox UI object
//-----------------------------------------------------------------------------
HRESULT CUIListBoxObject::GetState( DWORD ItemIndex, UIX_OBJSTATE_TYPE State,
                                    DWORD* pValue )
{
    switch( State )
    {
        // Get list selection index
        case UIX_OBJSTATE_LIST_SELECTION_INDEX:
        {
            (*pValue) = m_CurrentIndex;
            break;
        }

        // Get grey out list item flag
        case UIX_OBJSTATE_LIST_ITEM_GREYED:
        {
            if( ItemIndex >= m_dwNumChildObjects )
                return E_INVALIDARG;
            (*pValue) = (DWORD)m_ChildObjectArray[ItemIndex]->m_bIsGreyed;
            break;
        }

        default:
            return E_NOTIMPL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetText()
// Desc: Handler for setting text for listbox item owned by a listbox UI object
//-----------------------------------------------------------------------------
HRESULT CUIListBoxObject::SetText( DWORD ItemIndex, const WCHAR* pText, DWORD IconCount,
                                   const UIX_SKIN_ICON_INFO* pIconInfo )
{
    if( ItemIndex >= m_dwNumChildObjects )
        return E_INVALIDARG;

    // Set the text for the child object
    m_ChildObjectArray[ItemIndex]->SetText( ItemIndex, pText, IconCount, pIconInfo );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: HandleInput()
// Desc: Handler for processing input for a listbox UI object
//-----------------------------------------------------------------------------
HRESULT CUIListBoxObject::HandleInput( UIX_INPUT_TYPE InputKey )
{
    // Handle the listbox input
    switch( InputKey )
    {
        // Move up
        case UIX_INPUT_UP:
        case UIX_INPUT_DPAD_UP:
            if( m_CurrentIndex > 0 )
            {
                if( --m_CurrentIndex < m_TopIndex )
                    m_TopIndex = m_CurrentIndex;

                return S_OK;
            }
            break;

        // Move down
        case UIX_INPUT_DOWN:
        case UIX_INPUT_DPAD_DOWN:
            if( (m_CurrentIndex + 1) < m_dwNumChildObjects )
            {
                if( ++m_CurrentIndex > (m_TopIndex + m_dwMaxVisibleCount - 1) )
                    m_TopIndex++;

                return S_OK;
            }
            break;
    }

    return S_FALSE;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders a listbox UI object
//-----------------------------------------------------------------------------
HRESULT CUIListBoxObject::Render( FLOAT X, FLOAT Y )
{
    if( m_dwNumChildObjects == 0 )
        return S_FALSE;

    DWORD dwLayoutSpace = m_pLayout->Height;
    dwLayoutSpace -= m_pPlugin->m_UpArrowImage.m_dwHeight;
    dwLayoutSpace -= m_pPlugin->m_DownArrowImage.m_dwHeight;

    FLOAT fYAdvance = ((FLOAT)dwLayoutSpace) / m_dwMaxVisibleCount;
    DWORD EndIndex  = m_dwNumChildObjects - 1;
    FLOAT ItemY     = Y;

    // Render the up arrow
    if( m_TopIndex != 0 )
    {
        DrawTexture( m_pPlugin->m_UpArrowImage.m_pTexture,
                     (FLOAT)(X + (m_pLayout->Width - m_pPlugin->m_UpArrowImage.m_dwWidth) / 2), ItemY,
                     (FLOAT)m_pPlugin->m_UpArrowImage.m_dwWidth, (FLOAT)m_pPlugin->m_UpArrowImage.m_dwHeight );
    }

    ItemY += (FLOAT)m_pPlugin->m_UpArrowImage.m_dwHeight;

    // Render the list items
    for( DWORD i = 0; i < m_dwMaxVisibleCount; i++ )
    {
        CUITextBoxObject* Child = m_ChildObjectArray[m_TopIndex+i];

        // Draw the highlight
        if( m_TopIndex+i == m_CurrentIndex )
        {
            DrawTexture( m_pPlugin->m_HighlightImage.m_pTexture, X, ItemY,
                         (FLOAT)m_pLayout->Width, fYAdvance );
        }

        // Draw the list box item
        Child->Render( X + m_pLayout->XOffset, ItemY + m_pLayout->YOffset );

        ItemY += fYAdvance;

        if( m_TopIndex+i >= EndIndex )
            return S_OK;
    }

    // Render the bottom arrow
    DrawTexture( m_pPlugin->m_DownArrowImage.m_pTexture,
                 (FLOAT)(X + (m_pLayout->Width - m_pPlugin->m_DownArrowImage.m_dwWidth) / 2), ItemY,
                 (FLOAT)m_pPlugin->m_DownArrowImage.m_dwWidth, (FLOAT)m_pPlugin->m_DownArrowImage.m_dwHeight );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: WrapText()
// Desc: Wrap the text to fit it's the bounding rect
//-----------------------------------------------------------------------------
VOID CUITextBoxObject::WrapText()
{
    // Set the right scale factors
    FLOAT fScale = ((FLOAT)m_pLayout->FontHeight/m_pPlugin->m_pFont->m_fFontYAdvance);
    m_pPlugin->m_pFont->SetScaleFactors( fScale, fScale );

    WCHAR* ptr    = m_Text;
    WCHAR* strLine = ptr;

    while( *ptr )
    {
        // Add spaces and newlines
        while( *ptr == L' ' || *ptr == L'\n' )
        {
            if( *ptr == L'\n' )
                strLine = ++ptr;
            else
                ptr++;
        }

        // Add the next word
        DWORD dwWordLength;
        m_pPlugin->m_pPluginSupport->GetWordLength( ptr, &dwWordLength );
        if( ptr[dwWordLength-1] == L' ' )
            dwWordLength -= 1;

        // Check the line width
        WCHAR cSave = ptr[dwWordLength];
        ptr[dwWordLength] = L'\0';
        FLOAT fTextWidth, fTextHeight;
        m_pPlugin->m_pFont->GetTextExtent( strLine, &fTextWidth, &fTextHeight );
        ptr[dwWordLength] = cSave;

        // Check if we exceeded the bounds
        if( fTextWidth > (FLOAT)(m_pLayout->Width-m_pLayout->XOffset) )
        {
            // Allocate space for a bigger string
            WCHAR* strNewText = new WCHAR[1+(wcslen(m_Text)+1)];

            // Insert a newline
            int pos = ptr-m_Text;
            wcsncpy( strNewText, m_Text, pos );
            strNewText[pos] = L'\n';
            wcscpy( strNewText+pos+1, m_Text+pos );

            // Update pointers
            ptr = &strNewText[pos+1];
            strLine = ptr;
            delete[] m_Text;
            m_Text = strNewText;
        }
        else
        {
            // Accept the word
            ptr += dwWordLength;
        }
    }

    // Restore the scale factors and return
    m_pPlugin->m_pFont->SetScaleFactors( 1.0f, 1.0f );
}




//-----------------------------------------------------------------------------
// Name: DrawTexture()
// Desc: Helper function to render a texture
//-----------------------------------------------------------------------------
VOID CUIObject::DrawTexture( D3DTexture* pTexture, FLOAT Left, FLOAT Top,
                             FLOAT Width, FLOAT Height )
{
    // Set texcoord range depending on whether the texture is linear or not
    FLOAT MaxU = pTexture->Size ? Width  : 1.0f;
    FLOAT MaxV = pTexture->Size ? Height : 1.0f;

    // Set rendering state
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW | D3DFVF_TEX1 );
    D3DDevice::SetTexture( 0, pTexture );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );

    // Draw the quad
    D3DDevice::Begin( D3DPT_QUADLIST );
    D3DDevice::SetVertexData2f( D3DVSDE_TEXCOORD0, 0.0f, 0.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, Left, Top, 1.0f, 1.0f );
    D3DDevice::SetVertexData2f( D3DVSDE_TEXCOORD0, MaxU, 0.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, Left+Width, Top, 1.0f, 1.0f );
    D3DDevice::SetVertexData2f( D3DVSDE_TEXCOORD0, MaxU, MaxV );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, Left+Width, Top+Height, 1.0f, 1.0f );
    D3DDevice::SetVertexData2f( D3DVSDE_TEXCOORD0, 0.0f, MaxV );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, Left, Top+Height, 1.0f, 1.0f );
    D3DDevice::End();

    // Cleanup
    D3DDevice::SetTexture( 0, NULL );
}




//-----------------------------------------------------------------------------
// Name: CUIPlugin()
// Desc: Constructor
//-----------------------------------------------------------------------------
CUIPlugin::CUIPlugin()
{
    ZeroMemory( m_pObjects, sizeof(m_pObjects) );
}




//-----------------------------------------------------------------------------
// Name: ~CUIPlugin()
// Desc: Destructor
//-----------------------------------------------------------------------------
CUIPlugin::~CUIPlugin()
{
    delete m_pUIXFont;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes a UI plugin object
//-----------------------------------------------------------------------------
HRESULT CUIPlugin::Initialize( CXBFont* pFont )
{
    m_pFont = pFont;

    if( m_dwRefCount == 0 )
        m_pUIXFont = new UIXFont( pFont );

    m_dwRefCount++;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RetrieveImage()
// Desc: Helper function to get an image from the plugin's skin resource
//-----------------------------------------------------------------------------
HRESULT CUIPlugin::RetrieveImage( DWORD ImageResID, StdImage* pImage )
{
    if( FAILED( m_pPluginSupport->GetImage( ImageResID, &pImage->m_pTexture ) ) )
    {
        pImage->m_pTexture = NULL;
        return E_FAIL;
    }

    D3DSURFACE_DESC desc;
    pImage->m_pTexture->GetLevelDesc( 0, &desc );
    pImage->m_dwWidth  = desc.Width;
    pImage->m_dwHeight = desc.Height;
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Release()
// Desc: Reference-counting release function
//-----------------------------------------------------------------------------
ULONG _stdcall CUIPlugin::Release()
{
    if( --m_dwRefCount > 0 )
        return m_dwRefCount;

    delete this;
    return 0L;
}




//-----------------------------------------------------------------------------
// Name: SetPluginSupport()
// Desc: Attaches an IPluginSupport object to a plugin
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::SetPluginSupport( IPluginSupport* pPluginSupport )
{
    // Remember the plugin support interface
    m_pPluginSupport = pPluginSupport;

    // Get resource for required for list box rendering such as arrows
    RetrieveImage( UIX_RES_IMG_UP_ARROW,       &m_UpArrowImage );
    RetrieveImage( UIX_RES_IMG_DOWN_ARROW,     &m_DownArrowImage );
    RetrieveImage( UIX_RES_IMG_LIST_HIGHLIGHT, &m_HighlightImage );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CreateObject()
// Desc: Callback for creating a UI object
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::CreateObject( UIX_OBJECT_TYPE ObjectType,
                                          ULONG ScreenInstance, ULONG ScreenResID,
                                          ULONG ObjectResID, ULONG* pObjectID )
{
    HRESULT hr;

    // Get the resource information
    UIX_SKIN_LAYOUT_INFO* pLayout;
    hr = m_pPluginSupport->GetLayout( ScreenResID, ObjectResID, &pLayout );
    if( FAILED(hr) )
        return hr;

    // Construct a new object
    CUIObject* pObject;

    switch( ObjectType )
    {
        case UIX_OBJECT_BACKGROUND:
            pObject = new CUIBackgroundObject();
            break;
        case UIX_OBJECT_LISTBOX:
            pObject = new CUIListBoxObject();
            break;
        case UIX_OBJECT_TEXTBOX:
            pObject = new CUITextBoxObject();
            break;
        default:
            return E_NOTIMPL;
    }

    // Store all the data we have
    pObject->m_pPlugin        = this;
    pObject->m_pLayout        = pLayout;
    pObject->m_ScreenResID    = (USHORT)ScreenResID;
    pObject->m_ObjectResID    = (USHORT)ObjectResID;
    pObject->m_ScreenInstance = ScreenInstance;

    // Create the object
    hr = pObject->Create();
    if( FAILED(hr) )
    {
        delete pObject;
        return hr;
    }

    // RXDK: hoisted out of the for-init (MSVC's old for-scope leaked it past the loop)
    DWORD i;
    for( i=0; i<100; i++ )
        if( NULL == m_pObjects[i] )
            break;

    // Link the object in the arrary
    m_pObjects[i] = pObject;
    (*pObjectID) = i;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DestroyObject()
// Desc: Callback for destroying a UI object
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::DestroyObject( DWORD ObjectID )
{
    if( NULL == m_pObjects[ObjectID] )
        return E_INVALIDARG;

    m_pObjects[ObjectID]->Destroy();
    delete m_pObjects[ObjectID];
    m_pObjects[ObjectID] = NULL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DestroyScreenObjects()
// Desc: Callback for destroying all objects for one screen
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::DestroyScreenObjects( DWORD ScreenInstance )
{
    for( DWORD i = 0; i < 100; i++ )
    {
        if( m_pObjects[i] )
        {
            if( m_pObjects[i]->m_ScreenInstance == ScreenInstance )
            {
                m_pObjects[i]->Destroy();
                delete m_pObjects[i];
                m_pObjects[i] = NULL;
            }
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderObject()
// Desc: Callback for rendering a UI object
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::RenderObject( DWORD ObjectID )
{
    if( NULL == m_pObjects[ObjectID] )
        return E_INVALIDARG;

    return m_pObjects[ObjectID]->Render( m_pObjects[ObjectID]->m_pLayout->X,
                                         m_pObjects[ObjectID]->m_pLayout->Y );
}




//-----------------------------------------------------------------------------
// Name: Clear()
// Desc: Callback for clearing a listbox
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::Clear( DWORD ObjectID, BOOL ResetSelectionIndex )
{
    if( NULL == m_pObjects[ObjectID] )
        return E_INVALIDARG;

    return m_pObjects[ObjectID]->Clear( ResetSelectionIndex );
}




//-----------------------------------------------------------------------------
// Name: SetText()
// Desc: Callback for setting text for a UI object
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::SetText( DWORD ObjectID, DWORD ItemIndex,
                                     const WCHAR* pText, DWORD IconCount,
                                     const UIX_SKIN_ICON_INFO* pIconInfo )
{
    if( NULL == m_pObjects[ObjectID] )
        return E_INVALIDARG;

    return m_pObjects[ObjectID]->SetText( ItemIndex, pText, IconCount, pIconInfo );
}




//-----------------------------------------------------------------------------
// Name: InsertItem()
// Desc: Callback for inserting an item in a listbox UI object
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::InsertItem( ULONG ObjectID, ULONG ItemIndex,
                                        ULONG* pReturnIndex )
{
    if( NULL == m_pObjects[ObjectID] )
        return E_INVALIDARG;

    return m_pObjects[ObjectID]->InsertItem( ItemIndex, pReturnIndex );
}




//-----------------------------------------------------------------------------
// Name: GetState()
// Desc: Callback for getting state from a UI object
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::GetObjectState( DWORD ObjectID, DWORD ItemIndex,
                                            UIX_OBJSTATE_TYPE State,
                                            DWORD* pValue )
{
    if( NULL == m_pObjects[ObjectID] )
        return E_INVALIDARG;

    return m_pObjects[ObjectID]->GetState( ItemIndex, State, pValue );
}




//-----------------------------------------------------------------------------
// Name: SetObjectState()
// Desc: Callback for setting state in a UI object
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::SetObjectState( DWORD ObjectID, DWORD ItemIndex,
                                            UIX_OBJSTATE_TYPE State, DWORD Value )
{
    if( NULL == m_pObjects[ObjectID] )
        return E_INVALIDARG;

    return m_pObjects[ObjectID]->SetState( ItemIndex, State, Value );
}




//-----------------------------------------------------------------------------
// Name: PassInputToObject()
// Desc: Callback for passing input to a UI object
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::PassInputToObject( ULONG ObjectID,
                                               UIX_INPUT_TYPE InputKey )
{
    if( NULL == m_pObjects[ObjectID] )
        return E_INVALIDARG;

    return m_pObjects[ObjectID]->HandleInput( InputKey );
}


//-----------------------------------------------------------------------------
// Name: GetFont()
// Desc: Returns a pointer to the font renderer which may be used directly
//       by some extended UIX features.  Built-in UIX features do not need
//       this and this function can return E_NOTIMPL.
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::GetFont( ITitleFontRenderer** ppFont )
{
    *ppFont = m_pUIXFont;
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: DoWork()
// Desc: Callback to do any per-frame work inside a plugin
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::DoWork()
{
    // Called each frame to do work. For example, we could update animations.
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetRenderTarget()
// Desc:
//-----------------------------------------------------------------------------
HRESULT _stdcall CUIPlugin::SetRenderTarget( IDirect3DSurface8* pSurface )
{
    if( pSurface )
    {
        // When this function is called with a valid surface ptr, it indicates
        // the beginning of UIX rendering. This is a good place to set state.
        return SetState( pSurface );
    }
    else
    {
        // When this function is called with a NULL surface ptr, it indicates
        // the end of UIX rendering. This is a good place to restore state.
        return RestoreState();
    }
}




//-----------------------------------------------------------------------------
// Name: SetState()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CUIPlugin::SetState( D3DSurface* pRenderTarget )
{
    // Note: Rendering a UIX screen is probably the last thing an app will do,
    //       so trashing state is okay. Otherwise, an app may want to modify
    //       this code to save state so it can be restored later

    // Use the fixed function pixel shader
    D3DDevice::SetPixelShader( NULL );

    // Switch to 96CONSTANTS mode to use fixed function pipeline
    D3DDevice::SetShaderConstantMode( D3DSCM_96CONSTANTS );

    // Switch to the given render target
    if( pRenderTarget )
    {
        D3DDevice::SetRenderTarget( pRenderTarget, NULL );
    }
    else
    {
        // If the render target is not to be changed, reset the viewport
        D3DVIEWPORT8 Viewport = { 0, 0, 0x7fffffff, 0x7fffffff, 0.0f, 1.0f };
        D3DDevice::SetViewport( &Viewport );
    }

    // Set the screen-space offset
    FLOAT fOffsetX = 0.0f;
    FLOAT fOffsetY = 0.0f;

    D3DDISPLAYMODE  DisplayMode;
    D3DDevice::GetDisplayMode( &DisplayMode );
    if( DisplayMode.Flags & D3DPRESENTFLAG_FIELD )
    {
        D3DFIELD_STATUS FieldStatus;
        D3DDevice::GetDisplayFieldStatus( &FieldStatus );
        if( FieldStatus.Field == D3DFIELD_EVEN )
            fOffsetY = 0.5f;
    }
    D3DDevice::SetScreenSpaceOffset( fOffsetX, fOffsetY );

    // Set the base render states
    D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE,            TRUE );
    D3DDevice::SetRenderState( D3DRS_ALPHATESTENABLE,             TRUE );
    D3DDevice::SetRenderState( D3DRS_DITHERENABLE,                FALSE );
    D3DDevice::SetRenderState( D3DRS_COLORWRITEENABLE,            D3DCOLORWRITEENABLE_ALL );
    D3DDevice::SetRenderState( D3DRS_SWATHWIDTH,                  D3DSWATH_OFF );
    // RXDK: D3DRS_STIPPLEENABLE is a post-leak render state (5849 value 83 collides with the
    // leak's D3DRS_FOGTABLEMODE); the leak d3d8 has no stipple, and it defaults off anyway.
    // D3DDevice::SetRenderState( D3DRS_STIPPLEENABLE,               FALSE );
    D3DDevice::SetRenderState( D3DRS_FOGENABLE,                   FALSE );
    D3DDevice::SetRenderState( D3DRS_WRAP0,                       0 );
    D3DDevice::SetRenderState( D3DRS_WRAP1,                       0 );
    D3DDevice::SetRenderState( D3DRS_LIGHTING,                    FALSE );
    D3DDevice::SetRenderState( D3DRS_TWOSIDEDLIGHTING,            FALSE );
    D3DDevice::SetRenderState( D3DRS_SPECULARENABLE,              FALSE );
    D3DDevice::SetRenderState( D3DRS_VERTEXBLEND,                 D3DVBF_DISABLE );
    D3DDevice::SetRenderState( D3DRS_FILLMODE,                    D3DFILL_SOLID );
    D3DDevice::SetRenderState( D3DRS_NORMALIZENORMALS,            FALSE );
    D3DDevice::SetRenderState( D3DRS_CULLMODE,                    D3DCULL_CCW );
    D3DDevice::SetRenderState( D3DRS_TEXTUREFACTOR,               0 );
    D3DDevice::SetRenderState( D3DRS_LOGICOP,                     D3DLOGICOP_NONE );
    D3DDevice::SetRenderState( D3DRS_EDGEANTIALIAS,               FALSE );
    D3DDevice::SetRenderState( D3DRS_MULTISAMPLERENDERTARGETMODE, D3DMULTISAMPLEMODE_1X );
    D3DDevice::SetRenderState( D3DRS_YUVENABLE,                   FALSE );
    D3DDevice::SetRenderState( D3DRS_SRCBLEND,                    D3DBLEND_SRCALPHA );
    D3DDevice::SetRenderState( D3DRS_DESTBLEND,                   D3DBLEND_INVSRCALPHA );
    D3DDevice::SetRenderState( D3DRS_BLENDOP,                     D3DBLENDOP_ADD );
    D3DDevice::SetRenderState( D3DRS_ALPHAREF,                    0 );
    D3DDevice::SetRenderState( D3DRS_ALPHAFUNC,                   D3DCMP_GREATER );
    D3DDevice::SetRenderState( D3DRS_DXT1NOISEENABLE,             TRUE );
    D3DDevice::SetRenderState( D3DRS_SOLIDOFFSETENABLE,           FALSE );
    D3DDevice::SetRenderState( D3DRS_ZBIAS,                       0 );
    D3DDevice::SetRenderState( D3DRS_ZENABLE,                     D3DZB_FALSE );
    D3DDevice::SetRenderState( D3DRS_STENCILENABLE,               FALSE );

    // Set the texture stage states
    D3DDevice::SetTexture( 0, NULL );
    D3DDevice::SetTexture( 1, NULL );
    D3DDevice::SetTexture( 2, NULL );
    D3DDevice::SetTexture( 3, NULL );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ADDRESSU,              D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ADDRESSV,              D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ADDRESSW,              D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 0, D3DTSS_MAGFILTER,             D3DTEXF_LINEAR );
    D3DDevice::SetTextureStageState( 0, D3DTSS_MINFILTER,             D3DTEXF_LINEAR );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORKEYOP,            D3DTCOLORKEYOP_DISABLE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORSIGN,             0 );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAKILL,             D3DTALPHAKILL_DISABLE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP,               D3DTOP_MODULATE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG1,             D3DTA_TEXTURE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG2,             D3DTA_DIFFUSE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAOP,               D3DTOP_SELECTARG1 );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAARG1,             D3DTA_TEXTURE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAARG2,             D3DTA_DIFFUSE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX,         0 );
    D3DDevice::SetTextureStageState( 1, D3DTSS_COLOROP,               D3DTOP_DISABLE );
    D3DDevice::SetTextureStageState( 1, D3DTSS_ALPHAOP,               D3DTOP_DISABLE );
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RestoreState()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CUIPlugin::RestoreState()
{
    // Note: Rendering a UIX screen is probably the last thing an app will do,
    //       so trashing state is okay. Otherwise, an app may want to modify
    //       this code to restore any state that was set earlier

    return S_FALSE;
}




