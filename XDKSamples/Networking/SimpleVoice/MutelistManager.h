//-----------------------------------------------------------------------------
// File: MutelistManager.h
//
// Desc: Tracks mutelists for online players
//
// Hist: 08.12.03 - New for the Nov 2003 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

#ifndef _MUTELISTMANAGER_H_
#define _MUTELISTMANAGER_H_

#include <xtl.h>
#include <xonline.h>

enum PlayerState
{
    NotRegistered,
    GettingMutelist,
    UpToDate,
};

class CMutelistManager
{
public:
    CMutelistManager();
    ~CMutelistManager();

    HRESULT Initialize();
    HRESULT DoWork();
    HRESULT Shutdown();

    HRESULT RegisterLocalPlayer( DWORD dwPort );
    HRESULT UnregisterLocalPlayer( DWORD dwPort );
    HRESULT MutePlayer( DWORD dwPort, XUID xuidPlayer );
    HRESULT UnmutePlayer( DWORD dwPort, XUID xuidPlayer );
    BOOL    IsPlayerMuted( DWORD dwPort, XUID xuidPlayer );
    BOOL    IsUpToDate();

private:
    DWORD   GetMutelistIndex( DWORD dwPort, XUID xuidPlayer );

    XONLINETASK_HANDLE      m_hMutelistStartup;
    PlayerState             m_PlayerState[ XGetPortCount() ];
    XONLINETASK_HANDLE      m_hMutelistGet[ XGetPortCount() ];
    XONLINE_MUTELISTUSER*   m_MutelistUsers[ XGetPortCount() ];
    DWORD                   m_dwNumMutelistUsers[ XGetPortCount() ];
};

#endif // _MUTELISTMANAGER_H_