
//-----------------------------------------------------------------------------
// File: VoiceMgr.cpp
//
// VoiceMgr manages the voice packets in two local queues OutVoicePackets and 
// InVoicePackets, and handles the routing of voice packets between players
//
// InVoicePackets are packets from the network to be played locally
// OutVoicePackets are packets to be sent over the network
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "CommonInclude.h"



// Voice manager is included globally
VoiceMgr g_VoiceMgr;

///------------------------------------------------------------------------------
// Name: VoiceMgr::VoiceMgr()
// Desc: Initialize the voice manager
//------------------------------------------------------------------------------
VoiceMgr::VoiceMgr()
{
    m_LocalVoice.nPackets = 0;
}




//------------------------------------------------------------------------------
// Name: VoiceMgr::Update()
// Desc: Update function - called once per frame to handle local voice queueing
//------------------------------------------------------------------------------
VOID VoiceMgr::Update()
{     
    // if something has marked us as needing to rebuild routing info, rebuild it

    if ( m_bNeedToRebuildRoutes )
        RebuildRoutes();
 
    // Handle local voice
    
    if ( m_LocalVoice.nPackets != 0 )    
    {    
        m_LocalVoice.wVoiceFromID = g_PlayerMgr.GetLocalPlayer()->LUID();
            
        // If we are sending over the private channel or podium, it's the same
        // if peer-to-peer or host-forwarded
        if ( g_PlayerMgr.GetLocalPlayer()->IsPlayerFlagSet( PLAYERFLAG_TALKPRIVATE ) )
        {
            m_LocalVoice.byVoiceMode = VMM_PRIVATE;
            m_LocalVoice.wVoiceToID = ID_PRIVATE_CHANNEL;
        }            
        else if ( g_PlayerMgr.GetLocalPlayer()->IsPlayerFlagSet( PLAYERFLAG_TALKPODIUM ) ) 
        {            
            m_LocalVoice.byVoiceMode = VMM_PODIUM;
            m_LocalVoice.wVoiceToID = ID_PODIUM;
        }                    
        else 
        {
            // even though we set ID_VOICE_TO_HOST here, it is determined whether it is split
            // pre-or-post host by the serverflag in ProcessVoice
            m_LocalVoice.byVoiceMode = VMM_NORMAL;
            m_LocalVoice.wVoiceToID = ID_NORMAL_VOICE;
        }

        ProcessVoiceData( m_LocalVoice );
        m_LocalVoice.nPackets = 0;
    }
}




//------------------------------------------------------------------------------
// Name: VoiceMgr::SendNetworkVoice()
// Desc: Sends all voice data to peers that hasn't been appended through
//       calls to AppendVoiceData
//------------------------------------------------------------------------------
VOID VoiceMgr::SendNetworkVoice()
{
    std::list<VoicePacketWrapper>::iterator i, j;
    Player *plr;

    // go through our output queue and handle voice sends

    for ( i = m_OutVoicePackets.begin(); i != m_OutVoicePackets.end(); i = j )
    {
        j = i; j++;
        plr = g_PlayerMgr.PlayerFromLUID( i->wVoiceToID );
        
        // check if the player we're trying to send to still exists- if they don't, we 
        // can throw away the packet
        if ( (i->wVoiceToID != ID_PODIUM ) && (i->wVoiceToID != ID_PRIVATE_CHANNEL ) && ( !plr )  )
        {
            j = m_OutVoicePackets.erase( i );
            continue;
        }

        // if it's voice to a local player ( or mockup ), just move to the input queue
        if ( (i->wVoiceToID != ID_PODIUM ) && (i->wVoiceToID != ID_PRIVATE_CHANNEL ) && plr->IsLocal() )
        {
            m_InVoicePackets.push_back( *i );
            // Bots don't accrue network stats in peer-to-peer, and for server forward to local host
            // not going to worry about it
            j = m_OutVoicePackets.erase( i );
        }    
        else 
        {
            // ok, it's a remote player, so we need to create a packet and send it 
            Packet p;
            WORD wLoc;
            p.ResetPacket();           
            p << (WORD)MSG_VOICE;            
            p << (BYTE)1;

            wLoc = p.Size();
            WriteVoiceData( p, *i );
                                    

            // If we are the host and in peer-to-peer mode, don't count local chatter as part of the
            // network stats

            if ( g_Marketplace.IsHost() && ( g_Marketplace.GetHostProperties().byVoiceMode == VM_PEER_TO_PEER ))
            {
                if ( i->byVoiceMode == VMM_NORMAL )                     
                {
                    wLoc = p.Size();
                }
            }

            g_Marketplace.StatsSendVoiceData( FULL_PACKET_OVERHEAD + ( p.Size() - wLoc ) );
            
            // route the data to the appropriate place - hopefully we will be sending most
            // host data when we send in our player update, so these full-packet-overhead
            // host sends won't happen often           

            if (( i->wVoiceToID == ID_PODIUM ) || ( i->wVoiceToID == ID_PRIVATE_CHANNEL ))
                g_Marketplace.SendToHost( p );             
            else if ( ( g_Marketplace.GetHostProperties().byVoiceMode == VM_PEER_TO_PEER ) || (g_Marketplace.IsHost()) )
                g_Marketplace.SendToPlayer( p, i->wVoiceToID );            
            else
                g_Marketplace.SendToHost( p );          
            
            j = m_OutVoicePackets.erase( i );
        }        
    }    
    // clear out empty targetted voice packets
}




//------------------------------------------------------------------------------
// Name: VoiceMgr::MarkForRebuild()
// Desc: Marks voice for rebuild 
//------------------------------------------------------------------------------
VOID VoiceMgr::MarkForRebuild()
{  
    m_bNeedToRebuildRoutes = true;
}



//------------------------------------------------------------------------------
// Name: VoiceMgr::ReadVoiceData()
// Desc: Take voice data out of a packet, and put it in the in-queue
//       this function returnshow many bytes were read from the packet for voice
//------------------------------------------------------------------------------
WORD VoiceMgr::ReadVoiceData( Packet &p )
{
    VoicePacketWrapper vpw;      
    BYTE nSources;
    WORD wSaveReadPos, wSubSaveReadPos, wBytes;
  
    wSaveReadPos = p.GetReadPosition();
    
    // read in the data from the packet

    p >> nSources;
   
    while ( nSources-- )
    {
        wSubSaveReadPos = p.GetReadPosition();

        p >> vpw.wVoiceFromID;
        p >> vpw.wVoiceToID;
        p >> vpw.byVoiceMode;
        p >> vpw.nPackets;
        p.ReadRawBytes( vpw.bVoiceData, vpw.nPackets * COMPRESSED_VOICE_SIZE );        

        // if we're the host, and we are in peer-to-peer mode, traffic to the local players
        // and bots on the host shouldn't affect network stats

        if ( ( g_Marketplace.IsHost() ) && ( g_Marketplace.GetHostProperties().byVoiceMode == VM_PEER_TO_PEER ))
        {
            if ( ( vpw.wVoiceToID != ID_PODIUM ) && 
                 ( vpw.wVoiceToID != ID_PRIVATE_CHANNEL ) )
            {
                wSaveReadPos += ( p.GetReadPosition() - wSubSaveReadPos );
            }
        }

        ProcessVoiceData( vpw );
    }
    
    // If we are the host, we don't want traffic to local players or bots to confuse the stats
    // so we won't count the 1 byte of data above
  
    wBytes = ( p.GetReadPosition() - wSaveReadPos );
    if ( g_Marketplace.IsHost() && g_Marketplace.GetHostProperties().byVoiceMode == VM_PEER_TO_PEER &&
        (wBytes == 1 ) ) wBytes = 0;

    return wBytes;   
}




//------------------------------------------------------------------------------
// Name: VoiceMgr::ProcessVoiceData()
// Desc: Takes voice data out of a packet and puts it in the in-queue
//       Deals with podium and private voice if host by breaking it up into
//       individual packets for the target players
//------------------------------------------------------------------------------
VOID VoiceMgr::ProcessVoiceData( VoicePacketWrapper &vpw )
{        
    WORD i, j, k;
    Player *plr;

    // now see if we need to break up the voice yet - this only happens on server-forwarded packets             
    if ( ( vpw.wVoiceToID == ID_NORMAL_VOICE ) && 
         ( g_Marketplace.IsHost() || 
           g_Marketplace.GetHostProperties().byVoiceMode == VM_PEER_TO_PEER ) )
    {
        // find the index so we can associate it in the SendTo arrays
        for (j = 0; j < g_PlayerMgr.NumPlayers(); j++ )
            if ( g_PlayerMgr.PlayerFromIndex(j)->LUID() == vpw.wVoiceFromID )
                break;

        // go through our routes and make an individual entry for each person we 
        // are sending it to

        for( k = 0; k < m_byNumSendToPlayerIDs[ j ]; k++ )
        {
            vpw.wVoiceToID = m_wSendToPlayerIDs[ j ][ k ];
            plr = g_PlayerMgr.PlayerFromLUID( vpw.wVoiceToID );
            
            if ( plr )
            {
                if ( plr->IsLocal() )
                {
                    AddVoiceToQueue( m_InVoicePackets, vpw );
                                        
                    if ( ! plr->IsPlayerFlagSet( PLAYERFLAG_BOT ) )
                    {
                        // note that we heard from this player
                        plr = g_PlayerMgr.PlayerFromLUID( vpw.wVoiceFromID );
                        if ( plr )
                            plr->SetLastVoiceTime( g_Marketplace.Time() );                    
                    }
                }
                else
                    AddVoiceToQueue( m_OutVoicePackets, vpw ); 
            }            
        }
    }
    // if we're not the host we aren't going to do processing on the ID_ macros
    else if ( !g_Marketplace.IsHost() )
    {        
        plr = g_PlayerMgr.PlayerFromLUID( vpw.wVoiceToID );
                
        if ( plr && plr->IsLocal() )
        {
            AddVoiceToQueue( m_InVoicePackets, vpw );
                                         
            if ( ! plr->IsPlayerFlagSet( PLAYERFLAG_BOT ) )
            {
                // note that we heard from this player
                plr = g_PlayerMgr.PlayerFromLUID( vpw.wVoiceFromID );
                if ( plr )
                    plr->SetLastVoiceTime( g_Marketplace.Time() );                    
            }
        }
        else
            AddVoiceToQueue( m_OutVoicePackets, vpw ); 
        
        return;
    }
    else if ( vpw.wVoiceToID == ID_PRIVATE_CHANNEL )                 
    {
        // Loop through all of the players and put this packet in the outqueue for every player
        // on the private channel

        for ( i = 0; i < g_PlayerMgr.NumPlayers(); i++ )
        {               
            plr = g_PlayerMgr.PlayerFromIndex( i );
            if ( plr->IsPlayerFlagSet( PLAYERFLAG_ONPRIVATECHANNEL ) )
            {
                if ( plr->LUID() == vpw.wVoiceFromID ) continue;
                
                vpw.wVoiceToID = plr->LUID();                
                vpw.byVoiceMode = VMM_PRIVATE;
                
                if ( plr->IsLocal() )
                {
                    AddVoiceToQueue( m_InVoicePackets, vpw );
                    if ( ! plr->IsPlayerFlagSet( PLAYERFLAG_BOT ) )
                    {
                        // note that we heard from this player
                        plr = g_PlayerMgr.PlayerFromLUID( vpw.wVoiceFromID );
                        if ( plr )
                            plr->SetLastVoiceTime( g_Marketplace.Time() );                    
                    }
                }
                else
                    AddVoiceToQueue( m_OutVoicePackets, vpw ); 
            }
        }                       
    }
    else if ( vpw.wVoiceToID == ID_PODIUM )
    {
        // Loop through all the players and put this packet in the outqueue for every one

        for ( i = 0; i < g_PlayerMgr.NumPlayers(); i++ )
        {   
            plr = g_PlayerMgr.PlayerFromIndex( i );
            if ( plr->LUID() == vpw.wVoiceFromID ) continue;

            vpw.wVoiceToID = plr->LUID();            
            vpw.byVoiceMode = VMM_PODIUM;
                
            if ( plr->IsLocal() )
            {
                AddVoiceToQueue( m_InVoicePackets, vpw );
                if ( ! plr->IsPlayerFlagSet( PLAYERFLAG_BOT ) )
                {
                    // note that we heard from this player
                    plr = g_PlayerMgr.PlayerFromLUID( vpw.wVoiceFromID );
                    if ( plr )
                            plr->SetLastVoiceTime( g_Marketplace.Time() );                    
                }

            }
            else
                AddVoiceToQueue( m_OutVoicePackets, vpw ); 
        }                       
    }
    else
    {
        // Just take the packet and add it to the out queue if it's not special

        plr = g_PlayerMgr.PlayerFromLUID( vpw.wVoiceToID );
        if ( plr )
        {
            if ( plr->IsLocal() )
            {
                AddVoiceToQueue( m_InVoicePackets, vpw );
                if ( ! plr->IsPlayerFlagSet( PLAYERFLAG_BOT ) )
                {
                    // note that we heard from this player
                    plr = g_PlayerMgr.PlayerFromLUID( vpw.wVoiceFromID );
                    if ( plr )
                        plr->SetLastVoiceTime( g_Marketplace.Time() );                    
                }

            }
            else
                AddVoiceToQueue( m_OutVoicePackets, vpw ); 
        }
    }    
}




//------------------------------------------------------------------------------
// Name: VoiceMgr::AddVoiceToQueue()
// Desc: Adds voice to the queue, doubling up entries if possible
//------------------------------------------------------------------------------
VOID VoiceMgr::AddVoiceToQueue( std::list<VoicePacketWrapper> &list, VoicePacketWrapper &vpw )
{
    std::list<VoicePacketWrapper>::iterator i;

    // see if we already have an entry in the queue for this route + voice type

    for (i = m_OutVoicePackets.begin(); i != m_OutVoicePackets.end(); i++) 
    {
        if (( i->wVoiceFromID == vpw.wVoiceFromID ) && ( i->wVoiceToID == vpw.wVoiceToID ) &&
            ( i->byVoiceMode == vpw.byVoiceMode ))
            break;
    }

    // if not create it, otherwise append the data to the old one
    
    if ( i == m_OutVoicePackets.end() )
        m_OutVoicePackets.push_back( vpw );
    else
    {
        if (i->nPackets + vpw.nPackets > MAX_BUNDLED_PACKETS )
            vpw.nPackets = MAX_BUNDLED_PACKETS - i->nPackets;
        memcpy( i->bVoiceData + i->nPackets * COMPRESSED_VOICE_SIZE, vpw.bVoiceData, vpw.nPackets * COMPRESSED_VOICE_SIZE);
        i->nPackets += (BYTE)vpw.nPackets;
        i->byVoiceMode = vpw.byVoiceMode;      
    }
}





//------------------------------------------------------------------------------
// Name: VoiceMgr::GetVoiceForPlayer()
// Desc: Referenced by the player class when it's looking for audio data
//       It returns the voice bytes on the input queue.  If there are multiple
//       packets on the input queue, we return the last one only.
//------------------------------------------------------------------------------
BOOL VoiceMgr::GetVoiceForPlayer( WORD wPlayerLUID, VoicePacketWrapper &w )
{
    std::list<VoicePacketWrapper>::iterator i, j;
    bool rtn;

    rtn = false;
    for ( i = m_InVoicePackets.begin(); i != m_InVoicePackets.end(); i = j)
    { 
        j = i; j++;
        if ( i->wVoiceToID == wPlayerLUID ) 
        {
            w = (*i);
            m_InVoicePackets.erase( i );
            rtn = true;
        }
    }
    return rtn;
}





//------------------------------------------------------------------------------
// Name: VoiceMgr::RebuildRoutes()
// Desc: RebuildRoutes calculates the priorities for normal voice, and then
//       uses a quick and dirty round-robin algorithm to pick the appropriate
//       number of sends and receives to each player
//------------------------------------------------------------------------------
VOID VoiceMgr::RebuildRoutes()
{
    INT iPlayers, i, j;
    Player *p1, *p2;
    FLOAT fDistSq, fDot;
    D3DXVECTOR3 vFacing, vDir, vDist;
    INT iLocalPlayerIdx;

    iPlayers = g_PlayerMgr.NumPlayers();    

    // m_fPriorities[i][j] is how much I wants to receive from J; higher is more

    iLocalPlayerIdx = g_PlayerMgr.IndexFromPlayer( g_PlayerMgr.GetLocalPlayer() );

    for ( i = 0; i < iPlayers; i++ )
    {
        p1 = g_PlayerMgr.PlayerFromIndex( i );         
        for ( j = 0; j < iPlayers; j++ )
        {            
            if ( i == j ) continue;
            p2 = g_PlayerMgr.PlayerFromIndex( j );

            m_fPriorities[i][j] = 0.0f;
            
            // always calculate priorities as if the local player is talking, since we want to always show
            // who they would be sending to- for other players, skip it if they aren't talking.
            // Also, if they are talking privately or through the podium, then we are sending it to
            // the server anyways, so we don't need to calculate for them

            if ( ( !p2->IsPlayerFlagSet( PLAYERFLAG_TALKING ) ||
                  p2->IsPlayerFlagSet( PLAYERFLAG_TALKPRIVATE )  ||
                  p2->IsPlayerFlagSet( PLAYERFLAG_TALKPODIUM ) ) &&
                  ( iLocalPlayerIdx != j ) )       continue;                                    
            
            if ( !p2->IsPlayerFlagSet( PLAYERFLAG_MOVING ) )   
                m_fPriorities[i][j] += PRIORITY_STATIONARY;  

            // offset priorities a little to break ties
            m_fPriorities[i][j] += 0.01f * (j+1) + 0.001f * (i+1); // offset each by a little to break ties

            vDist = p1->Position() - p2->Position();

            fDistSq = D3DXVec3LengthSq( &vDist );
    
            if ( fDistSq  < PRIORITY_DISTANCE_NORMAL_MIN * PRIORITY_DISTANCE_NORMAL_MIN )
                m_fPriorities[i][j] += PRIORITY_DISTANCE_SCALE; 
            else
                m_fPriorities[i][j] += PRIORITY_DISTANCE_SCALE * PRIORITY_DISTANCE_NORMAL_MIN / sqrtf(fDistSq);            

            vDir = p1->Position() - p2->Position();
            D3DXVec3Normalize( &vDir, &vDir );
            vFacing = p2->Facing();
            fDot = D3DXVec3Dot( &vDir, &vFacing );
    
            if ( fDot > 0.0f )
                m_fPriorities[i][j] += fDot * PRIORITY_FACING_SCALE;           
        }
    }

    // priorities found, now build routes

    BYTE InChannelsUsed[ MAX_PLAYERS ];
    BYTE OutChannelsUsed[ MAX_PLAYERS ];
    INT iMaxIdx = 0, jMaxIdx = 0;
    FLOAT fMaxPriority;

    ZeroMemory( InChannelsUsed, MAX_PLAYERS );
    ZeroMemory( OutChannelsUsed, MAX_PLAYERS );
    
    // brute force N^4 method for now - this can be more efficient
    // right now we go through the NxN matrix, find the largest priority, make sure
    // they can send to each other, go through again, find the next smallest, etc.
    // We make sure to remove players that use up their sending quota or recieving
    // quota from getting more send tos or recv froms respectively.    
    
    for ( i = 0; i < iPlayers; i++ )
    {
        m_byNumSendToPlayerIDs[ i ] = 0;
        m_byNumRecvFromPlayerIDs[ i ] = 0;
    }

    for(;;)
    {
        fMaxPriority = 0.0f;     
        for ( i = 0; i < iPlayers; i++ )
            for ( j = 0; j < iPlayers; j++ )
            {
                if ( m_fPriorities[ i ][ j ] > fMaxPriority )
                {
                    fMaxPriority = m_fPriorities[ i ][ j ];
                    iMaxIdx = i;
                    jMaxIdx = j;
                }
            }

        if ( fMaxPriority == 0.0f ) break;

        m_fPriorities[ iMaxIdx ][ jMaxIdx ] = 0.0f;
                
        m_wSendToPlayerIDs[ jMaxIdx ][ OutChannelsUsed[ jMaxIdx ] ] = g_PlayerMgr.PlayerFromIndex( iMaxIdx )->LUID();
        OutChannelsUsed[ jMaxIdx ]++;
        m_byNumSendToPlayerIDs[ jMaxIdx ] = OutChannelsUsed[ jMaxIdx ];

        m_wRecvFromPlayerIDs[ iMaxIdx ][ InChannelsUsed[ iMaxIdx ] ] = g_PlayerMgr.PlayerFromIndex( jMaxIdx )->LUID();
        InChannelsUsed[ iMaxIdx ]++;
        m_byNumRecvFromPlayerIDs[ iMaxIdx ] = InChannelsUsed[ iMaxIdx ];

   
        // If we are at the limit recieving or sending, we clear out the column or row respectively

        if ( InChannelsUsed[ iMaxIdx ] == g_Marketplace.GetHostProperties().byNumClientChannelsIn )
        {
            for (i = 0;  i < iPlayers; i++)
                m_fPriorities[ iMaxIdx ][ i ] = 0.0f;
        }

        if ( OutChannelsUsed[ jMaxIdx ] == g_Marketplace.GetHostProperties().byNumClientChannelsOut )
        {
            for (i = 0;  i < iPlayers; i++)
                m_fPriorities[ i ][ jMaxIdx ] = 0.0f;
        }
    }   

    // copy over local priority information, so we don't have to do an index lookup every time

    m_byNumLocalSendToPlayerIDs = m_byNumSendToPlayerIDs[ iLocalPlayerIdx ];
    memcpy( m_wLocalSendToPlayerIDs, m_wSendToPlayerIDs[ iLocalPlayerIdx ], sizeof(WORD) * m_byNumLocalSendToPlayerIDs );     
}




//------------------------------------------------------------------------------
// Name: VoiceMgr::IsPlayerInLocalListenSet()
// Desc: Returns whether or not we would be sending to the given player if 
//       we were talking normally; used for highlighting
//------------------------------------------------------------------------------
BOOL VoiceMgr:: IsPlayerInLocalListenSet( WORD wPlayerLUID )
{
    for (int i = 0; i < m_byNumLocalSendToPlayerIDs; i++)
    {
        if ( m_wLocalSendToPlayerIDs[ i ] == wPlayerLUID ) 
            return true;
    }
    return false;
}




//------------------------------------------------------------------------------
// Name: VoiceMgr::WriteVoiceData()
// Desc: Adds the data of a single VoicePacketWrapper to a network packet
//------------------------------------------------------------------------------
VOID VoiceMgr::WriteVoiceData( Packet &p, VoicePacketWrapper &vpw )
{   
    // mark it so we don't encrypt data after this point
    p.WriteNoEncryptionMarker();
    p << vpw.wVoiceFromID;
    p << vpw.wVoiceToID;          
    p << vpw.byVoiceMode;
    p << vpw.nPackets;
    if ( vpw.nPackets == 0) return;
    p.WriteRawBytes( vpw.bVoiceData, vpw.nPackets * COMPRESSED_VOICE_SIZE );               
}




//------------------------------------------------------------------------------
// Name: VoiceMgr::AppendVoiceDataForPlayer()
// Desc: Appends voice data to a network packet- can be used when sending update
//       packets to avoid packet overhead.  If this function isn't called and we
//       have voice data for a specific player, it will get sent during the
//       SendNetworkVoice function above.  The function returns the # of
//       voice bytes appended.  Because voice can't be encrpyted in VDP, this 
//       must be the last thing added to a packet.  WriteVoiceData actually
//       clears the encryption market in the packet class for us.
//------------------------------------------------------------------------------
WORD VoiceMgr::AppendVoiceDataForPlayer( Packet &p, WORD wPlayerLUID )
{
    std::list<VoicePacketWrapper>::iterator i, j;
    BYTE   bySourceCount;
    WORD   wPacketPosSave, wSubSave;
    WORD    wBytes;

    wPacketPosSave = p.Size();
    bySourceCount = 0;
    
    // go through our output queue and find any data for this player - first we need a count

    for ( i = m_OutVoicePackets.begin(); i != m_OutVoicePackets.end(); i++ )
    {
        if ( i->wVoiceToID == wPlayerLUID ) 
            bySourceCount++;
    }

    p << bySourceCount;

    // now output the packets

    for ( i = m_OutVoicePackets.begin(); i != m_OutVoicePackets.end(); i = j )
    {
        j = i; j++;
        wSubSave = p.Size();

        if ( i->wVoiceToID == wPlayerLUID ) 
        {
            WriteVoiceData( p, *i );

            // if we're the host, and we are in peer-to-peer mode, traffic to the local players
            // and bots on the host shouldn't affect network stats

            if ( g_Marketplace.IsHost() && ( g_Marketplace.GetHostProperties().byVoiceMode == VM_PEER_TO_PEER ))
            {
                if ( i->byVoiceMode == VMM_NORMAL )                     
                {
                    wPacketPosSave += ( p.Size() - wSubSave );
                }
            }

            m_OutVoicePackets.erase( i );
        }     
    }    


    // If we are the host, we don't want traffic to local players or bots to confuse the stats
    // so we won't count the 1 byte of data above if we are in peer-to-peer mode
  
    wBytes = ( p.Size() - wPacketPosSave );
    if ( g_Marketplace.IsHost() && g_Marketplace.GetHostProperties().byVoiceMode == VM_PEER_TO_PEER &&
        (wBytes == 1 ) ) wBytes = 0;

    return wBytes;       
}




//------------------------------------------------------------------------------
// Name: VoiceMgr::AppendVoiceDataForHost()
// Desc: Appends voice data to a network packet- can be used when sending update
//       packets to avoid packet overhead.  If this function isn't called and we
//       have voice data for a specific player, it will get sent during the
//       SendNetworkVoice function above.  The function returns the # of
//       voice bytes appended.  Because voice can't be encrpyted in VDP, this 
//       must be the last thing added to a packet.  WriteVoiceData actually
//       clears the encryption market in the packet class for us.
//------------------------------------------------------------------------------
WORD VoiceMgr::AppendVoiceDataForHost( Packet &p )
{
    std::list<VoicePacketWrapper>::iterator i, j;
    BYTE   bySourceCount;
    WORD   wPacketPosSave;

    wPacketPosSave = p.Size();
    bySourceCount = 0;
    
    // go through our output queue and find any data for the host - first we need a count

    for ( i = m_OutVoicePackets.begin(); i != m_OutVoicePackets.end(); i++ )
    {
        if ( ( i->wVoiceToID == ID_NORMAL_VOICE )  ||
            ( i->wVoiceToID == ID_PODIUM ) ||
            ( i->wVoiceToID == ID_PRIVATE_CHANNEL ) )
            bySourceCount++;
    }

    p << bySourceCount;

    for ( i = m_OutVoicePackets.begin(); i != m_OutVoicePackets.end(); i = j )
    {
        j = i; j++;
    
        if ( ( i->wVoiceToID == ID_NORMAL_VOICE )  ||
            ( i->wVoiceToID == ID_PODIUM ) ||
            ( i->wVoiceToID == ID_PRIVATE_CHANNEL ) )
        {
            WriteVoiceData( p, *i );
            m_OutVoicePackets.erase( i );
        }     
    }

    return ( p.Size() - wPacketPosSave ); 
}




//------------------------------------------------------------------------------
// Name: VoiceMgr::LocalChatDataReady()
// Desc: Gets local voice data from XHV (this is a callback, part of ITitleXHV)
//------------------------------------------------------------------------------
HRESULT VoiceMgr::LocalChatDataReady( DWORD dwPort, DWORD dwSize, PVOID pData )
{      
    if ( m_LocalVoice.nPackets == MAX_BUNDLED_PACKETS ) return S_OK; 

    assert( dwSize == COMPRESSED_VOICE_SIZE );

    g_Marketplace.SetLastLocalSpeechTime( g_Marketplace.Time() );
    
    memcpy( m_LocalVoice.bVoiceData + m_LocalVoice.nPackets * COMPRESSED_VOICE_SIZE, pData, dwSize );
    m_LocalVoice.nPackets++;
     
    return S_OK;
}




//------------------------------------------------------------------------------
// Name: VoiceMgr::GetLocalVoiceData()
// Desc: Returns the data written to by LocalChatDataReady
//------------------------------------------------------------------------------
VoicePacketWrapper &VoiceMgr::GetLocalVoiceData()
{
    return m_LocalVoice;
}