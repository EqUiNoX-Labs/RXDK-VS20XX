//-----------------------------------------------------------------------------
// Name: XBDynamicFont.h
//
// Desc: XBFont-derived class that can be built on the fly
//
// Hist: 09.08.03 - New for November 2003 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef XBDYNAMICFONT_H
#define XBDYNAMICFONT_H

#include <xfont.h>
#include <xbfont.h>




//-----------------------------------------------------------------------------
// Name: class CXBDynamicFont
// Desc: A class to hold all information about a texture-based font
//-----------------------------------------------------------------------------
class CXBDynamicFont : public CXBFont
{
    HRESULT BuildTranslatorTable( BYTE ValidGlyphs[65536], BOOL bIncludeNullCharacter );

    HRESULT CalculateAndRenderGlyphs( XFONT* pFont, BYTE* ValidGlyphs,
                                      BOOL bOutlineEffect, BOOL bShadowEffect );
    VOID    DestroyObjects();

public:
    HRESULT Create( XFONT* pFont, BYTE ValidGlyphs[65536],
                    DWORD dwTextureWidth, DWORD dwTextureHeight, 
                    BOOL bOutlined, BOOL bShadowed );

    CXBDynamicFont();
    ~CXBDynamicFont();
};




#endif // XBDYNAMICFONT_H
