//-----------------------------------------------------------------------------
// File: VoiceCommunicator.h
//
// Desc: Class and strucuture definitions related to VoiceCommunicator support
//
// Hist: 1.17.02 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef VOICECOMMUNICATOR_H
#define VOICECOMMUNICATOR_H

#include <xtl.h>
#include <xvoice.h>
#include <xonline.h>

class CVoiceManager;

class CVoiceCommunicator
{
public:
    friend CVoiceManager;

    CVoiceCommunicator();
    ~CVoiceCommunicator();

    HRESULT Initialize( CVoiceManager* pManager );
    HRESULT Shutdown();

    HRESULT ResetMicrophone();
    HRESULT ResetHeadphone();
    HRESULT OnInsertion( DWORD dwSlot );
    HRESULT OnRemoval();

    HRESULT GetMicrophonePacket( XMEDIAPACKET* pPacket, DWORD dwIndex );
    HRESULT SubmitMicrophonePacket( XMEDIAPACKET* pPacket );

    HRESULT GetHeadphonePacket( XMEDIAPACKET* pPacket, DWORD dwIndex );
    HRESULT SubmitHeadphonePacket( XMEDIAPACKET* pPacket );

    friend VOID CALLBACK MicrophoneCallback( LPVOID, LPVOID, DWORD );
    friend VOID CALLBACK HeadphoneCallback( LPVOID, LPVOID, DWORD );

private:
    CVoiceManager*  m_pManager;
    LONG            m_lSlot;
    XMediaObject*   m_pMicrophoneXMO;
    XMediaObject*   m_pHeadphoneXMO;
    LPXVOICEENCODER m_pEncoderXMO;

    DWORD           m_dwSRCReadPosition;
    DWORD           m_dwRampTime;

    BYTE*           m_pbMicrophoneBuffer;
    BYTE*           m_pbHeadphoneBuffer;
};


#endif // VOICECOMMUNICATOR_H