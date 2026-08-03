//-----------------------------------------------------------------------------
// File: Player.h
//
// Desc: PlayerMgr manages the list of Players and PlayerBots
//       Player::Update pulls voice off the incoming queue
//       and submits it to the sound sources
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once

// Flags for player
#define PLAYERFLAG_MOVING       0x1
#define PLAYERFLAG_TALKING      0x2
#define PLAYERFLAG_TALKPODIUM   0x4
#define PLAYERFLAG_TALKPRIVATE  0x8
#define PLAYERFLAG_TALKSYSTEM   0x10
#define PLAYERFLAG_ONPRIVATECHANNEL 0x20
#define PLAYERFLAG_BOT          0x40

// How long the highlight for talking stays after we receive a network packet from a player
#define DELAY_HIGHLIGHT_TALK    1.0f

// local flags are not propogated over the network
#define LOCALFLAG_NORMAL        0x0
#define LOCALFLAG_BOT           0x1  // for host
#define LOCALFLAG_LOCAL         0x2  // player is controlled w/ controller
#define LOCALFLAG_RECENTTALK    0x4  // player recently talked to me
#define LOCALFLAG_WALLA         0x8  // player is walla-wallaing

class Player
{
friend class PlayerMgr;
friend class VoiceMgr;
public:       

    WORD            LUID();        // local user ID
    const XUID &    GetXUID();        // xonline user ID  (mocked up for Bots)
    
    DWORD                Flags();
    const D3DXVECTOR3 &  Position();
    const D3DXVECTOR3 &  Facing();
        
    
    VOID            StartWalla();   // play looping 'walla' noise when they are talking, but we have no voice data
    VOID            StopWalla();    // stop the walla sound

    VOID            ResetPlayerFlags();                 // clear all player flags
    VOID            SetPlayerFlag( DWORD dwFlag );      // set a player flag above
    VOID            ClearPlayerFlag( DWORD dwFlag );    // clear a player flag
    BOOL            IsPlayerFlagSet( DWORD dwFlag );    // test a player flag

    VOID            SetPosition( const D3DXVECTOR3 &vPos );
    VOID            SetFacing( const D3DXVECTOR3 &vFacing );  
    
    virtual VOID    Update( FLOAT fDt );                // Update the player
    
    // set the ip addr, etc associated with this player.
    // one of pxnAddr or pinAddr can be null- it will be calculated from the one specified.
    VOID            SetOnlineData( XNADDR *pxnAddr, IN_ADDR *pinAddr, XUID xuid );
    
    VOID            SetName( char *pData );             // set the name of a player
    VOID            SetName( WCHAR *pData );

    VOID            ReadPlayerFull( Packet &p );        // write full player state to a packet
    VOID            WritePlayerFull( Packet &p );       // read full player state from a packet
    
    VOID            ReadPlayerUpdate( Packet &p );      // write incremental player state to a packet
    VOID            WritePlayerUpdate( Packet &p );     // write incremental player state from a packet

    IN_ADDR &       InAddr();
    
    VOID            SetLastMessageTime( FLOAT fT );     // last time we recv'd a mesage from this player
    FLOAT           LastMessageTime();
    VOID            SetLastVoiceTime( FLOAT fT );       // last time we heard from this player

    PositionedVoiceSound *GetVoiceSound();              // get the associated voice sound
    BOOL                  IsLocal();                    // is this player local or a local bot?
protected:
    VOID            UpdateAnimation( FLOAT fDt );

    Player();
    virtual ~Player();

    // Flags
    DWORD           m_dwPlayerFlags;

    // Positional / Graphical
    D3DXVECTOR3     m_vFacing;                 // direction this player is facing   
    FLOAT           m_fAnimationTime;          // time so far for this animation frame
    DisplayedObject m_dObj;                    // object which displays this player in the world
    BYTE            m_byFacingIdx;             // index 1-8 of which direction we're facing
    BYTE            m_byAnimIdx;               // frame of animation we are on
    BYTE            m_byCurAnim;               // 0 = idle, 1 = walk

    // Identifying information
    XUID            m_xuid;                                     // xuid
    WORD            m_luid;                                     // local user id
    WCHAR           m_strPlayerName[ XONLINE_GAMERTAG_SIZE ];   // player name
    BYTE            m_byLocalFlags;                             // flags not propogated over the network

    // Network addresses
    IN_ADDR         m_inAddr;                  // will reflect the host for fake players
    XNADDR          m_xnAddr;     
    
    // Last we've heard from them
    FLOAT           m_fLastMessage;             // last time we got a message    
    FLOAT           m_fLastVoice;               // last we've heard them speak
};

class PlayerBot: public Player
{
public:
    PlayerBot();

    VOID SetVoice( DWORD dwVoice );

    virtual VOID Update( FLOAT fDt );

private:
    DWORD m_dwVoice;
    FLOAT m_fLastHeardSomething;
    float m_fWantToTalkMeter; 
};

class PlayerMgr
{
public:
    PlayerMgr();

    Player *    CreateNewPlayer( DWORD dwCreateFlags = LOCALFLAG_NORMAL );      // create a player (host)
    Player *    CreateNewPlayerWithLUID( WORD luid );                           // create a player with a specified local
                                                                                // user id (client)
    
    VOID        SetLocalPlayerLUID( WORD luid );    // sets which player is local
    Player *    GetLocalPlayer() ;                  // returns the local player

    VOID        RemovePlayer( WORD luid );          // removes a player from the player list

    Player *    PlayerFromLUID( WORD luid );        // gets a player from a local user id

    INT         NumPlayers();                       // used for iterating through all people
    Player *    PlayerFromIndex( WORD idx );        // used for iterating through all people
    INT         IndexFromPlayer( Player *p );       // used for iterating through all people

    VOID        Update( float fDt );

    VOID        WritePlayerListFull( Packet &p );   // write a full update for all players
    VOID        ReadPlayerListFull( Packet &p );

    VOID        WritePlayerListUpdate( Packet &p ); // write a incremental update for all players
    VOID        ReadPlayerListUpdate( Packet &p );

private:
    std::vector<Player *>   m_PlayerList;           // list of players
    WORD                    m_wNextLocalUserID;     // next free id
       
    Player *                m_pLocalPlayer;         // pointer to the local player
};

extern PlayerMgr g_PlayerMgr;