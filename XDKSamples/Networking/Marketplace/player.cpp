//-----------------------------------------------------------------------------
// File: Player.cpp
//
// Desc: PlayerMgr manages the list of Players and PlayerBots
//       Player::Update pulls voice off the incoming queue
//       and submits it to the sound sources
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "CommonInclude.h"


PlayerMgr g_PlayerMgr;




//------------------------------------------------------------------------------
// Name: PlayerMgr::PlayerMgr()
// Desc: Construct the player manager
//------------------------------------------------------------------------------
PlayerMgr::PlayerMgr()
{
    m_pLocalPlayer      = NULL;    
    m_wNextLocalUserID  = ID_FIRST_PLAYER; // start high with luids so we can detect index problems early
}





//------------------------------------------------------------------------------
// Name: PlayerMgr::CreateNewPlayer()
// Desc: Creates a player or mockup based on the create flags
//------------------------------------------------------------------------------
Player *PlayerMgr::CreateNewPlayer( DWORD dwCreateFlags )
{
    Player *plr;
    if ( dwCreateFlags & LOCALFLAG_BOT )
        plr = new PlayerBot;
    else
        plr = new Player;
    plr->m_luid = m_wNextLocalUserID++;
    
    if ( dwCreateFlags & LOCALFLAG_BOT )
    {        
        plr->m_xuid.qwUserID = plr->m_luid; // need a fake xuid        
    }   

    m_PlayerList.push_back( plr );
    g_VoiceMgr.MarkForRebuild();
    return plr;
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::CreateNewPlayerWithLUID()
// Desc: Creates a new player with the LUID given by the host
//------------------------------------------------------------------------------
Player *PlayerMgr::CreateNewPlayerWithLUID( WORD luid )
{
    Player *plr;
    plr = new Player;
    plr->m_luid = luid;
    m_PlayerList.push_back( plr );
    g_VoiceMgr.MarkForRebuild();
    return plr;
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::Update()
// Desc: Update players data ( moving, talking state, etc )
//------------------------------------------------------------------------------
VOID PlayerMgr::Update( float fDt )
{
    std::vector<Player *>::iterator i;

    for ( i = m_PlayerList.begin(); i != m_PlayerList.end(); i++ )
    {
        (*i)->Update( fDt );
    }
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::RemovePlayer()
// Desc: Remove a player from the session
//------------------------------------------------------------------------------
VOID PlayerMgr::RemovePlayer( WORD luid )
{
    std::vector<Player *>::iterator i;
    Player *p;

    for ( i = m_PlayerList.begin(); i != m_PlayerList.end(); i++ )
    {
        if ( (*i)->m_luid == luid )
        {
            p = *i;        
            m_PlayerList.erase( i ) ;             
            delete p;
            break;
        }
    }

    g_VoiceMgr.MarkForRebuild();
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::PlayerFromLUID()
// Desc: Return the player with the given local user id
//------------------------------------------------------------------------------
Player * PlayerMgr::PlayerFromLUID( WORD luid )
{
    std::vector<Player *>::iterator i;

    for ( i = m_PlayerList.begin(); i != m_PlayerList.end(); i++ )
    {
        if ( (*i)->m_luid == luid )
        {
            return *i;            
        }
    }
    return NULL;
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::NumPlayers()
// Desc: Return the number of players in our database
//------------------------------------------------------------------------------
INT PlayerMgr::NumPlayers()
{
    return m_PlayerList.size(); 
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::PlayerFromIndex()
// Desc: Return player with given index
//------------------------------------------------------------------------------
Player *PlayerMgr::PlayerFromIndex( WORD idx )
{
    return m_PlayerList[ idx ];
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::IndexFromPlayer()
// Desc: Returns the index the current player has in our list (not the LUID)
//------------------------------------------------------------------------------
INT PlayerMgr::IndexFromPlayer( Player *p )
{
   DWORD i;

   for ( i = 0; i < m_PlayerList.size(); i++ )
   {
      if ( m_PlayerList[ i ] == p )
      {
          return i; 
      }
   }
   return -1;
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::SetLocalPlayerLUID()
// Desc: Sets what the luid is for the player on this Xbox
//------------------------------------------------------------------------------
VOID PlayerMgr::SetLocalPlayerLUID( WORD luid )
{
    m_pLocalPlayer = PlayerFromLUID( luid );
    if ( m_pLocalPlayer == NULL )
        OutputDebugStringA("Error- local player doesn't exist\n");
    
    m_pLocalPlayer->m_byLocalFlags |= LOCALFLAG_LOCAL;
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::GetLocalPlayer()
// Desc: Returns the local player
//------------------------------------------------------------------------------
Player *PlayerMgr::GetLocalPlayer() 
{ 
    return m_pLocalPlayer;
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::WritePlayerListFull()
// Desc: Writes the entire state of the player list to a packet
//------------------------------------------------------------------------------
VOID PlayerMgr::WritePlayerListFull( Packet &p )
{
    std::vector<Player *>::iterator i;
    p << (BYTE)m_PlayerList.size();
    for ( i = m_PlayerList.begin(); i != m_PlayerList.end(); i++ )
    {
        p << (*i)->m_luid;
        (*i)->WritePlayerFull( p );
    }
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::ReadPlayerListFull()
// Desc: Reads the entire state of the player list to a packet
//------------------------------------------------------------------------------
VOID PlayerMgr::ReadPlayerListFull( Packet &p )
{
    std::vector<Player *>::iterator i;
    BYTE j, size;
    WORD luid;
    Player *pPlayer;
    
    p >> size;
    for ( j = 0; j < size; j++ )
    {
        p >> luid;
        for (i = m_PlayerList.begin(); i != m_PlayerList.end(); i++ )
        {
            if ( (*i)->m_luid == luid ) break;            
        }
        
        if ( i == m_PlayerList.end() )
            pPlayer = CreateNewPlayerWithLUID( luid );
        else
            pPlayer = *i;

        pPlayer->ReadPlayerFull( p );
    }
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::WritePlayerListUpdate()
// Desc: Writes a incremental update of the player list to a packet
//------------------------------------------------------------------------------
VOID PlayerMgr::WritePlayerListUpdate( Packet &p )
{
    std::vector<Player *>::iterator i;

    p << (BYTE)m_PlayerList.size();    
    for ( i = m_PlayerList.begin(); i != m_PlayerList.end(); i++ )
    {
        p << (*i)->m_luid;
        (*i)->WritePlayerUpdate( p );
    }
}




//------------------------------------------------------------------------------
// Name: PlayerMgr::ReadPlayerListUpdate()
// Desc: Reads a incremental update of the player list to a packet
//------------------------------------------------------------------------------
VOID PlayerMgr::ReadPlayerListUpdate( Packet &p )
{
    std::vector<Player *>::iterator i;
    BYTE j, size;
    WORD luid;    
    
    p >> size;
    for ( j = 0; j < size; j++ )
    {
        p >> luid;
        for (i = m_PlayerList.begin(); i != m_PlayerList.end(); i++ )
        {
            if ( (*i)->m_luid == luid ) break;            
        }
        
        if ( i == m_PlayerList.end() )
            OutputDebugStringA( "Tried to update a player that didn't exist (possible sync problem) \n" );
        else
        {
            (*i)->ReadPlayerUpdate( p );
        }
    }
}



//------------------------------------------------------------------------------
// Name: Player::Player()
// Desc: Constructor for a player object
//------------------------------------------------------------------------------
Player::Player()
{
    PositionedWaveBankSound *pSound;

    m_dwPlayerFlags             = 0;
    m_fAnimationTime            = 0.0f;
    m_luid                      = 0;
    m_strPlayerName[0]          = 0;
    m_byLocalFlags              = LOCALFLAG_NORMAL;
    m_fLastMessage              = 0.0f;
    m_fLastVoice                = 0.0f;
    m_byCurAnim                 = 0;
    m_byLocalFlags              = LOCALFLAG_NORMAL;

    
    SetFacing( D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) );
    m_dObj.Initialize( D3DXVECTOR3( 0.0f, 0.0f, 0.0f ), -1, GFX_PLAYER_RADIUS );    
    m_dObj.SetVoiceSound( g_AudioMgr.CreatePositionedVoiceSound() );   
    
    pSound = g_AudioMgr.CreatePositionedWaveBankSound();
    pSound->Initialize("walla", D3DXVECTOR3(0.f, 0.f, 0.f) );   
    pSound->SetAutoEnable( false );
    m_dObj.SetWavSound( pSound );
}




//------------------------------------------------------------------------------
// Name: Player::~Player()
// Desc: Destroy a player object
//------------------------------------------------------------------------------
Player::~Player()
{    
}



//------------------------------------------------------------------------------
// Name: Misc. accessors for player object
//------------------------------------------------------------------------------
DWORD               Player::Flags()      { return m_dwPlayerFlags; }
WORD                Player::LUID()       { return m_luid; }
const XUID &        Player::GetXUID()    { return m_xuid; }
const D3DXVECTOR3 & Player::Position()   { return m_dObj.GetPosition(); }
const D3DXVECTOR3 & Player::Facing()     { return m_vFacing; }




//------------------------------------------------------------------------------
// Name: Player::SetPosition()
// Desc: Sets a players position
//------------------------------------------------------------------------------
VOID Player::SetPosition( const D3DXVECTOR3 &vPos )
{
    m_dObj.Move( vPos );  
}




//------------------------------------------------------------------------------
// Name: Player::SetFacing()
// Desc: Updates the animation for a change in a players facing
//------------------------------------------------------------------------------
VOID Player::SetFacing( const D3DXVECTOR3 &vFacing )
{
    m_vFacing = vFacing;

    float fTheta;
    fTheta = -atan2f(vFacing.x, vFacing.y) + 1.0f * D3DX_PI/8.0f;
    if (fTheta < 0) fTheta += D3DX_PI*2.0f;

    m_byFacingIdx = (BYTE)( fTheta * 4.0f / D3DX_PI );
}




//------------------------------------------------------------------------------
// Name: Player::IsLocal()
// Desc: Returns whether the player is local or a bot on the local machine
//------------------------------------------------------------------------------
BOOL Player::IsLocal()
{
    return ((m_byLocalFlags & LOCALFLAG_LOCAL) || (m_byLocalFlags & LOCALFLAG_BOT ));
}




//------------------------------------------------------------------------------
// Name: Player::UpdateAnimation()
// Desc: Updates the animation state of the player
//------------------------------------------------------------------------------
VOID Player::UpdateAnimation( FLOAT fDt )
{   
   BYTE byAnim;
   INT  iFramesToAdd;
   D3DXVECTOR3 vDist;
   byAnim = ( m_dwPlayerFlags & PLAYERFLAG_MOVING ) ? 1 : 0;
 
   if ( byAnim != m_byCurAnim ) // switch animations
   {
      m_byAnimIdx = 0;
      m_byCurAnim = byAnim;
      m_fAnimationTime = 0.0f;
      m_byAnimIdx = 0;   
   }
   else
   {
        m_fAnimationTime += fDt;
            
        if ( byAnim == 0 ) // idle
        {
            iFramesToAdd = (INT)(m_fAnimationTime / GFX_IDLE_SPEED);
            m_fAnimationTime -= iFramesToAdd * GFX_IDLE_SPEED;
            
            // now no idle frames
            m_byAnimIdx = 0;
        }

        if ( byAnim == 1 ) // walk
        {
            iFramesToAdd = (INT)(m_fAnimationTime / GFX_WALK_SPEED);
            m_fAnimationTime -= iFramesToAdd * GFX_WALK_SPEED;
            m_byAnimIdx += iFramesToAdd;
            m_byAnimIdx %= 8; // 8 frames of animation
        }
   }
    
   if ( m_byCurAnim == 0 )
        m_dObj.SetImageIndex( GFX_GIRL_START_IDLE + m_byAnimIdx + GFX_GIRL_FACING_STRIDE * m_byFacingIdx );
   if ( m_byCurAnim == 1 )
        m_dObj.SetImageIndex( GFX_GIRL_START_WALK + m_byAnimIdx + GFX_GIRL_FACING_STRIDE * m_byFacingIdx );

   // label stuff

   if ( g_Marketplace.IsShowNames() )
   {
        m_dObj.SetLabel( m_strPlayerName, 0xffffffff );
   }
   else
   {
        WCHAR *glyph;    
        DWORD dwColor;

        glyph = GLYPH_DOWN_TICK;
        dwColor = REMOTE_PLAYER_COLOR;
        
        if ( m_dwPlayerFlags & PLAYERFLAG_TALKPODIUM )
            glyph = GLYPH_BROADCAST_ICON;
        else if ( m_dwPlayerFlags & PLAYERFLAG_TALKPRIVATE )
            glyph = GLYPH_PRIVATE_TALK_ICON;
        else if ( m_dwPlayerFlags & PLAYERFLAG_TALKING )
            glyph = GLYPH_TALK_ICON;
        else
            glyph = GLYPH_PLAYER_ICON;

        if ( m_dwPlayerFlags & PLAYERFLAG_ONPRIVATECHANNEL )
            m_dObj.SetSublabel( L"P", 0xffa03030 ) ;
        else if ( m_dwPlayerFlags & PLAYERFLAG_BOT )
            m_dObj.SetSublabel( GLYPH_BOT_ICON , 0xffa0a0a0 );
        else
            m_dObj.SetSublabel( NULL, 0x00000000 ) ;

        vDist = g_PlayerMgr.GetLocalPlayer()->Position() - PODIUM_LOCATION;
        
        if ( D3DXVec3LengthSq( &vDist ) < PODIUM_RADIUS )
        {
            // send to everyone if near the podium
            m_dObj.SetDiffuseColor( 0xffffffff );
        }
        else if ( g_PlayerMgr.GetLocalPlayer()->IsPlayerFlagSet( PLAYERFLAG_ONPRIVATECHANNEL ) &&
                g_Marketplace.GetDefaultGamepad()->bLastAnalogButtons[ XINPUT_GAMEPAD_A ] )
        {
            if ( m_dwPlayerFlags & PLAYERFLAG_ONPRIVATECHANNEL )
            {
                m_dObj.SetDiffuseColor( 0xffffffff );
            }
            else
            {
                m_dObj.SetDiffuseColor( 0xff404040 );            
            }
        }
        else if ( ( g_VoiceMgr.IsPlayerInLocalListenSet( m_luid ) ) || ( m_byLocalFlags & LOCALFLAG_LOCAL ) )
            m_dObj.SetDiffuseColor( 0xffffffff );
        else
            m_dObj.SetDiffuseColor( 0xff404040 );


        if ( m_byLocalFlags & LOCALFLAG_LOCAL )
            dwColor = LOCAL_PLAYER_COLOR;        
        else if ( m_byLocalFlags & LOCALFLAG_RECENTTALK )
            dwColor = TALKING_REMOTE_COLOR;
        else
            dwColor = REMOTE_PLAYER_COLOR;
           
        m_dObj.SetLabel( glyph, dwColor );   
   }
}




//------------------------------------------------------------------------------
// Name: Player::SetOnlineData()
// Desc: Sets the data associated with a player client
//------------------------------------------------------------------------------
VOID Player::SetOnlineData( XNADDR *pxnAddr, IN_ADDR *pinAddr, XUID xuid )
{  
    m_xuid = xuid;
    
    GetVoiceSound()->Initialize( m_xuid );

    if ( pxnAddr && !pinAddr )
    {
        g_Marketplace.GetInAddrFromXnAddr( &m_inAddr, pxnAddr );
        m_xnAddr = *pxnAddr;
    } 
    else if ( !pxnAddr && pinAddr )
    {
        g_Marketplace.GetXnAddrFromInAddr( &m_xnAddr, pinAddr );
        m_inAddr = *pinAddr;
    }
    else if ( pxnAddr && pinAddr )
    {
        m_xnAddr = *pxnAddr;
        m_inAddr = *pinAddr;
    }     
}




//------------------------------------------------------------------------------
// Name: Player::SetName()
// Desc: Sets the name of a player
//------------------------------------------------------------------------------
VOID Player::SetName( char *pData )
{
    swprintf( m_strPlayerName, L"%S", pData );         
}





//------------------------------------------------------------------------------
// Name: Player::SetName()
// Desc: Sets the name of a player, wide char style
//------------------------------------------------------------------------------
VOID Player::SetName( WCHAR *pData )
{
    wcscpy( m_strPlayerName, pData );
}




//------------------------------------------------------------------------------
// Name: Player::Update()
// Desc: If we are the local player ( LOCALFLAG_LOCAL ), we move around here--
//       otherwise, we just update our animation ( we've been slaved to the host )
//       If someone is talking and we can't hear them, we play a placeholder 
//       'walla walla' sound so you know there is noise coming from that direction in 5.1
//------------------------------------------------------------------------------

VOID Player::Update( FLOAT fDt )
{
    D3DXVECTOR3 vFacing, vPos, vGoalVelocity;
    VoicePacketWrapper vpw;
 
    // are we the local player?
    if ( m_byLocalFlags & LOCALFLAG_LOCAL )              
    {
        // ok, now local player update             
        vGoalVelocity.x = g_Marketplace.GetDefaultGamepad()->fX1; 
        vGoalVelocity.y = -g_Marketplace.GetDefaultGamepad()->fY1; 
        vGoalVelocity.z = 0.0f;
    
        if ( D3DXVec3LengthSq( &vGoalVelocity ) < 0.01f )  
        {
            vGoalVelocity.x = 0.0f; 
            vGoalVelocity.y = 0.0f;
        }

        vFacing = m_vFacing;
        vFacing += vGoalVelocity / 5.0f;

        vGoalVelocity *= MAX_SPEED;

        D3DXVec3Normalize( &vFacing, &vFacing );
        vPos = m_dObj.GetPosition();
        vPos += D3DXVec3Dot( &vFacing, &vGoalVelocity ) * vFacing * fDt;

        g_Marketplace.HandleCollisions( &m_dObj, &vPos );
        
        m_dwPlayerFlags &= ~PLAYERFLAG_MOVING;

        if ( ( m_dObj.GetPosition().x != vPos.x ) || ( m_dObj.GetPosition().y != vPos.y ) )
            m_dwPlayerFlags |= PLAYERFLAG_MOVING;
        
        // update actual position now
        m_dObj.Move( vPos );
        SetFacing( vFacing );

        // now get voice, and play at the location of the appropriate players around us        
        Player *plr; 

        // keep getting voice packets for the local player from the voice in queue
        while ( g_VoiceMgr.GetVoiceForPlayer( m_luid, vpw ) )
        {
            plr = g_PlayerMgr.PlayerFromLUID( vpw.wVoiceFromID );
            if (!plr) continue;

            // submit it to the PositionedVoiceSounds associated with the players
            
            if ( vpw.byVoiceMode == VMM_NORMAL ) 
            {
                 plr->GetVoiceSound()->SubmitVoicePacket( vpw.bVoiceData, vpw.nPackets, g_Marketplace.Time(), VOM_NORMAL );
            }
            if ( vpw.byVoiceMode == VMM_PODIUM )
            {                
                plr->GetVoiceSound()->SubmitVoicePacket( vpw.bVoiceData, vpw.nPackets, g_Marketplace.Time(), VOM_PODIUM );
            }
            if ( vpw.byVoiceMode == VMM_PRIVATE )
            {              
                plr->GetVoiceSound()->SubmitVoicePacket( vpw.bVoiceData, vpw.nPackets, g_Marketplace.Time(), VOM_COMMUNICATOR );            
            }
        }
    }
    else
    {   
        // If they are talking and we haven't heard from them in a while (which means we
        // aren't in their send list), start playing a walla-walla sound

        // when we actually hear them or not

        if ( ( g_Marketplace.Time() - m_fLastVoice ) < DELAY_HIGHLIGHT_TALK ) 
        {
            // we've heard from them recently
            m_byLocalFlags |= LOCALFLAG_RECENTTALK;
    
            // stop walla-ing if we are walla-ing
            if ( m_byLocalFlags & LOCALFLAG_WALLA )
            {
                StopWalla();
            }
        }
        else 
        {
            m_byLocalFlags &= ~LOCALFLAG_RECENTTALK;

            // we haven't heard from them recently
            if ( m_dwPlayerFlags & PLAYERFLAG_TALKING )
            {
                // they are talking, so we start walla-ing
                if ( !( m_byLocalFlags & LOCALFLAG_WALLA ) )
                {
                    StartWalla();
                }
            }
            else
            {
                // they aren't talking, so we stop walla-ing
                if ( m_byLocalFlags & LOCALFLAG_WALLA )
                {
                    StopWalla();
                }
            }
        }
    }

    UpdateAnimation( fDt );
}




//------------------------------------------------------------------------------
// Name: Player::GetVoiceSound()
// Desc: Returns the PositionedVoiceSound for a player
//------------------------------------------------------------------------------
PositionedVoiceSound *Player::GetVoiceSound()
{
    return (PositionedVoiceSound *)m_dObj.GetVoiceSound();
}




//------------------------------------------------------------------------------
// Name: Player::ReadPlayerFull()
// Desc: Reads a full player update from a packet
//------------------------------------------------------------------------------
VOID Player::ReadPlayerFull( Packet &p )
{  
    p.ReadRawBytes( m_strPlayerName, XONLINE_GAMERTAG_SIZE * sizeof( WCHAR ) );
    p >> m_xuid;
    p >> m_xnAddr;

    SetOnlineData( &m_xnAddr, NULL, m_xuid );

    g_Marketplace.GetInAddrFromXnAddr( &m_inAddr, &m_xnAddr );    
    ReadPlayerUpdate( p );
}




//------------------------------------------------------------------------------
// Name: Player::WritePlayerFull()
// Desc: Writes a full player update from a packet
//------------------------------------------------------------------------------
VOID Player::WritePlayerFull( Packet &p )
{
    p.WriteRawBytes( m_strPlayerName, XONLINE_GAMERTAG_SIZE * sizeof( WCHAR ) );  
    p << m_xuid;
    p << m_xnAddr;
    WritePlayerUpdate( p );
}




//------------------------------------------------------------------------------
// Name: Player::ReadPlayerUpdate()
// Desc: Reads a partial player update from a packet
//------------------------------------------------------------------------------
VOID Player::ReadPlayerUpdate( Packet &p )
{
    D3DXVECTOR3 vP, vF;
    DWORD dwPFlags;
        
    p >> dwPFlags;
    p >> vF.x;
    p >> vF.y;
    p >> vP.x;
    p >> vP.y;
    vF.z = vP.z = 0.0f;

    if (! ( m_byLocalFlags & LOCALFLAG_LOCAL ))
    {
        if (( dwPFlags & PLAYERFLAG_TALKING ) !=
            ( m_dwPlayerFlags & PLAYERFLAG_TALKING ))            
        {
            g_VoiceMgr.MarkForRebuild();            
        }

        m_dwPlayerFlags = dwPFlags;        
        SetFacing( vF );
        SetPosition( vP );
    }
}




//------------------------------------------------------------------------------
// Name: Player::WritePlayerUpdate()
// Desc: Writes a partial player update from a packet
//------------------------------------------------------------------------------
VOID Player::WritePlayerUpdate( Packet &p )
{
    p << m_dwPlayerFlags;    
    p << m_vFacing.x;
    p << m_vFacing.y;
    p << m_dObj.GetPosition().x;
    p << m_dObj.GetPosition().y;
}

     

//------------------------------------------------------------------------------
// Name: Player::ResetPlayerFlags()
// Desc: Resets the flags set on a player
//------------------------------------------------------------------------------
VOID Player::ResetPlayerFlags()
{
    m_dwPlayerFlags = 0;
}




//------------------------------------------------------------------------------
// Name: Player::SetPlayerFlag()
// Desc: Sets the given flag on a player
//------------------------------------------------------------------------------
VOID Player::SetPlayerFlag( DWORD dwFlag )
{
    m_dwPlayerFlags |= dwFlag;
}



//------------------------------------------------------------------------------
// Name: Player::ClearPlayerFlag()
// Desc: Clears the given flag on a player
//------------------------------------------------------------------------------
VOID Player::ClearPlayerFlag( DWORD dwFlag )
{
    m_dwPlayerFlags &= ~dwFlag;
}




//------------------------------------------------------------------------------
// Name: Player::IsPlayerFlagSet()
// Desc: Tests the given flag on a player
//------------------------------------------------------------------------------
BOOL Player::IsPlayerFlagSet( DWORD dwFlag )
{
    return (m_dwPlayerFlags & dwFlag)? TRUE: FALSE;
}



//------------------------------------------------------------------------------
// Name: Player::InAddr()
// Desc: Returns the Xbox InAddr for the player
//------------------------------------------------------------------------------
IN_ADDR &Player::InAddr()
{
    return m_inAddr;
}




//------------------------------------------------------------------------------
// Name: Player::SetLastMessageTime()
// Desc: Sets the last time we received network traffic from this player
//------------------------------------------------------------------------------
VOID Player::SetLastMessageTime( FLOAT fT )
{
    m_fLastMessage = fT;
}




//------------------------------------------------------------------------------
// Name: Player::SetLastVoiceTime()
// Desc: Sets the last time we heard voice from this player
//------------------------------------------------------------------------------
VOID Player::SetLastVoiceTime( FLOAT fT )
{
    m_fLastVoice = fT;
}




//------------------------------------------------------------------------------
// Name: Player::LastMessageTime()
// Desc: Returns the last time we got network traffic from this player
//------------------------------------------------------------------------------
FLOAT Player::LastMessageTime()
{
    return m_fLastMessage;
}




//------------------------------------------------------------------------------
// Name: Player::StartWalla()
// Desc: Starts the walla-walla sound which means someone is talking but
//       we can't hear them (i.e. there is voice in this direction cue)
//------------------------------------------------------------------------------
VOID Player::StartWalla()
{
    m_byLocalFlags |= LOCALFLAG_WALLA;
    if ( m_dObj.GetWavSound()) 
        m_dObj.GetWavSound()->Enable();
}




//------------------------------------------------------------------------------
// Name: Player::StopWalla()
// Desc: Stops the walla-walla sound
//------------------------------------------------------------------------------
VOID Player::StopWalla()
{
    m_byLocalFlags &= ~LOCALFLAG_WALLA;
    if ( m_dObj.GetWavSound()) 
        m_dObj.GetWavSound()->Disable();   
}




//------------------------------------------------------------------------------
// Name: PlayerBot::PlayerBot()
// Desc: Constructor
//------------------------------------------------------------------------------
PlayerBot::PlayerBot()
{
    m_dwVoice               = 0;
    m_fWantToTalkMeter      = 0.0f;
    m_byLocalFlags          |= LOCALFLAG_BOT;   
    m_dwPlayerFlags         |= PLAYERFLAG_BOT;
    m_fLastHeardSomething   = g_Marketplace.Time();
}




//------------------------------------------------------------------------------
// Name: PlayerBot::SetVoice()
// Desc: Sets which voice this bot uses
//------------------------------------------------------------------------------
VOID PlayerBot::SetVoice( DWORD dwVoice )
{
    m_dwVoice = dwVoice;
}




//------------------------------------------------------------------------------
// Name: PlayerBot::Update()
// Desc: Updates the player bot- we throw away voice the bot would hear,
//       but try and make it so the bot stops talking when he hears others 
//       speaking
//------------------------------------------------------------------------------
VOID PlayerBot::Update( FLOAT fDt )
{
    VoicePacketWrapper vpw;
    
    while ( g_VoiceMgr.GetVoiceForPlayer( m_luid, vpw) )    
        m_fLastHeardSomething = g_Marketplace.Time();

    if ( g_Marketplace.Time() - m_fLastHeardSomething > 0.1f )
        m_fWantToTalkMeter += ((FLOAT) rand() / RAND_MAX );  // add 0 to 1
    else if ( m_fWantToTalkMeter > -20.0f )
        m_fWantToTalkMeter -= ((FLOAT) rand() / RAND_MAX );  // subtract 0 to 1
    
    if ( m_dwPlayerFlags & PLAYERFLAG_TALKING ) 
    {
        if ( !g_SoundbitDB.IsPlaying( m_luid ) )
        {
            m_fWantToTalkMeter = -60.0f;
            m_dwPlayerFlags &= ~PLAYERFLAG_TALKING;
        }
        else if ( m_fWantToTalkMeter < 0 ) 
        {
            g_SoundbitDB.StopSoundbit( m_luid );            
            m_dwPlayerFlags &= ~PLAYERFLAG_TALKING;
        }
    }
    else if ( m_fWantToTalkMeter > 0 )
    {   
        m_dwPlayerFlags |= PLAYERFLAG_TALKING;
        
        g_SoundbitDB.PlaySoundbitToVoiceMgr( m_dwVoice, rand()%10, m_luid );           
    }

    Player::Update( fDt );    
}