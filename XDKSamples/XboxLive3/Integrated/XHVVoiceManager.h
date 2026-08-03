//-------------------------------------------------------------------------------------
// File: XHVVoiceManager.h
//
// Desc: Wraps the XHV voice engine and provides a simple interface to the
//          game
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------
#ifndef _XHVVOICEMANAGER_H_
#define _XHVVOICEMANAGER_H_

#include <xtl.h>
#include <xonline.h>
#include <xhv.h>


class CXHVVoiceManager
{
public:
    CXHVVoiceManager();
    ~CXHVVoiceManager();

    static const DWORD MIN_VOICEMAIL_DURATION_MS = 1000;
    static const DWORD MAX_VOICEMAIL_DURATION_MS = 5000;

    HRESULT Initialize( LPDIRECTSOUND8 pDSound,
                        XHV_RUNTIME_PARAMS* pXHVParams,
                        ITitleXHV* pTitleXHV );
    HRESULT Shutdown();

    // These functions are simple passthrough functions to m_pXHVEngine:
    HRESULT DoWork()
        { return m_pXHVEngine->DoWork(); }
    HRESULT SetCallbackInterface( ITitleXHV* pTitleXHV )
        { return m_pXHVEngine->SetCallbackInterface( pTitleXHV ); }
    HRESULT GetLocalTalkerStatus( DWORD dwPort, 
                                  XHV_LOCAL_TALKER_STATUS* pLocalTalkerStatus )
        { return m_pXHVEngine->GetLocalTalkerStatus( dwPort, pLocalTalkerStatus ); }
    HRESULT SetMaxPlaybackStreamsCount( DWORD dwStreamsCount )
        { return m_pXHVEngine->SetMaxPlaybackStreamsCount( dwStreamsCount ); }
    HRESULT SetProcessingMode( DWORD dwPort, XHV_PROCESSING_MODE processingMode )
        { return m_pXHVEngine->SetProcessingMode( dwPort, processingMode ); }

    // Records voice mail, given port, max time, and recording buffer
    HRESULT StartRecordVoiceMail( DWORD dwPort , DWORD dwMaxTimeMs ,
                                  BYTE* pRecordBuffer , DWORD dwBufferSize );
    HRESULT PrepareVoiceMailPlay( DWORD dwPort, DWORD dwMaxTimeMs ,
                                  BYTE* pRecordBuffer , DWORD dwBufferSize );
    // Plays back voice mail
    HRESULT PlayRecordedVoiceMail( BOOL bOutputToSpeakers );
    HRESULT StopVoiceMail();
    HRESULT ClearVoiceMail();
    VOID    AlertOfVoiceMailStopped();
    VOID    AlertOfVoiceMailDataReady();
    BOOL    RecordingNow() const { return m_bRecordingNow; }
    BOOL    PlayingNow() const { return m_bPlayingNow; }

    // These functions wrap handling local talkers
    HRESULT RegisterLocalTalker( DWORD dwPort );
    HRESULT UnregisterLocalTalker( DWORD dwPort );

protected:

    // basic sound components needed to allow voice mail functionality
    LPDIRECTSOUND8      m_pDSound;
    PXHVENGINE          m_pXHVEngine;
    DWORD               m_dwMaxRemoteTalkers;
    DWORD               m_dwNumRemoteTalkers;

    // keeps tabs on ability for user to be voice mail enabled
    BOOL                m_bEnabledLocalTalkers[ XGetPortCount() ];

    BOOL                m_bRecordingNow;            // TRUE if recording now
    BOOL                m_bPlayingNow;              // TRYE if playing now
    DWORD               m_dwVoiceMailPort;          // port being used to record
    DWORD               m_dwVoiceMailDuration;      // voice mail duration in ms
    DWORD               m_dwVoiceMailBufferSize;    // size of record buffer
    BYTE*               m_pVoiceMailBuffer;         // pointer to record buffer
    BOOL                m_bOutputToSpeakers;        // TRUE if outputing to speakers
};

#endif // _XHVVOICEMANAGER_H_