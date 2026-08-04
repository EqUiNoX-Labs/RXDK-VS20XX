//-----------------------------------------------------------------------------
// File: VoiceManager.h
//
// Desc: Class and strucuture definitions related to VoiceCommunicator support
//
// Hist: 04.29.02 - New for June XDK
//       08.20.02 - Overhauled for October XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef VOICEMANAGER_H
#define VOICEMANAGER_H

#include <xtl.h>
#include <xvoice.h>
#include <xonline.h>
#include "VoiceCommunicator.h"
#include <vector>


// Enumeration for communicator-related events
enum VOICE_COMMUNICATOR_EVENT
{
    VOICE_COMMUNICATOR_INSERTED,
    VOICE_COMMUNICATOR_REMOVED,
};

// Callback function signature for communicator-related events
typedef VOID (*PFNCOMMUNICATORCALLBACK)( DWORD dwPort, VOICE_COMMUNICATOR_EVENT event, VOID* pContext );

// Callback function signature for voice data 
typedef VOID (*PFNVOICEDATACALLBACK)( DWORD dwPort, DWORD dwSize, VOID* pvData, VOID* pContext );

// With everything running at DPC, we only need 2 packets to ping-pong
static const DWORD NUM_PACKETS = 2;

//-----------------------------------------------------------------------------
// Name: struct VOICE_MANAGER_CONFIG
// Desc: Configuration structure for the voice manager
//-----------------------------------------------------------------------------
struct VOICE_MANAGER_CONFIG
{
    DWORD   dwVoicePacketTime;      // Packet time, in ms
    DWORD   dwMaxRemotePlayers;     // Maximum # of remote players
    DWORD   dwMaxStoredPackets;     // Maximum # of stored encoded packets

    LPDIRECTSOUND8      pDSound;                // DirectSound object
    LPDSEFFECTIMAGEDESC pEffectImageDesc;       // DSP Effect Image Desc
    DWORD               dwFirstSRCEffectIndex;  // Effect index of first SRC effect in DSP

    // Will need callbacks for notifying of certain events
    VOID*                   pCallbackContext;
    PFNCOMMUNICATORCALLBACK pfnCommunicatorCallback;
    PFNVOICEDATACALLBACK    pfnVoiceDataCallback;
};

// Typedefs and class/struct declaration
typedef std::vector<XUID> MuteList;
struct REMOTE_CHATTER;

// Font for drawing debug info - only needed for sample
class CXBFont;

// RXDK: forward-declare the DPC callbacks at namespace scope. The class below
// only declares them as friends, which does not make the names visible to the
// call sites (MSVC's friend name injection allowed it; standard C++ does not).
VOID CALLBACK StreamCallback( LPVOID, LPVOID, DWORD );
VOID CALLBACK MicrophoneCallback( LPVOID, LPVOID, DWORD );
VOID CALLBACK HeadphoneCallback( LPVOID, LPVOID, DWORD );



//-----------------------------------------------------------------------------
// Name: class CVoiceManager
// Desc: Workhorse class for processing voice.  Keeps track of communicators,
//          coordinates encoding/decoding/mixing/muting, etc. 
//-----------------------------------------------------------------------------
class CVoiceManager
{
public:
    CVoiceManager();
    ~CVoiceManager();

    // Methods for controlling overall voice state
    HRESULT Initialize( VOICE_MANAGER_CONFIG* pConfig );
    HRESULT Shutdown();
    VOID    EnterChatSession();
    VOID    LeaveChatSession();
    BOOL    IsInChatSession() { return m_bIsInChatSession; }

    // Common methods
    HRESULT ReceivePacket( XUID xuidFromPlayer, VOID* pvData, INT nSize );
    HRESULT ProcessVoice();

    // Methods for managing the chatter list, including muting
    HRESULT AddChatter( XUID xuidPlayer );
    HRESULT RemoveChatter( XUID xuidPlayer );
    HRESULT ResetChatter( XUID xuidPlayer );
    HRESULT MutePlayer( XUID xuidPlayer, DWORD dwControllerPort );
    HRESULT UnMutePlayer( XUID xuidPlayer, DWORD dwControllerPort );
    BOOL    IsPlayerMuted( XUID xuidPlayer, DWORD dwControllerPort );
    HRESULT RemoteMutePlayer( XUID xuidPlayer, DWORD dwControllerPort );
    HRESULT UnRemoteMutePlayer( XUID xuidPlayer, DWORD dwControllerPort );
    BOOL    IsPlayerRemoteMuted( XUID xuidPlayer, DWORD dwControllerPort );

    // Methods for getting information about voice state
    BOOL    IsCommunicatorInserted( DWORD dwControllerPort ) { return m_dwConnectedCommunicators & ( 1 << dwControllerPort ); }
    BOOL    IsPlayerTalking( XUID xuidPlayer );

    // Methods for controlling communicator operation
    HRESULT EnableCommunicator( DWORD dwControllerPort, BOOL bEnabled );
    HRESULT SetVoiceMask( DWORD dwControllerPort, XVOICE_MASK mask );
    HRESULT SetLoopback( DWORD dwControllerPort, BOOL bLoopback );
    HRESULT SetVoiceThroughSpeakers( BOOL bEnabled );

#if _DEBUG
    // Debug output will throw timing off, so we define a separate
    // mechanism for tracking interesting events
    VOID    VoiceLog( WCHAR* format, ... );
    VOID    RenderDebugInfo( CXBFont* pFont );

    const static DWORD s_dwLogEntrySize = 256;
    const static DWORD s_dwNumLogEntries = 5;

    WCHAR   m_strDebugLog[s_dwNumLogEntries][256];
    DWORD   m_dwCurrentLogEntry;
    DWORD   m_dwTotalLogEntries;
#else
    VOID    VoiceLog( WCHAR* format, ... ) {}
    VOID    RenderDebugInfo( CXBFont* pFont ) {}
#endif // _DEBUG8

protected:
    // Helper methods, primarily for use by CVoiceCommunicator class
    friend CVoiceCommunicator;
    HRESULT GetSRCInfo( DWORD dwControllerPort, DWORD* pdwBufferSize, VOID** ppvBufferData, DWORD* pdwWritePosition );
    HRESULT SetSRCGain( DWORD dwControllerPort, FLOAT fGain );
    DWORD   GetNumPackets() { return NUM_PACKETS; }
    DWORD   GetPacketSize() { return m_dwPacketSize; }
    const WAVEFORMATEX* GetWaveFormat() { return &m_wfx; }

    // DPC callback functions
    friend VOID CALLBACK StreamCallback( LPVOID, LPVOID, DWORD );
    friend VOID CALLBACK MicrophoneCallback( LPVOID, LPVOID, DWORD );
    friend VOID CALLBACK HeadphoneCallback( LPVOID, LPVOID, DWORD );

    // VoiceManager implementations of DPC callbacks
    HRESULT StreamPacketCallback( REMOTE_CHATTER* pChatter, LPVOID pPacketContext, DWORD dwStatus );
    HRESULT MicrophonePacketCallback( DWORD dwPort, LPVOID pPacketContext, DWORD dwStatus );
    HRESULT HeadphonePacketCallback( DWORD dwPort, LPVOID pPacketContext, DWORD dwStatus );

    // Internal-only functions for dealing with communicators
    HRESULT OnCompletedPacket( DWORD dwControllerPort, VOID* pvData, DWORD dwSize );
    HRESULT OnCommunicatorInserted( DWORD dwControllerPort );
    HRESULT OnCommunicatorRemoved( DWORD dwControllerPort );
    HRESULT CheckDeviceChanges();

    // Internal-only functions for dealing with chatters
    HRESULT InitChatter( REMOTE_CHATTER*            pChatter, 
                         XVOICE_QUEUE_XMO_CONFIG*   pVoiceQueueCfg,
                         DSSTREAMDESC*              pdssd );
    DWORD   ChatterIndexFromXUID( XUID xuidPlayer);
    HRESULT RecalculateMixBins( REMOTE_CHATTER* pChatter );

    // Helper functions for filling out XMEDIAPACKETs
    HRESULT GetDriftCompensationPacket( XMEDIAPACKET* pPacket, REMOTE_CHATTER* pChatter );
    HRESULT GetTemporaryPacket( XMEDIAPACKET* pPacket );
    HRESULT GetStreamPacket( XMEDIAPACKET* pPacket, REMOTE_CHATTER* pChatter, DWORD dwIndex );
    HRESULT SubmitStreamPacket( DWORD dwIndex, REMOTE_CHATTER* pChatter );
    
    // Copy of configuration struct passed in to Initialize()
    VOICE_MANAGER_CONFIG        m_cfg;

    // Handy data to keep cached
    WAVEFORMATEX                m_wfx;
    DWORD                       m_dwPacketSize;
    DWORD                       m_dwBufferSize;
    DWORD                       m_dwCompressedSize;
    DWORD                       m_adwHeadphoneSends[ XGetPortCount() ];

    // Chatter list
    REMOTE_CHATTER*             m_pChatters;
    BOOL                        m_bIsInChatSession;
    REMOTE_CHATTER*             m_pLoopbackChatters;

    // Communicator info
    DWORD                       m_dwConnectedCommunicators;
    DWORD                       m_dwMicrophoneState;
    DWORD                       m_dwHeadphoneState;
    DWORD                       m_dwLoopback;
    DWORD                       m_dwEnabled;
    CVoiceCommunicator          m_aVoiceCommunicators[XGetPortCount()];
    BOOL                        m_bVoiceThroughSpeakers;

    // Temporary buffer to hold a compressed packet
    BYTE*                       m_pbTempEncodedPacket;

    // Buffer to hold completed, encoded packets
    DWORD                       m_dwCompletedPacketSize;
    DWORD                       m_dwNumCompletedPackets;
    BYTE*                       m_pbCompletedPackets;

    // Mute lists
    MuteList    m_MuteList[XGetPortCount()];
    MuteList    m_RemoteMuteList[XGetPortCount()];

#if _DEBUG
    HRESULT ValidateStateDbg();
#endif // _DEBUG
};


// Single global instance of class is in cpp file
extern CVoiceManager g_VoiceManager;

#endif // VOICEMANAGER_H