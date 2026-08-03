//-----------------------------------------------------------------------------
// File: UIXCustomUIPlugin.h
//
// Desc: A custom UI plugin for UIX. Apps can use this code as a starting point
//       for writing their own custom UI for UIX..
//
// Hist: 11.24.03 - New for December release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef UIXCUSTOMUIPLUGIN_H
#define UIXCUSTOMUIPLUGIN_H

#include <xtl.h>
#include <uix.h>


// Forward class reference
class CUIPlugin;
class CXBFont;



// Custom icon positioning flag (feature added to default UIX behavior)
#define UIX_ICON_CURSOR 0x00008000




//-----------------------------------------------------------------------------
// Name: class UIXFontWrapper
// Desc: A wrapper around a CXBFont class that works with UIX
//-----------------------------------------------------------------------------
class UIXFontWrapper : public ITitleFontRenderer
{
    CXBFont* m_pFont;
    DWORD    m_dwCurrentColor;

public:
    UIXFontWrapper( CXBFont* pFont = NULL );
    
    VOID SetFont( CXBFont* pFont );
    
    virtual ULONG   __stdcall Release();
    virtual HRESULT __stdcall SetHeight( DWORD Height ) ;
    virtual HRESULT __stdcall SetColor( D3DCOLOR Color );
    virtual HRESULT __stdcall DrawText( const WCHAR* pText,
                                        DWORD X, DWORD Y, DWORD dwMaxWidth );
    virtual HRESULT __stdcall GetTextSize( const WCHAR* pText,
                                           DWORD* pWidth, DWORD* pHeight );
};




//-----------------------------------------------------------------------------
// Name: class CUIObject
// Desc: Base object (textbox, listbox, etc.) for the UIPlugin
//-----------------------------------------------------------------------------
class CUIObject
{
public:
    // Public member variables
    UIX_OBJECT_TYPE         m_ObjectType;
    DWORD                   m_Flags;
    CUIPlugin*              m_pPlugin;
    PUIX_SKIN_LAYOUT_INFO   m_pLayout;
    USHORT                  m_ScreenResID;
    USHORT                  m_ObjectResID;
    DWORD                   m_ScreenInstance;

    virtual HRESULT Create()
    { return E_NOTIMPL; }

    virtual VOID    Destroy()
    { return; }

    virtual HRESULT HandleInput( UIX_INPUT_TYPE InputKey )
    { return S_FALSE; }

    virtual HRESULT GetState( DWORD ItemIndex, UIX_OBJSTATE_TYPE State, DWORD* pValue )
    { return E_NOTIMPL; }

    virtual HRESULT SetState( DWORD ItemIndex, UIX_OBJSTATE_TYPE State,
                              DWORD Value )
    { return E_NOTIMPL; }

    virtual HRESULT Clear( BOOL ResetSelectionIndex )
    { return E_NOTIMPL; }

    virtual HRESULT InsertItem( ULONG ItemIndex, ULONG* pReturnIndex )
    { return E_NOTIMPL; }

    virtual HRESULT Render( FLOAT X, FLOAT Y ) = 0;

    virtual HRESULT SetText( DWORD ItemIndex, LPCWSTR pText, DWORD IconCount,
                             const UIX_SKIN_ICON_INFO* pIconInfo )
    { return E_NOTIMPL; }

public:

    // Functions that do D3D related stuff
    VOID DrawTexture( IDirect3DTexture8* pTexture, FLOAT X, FLOAT Y,
                      FLOAT RenderWidth, FLOAT RenderHeight );

    CUIObject() {}
    ~CUIObject() {}
};




//-----------------------------------------------------------------------------
// Name: class CUITextBoxObject
// Desc: A UI object for rendering text with embedded icons
//-----------------------------------------------------------------------------
class CUITextBoxObject : public CUIObject
{
    struct TEXTBOX_ICON
    {
        D3DTexture* pTexture;
        FLOAT       fWidth;
        FLOAT       fHeight;
        DWORD       dwInsertPosInText;
        DWORD       dwFlags;
    };

public:
    BOOL                m_bIsGreyed;
    BOOL                m_bWrapText;

    TEXTBOX_ICON*       m_Icons;
    DWORD               m_IconCount;

    WCHAR*              m_Text;

public:
    HRESULT Create();
    HRESULT SetState( DWORD ItemIndex, UIX_OBJSTATE_TYPE State, DWORD Value );
    HRESULT SetText( DWORD ItemIndex, LPCWSTR pText, DWORD IconCount,
                     const UIX_SKIN_ICON_INFO* pIconInfo );
    VOID    WrapText();
    HRESULT Render( FLOAT X, FLOAT Y );
    VOID    Destroy();
};




//-----------------------------------------------------------------------------
// Name: class CUIListBoxObject
// Desc: A UI object for rendering a listbox
//-----------------------------------------------------------------------------
class CUIListBoxObject : public CUIObject
{
    enum { DEFAULT_LB_VISIBLE_ITEMS = 5 };

public:
    DWORD                   m_CurrentIndex;
    DWORD                   m_TopIndex;
    DWORD                   m_dwMaxVisibleCount;

    CUITextBoxObject*       m_ChildObjectArray[100];
    DWORD                   m_dwNumChildObjects;

public:
    HRESULT Create();
    HRESULT Clear( BOOL ResetSelectionIndex );
    HRESULT InsertItem( ULONG ItemIndex, ULONG* pReturnIndex );
    HRESULT HandleInput( UIX_INPUT_TYPE InputKey );
    HRESULT GetState( DWORD ItemIndex, UIX_OBJSTATE_TYPE State, DWORD* pValue );
    HRESULT SetState( DWORD ItemIndex, UIX_OBJSTATE_TYPE State, DWORD Value );
    HRESULT SetText( DWORD ItemIndex, LPCWSTR pText, DWORD IconCount,
                     const UIX_SKIN_ICON_INFO* pIconInfo );
    HRESULT Render( FLOAT X, FLOAT Y );
    VOID    Destroy();
    VOID    DestroyChildObjects();
};




//-----------------------------------------------------------------------------
// Name: class CUIBackgroundObject
// Desc: A UI object for rendering a background
//-----------------------------------------------------------------------------
class CUIBackgroundObject : public CUIObject
{
    D3DTexture* m_pTexture;

public:
    HRESULT Create();
    HRESULT Render( FLOAT X, FLOAT Y );
};




//-----------------------------------------------------------------------------
// Name: class CUIPlugin
// Desc: A custom UI plugin to replace the default UI in UIX
//-----------------------------------------------------------------------------
class CUIPlugin : public ITitleUIPlugin
{
    DWORD           m_dwRefCount;

    CUIObject*      m_pObjects[100];

    struct StdImage
    {
        D3DTexture* m_pTexture;
        DWORD       m_dwWidth;
        DWORD       m_dwHeight;
    };

    HRESULT RetrieveImage( DWORD ImageResID, StdImage* pImage );

    HRESULT SetState( D3DSurface* pSurface );
    HRESULT RestoreState();

public:
    CXBFont*        m_pFont;
    UIXFontWrapper  m_UIXFont;

    IPluginSupport* m_pPluginSupport;

    StdImage        m_UpArrowImage;
    StdImage        m_DownArrowImage;
    StdImage        m_HighlightImage;

public:
    CUIPlugin();
    ~CUIPlugin();

    HRESULT Initialize( CXBFont* pFont );

    // ITitleUIPlugin methods
    ULONG   _stdcall Release();

    HRESULT _stdcall SetPluginSupport( IPluginSupport* pPluginSupport );

    HRESULT _stdcall CreateObject( UIX_OBJECT_TYPE ObjectType,
                                   DWORD ScreenInstance, DWORD ScreenResID,
                                   DWORD ObjectResID, DWORD* pObjectID );

    HRESULT _stdcall DestroyObject( DWORD ObjectID );

    HRESULT _stdcall DestroyScreenObjects( DWORD ScreenInstance );

    HRESULT _stdcall SetRenderTarget( IDirect3DSurface8* pSurface );

    HRESULT _stdcall RenderObject( DWORD ObjectID );

    HRESULT _stdcall Clear( DWORD ObjectID, BOOL ResetSelectionIndex );

    HRESULT _stdcall SetText( DWORD ObjectID, DWORD ItemIndex, LPCWSTR strText,
                              DWORD IconCount, const UIX_SKIN_ICON_INFO* pIconInfo );

    HRESULT _stdcall InsertItem( DWORD ObjectID, ULONG ListIndex,
                                 DWORD* pReturnIndex );

    HRESULT _stdcall SetObjectState( DWORD ObjectID, DWORD ItemIndex,
                                     UIX_OBJSTATE_TYPE State, DWORD Value );

    HRESULT _stdcall GetObjectState( DWORD ObjectID, DWORD ItemIndex,
                                     UIX_OBJSTATE_TYPE State, DWORD* pValue );

    HRESULT _stdcall PassInputToObject( DWORD ObjectID, UIX_INPUT_TYPE InputKey );

    HRESULT _stdcall DoWork();

    HRESULT _stdcall GetFont( ITitleFontRenderer** ppFont );
};




#endif // UIXCUSTOMUIPLUGIN_H
