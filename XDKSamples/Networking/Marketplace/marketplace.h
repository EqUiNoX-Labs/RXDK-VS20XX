#pragma once
//-----------------------------------------------------------------------------
// File: Marketplace.h
//
// Desc: The Marketplace demo demonstrates proximity-based conversations for large 
//       numbers of users over speaker and headset.   It shows a method for dynamically
//       allocating bandwidth using simple peer prediction for a scalable mix of
//       peer-to-peer and client-server voice traffic.
//
//       Note that this is a demo more than a sample; the source code probably isn't
//       directly reusable, but it is more an implementation of the concepts that 
//       will be in the forthcoming whitepaper.  
//
//       Marketplace.h defines the main application class.  Read readme.txt for an 
//       overview of this sample
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


// number of properties in the following structure
#define NUM_HOST_PROPS 6

// host properties
struct HostProperties
{
    BYTE    byVoiceMode;          // VM_ defines in constants.h- peer to peer or server
    BYTE    byNetworkUpdateRate;  // 1/100ths of a second
    BYTE    byVoiceUpdateRate;    // 1/100ths of a second
    BYTE    bySpeechTimeout;      // 1/10ths of a second    
    BYTE    byNumClientChannelsIn;  // number of peer-to-peer channels
    BYTE    byNumClientChannelsOut; // number of peer-to-peer channels
};

// This is the state enumeration for what the sample is currently doing
enum State
{
    STATE_NONE,                 
    STATE_SELECT_ACCOUNT,       // selecting account to login on
    STATE_ERROR,                // error
    STATE_LOGGING_IN,           // pumping login task handle 
    STATE_SELECT_MODE,          // selecting host/client/soundbit editor
    STATE_HOST_GAME,            // hosting a game
    STATE_FIND_HOSTS,           // matchmaking
    
    STATE_HOST_MENU,            // displaying host property menu

    STATE_JOINING_GAME,         // waiting for host response
    STATE_PLAYING_GAME,         // playing the game

    STATE_SOUNDBIT_EDITOR       // editing soundbits
};

// Network messages involve sending this as a word, followed by message specific data
// all networking handling is done in UpdateNetwork

enum Message
{
    MSG_JOIN_GAME,
    MSG_JOIN_OK,
    MSG_ADD_PLAYER,
    MSG_REMOVE_PLAYER,
    MSG_UPDATE_PLAYER,
    MSG_UPDATE_ALLPLAYERS,
    MSG_VOICE,
};

//-----------------------------------------------------------------------------
// Name: class Marketplace
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class Marketplace : public CXBApplication
{
public:
    // initialization
    Marketplace();     
    virtual HRESULT Initialize();         
    
    // updates each frame 
    virtual HRESULT FrameMove();    // updates menus, spawns out the update  functions
    void UpdateMarketplace();       // called when in the game to update network, audio, and players
    void UpdateSoundbitEditor();    // called when editing soundbits
    void UpdateNetwork();           // handles networking messages, online task pumping
    void UpdateSounds();            // handles audio pumping

    // draws a frame
    virtual HRESULT Render();       // renders menu, spawns out drawmarketplace and drawsoundbiteditor functions
    void DrawMarketplace();         // draws the marketplace walls, trees, etc.
    void DrawSoundbitEditor();      // draws the soundbit editor
    
    // draws a sprite, a person, and text, all scaled to the marketplace (positions x,y are 0-100.0, scaled across the marketplace)
    void DrawScaledObject( D3DXVECTOR3 &vP, DWORD dwColor, int idx );
    void DrawPerson( D3DXVECTOR3 &vP, D3DXVECTOR3 &vF, DWORD dwColor );
    void DrawScaledGlyph( float fX, float fY, float fZ, float fScale, WCHAR *pString, DWORD dwColor );

    // creates the mockups for simulating multiple players
    void CreateBots( XNADDR *pHostXNADDR );

    // creates the trees and fountains in the marketplace
    void CreateObjects();

    // simple radius-based collision handler 
    void HandleCollisions( DisplayedObject *pMovingObject, D3DXVECTOR3 *vModifyPos ); // only modifies vmodifypos

    XBGAMEPAD *GetDefaultGamepad();

    // convert between xnaddrs and inaddrs, based on the current session id and key
    VOID GetInAddrFromXnAddr( IN_ADDR *ina, XNADDR *pxna );
    VOID GetXnAddrFromInAddr( XNADDR *pxna, IN_ADDR *ina );

    // send a network packet to a specific player or the host
    VOID SendToPlayer( Packet &p, WORD wLUID );
    VOID SendToHost( Packet &p );

    // accumulate stats, draw stats, update stats
    VOID StatsSendVoiceData( DWORD dwBytes );
    VOID StatsRecvVoiceData( DWORD dwBytes );
    VOID StatsReset();
    VOID StatsAdvanceFrame();
    VOID StatsDraw();

    FLOAT &Time();
    
    HostProperties &  GetHostProperties();
    BOOL              IsHost();
    BOOL              IsShowNames();

    // called by the network manager so we can set our player flags
    void SetLastLocalSpeechTime( float fTime );

private:
    XBUserList    m_UserAccountList;      // list of available accounts
    unsigned int  m_iChosenAccount;     // account chosen

    unsigned int  m_iUIRow; // row/col for soundbit editor
    unsigned int  m_iUICol;
    
    unsigned int  m_iCurrentUISelection;  // Current ui selection
    unsigned int  m_iCurrentUIDisplay;    // top of ui display
    
    XONLINETASK_HANDLE  m_hOnlineTask;    // Online task handle    

    AmbientSound *m_pCrowdAmbient;  // crowd ambient
    AmbientSound *m_pDoorbell;      // doorbell sound

    CXBFont     m_Font;             // Font object
    CXBFont     m_IconFont;         // Icon font object
    CXBHelp     m_Help;             // Help object
    BOOL        m_bDrawHelp;        // TRUE to draw help screen


    WCHAR       m_ErrorMsg [ERROR_MSG_LEN];     // Error message to be displayed in the error state
    State       m_State;                        // Current state we are in
    State       m_ErrorNextState;               // State to continue to once the user acknowledges the error
   
    CDraw2D          m_SpriteDraw;  // sprite drawing class

    CXBSocket        m_VDPSock;     // socket we send data on
    CQuickmatchQuery m_Query;       // matchmaking query

    XNKID m_SessionID;              // session id and key for Live!
    XNKEY m_KeyExchangeKey;    

    // Timers for network and voice updates

    float m_fLastNetworkUpdate;
    float m_fLastLocalSpeechTime;
    float m_fLastVoiceUpdate;

    // The host Xbox Network Address (XNADDR) and Internet Address (IN_ADDR)
    // are stored in HostXnAddr and HostInAddr respectively.
    
    XNADDR  m_HostXnAddr;
    IN_ADDR m_HostInAddr;

    // Session we are currently in
    CSession        m_Session;  

    // current server flags - bit set when the server properties have changed,
    // to notify the client it needs to read them
    DWORD           m_dwServerFlags;

    
    BOOL            m_bHost;        // am i the host?
    BOOL            m_bShowStats;   // am i showing the voice stat graphs?
    BOOL            m_bShowNames;   // am i showing player names?
    BOOL            m_bBots;        // are there bots in the game (if i'm host)?

    SOCKADDR_IN     m_SockAddr;     // sockaddr structures for determining ip addresses
    SOCKADDR_IN     m_RecvSockAddr;

    HostProperties m_HostProps;     // current host properties
   
    FLOAT   m_fTime;                // current game time

    // stats data buffers
    DWORD   m_dwRecvVoiceBytes [ STATS_WINDOW_SIZE ];
    DWORD   m_dwSendVoiceBytes [ STATS_WINDOW_SIZE ];
    DWORD   m_dwRecvNumPackets [ STATS_WINDOW_SIZE ];
    DWORD   m_dwSendNumPackets [ STATS_WINDOW_SIZE ];

    // stats totals
    DWORD   m_dwTotalRecvVoiceBytes;
    DWORD   m_dwTotalSendVoiceBytes;
    DWORD   m_dwTotalRecvNumPackets;
    DWORD   m_dwTotalSendNumPackets;
    
    // stat maxes for send and receive 
    FLOAT   m_fMaxRecvKbps;
    FLOAT   m_fMaxSendKbps;

    // current position in the rotating stat frame
    WORD    m_wCurStatPos;    

    // time we started collecting stats
    FLOAT   m_fStatsTime;
       
};

inline FLOAT &Marketplace::Time()
{
    return m_fTime;
}

extern Marketplace g_Marketplace;
