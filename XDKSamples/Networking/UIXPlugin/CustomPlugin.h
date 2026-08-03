//-----------------------------------------------------------------------------
// File: CustomPlugin.h
//
// Desc: A custom UI plugin for UIX. Apps can use this code as a starting point
//       for writing their own custom UI for UIX..
//
// Hist: 07.14.03 - New for August release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef CUSTOMPLUGIN_H
#define CUSTOMPLUGIN_H

// Include the resource file created by running skinbld with the /header option
// This gives us the offsets for the different images.
// If this file does not exist that means you need to run copytoxb.bat. That
// batch file builds the resources, copies them to the Xbox, and generates this
// header file. The #pragma message is to remind you of this - if you have
// already run the batch file you can ignore it. Be sure to rerun the batch
// file with each new version of the XDK.
#pragma message("You must run copytoxb.bat before building this sample in order to build sk_res.h")
#include "sk_res.h"



//-----------------------------------------------------------------------------
// Name: enum UIX_RES_IMGAGE_ENUM
// Desc: Enumerated list of resource constants
//-----------------------------------------------------------------------------
enum UIX_RES_IMGAGE_ENUM
{
    UIX_RES_IMG_BLANK                        = IMG_BLANK,
    UIX_RES_IMG_A                            = IMG_A,
    UIX_RES_IMG_B                            = IMG_B,
    UIX_RES_IMG_X                            = IMG_X,
    UIX_RES_IMG_Y                            = IMG_Y,
    UIX_RES_IMG_FRIEND_ONLINE                = IMG_FRIEND_ONLINE,
    UIX_RES_IMG_FRIENDS_REQUEST_RECEIVED     = IMG_FRIENDS_REQUEST_RECEIVED,
    UIX_RES_IMG_FRIENDS_REQUEST_SENT         = IMG_FRIENDS_REQUEST_SENT,
    UIX_RES_IMG_GAME_INVITE_RECV             = IMG_GAME_INVITE_RECV,
    UIX_RES_IMG_GAME_INVITE_SENT             = IMG_GAME_INVITE_SENT,
    UIX_RES_IMG_VOICE_HEADSET                = IMG_VOICE_HEADSET,
    UIX_RES_IMG_FRIEND_ONLINE_ON             = IMG_FRIEND_ONLINE_ON,
    UIX_RES_IMG_FRIENDS_REQUEST_RECEIVED_ON  = IMG_FRIENDS_REQUEST_RECEIVED_ON,
    UIX_RES_IMG_FRIENDS_REQUEST_SENT_ON      = IMG_FRIENDS_REQUEST_SENT_ON,
    UIX_RES_IMG_GAME_INVITE_RECV_ON          = IMG_GAME_INVITE_RECV_ON,
    UIX_RES_IMG_GAME_INVITE_SENT_ON          = IMG_GAME_INVITE_SENT_ON,
    UIX_RES_IMG_VOICE_HEADSET_ON             = IMG_VOICE_HEADSET_ON,
    UIX_RES_IMG_PASSCODE_BLANK               = IMG_PASSCODE_BLANK,
    UIX_RES_IMG_PASSCODE_FILLED              = IMG_PASSCODE_FILLED,
    UIX_RES_IMG_UP_ARROW                     = IMG_UP_ARROW,
    UIX_RES_IMG_DOWN_ARROW                   = IMG_DOWN_ARROW,
    UIX_RES_IMG_LIST_HIGHLIGHT               = IMG_LIST_HIGHLIGHT,
    UIX_RES_IMG_SPIN_1                       = IMG_SPIN_1,
    UIX_RES_IMG_SPIN_2                       = IMG_SPIN_2,
    UIX_RES_IMG_SPIN_3                       = IMG_SPIN_3,
    UIX_RES_IMG_SPIN_4                       = IMG_SPIN_4,
};


class CUIPlugin;




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
    UIXFont*        m_pUIXFont;

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




#endif // CUSTOMPLUGIN_H
