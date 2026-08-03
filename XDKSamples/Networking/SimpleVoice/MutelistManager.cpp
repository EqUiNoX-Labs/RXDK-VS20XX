//-----------------------------------------------------------------------------
// File: MutelistManager.cpp
//
// Desc: Tracks mutelists for online players
//
// Hist: 08.12.03 - New for the Nov 2003 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "MutelistManager.h"
#include <assert.h>

// # of users to shift off list when full
const DWORD MUTELIST_DECREMENT = 25;

//-----------------------------------------------------------------------------
// Name: CMutelistManager (ctor)
// Desc: Performs one-time initialization
//-----------------------------------------------------------------------------
CMutelistManager::CMutelistManager()
{
    m_hMutelistStartup      = NULL;
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        m_PlayerState[ i ]          = NotRegistered;
        m_hMutelistGet[ i ]         = NULL;
        m_MutelistUsers[ i ]        = NULL;
        m_dwNumMutelistUsers[ i ]   = 0;
    }
}


//-----------------------------------------------------------------------------
// Name: ~CMutelistManager (dtor)
// Desc: Ensures final cleanup
//-----------------------------------------------------------------------------
CMutelistManager::~CMutelistManager()
{
    Shutdown();
}


//-----------------------------------------------------------------------------
// Name: Initialize
// Desc: Acquires resources and performs other initialization
//-----------------------------------------------------------------------------
HRESULT CMutelistManager::Initialize()
{
    HRESULT hr = XOnlineMutelistStartup( NULL, &m_hMutelistStartup );
    if( FAILED( hr ) )
        return hr;

    return hr;
}


//-----------------------------------------------------------------------------
// Name: DoWork
// Desc: Continues to work on any pending tasks
//-----------------------------------------------------------------------------
HRESULT CMutelistManager::DoWork()
{
    // Keep pumping any pending mutelist gets
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        if( m_PlayerState[ i ] == GettingMutelist )
        {
            assert( m_hMutelistGet[ i ] != NULL );

            HRESULT hr = XOnlineTaskContinue( m_hMutelistGet[ i ] );
            if( hr != XONLINETASK_S_RUNNING )
            {
                XOnlineTaskClose( m_hMutelistGet[ i ] );
                m_hMutelistGet[ i ] = NULL;
                m_PlayerState[ i ]  = UpToDate;
            }
        }
    }

    XOnlineTaskContinue( m_hMutelistStartup );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Shutdown
// Desc: Releases resources and performs other cleanup
//-----------------------------------------------------------------------------
HRESULT CMutelistManager::Shutdown()
{
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        UnregisterLocalPlayer( i );
    }

    return S_OK;
}

HRESULT CMutelistManager::RegisterLocalPlayer( DWORD dwPort )
{
    m_MutelistUsers[ dwPort ] = new XONLINE_MUTELISTUSER[ MAX_MUTELISTUSERS ];
    if( !m_MutelistUsers[ dwPort ] ) 
        return E_OUTOFMEMORY;

    // Request the mutelist.  If this fails, we'll treat it as empty
    HRESULT hr = XOnlineMutelistGet( dwPort,
                                     MAX_MUTELISTUSERS,
                                     NULL,
                                     &m_hMutelistGet[ dwPort ],
                                     m_MutelistUsers[ dwPort ],
                                     &m_dwNumMutelistUsers[ dwPort ] );
    if( FAILED( hr ) )
    {
        m_PlayerState[ dwPort ] = UpToDate;
    }
    else
    {
        m_PlayerState[ dwPort ] = GettingMutelist;
    }

    return S_OK;
}

HRESULT CMutelistManager::UnregisterLocalPlayer( DWORD dwPort )
{
    HRESULT hr = S_OK;

    // Shutdown any pending mutelist get
    if( m_hMutelistGet[ dwPort ] != NULL )
    {
        assert( m_PlayerState[ dwPort ] == GettingMutelist );

        do
        {
            hr = XOnlineTaskContinue( m_hMutelistGet[ dwPort ] );
        } while( hr == XONLINETASK_S_RUNNING );

        XOnlineTaskClose( m_hMutelistGet[ dwPort ] );
        m_hMutelistGet[ dwPort ] = NULL;
        m_dwNumMutelistUsers[ dwPort ] = 0;
    }

    if( m_MutelistUsers[ dwPort ] != NULL )
    {
        delete[] m_MutelistUsers[ dwPort ];
        m_MutelistUsers[ dwPort ] = NULL;
    }

    m_PlayerState[ dwPort ] = NotRegistered;

    return hr;
}

HRESULT CMutelistManager::MutePlayer( DWORD dwPort, XUID xuidPlayer )
{
    // Can't mute players while getting the list
    if( m_PlayerState[ dwPort ] != UpToDate )
        return E_FAIL;

    HRESULT hr = XOnlineMutelistAdd( dwPort, xuidPlayer );
    if( SUCCEEDED( hr ) )
    {
        // Once the initial list is populated, we handle our own updates
        // by emulating the mute list behavior.

        // If the player is not already in the list, add them.
        if( !IsPlayerMuted( dwPort, xuidPlayer ) )
        {
            if( m_dwNumMutelistUsers[ dwPort ] == MAX_MUTELISTUSERS )
            {
                memmove( &m_MutelistUsers[ dwPort ][ 0 ], 
                         &m_MutelistUsers[ dwPort ][ MUTELIST_DECREMENT ],
                         sizeof( XONLINE_MUTELISTUSER ) * ( MAX_MUTELISTUSERS - MUTELIST_DECREMENT ) );
                m_dwNumMutelistUsers[ dwPort ] -= MUTELIST_DECREMENT;
            }

            m_MutelistUsers[ dwPort ][ m_dwNumMutelistUsers[ dwPort ] ].xuid = xuidPlayer;
            ++m_dwNumMutelistUsers[ dwPort ];
        }
    }

    return hr;
}

HRESULT CMutelistManager::UnmutePlayer( DWORD dwPort, XUID xuidPlayer )
{
    // Can't mute players while getting the list
    if( m_PlayerState[ dwPort ] != UpToDate )
        return E_FAIL;

    assert( IsPlayerMuted( dwPort, xuidPlayer ) );

    HRESULT hr = XOnlineMutelistRemove( dwPort, xuidPlayer );
    if( SUCCEEDED( hr ) )
    {
        DWORD dwIndex = GetMutelistIndex( dwPort, xuidPlayer );
        assert( dwIndex < MAX_MUTELISTUSERS );

#if 1
        memmove( &m_MutelistUsers[ dwPort ][ dwIndex ],
                 &m_MutelistUsers[ dwPort ][ dwIndex + 1 ],
                 sizeof( XONLINE_MUTELISTUSER ) * ( m_dwNumMutelistUsers[ dwPort ] - dwIndex - 1 ) );
#else
        m_MutelistUsers[ dwPort ][ dwIndex ] = m_MutelistUsers[ dwPort ][ m_dwNumMutelistUsers[ dwPort ] - 1 ];
#endif // 0
        --m_dwNumMutelistUsers[ dwPort ];
    }

    return hr;
}

BOOL CMutelistManager::IsPlayerMuted( DWORD dwPort, XUID xuidPlayer )
{
    return ( GetMutelistIndex( dwPort, xuidPlayer ) != MAX_MUTELISTUSERS );
}

BOOL CMutelistManager::IsUpToDate()
{
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        if( m_PlayerState[ i ] == GettingMutelist )
            return FALSE;
    }

    return TRUE;
}

DWORD CMutelistManager::GetMutelistIndex( DWORD dwPort, XUID xuidPlayer )
{
    // Note that if the player is not up-to-date, then m_dwNumMutelistUsers
    // will be zero, so we'll return MAX_MUTELISTUSERS as expected.
    for( DWORD i = 0; i < m_dwNumMutelistUsers[ dwPort ]; i++ )
    {
        if( XOnlineAreUsersIdentical( &xuidPlayer,
                                      &m_MutelistUsers[ dwPort ][ i ].xuid ) )
        {
            return i;
        }
    }

    return MAX_MUTELISTUSERS;
}

