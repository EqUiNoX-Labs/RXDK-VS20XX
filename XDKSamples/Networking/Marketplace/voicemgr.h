//-----------------------------------------------------------------------------
// File: VoiceMgr.h
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

#pragma once

// Defines for size of VoicePacketWrapper below

#define COMPRESSED_VOICE_SIZE           10     // size in bytes of each packet of voice
#define MAX_BUNDLED_PACKETS             20     // max # of packets sent in a single network packet
#define VOICE_DATA_SIZE                 ( COMPRESSED_VOICE_SIZE * MAX_BUNDLED_PACKETS )

// These defines define the priorities used to figure out which people we are going to send or receive from

#define PRIORITY_DISTANCE_SCALE         5.0f   // falloff will be about -1.0f per MIN_DIST below to a min of 0
#define PRIORITY_DISTANCE_NORMAL_MIN    5.0f   // everything within 5 meters is worth DISTANCE_SCALE priority points
#define PRIORITY_FACING_SCALE           0.5f   // directly in front = 0.5, reduced to 0 to the sides
#define PRIORITY_STATIONARY             1.0f   // stationary bonus

// Voice modes

#define VMM_NORMAL   0  // voice is routed peer-to-peer to a 3d voice
#define VMM_PODIUM   1  // voice is routed through the host to a 3d voice with no falloff
#define VMM_PRIVATE  2  // voice is routed through the host to the headphones

// This wraps a single 'chunk' of voice data

struct VoicePacketWrapper
{    
    BYTE nPackets;                      // number of packets   
    BYTE byVoiceMode;                   // one of the VMM defines above 
    WORD wVoiceFromID;                  // local user id voice is coming from
    WORD wVoiceToID;                    // local user id voice is sending to
    BYTE bVoiceData[ VOICE_DATA_SIZE ];            
};

// Global voice manager class.  This class inheirits from ITitleXHV so it can be
//   registered as a callback and collect data packets.

class VoiceMgr : public ITitleXHV
{
public:    
    VoiceMgr();

    // functions defined through the ITitleXHV interface
    // because this is just a sample, we assume there is always a headset available in controller 1
    // and we only need to define LocalChatDataReady
    STDMETHODIMP CommunicatorStatusUpdate( DWORD dwPort, XHV_VOICE_COMMUNICATOR_STATUS status ) { return S_OK; }
    STDMETHODIMP LocalChatDataReady( DWORD dwPort, DWORD dwSize, PVOID pData );
    STDMETHODIMP SpeechRecognized( DWORD, PXHV_SR_ITEM, DWORD ) { return E_NOTIMPL; }

    // get the local voice data- used by the soundbit editor
    VoicePacketWrapper &GetLocalVoiceData();

    // Update should be called once a frame 
    VOID Update();

    // SendNetworkVoice() should be called every 1/10th of a second or so- it will send all voice
    // queued up that hasn't been appended to a packet with the AppendVoiceForPlayer function
    VOID SendNetworkVoice();

    // read voice data from a network packet - returns # of voice bytes read
    WORD ReadVoiceData( Packet &p );

    // append voice data to a network packet- can be used when sending packets to players
    // to avoid packet overhead.  If this function isn't called and we have voice data for
    // a specific player, it will get sent during the SendNetworkVoice function above
    // returns # of voice bytes appended.  Because of the encryption in VDP, this must
    // be the last thing added to a packet
    WORD AppendVoiceDataForPlayer( Packet &p, WORD wPlayerLUID );
    WORD AppendVoiceDataForHost( Packet &p );

    // set the current local voice mode 
    VOID SetCurrentLocalVoiceMode( BYTE bVoiceFlags );          

    // we don't want to rebuild the routing info every frame, so we allow 
    // it to be rebuilt as we need it; i.e. talking player moves, or new players started or left
    VOID MarkForRebuild();              

    // query whether the local player would send to the given player
    BOOL IsPlayerInLocalListenSet( WORD wRemotePlayerLUID );       

    // returns current incoming voice for me- call repeatedly until it returns 'false'
    BOOL GetVoiceForPlayer( WORD wPlayerLUID, VoicePacketWrapper &w );
    
    // handle a packet- most things should not call this directly
    VOID ProcessVoiceData( VoicePacketWrapper &vpw );

protected:
    // called by Update() if we need to recalculate priorities and routing grid
    // This is the 'meat' function of this
    VOID RebuildRoutes();             

    // helper functions to do the right thing with incoming voice    
    VOID AddVoiceToQueue( std::list<VoicePacketWrapper> &list, VoicePacketWrapper &vpw );

    // write a VoicePacketWrapper directly to a packet
    VOID WriteVoiceData( Packet &p, VoicePacketWrapper &vpw );

    // List of incoming and outgoing voice packets - for the host these may
    // include packets for other targets than the host ( i.e. host-forwarding,
    // podium, and private message channel )
    std::list<VoicePacketWrapper> m_InVoicePackets;   
    std::list<VoicePacketWrapper> m_OutVoicePackets;   

    // the interrupt stores voice data here ( LocalChatDataReady above )
    VoicePacketWrapper  m_LocalVoice;

    // boolean to say whether we need to call RebuildRoutes on our update
    BYTE            m_bNeedToRebuildRoutes;

    // Data for the local player
    WORD            m_wLocalSendToPlayerIDs[ MAX_CHANNELS ];
    WORD            m_byNumLocalSendToPlayerIDs;

    // Grid for storing relative priorities
    FLOAT           m_fPriorities[ MAX_PLAYERS ][ MAX_PLAYERS ]; 
    
    // This is the list of players each other player should send to
    WORD            m_wSendToPlayerIDs[ MAX_PLAYERS ][ MAX_CHANNELS ];
    BYTE            m_byNumSendToPlayerIDs[ MAX_PLAYERS ];
    
    // This is the list of players each player should expect to receive from
    WORD            m_wRecvFromPlayerIDs[ MAX_PLAYERS ][ MAX_CHANNELS ];
    BYTE            m_byNumRecvFromPlayerIDs[ MAX_PLAYERS ];  
};

extern VoiceMgr g_VoiceMgr;
