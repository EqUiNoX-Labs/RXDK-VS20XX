//-----------------------------------------------------------------------------
// File: Audio.h
//
// Desc: Handles the classes responsible directly for the output of sound 
//       in marketplace.  Also wraps the Xbox High-level Voice (XHV) objects
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

// define used when we aren't one of the allocated buffers 
// this should only be used by the AudioMgr class
#define IDX_NO_BUFFER   ( MAX_SIMULTANEOUS_REMOTE_TALKERS + 1 )

enum SoundType
{
    SOUNDTYPE_AMBIENT,  // no 3d processing .wav
    SOUNDTYPE_3DWAVE,   // positioned .wav
    SOUNDTYPE_3DVOICE,  // positioned voice
    SOUNDTYPE_ERROR
};

enum VoiceOutputMethod
{
    VOM_NONE,        
    VOM_COMMUNICATOR,   // through the headset
    VOM_NORMAL,         // normal
    VOM_PODIUM,         // from the podium
};

//-----------------------------------------------------------------------------
// Name: class SingleSound
// Desc: This is the base class for each sound type below
//-----------------------------------------------------------------------------
class SingleSound
{
public:
    SingleSound() { m_bAutoEnable = true; }    
    virtual ~SingleSound() {};

    // Auto enable means the sound is played on startup
    // this is set to false for things like the door chime (when players join)
    // but true for ambient noise like the crowd

    VOID SetAutoEnable( BOOL b ) { m_bAutoEnable = b; };        
    BOOL AutoEnable()            { return m_bAutoEnable; };     

    virtual VOID Move( const D3DXVECTOR3 & vNewPos ) {}          

    virtual VOID Disable() = 0;
    virtual VOID Enable() = 0;
  
    SoundType   m_Type;
    BOOL        m_bEnabled;
    BOOL        m_bAutoEnable;
};




//-----------------------------------------------------------------------------
// Name: class AmbientSound
// Desc: A sound with no position like the crowd or doorbell
//-----------------------------------------------------------------------------
class AmbientSound : public SingleSound
{
public:
    AmbientSound();
    ~AmbientSound();

    VOID    Initialize( char *pszSoundName );
       
    // we can set the volume of ambient sounds for things
    // like crowd noise level.  This is an attentuation, so
    // 0 is loud, negative numbers are more quiet

    VOID    SetVolume( float fVol );

    virtual VOID    Disable();
    virtual VOID    Enable();    
private:
    DWORD                m_dwIndex;  // index in sound bank
};




//-----------------------------------------------------------------------------
// Name: class PositionedWaveBankSound
// Desc: A sound with a 3d position that plays a .wav
//-----------------------------------------------------------------------------
class PositionedWaveBankSound : public SingleSound
{
public:
    PositionedWaveBankSound();
    ~PositionedWaveBankSound();

    VOID    Initialize( char *pszSoundName, const D3DXVECTOR3 &vPos );
    virtual VOID    Move( const D3DXVECTOR3 & vNewPos );   
   
    virtual VOID    Disable();
    virtual VOID    Enable();    
private:
    IXACTSoundSource     *m_p3DSoundSource;
    DWORD                m_dwIndex;  // index in sound bank
};




//-----------------------------------------------------------------------------
// Name: class PositionedVoiceSounds
// Desc: A sound with a 3d position that plays voice through XHV
//-----------------------------------------------------------------------------
class PositionedVoiceSound : public SingleSound
{
public:
    PositionedVoiceSound();
    ~PositionedVoiceSound();

    VOID    Initialize( XUID xuid );
   
    virtual VOID    Move( const D3DXVECTOR3 &vNewPos );   

    // SubmitVoicePacket grabs one of the buffers, locates it to this sound, then plays the voice data
    VOID    SubmitVoicePacket( BYTE *pData, DWORD dwSize, FLOAT fTime, BYTE byMethod = VOM_NORMAL );     
    
    virtual VOID    Disable();
    virtual VOID    Enable();
    
private:
    WORD                 m_wPriorMethod;
    D3DXVECTOR3          m_vSavedPos;           // saved position
    WORD                 m_wVoiceBufferIdx;     // Index of which buffer I'm using.. IDX_NO_BUFFER means I don't have one allocated
    XUID                 m_xuid;                // xuid associated with me
    FLOAT                m_fTimeLastUsed;       // time stamp of most recent use
};




//-----------------------------------------------------------------------------
// Name: class AudioMgr
// Desc: AudioMgr manages all of the output of audio for this sample
//-----------------------------------------------------------------------------
class AudioMgr
{
public:
    AudioMgr();    
    
    // Initialize
    VOID Initialize( char *pszWavebank, char *pszSoundbank, char *pszDSPImage );
        
    // create the various types of sound
    AmbientSound *              CreateAmbientSound();
    PositionedWaveBankSound *   CreatePositionedWaveBankSound();
    PositionedVoiceSound *      CreatePositionedVoiceSound();
    
    // free an allocated sound
    VOID FreeSound( SingleSound *pSound );

    // simulation mode is where we fake being in a session 
    // for recording the bot voice bits
    VOID SetSimulationMode( BOOL bMode );                // set up m_fakeXUID routing to headphones
    XUID &SimulationXUID();                              // returns the simulation xuid
    
    // SubmitVoicePacket is only called by the bot voice database when in edit mode.
    // Voice in the game itself is done through the PositionedVoiceSound class
    VOID SubmitVoicePacket( XUID xuid, BYTE *packet ); 

    // stops and starts the XHV engine
    VOID StopXHV();
    VOID StartXHV();

    // enables all of the sounds once we start the game
    void EnableAll();
    
    // stops all sounds
    void DisableAll();

    // when we have a block of settings in D3D, we set them all with D3DS_DEFERRED
    // CommitSettings applies them all at once when we are done to minimize overhead
    void CommitSettings();  
    
    // need an update every frame with the current player position
    void Update( float fDt, const D3DXVECTOR3 &vPos, const D3DXVECTOR3 &vFace );  

    // only update audio engines, not facing; this is used by the bot voice editing (SoundBitDb) class
    void Update( float fDt );
  
    // only classes above should reference these
public:
    IXACTEngine*            m_pXACT;                // XACT Engine instance
    BYTE*                   m_pbWaveBank;           // Wave Bank data   
    PXACTWAVEBANK           m_pWaveBank;            // XACT Wave Bank
    BYTE*                   m_pbSoundBank;          // Sound Bank data
    IXACTSoundBank*         m_pSoundBank;           // XACT Sound Bank

    XUID                    m_fakeXUID;             // fake xuid for non-game calls to xhv
   
    std::vector<SingleSound *>   m_pSoundList;      // list of all sounds currently being played
    
    LPDSEFFECTIMAGEDESC     m_pdspDesc;             // direct sound effect image pointer

    // This is our cache of sound buffers based on who is currently talking.
    // The m_pVoiceBuffers are allocated when someone talks on a LRU basis

    PositionedVoiceSound *  m_pCurrentBufferOwner[ MAX_SIMULTANEOUS_REMOTE_TALKERS ]; 
    LPDIRECTSOUNDBUFFER     m_pVoiceBuffer[ MAX_SIMULTANEOUS_REMOTE_TALKERS ];

    PXHVENGINE              m_pXHVEngine;           // Voice engine 
    LPDIRECTSOUND8          m_pDirectSound;         // pointer to the directsound object
};

// extern the global audiomgr object
extern AudioMgr g_AudioMgr;