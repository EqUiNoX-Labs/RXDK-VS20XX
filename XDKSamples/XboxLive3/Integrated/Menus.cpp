//-------------------------------------------------------------------------------------
// File: Menus.cpp
//
// Desc: Holds menu utilitys and constant text used in the sample for menu choices.
//       Provides menu selection and rendering helpers.
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "Menus.h"
#include "xbinput.h"
#include "XBStopWatch.h"


//-------------------------------------------------------------------------------------
// Name: GetMenuPosition()
// Desc: Returns the postion the cursion should be at after the given input.
//       This variant is intended for scrolling menus and modifies and returns
//       the render start variable.
//-------------------------------------------------------------------------------------
DWORD GetMenuPosition( DWORD dwCurMenuPosition,
                       Event event,
                       DWORD& dwMenuRenderStart,
                       DWORD dwNumMenuItems,
                       DWORD dwNumItemsPerScreen )
{
    switch( event )
    {
    case EV_UP:
        // Move the roster list up until we hit the top
        dwCurMenuPosition = ( dwCurMenuPosition > 0 ) ? ( dwCurMenuPosition - 1 ) : 0;

        if( dwCurMenuPosition < dwMenuRenderStart )
            dwMenuRenderStart  = ( dwMenuRenderStart > 0 ) ? ( dwMenuRenderStart - 1 ) : 0;

        break;

    case EV_DOWN:
        // Move the roster list down until we hit the bottom
        dwCurMenuPosition = 
            ( dwCurMenuPosition < ( dwNumMenuItems - 1 ) ) ? 
            ( dwCurMenuPosition + 1 ) : 
            dwCurMenuPosition;

        if( dwCurMenuPosition >= ( dwMenuRenderStart + dwNumItemsPerScreen ) )
        {
            ++dwMenuRenderStart;

            if( dwMenuRenderStart > ( dwNumMenuItems - dwNumItemsPerScreen ) )
                dwMenuRenderStart = dwNumMenuItems - dwNumItemsPerScreen;
        }
        break;
    }

    return dwCurMenuPosition;
}

//-------------------------------------------------------------------------------------
// Name: GetMenuPosition()
// Desc: Takes the current menu position, the number of items in the menu
//       and the menu event and returns the new menu position. Handles wrap-around
//       of the menu in both directions
//-------------------------------------------------------------------------------------
INT GetMenuPosition( INT iCurMenuPosition,
                     INT iNumMenuItems,
                     Event event,
                     BOOL bMenuWrap )
{
    switch( event )
    {
    case EV_UP:
        --iCurMenuPosition;

        if( bMenuWrap )
        {
            // Wrap the input to goto the bottom
            iCurMenuPosition = ( iCurMenuPosition < 0 ) ?
                                 ( iNumMenuItems - 1 ) : iCurMenuPosition;
        }
        else
        {
            // Don't wrap. Just stick to the first index
            iCurMenuPosition = ( iCurMenuPosition < 0 ) ?
                                 0 : iCurMenuPosition;
        }

        break;

    case EV_DOWN:
        ++iCurMenuPosition;

        if( bMenuWrap )
        {
        // Wrap the input to goto the top
            iCurMenuPosition = ( iCurMenuPosition >= iNumMenuItems ) ?
                                 0 : iCurMenuPosition;
        }
        else
        {
            // Don't wrap. Just stick to the last index
            iCurMenuPosition = ( iCurMenuPosition >= iNumMenuItems ) ?
                                 ( iNumMenuItems - 1 ) : iCurMenuPosition;
        }

        // Wrap the input to goto the top
        iCurMenuPosition = ( iCurMenuPosition >= iNumMenuItems ) ?
                             0 : iCurMenuPosition;
    }

    return iCurMenuPosition;
}

//-------------------------------------------------------------------------------------
// Name: GetEvent()
// Desc: Returns the state of the controller
//-------------------------------------------------------------------------------------
Event GetEvent()
{
    return ( GetEvent( 0 ) );
}

//-------------------------------------------------------------------------------------
// Name: GetEvent()
// Desc: Returns the state of the controller at the given port
//-------------------------------------------------------------------------------------
Event GetEvent( const INT iController )
{
    // Use a timer repeat a button press
    // when the user holds down the D-PAD
    static CXBStopWatch g_repeatTimer;

    const  FLOAT REPEAT_RATE = 0.25f;

    BOOL bRepeat = FALSE;

    if( !g_repeatTimer.IsRunning() )
        g_repeatTimer.StartZero();

    if( g_repeatTimer.GetElapsedSeconds() > REPEAT_RATE )
    {
        g_repeatTimer.StartZero();
        bRepeat = TRUE;
    }

    // "A"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
        return EV_BUTTON_A;

    // "B"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        return EV_BUTTON_B;

    // "X"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
        return EV_BUTTON_X;

    // "Y"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
        return EV_BUTTON_Y;

    // "Back"
    if( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_BACK )
        return EV_BUTTON_BACK;

    // "Start"
    if( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_START )
        return EV_BUTTON_START;

    // "White"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
        return EV_BUTTON_WHITE;

    // "Black"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK] )
        return EV_BUTTON_BLACK;


    // Movement

    if( ( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        || ( bRepeat && ( g_Gamepads[iController].wLastButtons & XINPUT_GAMEPAD_DPAD_UP ) ) )
    {
        g_repeatTimer.StartZero();
        return EV_UP;
    }

    if( ( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        || ( bRepeat && ( g_Gamepads[iController].wLastButtons & XINPUT_GAMEPAD_DPAD_DOWN ) ) )
    {
        g_repeatTimer.StartZero();
        return EV_DOWN;
    }

    if( ( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        || ( bRepeat && ( g_Gamepads[iController].wLastButtons & XINPUT_GAMEPAD_DPAD_LEFT ) ) )
    {
        g_repeatTimer.StartZero();
        return EV_LEFT;
    }

    if( ( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        || ( bRepeat && ( g_Gamepads[iController].wLastButtons & 
             XINPUT_GAMEPAD_DPAD_RIGHT ) ) )
    {
        g_repeatTimer.StartZero();
        return EV_RIGHT;
    }


    return EV_NULL;
}

//-------------------------------------------------------------------------------------
// Name: RenderFooter()
// Desc: Renders a footer at the bottom of the screen. Takes a bitflag to
//       determine which items to render.
//-------------------------------------------------------------------------------------
VOID RenderFooter( CXBFont& font, WORD flags )
{
    if( flags & FOOTER_RENDER_CANCEL )
    {
        // Bottom Help text
        font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                       COLOR_NORMAL, GLYPH_B_BUTTON L" back",
                       XBFONT_LEFT );
    }

    if( flags & FOOTER_RENDER_SELECT )
    {
        font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                       COLOR_NORMAL, GLYPH_A_BUTTON L" select",
                       XBFONT_RIGHT );
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderMenu()
// Desc: Draws the given menu to the screen along with a point next to the
//       currently selected item
//-------------------------------------------------------------------------------------
VOID RenderMenu( CXBFont& font,
                 const WCHAR* strMenuName,
                 const WCHAR** rwMenuText,
                 const WORD wNumMenuItems,
                 const INT iCurMenuItem )
{
    // Menu Title
    font.DrawText( SCREEN_CENTER_X,
                   POS_SCREEN_TITLE_Y,
                   COLOR_NORMAL,
                   strMenuName,
                   XBFONT_CENTER_X );

    // Attempt to center the menu
    float fMenuStartPos = SCREEN_CENTER_Y - ( wNumMenuItems * DEFAULT_TEXT_PADDING * 0.5f );

    assert( ( fMenuStartPos > POS_SCREEN_TITLE_Y ) && "Menu too large!" );

    // If we are given an empty menu just render
    // the name of the screen and the footer
    if( wNumMenuItems > 0 )
    {
        // Menu Items
        for( WORD i = 0; i < wNumMenuItems; ++i )
        {
            // Highlight the selected item
            DWORD dwColor = ( iCurMenuItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;

            font.DrawText( SCREEN_CENTER_X,
                           fMenuStartPos + (DEFAULT_TEXT_PADDING * i),
                           dwColor, rwMenuText[i], XBFONT_CENTER_X );
        }

        // Show selected item with a little triangle
        FLOAT fTextOffset   = font.GetTextWidth( rwMenuText[ iCurMenuItem ] ) / 2.0f;
        FLOAT fTextPos      = SCREEN_CENTER_X -
                              ( fTextOffset + font.GetTextWidth( GLYPH_RIGHT_TICK ) );

        font.DrawText( fTextPos,
                       fMenuStartPos + ( DEFAULT_TEXT_PADDING * iCurMenuItem ),
                       COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_CENTER_X );
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderScrollingMenu()
// Desc: Draws the given menu to the screen along with a point next to the
//       currently selected item
//-------------------------------------------------------------------------------------
VOID RenderScrollingMenu( CXBFont& font,
                          const WCHAR* wszMenuName,
                          DWORD dwRenderStart,
                          DWORD dwItemSelected,
                          DWORD dwNumItems,
                          const WCHAR* wszLeftHeader,
                          MENU_LIST rwLeftList,
                          const WCHAR* wszRightHeader,
                          MENU_LIST rwRightList,
                          BOOL bZeroIndexed )
{
    // Render the name of the menu
    RenderMenu( font, wszMenuName, NULL, 0, 0 );

    FLOAT fListStartY = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2 );
    FLOAT fListStartX = SCREEN_CENTER_X * 0.25f;
    FLOAT fRightX     = SCREEN_SIZE_X * 0.9f;
    DWORD dwColor     = COLOR_WHITE;


    if( wszLeftHeader )
    {
        // Render the column headers
        font.DrawText( fListStartX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                       COLOR_GREEN,
                       wszLeftHeader,
                       XBFONT_LEFT );
    }

    if( wszRightHeader )
    {
        font.DrawText( fRightX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                       COLOR_GREEN,
                       wszRightHeader,
                       XBFONT_RIGHT );
    }

    // If we are starting the render from a position
    // other than the first user in the list
    // the draw a little arrow on the side telling
    // the user they can scroll up
    if( dwRenderStart > 0 )
    {
        font.DrawText( fListStartX, fListStartY,
                       COLOR_HIGHLIGHT,
                       GLYPH_UP_ARROW L"    ",
                       XBFONT_RIGHT );
    }

    // Get the details of each team member
    for( DWORD i = dwRenderStart; i < dwNumItems; ++i )
    {
        // Stop rendering if we hit the maximum number
        // team members viewable at once
        if( ( i - dwRenderStart ) >= NUM_ENTRIES_PER_SCREEN )
        {
            // If more team roster entries are below
            // the last entry drawn, then add a down
            // arrow on the side telling the user
            // can scroll down
            float fDownArrowY = fListStartY
                                + ( DEFAULT_TEXT_PADDING * ( NUM_ENTRIES_PER_SCREEN - 1 ) );

            font.DrawText( fListStartX, fDownArrowY,
                           COLOR_HIGHLIGHT,
                           GLYPH_DOWN_ARROW L"   ",
                           XBFONT_RIGHT );

            break;
        }

        // Render to the screen!
        INT   iScreenItem = ( i - dwRenderStart );
        FLOAT fPosY       = fListStartY + ( DEFAULT_TEXT_PADDING * iScreenItem );

        INT iDataIndex = bZeroIndexed ? iScreenItem : i;

        // Render the left list if it exists
        if( rwLeftList )
        {
            font.DrawText( fListStartX, fPosY,
                        dwColor,
                        rwLeftList[iDataIndex],
                        XBFONT_LEFT );
        }

        // Render the right list if it exists
        if( rwRightList )
        {
            font.DrawText( fRightX, fPosY,
                        dwColor,
                        rwRightList[iDataIndex],
                        XBFONT_RIGHT );
        }

    }

    if( dwNumItems )
    {
        // Allow the user to move the selector
        // up and down to select a specific user
        // to give an permissions to or
        // to remove from the team
        //
        // Show selected item with a little triangle
        FLOAT fIconPosY = fListStartY + 
                          ( DEFAULT_TEXT_PADDING * ( dwItemSelected - dwRenderStart ) );

        font.DrawText( fListStartX, fIconPosY,
                       COLOR_POINTER,
                       GLYPH_RIGHT_TICK,
                       XBFONT_RIGHT );
    }
}
