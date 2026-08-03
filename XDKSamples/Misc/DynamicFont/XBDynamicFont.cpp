//-----------------------------------------------------------------------------
// Name: XBDynamicFont.cpp
//
// Desc: XBFont-derived class that can be built on the fly. An app would use
//       this class after downloading new text stings and needing to guarantee
//       that it's font can display all the glyphs in the new text strings.
//       Since XFont is too slow to run in real-time, this solution uses XFont
//       just one time, to render into a texture that can then be used by the
//       CXBFont base class. In effect, this sample is akin to the XDK's
//       FontMaker tool running on the Xbox.
//
// Hist: 09.08.03 - New for November 2003 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#define XFONT_TRUETYPE
#include <xtl.h>
#include <xfont.h>
#include <xgraphics.h>
#include "XBDynamicFont.h"




//-----------------------------------------------------------------------------
// Name: XFONT_GetCharacterData() 
// Desc: Exposes an XFONT internal function that returns glyph data
//-----------------------------------------------------------------------------
struct Glyph
{
    BYTE uBitmapHeight; // Height of bitmap for the glyph
    BYTE uBitmapWidth;  // Height of bitmap for the glyph
    BYTE uAdvance;      // # of pixels to advance to get to the next character
    CHAR iBearingX;     // Horizontal offset to the left side of the bitmap, may be negative
    CHAR iBearingY;     // Vertical offset to the top of the bitmap, may be negative
};

extern "C" HRESULT __fastcall XFONT_GetCharacterData( XFONT*, WCHAR, Glyph**, unsigned* );




//-----------------------------------------------------------------------------
// Name: CXBDynamicFont()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBDynamicFont::CXBDynamicFont()
               :CXBFont()
{
    // Glyph info
    m_cMaxGlyph             = 0;
    m_TranslatorTable       = NULL;
    m_dwNumGlyphs           = 0;
    m_Glyphs                = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CXBDynamicFont()
// Desc: Destructor
//-----------------------------------------------------------------------------
CXBDynamicFont::~CXBDynamicFont()
{
    DestroyObjects();

}




//-----------------------------------------------------------------------------
// Name: DestroyObjects()
// Desc: Cleans up all allocated resources for the class
//-----------------------------------------------------------------------------
VOID CXBDynamicFont::DestroyObjects()
{
    if( m_Glyphs )
        delete[] m_Glyphs;
    if( m_TranslatorTable )
        delete[] m_TranslatorTable;
    if( m_pFontTexture )
        m_pFontTexture->Release();

    m_cMaxGlyph       = 0;
    m_dwNumGlyphs     = 0;
    m_Glyphs          = NULL;
    m_TranslatorTable = NULL;
    m_pFontTexture    = NULL;
}




//-----------------------------------------------------------------------------
// Name: BuildTranslatorTable()
// Desc: Builds a table to translate from a WCHAR to a glyph index.
//-----------------------------------------------------------------------------
HRESULT CXBDynamicFont::BuildTranslatorTable( BYTE ValidGlyphs[65536], 
                                              BOOL bIncludeNullCharacter )
{
    // Insure the \0 is there
    if( bIncludeNullCharacter && 0 == ValidGlyphs['\0'] )
        ValidGlyphs[0] = 1;

    // Find the highest glyph and the total number of glyphs
    m_cMaxGlyph   = 0;
    m_dwNumGlyphs = 0;

    for( DWORD i=0; i<65536; i++ )
    {
        if( ValidGlyphs[i] )
        {
            if( i > m_cMaxGlyph )
                m_cMaxGlyph = (WCHAR)i;

            m_dwNumGlyphs++;
        }
    }

    // Fill the string of all valid glyphs and build the translator table
    if( m_TranslatorTable )
        delete[] m_TranslatorTable;
    m_TranslatorTable = new SHORT[m_cMaxGlyph+1];
    ZeroMemory( m_TranslatorTable, sizeof(SHORT)*(m_cMaxGlyph+1) );

    DWORD dwGlyph = 0;
    for( DWORD i=0; i<65536; i++ )
    {
        if( ValidGlyphs[i] )
        {
            m_TranslatorTable[i] = (SHORT)dwGlyph;
            dwGlyph++;
        }
    }
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: AlphaBlt() 
// Desc: Manually alpha blends one bitmap into another
//-----------------------------------------------------------------------------
VOID AlphaBlt( DWORD* pDst, DWORD* pSrc, DWORD dwWidth, DWORD dwHeight,
               DWORD x, DWORD y, DWORD w, DWORD h )
{
    // Don't write pixels that are out of bounds
    if( y+h > dwHeight )
        return;

    // Loop through every pixel (slow!) and perform the alpha blend
    for( DWORD j=y; j<y+h; j++ )
    {
        for( DWORD i=x; i<x+w; i++ )
        {
            DWORD src  = pSrc[j*dwWidth+i];
            DWORD dst  = pDst[j*dwWidth+i];

            if( 0 == ( 0xff000000 & src ) )
                continue;

            FLOAT alpha = (FLOAT)( ( 0xff000000 & src ) >> 24L ) / 255.0f;

            FLOAT srcr  = (FLOAT)( ( 0x00ff0000 & src ) >> 16L );
            FLOAT srcg  = (FLOAT)( ( 0x0000ff00 & src ) >>  8L );
            FLOAT srcb  = (FLOAT)( ( 0x000000ff & src ) >>  0L );
            
            FLOAT dstr  = (FLOAT)( ( 0x00ff0000 & dst ) >> 16L );
            FLOAT dstg  = (FLOAT)( ( 0x0000ff00 & dst ) >>  8L );
            FLOAT dstb  = (FLOAT)( ( 0x000000ff & dst ) >>  0L );

            DWORD r = (DWORD)( (alpha)*srcr + (1-alpha)*dstr );
            DWORD g = (DWORD)( (alpha)*srcg + (1-alpha)*dstg );
            DWORD b = (DWORD)( (alpha)*srcb + (1-alpha)*dstb );

            pDst[j*dwWidth+i] = 0xff000000 | (r<<16L) | (g<<8L) | (b<<0L);
        }
    }
}




//-----------------------------------------------------------------------------
// Name: CalculateAndRenderGlyphs() 
// Desc: Draws the list of font glyphs. Note that this function is guaranteed
//       to be slow.
//-----------------------------------------------------------------------------
HRESULT CXBDynamicFont::CalculateAndRenderGlyphs( XFONT* pFont, BYTE* ValidGlyphs,
                                                  BOOL bOutlined, BOOL bShadowed )
{
    D3DSURFACE_DESC desc;
    m_pFontTexture->GetLevelDesc( 0, &desc );
    DWORD dwTextureWidth  = desc.Width;
    DWORD dwTextureHeight = desc.Height;

    // Allocate memory for two bitmaps. The first is drawn into with XFONT and
    // the second is used to compose multiple passes. This is (unfortunately)
    // necessary because the XFONT API do not perform true alpha-blending
    DWORD* pMem1 = new DWORD[dwTextureWidth*dwTextureHeight];
    DWORD* pMem2 = new DWORD[dwTextureWidth*dwTextureHeight];
    ZeroMemory( pMem1, sizeof(DWORD)*dwTextureWidth*dwTextureHeight );
    ZeroMemory( pMem2, sizeof(DWORD)*dwTextureWidth*dwTextureHeight );
    for( DWORD i=0; i<dwTextureWidth*dwTextureHeight; i++ )
        pMem2[i] = 0xff0000ff;

    // Define a macro for drawing text. This uses XFONT to write a glyph out to
    // memory, then calls AlphaBlt() to blend in the glyph with other passes.
    #define DRAWTEXT(x,y)                                             \
    {                                                                 \
        pFont->TextOutToMemory( pMem1, dwTextureWidth*sizeof(DWORD),  \
                                dwTextureWidth, dwTextureHeight,      \
                                D3DFMT_LIN_A8R8G8B8, str, 1, x, y );  \
        AlphaBlt( pMem2, pMem1, dwTextureWidth, dwTextureHeight,      \
                  (DWORD)x, (DWORD)y, (DWORD)w, (DWORD)h );           \
    }

    // Setup the font
    const DWORD COLOR_WHITE   = 0xffffffff;
    const DWORD COLOR_BLACK   = 0xff000000;
    const DWORD COLOR_BLUE    = 0xff0000ff;
    pFont->SetTextColor( COLOR_WHITE );
    pFont->SetBkMode( XFONT_TRANSPARENT );
    pFont->SetBkColor( COLOR_BLUE );

    // Determine padding for outline and shadow effects
    int left_padding   = ( bOutlined ? 1 : 0 );
    int right_padding  = ( bOutlined ? ( bShadowed ? 2 : 1 ) : ( bShadowed ? 2 : 0 ) );
    int top_padding    = ( bOutlined ? 1 : 0 );
    int bottom_padding = ( bOutlined ? ( bShadowed ? 2 : 1 ) : ( bShadowed ? 2 : 0 ) );
    
    // Font metrics
    m_fFontTopPadding    = (FLOAT)top_padding;
    m_fFontBottomPadding = (FLOAT)bottom_padding;
    m_fFontYAdvance      = (FLOAT)pFont->GetTextHeight();
    m_fFontHeight        = m_fFontYAdvance + m_fFontTopPadding + m_fFontBottomPadding;
    UINT size_cy         = pFont->GetTextHeight();

    // Allocate memory for the glyph data structures
    if( m_Glyphs )
        delete[] m_Glyphs;
    m_Glyphs = new GLYPH_ATTR[m_dwNumGlyphs];
    
    // Loop through the entire glyph range, and output the valid glyphs.
    DWORD index = 0;
    DWORD dwLeftOrigin = 1;
    DWORD dwTopOrigin  = 1;
    DWORD x = dwLeftOrigin;
    DWORD y = dwTopOrigin;

    for( DWORD i=0; i<65536; i++ )
    {
        if( 0 == ValidGlyphs[i] )
            continue;

        WCHAR str[2] = L"A";
        str[0] = (WCHAR)i;

        // Set an unprintable character
        if( i==0 && ValidGlyphs[i] == 1 )
            str[0] = (WCHAR)0xffff;

        // Get glyph spacing information from XFONT
        Glyph*   pGlyph;
        unsigned cbGlyphSize;
        XFONT_GetCharacterData( pFont, (WCHAR)i, &pGlyph, &cbGlyphSize );
        
        // Get the ABC widths for the letter
        struct ABC { int abcA, abcB, abcC; } abc;
        abc.abcA = pGlyph->iBearingX;
        abc.abcB = pGlyph->uBitmapWidth;
        abc.abcC = pGlyph->uAdvance - pGlyph->uBitmapWidth - pGlyph->iBearingX;

        int w = abc.abcB;
        int h = size_cy;

        // Advance to the next line, if necessary
        if( x + w + left_padding + right_padding + 1 >= (int)dwTextureWidth )
        {
            x  = dwLeftOrigin;
            y += h + top_padding + bottom_padding + 1;
        }

        int sx = x;
        int sy = y;

        // Adjust ccordinates to account for the leading edge
        if( abc.abcA >= 0 )
            x += abc.abcA;
        else
            sx -= abc.abcA;

        // Add padding to the width and height
        w += left_padding + right_padding;
        h += top_padding + bottom_padding;
        abc.abcA -= left_padding;
        abc.abcB += left_padding + right_padding;
        abc.abcC -= right_padding;

        if( bOutlined )
        {
            sx++;
            sy++;
        }

        // Draw the outline
        if( bOutlined )
        {
            pFont->SetTextColor( COLOR_BLACK );
            DRAWTEXT( sx-1, sy-1 );
            DRAWTEXT( sx+0, sy-1 );
            DRAWTEXT( sx+1, sy-1 );
            DRAWTEXT( sx-1, sy+0 );
            DRAWTEXT( sx+1, sy+0 );
            DRAWTEXT( sx-1, sy+1 );
            DRAWTEXT( sx+0, sy+1 );
            DRAWTEXT( sx+1, sy+1 );
        }

        // Draw the drop-shadow
        if( bShadowed )
        {
            pFont->SetTextColor( COLOR_BLACK );
            DRAWTEXT( sx+2, sy+2 );
        }

        // Draw the letter
        pFont->SetTextColor( COLOR_WHITE );
        DRAWTEXT( sx, sy );

        // Store the glyph attributes
        m_Glyphs[index].tu1      = ((FLOAT)(x+0)) / dwTextureWidth;
        m_Glyphs[index].tv1      = ((FLOAT)(y+0)) / dwTextureHeight;
        m_Glyphs[index].tu2      = ((FLOAT)(x+w)) / dwTextureWidth;
        m_Glyphs[index].tv2      = ((FLOAT)(y+h)) / dwTextureHeight;
        m_Glyphs[index].wOffset  = (short)(abc.abcA);
        m_Glyphs[index].wWidth   = (short)(abc.abcB);
        m_Glyphs[index].wAdvance = (short)(abc.abcB + abc.abcC);
        index++;

        // Advance the cursor to the next position
        x += w + 1;
    }

    // Post-process the texture data to create a swizzled texture with alpha
    D3DLOCKED_RECT lock;
    m_pFontTexture->LockRect( 0, &lock, 0, 0 );
    {
        // Convert the data to an alpha channel
        for( DWORD i=0; i<dwTextureWidth*dwTextureHeight; i++ )
        {
            FLOAT r = ( ( 0x00ff0000 & pMem2[i] ) >> 16L ) / 255.0f;
            FLOAT g = ( ( 0x0000ff00 & pMem2[i] ) >>  8L ) / 255.0f;
            FLOAT b = ( ( 0x000000ff & pMem2[i] ) >>  0L ) / 255.0f;

            FLOAT a = r + (1-b); // Extract alpha from above
            FLOAT l = r / a;     // Extract lum from above

            WORD alpha = (WORD)( a * 15.0f );
            WORD lum   = (WORD)( l * 15.0f );

            ((WORD*)lock.pBits)[i] = (alpha<<12L) | (lum<<8L) | (lum<<4L) | (lum<<0L);
        }

        // Swizzle the result
        XGSwizzleRect( lock.pBits, 0, NULL, pMem1, dwTextureWidth, dwTextureHeight, NULL, sizeof(WORD) );
        memcpy( lock.pBits, pMem1, sizeof(WORD)*dwTextureWidth*dwTextureHeight );
    }
    m_pFontTexture->UnlockRect( 0 );

    delete[] pMem1;
    delete[] pMem2;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Create the font's internal objects (texture and array of glyph info)
//-----------------------------------------------------------------------------
HRESULT CXBDynamicFont::Create( XFONT* pFont, BYTE ValidGlyphs[65536],
                                DWORD dwTextureWidth, DWORD dwTextureHeight,
                                BOOL bOutlined, BOOL bShadowed )
{
    // Create the texture
    D3DDevice_CreateTexture( dwTextureWidth, dwTextureHeight, 1, 0,
                             D3DFMT_A4R4G4B4, 0, &m_pFontTexture );

    // Build the translator table (compute m_dwNumGlyphs, m_cMaxGlyph,
    // and m_TranslatorTable from the array of ValidGlyphs)
    BuildTranslatorTable( ValidGlyphs, TRUE );

    // Allocate glyphs
    CalculateAndRenderGlyphs( pFont, ValidGlyphs, bOutlined, bShadowed );

    // Creates the vertex shader for rendering fonts, and a pixel shader for
    // font rendering to ensure NTSC safe colors
    if( FAILED( CreateShaders() ) )
        return E_FAIL;

    // Determine whether we should save/restore state
    D3DDEVICE_CREATION_PARAMETERS d3dcp;
    D3DDevice::GetCreationParameters( &d3dcp );
    m_bSaveState = (d3dcp.BehaviorFlags&D3DCREATE_PUREDEVICE) ? FALSE : TRUE;

    return S_OK;
}



