//-------------------------------------------------------------------------------------
// File: Match.h
//
// Desc: Definitions for match-making and network
//       messaging objects used by the Elo demo.
//       Modified from the Match-Making sample.
//
// Hist: 09.10.04 - New for Sept release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef MATCH_H
#define MATCH_H

#include <xtl.h>
#include <xonline.h>
#include <assert.h>

//-------------------------------------------------------------------------------------
// Constants
//-------------------------------------------------------------------------------------

//
// Attribute IDs                            ID num   Data Type
//                                          ------   ----------------------------
const DWORD XATTRIB_GAME_TYPE              = 0x0001 | X_ATTRIBUTE_DATATYPE_INTEGER;
const DWORD XATTRIB_PLAYER_LEVEL           = 0x0002 | X_ATTRIBUTE_DATATYPE_INTEGER;
const DWORD XATTRIB_SESSION_NAME           = 0x0003 | X_ATTRIBUTE_DATATYPE_STRING;
const DWORD XATTRIB_GAME_STYLE             = 0x0004 | X_ATTRIBUTE_DATATYPE_INTEGER;
const DWORD XATTRIB_OWNER_NAME             = 0x0005 | X_ATTRIBUTE_DATATYPE_STRING;
const DWORD XATTRIB_CONFIG_INFO            = 0x0006 | X_ATTRIBUTE_DATATYPE_BLOB;

//
// Attribute Maximum Lengths
// (for strings, this doesn't include the terminating NULL)
//
const DWORD XATTRIB_SESSION_NAME_MAX_LEN   = 11; // SessionName attribute
const DWORD XATTRIB_OWNER_NAME_MAX_LEN     = 32; // OwnerName attribute
const DWORD XATTRIB_CONFIG_INFO_MAX_LEN    = 32; // ConfigInfo attribute

// Specify X_MATCH_NULL_INTEGER for optional integer query arguments
const ULONGLONG X_MATCH_NULL_INTEGER       = 0x7FFFFFFFFFFFFFFFui64;

// Maximum number of sessions returned by OptiMatch query
const DWORD MAX_OPTI_MATCH_RESULTS         = 25;

// Maximum number of sessions returned by FindSessionByID query
const DWORD MAX_FIND_SESSION_BY_ID_RESULTS = 1;

// Number of QoS probes
const DWORD NUM_QOS_PROBES                 = 8;

// Maximum bandwidth to consume for QoS probes
const DWORD QOS_BITS_PER_SEC               = 64000;

const WORD TYPE_ANY                        = 0;
const WORD STYLE_ANY                       = 0;
const WORD LEVEL_ANY                       = 0;

//-------------------------------------------------------------------------------------
// Types
//-------------------------------------------------------------------------------------

// Helper class for getting/setting blob attributes
class CBlob
{
public:
    CBlob() : m_wLength( 0 ), m_pvData( NULL ) {}
    CBlob( WORD wLength, const PVOID pvData ) : 
           m_wLength( wLength ) , m_pvData( pvData ) {}
    CBlob( const CBlob & b ) { *this = b; }

    CBlob&      operator=( const CBlob & b ) 
    { m_wLength = b.Length; m_pvData = b.Data; return *this; }
    BOOL        operator==( const CBlob & b ) const  
    { return m_wLength == b.Length && memcmp( m_pvData, b.Data, b.Length ) == 0; }

    BOOL        IsNull() { return m_pvData == NULL; }

    __declspec( property( put = SetData, get=GetData ) ) const PVOID Data;
    const PVOID GetData() const { return m_pvData; }
    VOID        SetData( const PVOID pvData ) { m_pvData = pvData; }

    __declspec( property( put = SetLength, get=GetLength ) ) WORD Length;
    WORD        GetLength() const { return m_wLength; }
    VOID        SetLength( WORD wLength ) { m_wLength = wLength; }

private:
    WORD        m_wLength;
    PVOID       m_pvData;
};

// A null blob is defined as having a NULL pointer
#define NULL_BLOB CBlob( 0, NULL )

// Macro for declaring "blob literals" (much like the _T() macro does for strings)
#define B( length, ptr ) CBlob( length, (const PVOID) ptr )

class CSession
{
public:
    CSession();
    ~CSession();

    HRESULT     Create();
    HRESULT     Update();
    HRESULT     Delete();
    HRESULT     Process();

    VOID        Reset();

    DWORD       m_dwPublicFilled;
    DWORD       m_dwPublicOpen;
    DWORD       m_dwPrivateFilled;
    DWORD       m_dwPrivateOpen;
    XNKEY       m_KeyExchangeKey;
    XNKID       m_SessionID;

    BOOL        IsUpdating() const { return m_State == STATE_UPDATING; }
    BOOL        IsDeleting() const { return m_State == STATE_DELETING; }
    BOOL        IsCreating() const { return m_State == STATE_CREATING; }
    BOOL        Exists()     const { return m_State == STATE_ACTIVE || IsUpdating(); }

    // Attribute Accessors

    BOOL        IsListening() const { return m_bListening; }
    __declspec( property( put = SetQosResponse, get = GetQosResponse ) ) CBlob QosResponse;
    CBlob       GetQosResponse();
    VOID        SetQosResponse( CBlob Value );
    VOID        Listen( BOOL bEnable = TRUE, DWORD dwBitsPerSec = 0 );
    static const DWORD NO_WAIT = 1;

    __declspec( property( put = SetGameType, get=GetGameType ) ) ULONGLONG GameType;
    ULONGLONG   GetGameType();
    VOID        SetGameType( ULONGLONG Value );

    __declspec( property( put = SetPlayerLevel, get=GetPlayerLevel ) ) ULONGLONG PlayerLevel;
    ULONGLONG   GetPlayerLevel();
    VOID        SetPlayerLevel( ULONGLONG Value );

    __declspec( property( put = SetSessionName, get=GetSessionName ) ) const WCHAR * SessionName;
    const WCHAR* GetSessionName();
    VOID         SetSessionName( const WCHAR * Value );

    __declspec( property( put = SetGameStyle, get=GetGameStyle ) ) ULONGLONG GameStyle;
    ULONGLONG   GetGameStyle();
    VOID        SetGameStyle( ULONGLONG Value );

    __declspec( property( put = SetOwnerName, get=GetOwnerName ) ) const WCHAR * OwnerName;
    const WCHAR* GetOwnerName();
    VOID         SetOwnerName( const WCHAR * Value );

    __declspec( property( put = SetConfigInfo, get=GetConfigInfo ) ) CBlob ConfigInfo;
    CBlob       GetConfigInfo();
    VOID        SetConfigInfo( CBlob Value );

private:

    // The m_Attributes array is accessed using predefined constants:
    enum
    {
        GAME_TYPE_INDEX,
        PLAYER_LEVEL_INDEX,
        SESSION_NAME_INDEX,
        GAME_STYLE_INDEX,
        OWNER_NAME_INDEX,
        CONFIG_INFO_INDEX,
        NUM_ATTRIBUTES
    };

    XONLINE_ATTRIBUTE   m_Attributes[ NUM_ATTRIBUTES ];

    // Storage for the SessionName string attribute
    WCHAR               m_strSessionName[ XATTRIB_SESSION_NAME_MAX_LEN + 1 ];

    // Storage for the OwnerName string attribute
    WCHAR               m_strOwnerName[ XATTRIB_OWNER_NAME_MAX_LEN + 1 ];

    // Storage for the ConfigInfo blob attribute
    BYTE                m_rbBlobConfigInfo[ XATTRIB_CONFIG_INFO_MAX_LEN ];


    // Qos listening
    struct QosQEntry
    {
        XNKID  SessionID;            // Session to unregister
        DWORD  dwStartTick;          // Time entry was added
        struct QosQEntry *pNext;     // Next item in the queue
    };
    class CSessionQosQ
    {
    public:
        CSessionQosQ();
        VOID            Add( XNKID& SessionID, DWORD dwStartTick );
        VOID            Remove( XNKID& SessionID );
        VOID            Dequeue();
        const           QosQEntry *Head() const { return m_pHead; }
        const           QosQEntry *Tail() const { return m_pTail; }
    private:

        QosQEntry*      m_pHead;
        QosQEntry*      m_pTail;
    };

    CSessionQosQ        m_SessionQosQ;
    BOOL                m_bListening;              // Listening for Qos probes
    CBlob               m_QosResponse;
    VOID                PurgeSessionQ( BOOL fRemoveAll = FALSE );
    VOID                PurgeSessionQHead();

    HRESULT             ProcessStateCreateSession();
    HRESULT             ProcessStateUpdateSession();
    HRESULT             ProcessStateDeleteSession();
    HRESULT             ProcessStateActiveSession();

    VOID                Close();
    VOID                SetupAttributes();

    XONLINETASK_HANDLE  m_hSessionTask;

    enum STATE
    {
        STATE_IDLE,
        STATE_CREATING,
        STATE_UPDATING,
        STATE_DELETING,
        STATE_ACTIVE
    };

    STATE               m_State;
    BOOL                m_bKeyRegistered;
    BOOL                m_bUpdate;

};

//
// COptiMatchResult represents a single return result from the
// OptiMatch query
//
#pragma pack(push, 1)
class COptiMatchResult
{
public:
    // The query return attributes must come first, and in the order
    // returned by the query
    ULONGLONG           GameType;
    ULONGLONG           GameStyle;
    ULONGLONG           PlayerLevel;
    WCHAR               SessionName[ XATTRIB_SESSION_NAME_MAX_LEN + 1 ];
    WCHAR               OwnerName[ XATTRIB_OWNER_NAME_MAX_LEN + 1 ];
    __declspec( property( get=GetConfigInfo ) ) CBlob ConfigInfo;
    CBlob               GetConfigInfo() 
        { return CBlob( m_wBlobConfigInfoLength, m_rbBlobConfigInfo ); }

private:
    WORD                m_wBlobConfigInfoLength;
    BYTE                m_rbBlobConfigInfo[ XATTRIB_CONFIG_INFO_MAX_LEN ];

public:


    XNKID               m_SessionID;
    XNKEY               m_KeyExchangeKey;
    XNADDR              m_HostAddress;
    DWORD               m_dwPublicOpen;
    DWORD               m_dwPrivateOpen;
    DWORD               m_dwPublicFilled;
    DWORD               m_dwPrivateFilled;
    XNQOSINFO*          m_pQosInfo;

};
#pragma pack(pop)



//  Collection of results for the OptiMatch query
class COptiMatchQueryResults
{
public:
    COptiMatchQueryResults() { m_dwSize = 0; }
    DWORD               Size() { return m_dwSize; }
    COptiMatchResult&   operator[]( DWORD i ) { return m_v[i]; }
private:
    void                Clear() { m_dwSize = 0; }
    void                Remove( DWORD i )
    {
        assert( i < m_dwSize );
        if( i < m_dwSize )
        {
            m_dwSize--;
            memcpy( &m_v[i], &m_v[i+1], sizeof( m_v[0] ) * ( m_dwSize - i ) );
        }
    }
    void                SetSize( DWORD dwSize ) { m_dwSize = dwSize; }
    friend class COptiMatchQuery;
    COptiMatchResult    m_v[MAX_OPTI_MATCH_RESULTS];
    DWORD               m_dwSize;
};



//
// Query object for OptiMatch query (id 0x1)
//
class COptiMatchQuery
{
public:
    COptiMatchQuery();
    ~COptiMatchQuery();
    COptiMatchQueryResults Results;

    HRESULT         Process();
    void            Cancel();
    void            Clear();
    BOOL            Done() const { return m_State == STATE_DONE; }
    BOOL            Succeeded() const { return SUCCEEDED( m_hrQuery ); }
    HRESULT         Query(
                     ULONGLONG GameType, // Optional: X_MATCH_NULL_INTEGER to omit
                     ULONGLONG PlayerLevel, // Optional: X_MATCH_NULL_INTEGER to omit
                     ULONGLONG GameStyle // Optional: X_MATCH_NULL_INTEGER to omit
                    );
    BOOL            IsRunning() const 
        { return m_State == STATE_RUNNING || m_State == STATE_PROBING_CONNECTIVITY; }
    BOOL            IsProbing() const 
        { return m_State == STATE_PROBING_BANDWIDTH; }
    HRESULT         Probe();

private:

    // Quality of Service
    const XNADDR*   m_rgpXnAddr[ MAX_OPTI_MATCH_RESULTS ];
    const XNKID*    m_rgpXnKid [ MAX_OPTI_MATCH_RESULTS ];
    const XNKEY*    m_rgpXnKey [ MAX_OPTI_MATCH_RESULTS ];
    XNQOS*          m_pXnQos;

    enum STATE
    {
        STATE_IDLE,
        STATE_RUNNING,
        STATE_PROBING_CONNECTIVITY,
        STATE_PROBING_BANDWIDTH,
        STATE_DONE
    };

    STATE               m_State;
    HRESULT             m_hrQuery;
    XONLINETASK_HANDLE  m_hSearchTask;
};




//
// CFindSessionByIDResult represents a single return result from the
// FindSessionByID query
//
#pragma pack(push, 1)
class CFindSessionByIDResult
{
public:

    XNKID      SessionID;
    XNKEY      KeyExchangeKey;
    XNADDR     HostAddress;
    DWORD      dwPublicOpen;
    DWORD      dwPrivateOpen;
    DWORD      dwPublicFilled;
    DWORD      dwPrivateFilled;

};
#pragma pack(pop)



//  Collection of results for the FindSessionByID query
class CFindSessionByIDQueryResults
{
public:
    CFindSessionByIDQueryResults() { m_dwSize = 0; }
    DWORD                   Size() { return m_dwSize; }
    CFindSessionByIDResult& operator[]( DWORD i ) { return m_v[i]; }

private:
    void        Clear() { m_dwSize = 0; }
    void        Remove( DWORD i )
    {
        assert( i < m_dwSize );
        if( i < m_dwSize )
        {
            m_dwSize--;
            memcpy( &m_v[i], &m_v[i+1], sizeof( m_v[0] ) * ( m_dwSize - i ) );
        }
    }
    void        SetSize( DWORD dwSize ) { m_dwSize = dwSize; }
    friend class CFindSessionByIDQuery;

    CFindSessionByIDResult  m_v[MAX_FIND_SESSION_BY_ID_RESULTS];
    DWORD                   m_dwSize;
};



//
// Query object for FindSessionByID query (id 0x2)
//
class CFindSessionByIDQuery
{
public:
    CFindSessionByIDQuery();
    ~CFindSessionByIDQuery();
    CFindSessionByIDQueryResults Results;

    HRESULT     Process();
    void        Cancel();
    void        Clear();
    BOOL        Done()      const { return m_State == STATE_DONE; }
    BOOL        Succeeded() const { return SUCCEEDED( m_hrQuery ); }
    HRESULT     Query(
                    ULONGLONG SessionID
                     );
    BOOL        IsRunning()  const { return m_State == STATE_RUNNING; }

private:

    enum STATE
    {
        STATE_IDLE,
        STATE_RUNNING,
        STATE_DONE
    };

    STATE               m_State;
    HRESULT             m_hrQuery;
    XONLINETASK_HANDLE  m_hSearchTask;
};

//-------------------------------------------------------------------------------------
// Name: class SessionInfo
// Desc: Session information from the matchmaking server
//-------------------------------------------------------------------------------------
class SessionInfo
{
    XNKID       m_SessionID;
    XNKEY       m_KeyExchangeKey;
    XNADDR      m_HostAddress;
    DWORD       m_dwPublicOpen;

    ULONGLONG   m_qwGameType;
    ULONGLONG   m_qwPlayerLevel;
    ULONGLONG   m_qwGameStyle;
    CBlob       m_ConfigInfo;
    WCHAR       m_strOwnerName[XATTRIB_OWNER_NAME_MAX_LEN+1];
    WCHAR       m_strSessionName[XATTRIB_SESSION_NAME_MAX_LEN+1];


public:
    SessionInfo();
    SessionInfo( COptiMatchResult& );
    SessionInfo( CFindSessionByIDResult& );

    XNKID*      GetSessionID()           { return &m_SessionID; }
    XNKEY*      GetKeyExchangeKey()      { return &m_KeyExchangeKey; }
    XNADDR*     GetHostAddr()            { return &m_HostAddress; }

    // Session attributes
    DWORD       GetPublicAvail()         { return m_dwPublicOpen; }
    ULONGLONG   GetGameType()            { return m_qwGameType; }
    ULONGLONG   GetPlayerLevel()         { return m_qwPlayerLevel; }
    ULONGLONG   GetGameStyle()           { return m_qwGameStyle; }
    WCHAR*      GetSessionName()         { return m_strSessionName; }
    WCHAR*      GetOwnerName()           { return m_strOwnerName; }

    CBlob       GetConfigInfo()          { return m_ConfigInfo; }

    VOID        SetGameType( ULONGLONG );
    VOID        SetPlayerLevel( ULONGLONG );
    VOID        SetGameStyle( ULONGLONG );
    VOID        SetSessionName( const WCHAR* );
    VOID        SetOwnerName( const WCHAR* );
    VOID        SetConfigInfo( const CBlob & );

    VOID        GenRandSessionName();

};

#endif // MATCH_H