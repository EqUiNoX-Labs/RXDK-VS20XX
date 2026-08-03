//-----------------------------------------------------------------------------
// File: XbNetMsg.cpp
//
// Desc: Simple "waving" game messages to show Xbox communication.
//       Requires linking with XNET[D][S].LIB or XONLINE[D][S].LIB
//
// Hist: 10.20.01 - New for November XDK release
//       02.15.02 - Updated for Mar release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "XBNetMsg.h"
#include <cassert>
#include <algorithm>




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const XNKID EMPTY_KEY = { 0 };




//-----------------------------------------------------------------------------
// Name: class MatchInAddr
// Desc: Predicate functor used to match on IN_ADDRs in player lists
//-----------------------------------------------------------------------------
struct MatchInAddr
{
    IN_ADDR ia;

    explicit MatchInAddr( const SOCKADDR_IN& sa )
    {
        ia = sa.sin_addr;
    }

    bool operator()( const CXBNetPlayerInfo& playerInfo ) const
    {
        return playerInfo.inAddr.s_addr == ia.s_addr;
    }
};




//-----------------------------------------------------------------------------
// Name: class MatchUserID
// Desc: Predicate functor used to match on user id in player lists
//-----------------------------------------------------------------------------
struct MatchUserID
{
    ULONGLONG qwID;

    explicit MatchUserID( ULONGLONG qwUserID )
    {
        qwID = qwUserID;
    }

    bool operator()( const CXBNetPlayerInfo& playerInfo ) const
    {
        return playerInfo.qwUserID == qwID;
    }
};




//-----------------------------------------------------------------------------
// Name: CXBNetMessage()
// Desc: constructor
//-----------------------------------------------------------------------------
CXBNetMessage::CXBNetMessage( BYTE byMessageId )
{
    m_byMessageId = byMessageId;
}




//-----------------------------------------------------------------------------
// Name: CXBNetMessage()
// Desc: constructor
//-----------------------------------------------------------------------------
INT CXBNetMessage::GetSize() const
{ 
    switch( m_byMessageId )
    {
        case MSG_JOIN_GAME:     return sizeof(BYTE) + sizeof(CXBNetMsgJoinGame);
        case MSG_JOIN_APPROVED: return sizeof(BYTE) + sizeof(CXBNetMsgJoinApproved);
        case MSG_JOIN_DENIED:   return sizeof(BYTE) + sizeof(CXBNetMsgJoinDenied);
        case MSG_PLAYER_JOINED: return sizeof(BYTE) + sizeof(CXBNetMsgPlayerJoined);
        case MSG_WAVE:          return sizeof(BYTE) + sizeof(CXBNetMsgWave);
        case MSG_HEARTBEAT:     return sizeof(BYTE) + sizeof(CXBNetMsgHeartbeat);
        case MSG_BLOB:          return sizeof(BYTE) + sizeof(CXBNetBlob);
        default:                assert( FALSE ); return 0;
    }
}




//-----------------------------------------------------------------------------
// Name: CXBMsgHandler()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBNetMsgHandler::CXBNetMsgHandler()
{
    ZeroMemory( &m_xnSessionID, sizeof( m_xnSessionID ) );
    (*m_strHost) = '\0';
    m_bIsHost    = FALSE;
}




//-----------------------------------------------------------------------------
// Name: ~CXBNetMsgHandler()
// Desc: destructor
//-----------------------------------------------------------------------------
CXBNetMsgHandler::~CXBNetMsgHandler()
{
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Init object
//-----------------------------------------------------------------------------
HRESULT CXBNetMsgHandler::Initialize()
{
    // The game socket is a non-blocking socket on port GAME_PORT.
    BOOL bSuccess = m_Sock.Open( CXBSocket::Type_UDP );
    if( !bSuccess )
        return E_FAIL;

    SOCKADDR_IN directAddr;
    directAddr.sin_family      = AF_INET;
    directAddr.sin_addr.s_addr = INADDR_ANY;
    directAddr.sin_port        = htons( GAME_PORT );

    INT iResult = m_Sock.Bind( &directAddr );
    if( iResult == SOCKET_ERROR )
        return E_FAIL;

    DWORD dwNonBlocking = 1;
    iResult = m_Sock.IoCtlSocket( FIONBIO, &dwNonBlocking );
    if( iResult == SOCKET_ERROR )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetUser()
// Desc: Set user name
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::SetUser( const WCHAR* strUser, BOOL bIsHost )
{
    lstrcpynW( m_strHost, strUser, MAX_PLAYER_STR );
    m_bIsHost = bIsHost;
}




//-----------------------------------------------------------------------------
// Name: SetSessionID()
// Desc: Set user name
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::SetSessionID( const XNKID& xnSessionID )
{
    CopyMemory( &m_xnSessionID, &xnSessionID, sizeof( xnSessionID ) );
}




//-----------------------------------------------------------------------------
// Name: SendJoinGame()
// Desc: Issue a MSG_JOIN_GAME from our client to the game host
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::SendJoinGame( const SOCKADDR_IN& saGameHost, 
                                     const WCHAR* strUser, ULONGLONG qwUserID )
{
    assert( !m_bIsHost );
    CXBNetMessage msgJoinGame( MSG_JOIN_GAME );
    CXBNetMsgJoinGame& msg = msgJoinGame.GetJoinGame();

    // Include our player name
    lstrcpynW( msg.strPlayerName, strUser, MAX_PLAYER_STR );

    // Include our player ID
    msg.qwUserID = qwUserID;

    // We can send this message directly to the host
    INT nBytes = m_Sock.SendTo( &msgJoinGame, msgJoinGame.GetSize(),
                                &saGameHost );
    
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    //  assert( nBytes == msgJoinGame.GetSize() );
    
    (VOID)nBytes; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: SendWave()
// Desc: Wave to all other players in game
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::SendWave( const CXBNetPlayerList& PlayerList )
{
    // Send a "wave" message to each of the other players in the game
    CXBNetPlayerList::const_iterator i = PlayerList.begin();
    for( ; i != PlayerList.end(); ++i )
    {
        SOCKADDR_IN sa;
        sa.sin_family = AF_INET;
        sa.sin_addr   = i->inAddr;
        sa.sin_port   = htons( GAME_PORT );
        SendWave( sa );
    }
}




//-----------------------------------------------------------------------------
// Name: SendWave()
// Desc: Issue a MSG_WAVE from ourself (either a host or player) to another
//       player
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::SendWave( const SOCKADDR_IN& saPlayer ) // private
{
    CXBNetMessage msgWave( MSG_WAVE );
    INT nBytes = m_Sock.SendTo( &msgWave, msgWave.GetSize(), &saPlayer );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    // assert( nBytes == msgWave.GetSize() );
    (VOID)nBytes; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: SendBlob()
// Desc: Send a generic blob message to all other players
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::SendBlob( const CXBNetPlayerList& PlayerList, const CXBNetBlob& blob )
{
    // Make sure we haven't overwritten our buffer.
    assert( blob.dataSize < sizeof( blob.data ) );

    // Send a "blob" message to each of the other players in the game
    CXBNetPlayerList::const_iterator i = PlayerList.begin();
    for( ; i != PlayerList.end(); ++i )
    {
        SOCKADDR_IN sa;
        sa.sin_family = AF_INET;
        sa.sin_addr   = i->inAddr;
        sa.sin_port   = htons( GAME_PORT );
        SendBlob( sa, blob );
    }
}




//-----------------------------------------------------------------------------
// Name: SendBlob()
// Desc: Send a generic blob message to another player
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::SendBlob( const SOCKADDR_IN& saPlayer, const CXBNetBlob& blob ) // private
{
    CXBNetMessage msgBlob( MSG_BLOB );
    msgBlob.GetBlob() = blob;
    m_Sock.SendTo( &msgBlob, msgBlob.GetSize(), &saPlayer );
}




//-----------------------------------------------------------------------------
// Name: SendHeartbeat()
// Desc: Send heartbeat to players in the game
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::SendHeartbeat( const CXBNetPlayerList& PlayerList )
{
    // Send a "heartbeat" message to each of the other players in the game
    // to let them know we're alive
    CXBNetPlayerList::const_iterator i = PlayerList.begin();
    for( ; i != PlayerList.end(); ++i )
    {
        SOCKADDR_IN sa;
        sa.sin_family = AF_INET;
        sa.sin_addr   = i->inAddr;
        sa.sin_port   = htons( GAME_PORT );
        SendHeartbeat( sa );
    }
}




//-----------------------------------------------------------------------------
// Name: SendHeartbeat()
// Desc: Issue a MSG_HEARTBEAT from ourself (either a host or player) to
//       another player
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::SendHeartbeat( const SOCKADDR_IN& saPlayer ) // private
{
    // Send the heartbeat
    CXBNetMessage msgHeartbeat( MSG_HEARTBEAT );
    INT nBytes = m_Sock.SendTo( &msgHeartbeat, msgHeartbeat.GetSize(),
                                &saPlayer );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    
    //assert( nBytes == msgHeartbeat.GetSize() );
    (VOID)nBytes; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: ProcessMessages()
// Desc: Checks to see if any messages are waiting on the game socket.
//       If a message is waiting, it is routed and processed.
//       If no messages are waiting, the function returns immediately.
//       Returns TRUE if a message was processed.
//-----------------------------------------------------------------------------
BOOL CXBNetMsgHandler::ProcessMessages( CXBNetPlayerList& PlayerList )
{
    if( !m_Sock.IsOpen() )
        return FALSE;

    // See if a network message is waiting for us
    CXBNetMessage msg;
    SOCKADDR_IN saFromIn;
    INT iResult = m_Sock.RecvFrom( &msg, msg.GetMaxSize(), &saFromIn );
    SOCKADDR_IN saFrom( saFromIn );

    // If message waiting, process it
    if( iResult != SOCKET_ERROR && iResult > 0 )
    {
        assert( iResult == msg.GetSize() );
        ProcessMessage( msg, saFrom, PlayerList );
        return TRUE;
    }
    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: ProcessPlayersDropouts()
// Desc: Process players and determine if anybody has left the game.
//       Returns TRUE if any player left the game. It's up to the caller
//       to remove elements from the player list on dropouts.
//-----------------------------------------------------------------------------
BOOL CXBNetMsgHandler::ProcessPlayerDropouts( const CXBNetPlayerList& PlayerList,
                                              DWORD dwTimeout )
{
    DWORD dwTickCount = GetTickCount();
    CXBNetPlayerList::const_iterator i = PlayerList.begin();
    for( ; i != PlayerList.end(); ++i )
    {
        CXBNetPlayerInfo playerInfo = *i;
        DWORD dwElapsed = dwTickCount - playerInfo.dwLastHeartbeat;
        if( dwElapsed > dwTimeout )
        {
            // This player hasn't sent a heartbeat message in a long time.
            // Assume they left the game.

            if( !m_bIsHost && i == PlayerList.begin() )
            {
                OnPlayerDropout( playerInfo, TRUE );
            }
            else
            {
                OnPlayerDropout( playerInfo, FALSE );
            }
            return TRUE;
        }
    }
    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: ProcessMessage()
// Desc: Routes any messages
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::ProcessMessage( CXBNetMessage& msg, 
                                       const SOCKADDR_IN& saFrom,
                                       CXBNetPlayerList& PlayerList ) // private
{
    // Process the message
    switch( msg.GetId() )
    {
        case MSG_BLOB:
            // From host to all clients, or from client to host
            ProcessBlob( msg.GetBlob(), saFrom, PlayerList );
            break;

            // From client to host; processed by host
        case MSG_JOIN_GAME:
            ProcessJoinGame( msg.GetJoinGame(), saFrom, PlayerList );
            break;

        // From host to client: processed by client
        case MSG_JOIN_APPROVED:
            ProcessJoinApproved( msg.GetJoinApproved(), saFrom );
            break;
        case MSG_JOIN_DENIED:
            ProcessJoinDenied();
            break;
        case MSG_PLAYER_JOINED:
            ProcessPlayerJoined( msg.GetPlayerJoined(), saFrom );
            break;

        // From player to player: processed by client player
        case MSG_WAVE:
            ProcessWave( saFrom, PlayerList );
            break;
        case MSG_HEARTBEAT:
            ProcessHeartbeat( saFrom, PlayerList );
            break;

        // Any other message on this port is invalid and we ignore it
        default:
            assert( FALSE );
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: ProcessJoinGame()
// Desc: Process the join game message.
//       Returns TRUE if the player successfully joined, FALSE if not.
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::ProcessJoinGame( const CXBNetMsgJoinGame& joinGame,
                                        const SOCKADDR_IN& saFrom,
                                        CXBNetPlayerList& PlayerList ) // private
{
    // If for some reason we receive a "join game" message and we're not a
    // host, ignore it. Only hosts respond to "join game" messages
    if( !m_bIsHost )
        return;

    // We're hosting

    // A session exists between us (the host) and the client. We can now
    // convert the incoming IP address (saFrom) into a valid XNADDR.
    XNADDR xnAddrClient;
    INT iResult = XNetInAddrToXnAddr( saFrom.sin_addr, &xnAddrClient, NULL );
    if( iResult == SOCKET_ERROR )
    {
        // If the client INADDR can't be converted to an XNADDR, then
        // this client does not have a valid session established, and
        // we ignore the message.
        assert( FALSE );
        return;
    }

    MatchUserID matchUserID( joinGame.qwUserID );

    // First check to make sure the player isn't already in the list.
    // If so, remove the player first.  This can happen if the player
    // drops out of a game and rejoins before the next heartbeat.
    CXBNetPlayerList::iterator i = 
        std::find_if( PlayerList.begin(), PlayerList.end(), matchUserID );

    if( i != PlayerList.end() )
    {
        PlayerList.erase( i );
    }
 
    // A player may join if we haven't reached the player limit.
    // In a real game, you would need to "lock" the game during a join
    // or track the number of joins in progress so that if multiple
    // players were attempting to join at the same time, they wouldn't
    // all be granted access and then exceed the player maximum.
    if( PlayerList.size() + 1 < MAX_PLAYERS )
    {
        SendJoinApproved( saFrom, PlayerList );

        // Notify the other players about the new guy
        CXBNetPlayer player;
        CopyMemory( &player.xnAddr, &xnAddrClient, sizeof( XNADDR ) );
        lstrcpynW( player.strPlayerName, joinGame.strPlayerName, MAX_PLAYER_STR );
        CXBNetPlayerList::const_iterator j = PlayerList.begin(); 
        for( ; j != PlayerList.end(); ++j )
        {
            SOCKADDR_IN sa;
            sa.sin_family = AF_INET;
            sa.sin_addr   = j->inAddr;
            sa.sin_port   = htons( GAME_PORT );
            SendPlayerJoined( player, sa );
        }

        // Add this new player to our player list
        CXBNetPlayerInfo playerInfo;
        CopyMemory( &playerInfo.xnAddr, &xnAddrClient, sizeof( XNADDR ) );
        playerInfo.inAddr = saFrom.sin_addr;
        lstrcpynW( playerInfo.strPlayerName, joinGame.strPlayerName, 
                   MAX_PLAYER_STR );
        playerInfo.qwUserID = joinGame.qwUserID;
        playerInfo.dwLastHeartbeat = GetTickCount();

        OnJoinGame( playerInfo );
    }
    else // We've reached the player limit; inform the client that the game is full    
        SendJoinDenied( saFrom );
}




//-----------------------------------------------------------------------------
// Name: ProcessJoinApproved()
// Desc: Process the join approved message
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::ProcessJoinApproved( const CXBNetMsgJoinApproved& joinApproved, 
                                            const SOCKADDR_IN& saFrom ) // private
{
    // Only clients should receive "join approved" messages
    assert( !m_bIsHost );

    // If for some reason we receive a "join approved" message and we're hosting
    // a game, ignore the message. Only clients handle this message
    if( m_bIsHost )
        return;

    // Add the host to the list of players (always at position 0 )

    // Declare and zero hostInfo
    CXBNetPlayerInfo hostInfo = { 0 };

    // Client doesn't need the XNADDR of the host, so we just leave it zero.
    // This data member is only used by hosts.
    hostInfo.inAddr = saFrom.sin_addr;
    lstrcpynW( hostInfo.strPlayerName, joinApproved.strHostName, MAX_PLAYER_STR );
    hostInfo.dwLastHeartbeat = GetTickCount();

    OnJoinApproved( hostInfo );

    // Build the list of the other players
    for( BYTE i = 0; i < joinApproved.byNumPlayers; ++i )
    {
        CXBNetPlayerInfo playerInfo;

        // Convert the XNADDR of the player to the INADDR we'll use to wave
        // to the player
        assert( memcmp( &m_xnSessionID, &EMPTY_KEY, sizeof( XNKID ) ) != 0 );
        INT iResult = XNetXnAddrToInAddr( &joinApproved.PlayerList[ i ].xnAddr,
                                          &m_xnSessionID, &playerInfo.inAddr );
        if( iResult == SOCKET_ERROR )
        {
            // If the client XNADDR can't be converted to an INADDR, then
            // the client does not have a valid session established, and
            // we ignore that client.
            assert( FALSE );
            continue;
        }

        // Client doesn't need the XNADDR of the host anymore, 
        // so we just leave it zero. This data member is only used by hosts.
        ZeroMemory( &playerInfo.xnAddr, sizeof( XNADDR ) );

        // Save the player name
        lstrcpynW( playerInfo.strPlayerName, 
                   joinApproved.PlayerList[ i ].strPlayerName, MAX_PLAYER_STR );

        // Last heartbeat
        playerInfo.dwLastHeartbeat = GetTickCount();

        playerInfo.qwUserID = joinApproved.PlayerList[ i ].qwUserID;


        OnJoinApprovedAddPlayer( playerInfo );
    }
}




//-----------------------------------------------------------------------------
// Name: ProcessJoinDenied()
// Desc: Process the join denied message
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::ProcessJoinDenied() // private
{
    // Only clients should receive "join denied" messages
    assert( !m_bIsHost );

    // If for some reason we receive a "join denied" message and we're hosting
    // a game, ignore the message. Only clients handle this message
    if( m_bIsHost )
        return;

    OnJoinDenied();
}




//-----------------------------------------------------------------------------
// Name: ProcessPlayerJoined()
// Desc: Process the player joined message
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::ProcessPlayerJoined( const CXBNetMsgPlayerJoined& playerJoined, 
                                            const SOCKADDR_IN& saFrom ) // private
{
    // Only clients should receive "player joined" messages
    assert( !m_bIsHost );

    // saFrom is the address of the host that sent this message, but we
    // we already have his address, so throw it away
    (VOID)saFrom; // avoid compiler warning

    const CXBNetPlayer& player = playerJoined.player;
    CXBNetPlayerInfo playerInfo;

    // Convert the XNADDR of the player to the INADDR we'll use to wave
    // to the player
    assert( memcmp( &m_xnSessionID, &EMPTY_KEY, sizeof( XNKID ) ) != 0 );
    INT iResult = XNetXnAddrToInAddr( &player.xnAddr, &m_xnSessionID,
                                      &playerInfo.inAddr );
    if( iResult == SOCKET_ERROR )
    {
        // If the client XNADDR can't be converted to an INADDR, then
        // this client does not have a valid session established, and
        // we ignore the message.
        assert( FALSE );
        return;
    }

    // Client doesn't need the XNADDR of the new player, so we just 
    // leave it zero. This data member is only used by hosts.
    ZeroMemory( &playerInfo.xnAddr, sizeof( XNADDR ) );

    // Save the player name
    lstrcpynW( playerInfo.strPlayerName, player.strPlayerName, MAX_PLAYER_STR );

    playerInfo.qwUserID = player.qwUserID;

    // Last heartbeat
    playerInfo.dwLastHeartbeat = GetTickCount();

    OnPlayerJoined( playerInfo );
}




//-----------------------------------------------------------------------------
// Name: ProcessWave()
// Desc: Process the wave message
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::ProcessWave( const SOCKADDR_IN& saFrom,
                                    const CXBNetPlayerList& PlayerList ) // private
{
    MatchInAddr matchInAddr( saFrom );

    // Find out who waved by matching the INADDR
    CXBNetPlayerList::const_iterator i = 
        std::find_if( PlayerList.begin(), PlayerList.end(), matchInAddr );

    if( i != PlayerList.end() )
        OnWave( *i );
}




//-----------------------------------------------------------------------------
// Name: ProcessBlob()
// Desc: Process the blob message
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::ProcessBlob( const CXBNetBlob& blob, const SOCKADDR_IN& saFrom,
                                    const CXBNetPlayerList& PlayerList ) // private
{
    MatchInAddr matchInAddr( saFrom );

    // Find out who waved by matching the INADDR
    CXBNetPlayerList::const_iterator i = 
        std::find_if( PlayerList.begin(), PlayerList.end(), matchInAddr );

    if( i != PlayerList.end() )
        OnBlob( *i, blob );
}




//-----------------------------------------------------------------------------
// Name: ProcessHeartbeat()
// Desc: Process the heartbeat message
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::ProcessHeartbeat( const SOCKADDR_IN& saFrom,
                                         const CXBNetPlayerList& PlayerList ) // private
{
    MatchInAddr matchInAddr( saFrom );

    // Find out who sent a heartbeat by matching the INADDR
    CXBNetPlayerList::const_iterator i = 
        std::find_if( PlayerList.begin(), PlayerList.end(), matchInAddr );

    // Update that player's heartbeat time
    if( i != PlayerList.end() )
        OnHeartbeat( *i );
}




//-----------------------------------------------------------------------------
// Name: SendJoinApproved()
// Desc: Issue a MSG_JOIN_APPROVED from our host to the requesting client.
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::SendJoinApproved( const SOCKADDR_IN& saClient, 
                                         const CXBNetPlayerList& PlayerList ) // private
{
    assert( m_bIsHost );
    CXBNetMessage msgJoinApproved( MSG_JOIN_APPROVED );
    CXBNetMsgJoinApproved& msg = msgJoinApproved.GetJoinApproved();

    // The host is us
    assert( *m_strHost != 0 );
    lstrcpynW( msg.strHostName, m_strHost, MAX_PLAYER_STR );

    // Send the list of all the current players to the new player.
    // We don't send the host player info, since the new player 
    // already has all of the information it needs about the host player.
    msg.byNumPlayers = BYTE( PlayerList.size() );
    BYTE j = 0;
    CXBNetPlayerList::const_iterator i = PlayerList.begin();
    for( ; i != PlayerList.end(); ++i, ++j )
    {
        CXBNetPlayerInfo playerInfo = *i;
        CopyMemory( &msg.PlayerList[j].xnAddr, &playerInfo.xnAddr, 
                    sizeof( XNADDR ) );
        lstrcpynW( msg.PlayerList[j].strPlayerName, 
                   playerInfo.strPlayerName, MAX_PLAYER_STR );
        msg.PlayerList[j].qwUserID = playerInfo.qwUserID;
    }

    // We can send this message directly back to the requesting client
    INT nBytes = m_Sock.SendTo( &msgJoinApproved, msgJoinApproved.GetSize(),
                                &saClient );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    // assert( nBytes == msgJoinApproved.GetSize() );
    (VOID)nBytes; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: SendJoinDenied()
// Desc: Issue a MSG_JOIN_DENIED from our host to the requesting client
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::SendJoinDenied( const SOCKADDR_IN& saClient ) // private
{
    assert( m_bIsHost );
    CXBNetMessage msgJoinDenied( MSG_JOIN_DENIED );

    // We can send this message directly back to the requesting client
    INT nBytes = m_Sock.SendTo( &msgJoinDenied, msgJoinDenied.GetSize(),
                                &saClient );

    
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    //    assert( nBytes == msgJoinDenied.GetSize() )

    (VOID)nBytes; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: SendPlayerJoined()
// Desc: Issue a MSG_PLAYER_JOINED from our host to a player in the game
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::SendPlayerJoined( const CXBNetPlayer& player, 
                                         const SOCKADDR_IN& saPlayer ) // private
{
    assert( m_bIsHost );
    CXBNetMessage msgPlayerJoined( MSG_PLAYER_JOINED );
    CXBNetMsgPlayerJoined& msg = msgPlayerJoined.GetPlayerJoined();

    // The payload is the information about the player who just joined
    CopyMemory( &msg.player, &player, sizeof(player) );

    // We send this message directly to the player
    INT nBytes = m_Sock.SendTo( &msgPlayerJoined, msgPlayerJoined.GetSize(),
                                &saPlayer );
    
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    
    //    assert( nBytes == msgPlayerJoined.GetSize() );
    (VOID)nBytes; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: OnXxx()
// Desc: Default implementations do nothing
//-----------------------------------------------------------------------------
VOID CXBNetMsgHandler::OnJoinGame( const CXBNetPlayerInfo& )
{
}

VOID CXBNetMsgHandler::OnJoinApproved( const CXBNetPlayerInfo& )
{
}

VOID CXBNetMsgHandler::OnJoinApprovedAddPlayer( const CXBNetPlayerInfo& )
{
}

VOID CXBNetMsgHandler::OnJoinDenied()
{
}

VOID CXBNetMsgHandler::OnPlayerJoined( const CXBNetPlayerInfo& )
{
}

VOID CXBNetMsgHandler::OnWave( const CXBNetPlayerInfo& )
{
}

VOID CXBNetMsgHandler::OnHeartbeat( const CXBNetPlayerInfo& )
{
}

VOID CXBNetMsgHandler::OnPlayerDropout( const CXBNetPlayerInfo&, BOOL )
{
}

VOID CXBNetMsgHandler::OnBlob( const CXBNetPlayerInfo&, const CXBNetBlob& )
{
}
