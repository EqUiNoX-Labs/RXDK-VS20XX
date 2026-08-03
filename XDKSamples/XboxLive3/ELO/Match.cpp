//-------------------------------------------------------------------------------------
// File: Match.cpp
//
// Desc: Definitions for match-making and network
//       messaging objects used by the Elo demo.
//       Modified from the Match-Making sample.
//
// Hist: 09.10.04 - New for Sept release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "match.h"
#include "XbRandName.h"

#ifdef _DEBUG
//-------------------------------------------------------------------------------------
// Name: Print
// Desc: Write formatted debug output
//-------------------------------------------------------------------------------------
static VOID __cdecl Print( const WCHAR* strFormat, ... )
{
    const int MAX_OUTPUT_STR = 80;
    WCHAR strBuffer[ MAX_OUTPUT_STR ];
    va_list pArglist;
    va_start( pArglist, strFormat );

    INT iChars= wvsprintfW( strBuffer, strFormat, pArglist );
    assert( iChars < MAX_OUTPUT_STR );
    (VOID) iChars; // Avoid compiler warning

    OutputDebugStringW( L"\n*** Matchmaking: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );

    va_end( pArglist );
}
#endif




//-------------------------------------------------------------------------------------
// Name: CSession
// Desc: Constructor
//-------------------------------------------------------------------------------------
CSession::CSession()
{
    m_State = STATE_IDLE;
    m_hSessionTask = NULL;
    m_bUpdate = FALSE;
    m_bListening = FALSE;
    m_bKeyRegistered = FALSE;
    SetupAttributes();
}




//-------------------------------------------------------------------------------------
// Name: ~CSession
// Desc: Destructor
//-------------------------------------------------------------------------------------
CSession::~CSession()
{
    Reset();
}




//-------------------------------------------------------------------------------------
// Name: Reset
// Desc: Reset the Session
//-------------------------------------------------------------------------------------
VOID CSession::Reset()
{
    Close();
    Listen( FALSE, NO_WAIT );
    PurgeSessionQ( TRUE );
    if( m_bKeyRegistered )
    {
        XNetUnregisterKey( &m_SessionID );
        m_bKeyRegistered = FALSE;
    }
}




//-------------------------------------------------------------------------------------
// Name: Create
// Desc: Create a new matchmaking session
//-------------------------------------------------------------------------------------
HRESULT CSession::Create()
{
    assert( m_State == STATE_IDLE );

    HRESULT hr = XOnlineMatchSessionCreate( m_dwPublicFilled, m_dwPublicOpen,
        m_dwPrivateFilled, m_dwPrivateOpen, NUM_ATTRIBUTES, m_Attributes, NULL,
        &m_hSessionTask );

    if( hr == S_OK )
    {
        m_State = STATE_CREATING;
    }
    else
    {
#ifdef _DEBUG
        Print( L"Session Creation Failed with 0x%%x", hr );

#endif
    }

    return hr;

}




//-------------------------------------------------------------------------------------
// Name: Update
// Desc: Update an existing session
//-------------------------------------------------------------------------------------
HRESULT CSession::Update()
{
    switch ( m_State )
    {
    case STATE_IDLE:
    case STATE_DELETING:
        return E_UNEXPECTED;
    case STATE_CREATING:
    case STATE_UPDATING:
        // If the session is still being created or is in the process of
        // updating, set m_bUpdate so that another update will be done
        // afterwards
        m_bUpdate = TRUE;
        return S_OK;
    case STATE_ACTIVE:
        {
            XOnlineTaskClose( m_hSessionTask );
            m_hSessionTask = NULL;

            HRESULT hr = XOnlineMatchSessionUpdate( m_SessionID,
                m_dwPublicFilled, m_dwPublicOpen, m_dwPrivateFilled,
                m_dwPrivateOpen, NUM_ATTRIBUTES, m_Attributes, NULL,
                &m_hSessionTask );

            m_bUpdate = FALSE;  // Clear update flag since we just updated

            if( SUCCEEDED( hr ) )
            {
                m_State = STATE_UPDATING;
            }
            else
            {
#ifdef _DEBUG
                Print( L"Session Update Failed with 0x%%x", hr );
#endif
                Close();
            }

            return hr;
        }
    default:
        assert(0);
        return E_UNEXPECTED;

    }
}




//-------------------------------------------------------------------------------------
// Name: Delete
// Desc: Delete an existing session
//-------------------------------------------------------------------------------------
HRESULT CSession::Delete()
{
    HRESULT hr = S_OK;


    switch( m_State )
    {
    case STATE_IDLE:
        break;
    case STATE_CREATING:
        Close();  // Close down create task
        break;
    case STATE_UPDATING:
        Close();  // Close down update task
        Listen( FALSE );  // Send go-aways
        break;
    case STATE_DELETING:
        break;
    case STATE_ACTIVE:
        if( m_bListening )
        {
            Listen( FALSE );
        }
        else
        {
            INT iResult = XNetUnregisterKey( &m_SessionID );
            assert( iResult == 0 );
            (VOID) iResult; // Avoid compiler warning
        }
        m_bKeyRegistered = FALSE;
        Close();
        hr = XOnlineMatchSessionDelete( m_SessionID, NULL, &m_hSessionTask );
        if(SUCCEEDED( hr ) )
        {
            m_State = STATE_DELETING;
        }
        else
        {
#ifdef _DEBUG
            Print( L"Session Delete Failed with 0x%x", hr );
#endif
        }

        break;
    }

    return hr;
}




//-------------------------------------------------------------------------------------
// Name: ProcessStateCreateSession
// Desc: Continue servicing the creation task
//-------------------------------------------------------------------------------------
HRESULT CSession::ProcessStateCreateSession()
{
    HRESULT hr  = XOnlineTaskContinue( m_hSessionTask );
    if( hr != XONLINETASK_S_RUNNING )
    {
        if( FAILED( hr ) )
        {
            Close();
#ifdef _DEBUG
            Print( L"Session Creation Failed with 0x%x", hr );
#endif
        }
        else
        {
 
            // Extract the new session ID and Key-Exchange Key
            HRESULT hrGet = XOnlineMatchSessionGetInfo(
                m_hSessionTask,  &m_SessionID, &m_KeyExchangeKey );
            assert( SUCCEEDED( hrGet ) );
            (VOID)hrGet; // Avoid compiler warning
            m_State = STATE_ACTIVE;

            INT iKeyRegistered = XNetRegisterKey( &m_SessionID, 
                &m_KeyExchangeKey );
            if( iKeyRegistered == WSAENOMORE )
            {
#ifdef _DEBUG
                Print( L"Out of keys... Purging SessionQ and trying again" );
#endif
                // Too many keys have been registered, remove
                // the registered key on the session queue and try again
                PurgeSessionQHead();
                iKeyRegistered = XNetRegisterKey( &m_SessionID, 
                    &m_KeyExchangeKey );
            }
            assert( iKeyRegistered == NO_ERROR );
            m_bKeyRegistered = ( iKeyRegistered == NO_ERROR);

            // Start listening for Qos probes for this new session
            Listen( TRUE );

            // If a call to Update() was made while the session
            // was being created, call Update to send the new
            // information.
            if( m_bUpdate )
            {
                hr = Update();
            }
        }
    }

    return hr;
}




//-------------------------------------------------------------------------------------
// Name: ProcessStateUpdateSession
// Desc: Continue servicing the session update task
//-------------------------------------------------------------------------------------
HRESULT CSession::ProcessStateUpdateSession()
{
    HRESULT hr  = XOnlineTaskContinue( m_hSessionTask );
    if( hr != XONLINETASK_S_RUNNING )
    {
        if( FAILED( hr ) )
        {
            Close();
#ifdef _DEBUG
            Print( L"Session Update Failed with 0x%x", hr );
#endif
        }
        else
        {
            m_State = STATE_ACTIVE;

            // If a call to Update() was made while the session
            // was already being updated, call Update to send the new
            // information.
            if( m_bUpdate )
            {
                hr = Update();
            }
        }
    }

    return hr;
}




//-------------------------------------------------------------------------------------
// Name: ProcessStateDeleteSession
// Desc: Continue servicing the session deletion task
//-------------------------------------------------------------------------------------
HRESULT CSession::ProcessStateDeleteSession()
{
    HRESULT hr  = XOnlineTaskContinue( m_hSessionTask );
    if( hr != XONLINETASK_S_RUNNING )
    {
        if( FAILED( hr ) )
        {
#ifdef _DEBUG
            Print( L"Session Delete Failed with 0x%%x", hr );
#endif
        }

        Close();
    }

    return hr;
}




//-------------------------------------------------------------------------------------
// Name: ProcessStateActiveSession
// Desc: Continue servicing the session task
//-------------------------------------------------------------------------------------
HRESULT CSession::ProcessStateActiveSession()
{
    HRESULT hr  = XOnlineTaskContinue( m_hSessionTask );
    if( FAILED( hr ) )
    {
        Close();
#ifdef _DEBUG
        Print( L"Session Task Failed with 0x%%x", hr );
#endif
    }

    return hr;
}




//-------------------------------------------------------------------------------------
// Name: Process
// Desc: Perform a unit of work (such as servicing tasks) as necessary
//-------------------------------------------------------------------------------------
HRESULT CSession::Process()
{
    PurgeSessionQ();
    HRESULT hr;

    switch( m_State )
    {
    case STATE_IDLE:       hr = XONLINETASK_S_SUCCESS;       break;
    case STATE_CREATING:   hr = ProcessStateCreateSession(); break;
    case STATE_UPDATING:   hr = ProcessStateUpdateSession(); break;
    case STATE_DELETING:   hr = ProcessStateDeleteSession(); break;
    case STATE_ACTIVE:     hr = ProcessStateActiveSession(); break;
    default: assert(0);    return E_UNEXPECTED;
    }

    if( FAILED( hr ) )
    {
        Reset();
    }

    return hr;
}




//-------------------------------------------------------------------------------------
// Name: Close
// Desc: Close down any session tasks
//-------------------------------------------------------------------------------------
VOID CSession::Close()
{
    if( m_hSessionTask )
    {
        XOnlineTaskClose( m_hSessionTask);
        m_hSessionTask = NULL;
        m_State = STATE_IDLE;
    }

    m_bUpdate = FALSE;
}




//-------------------------------------------------------------------------------------
// Name: PurgeSessionQ
// Desc: Cease Qos listening, and unregister, old SessionIDs
//-------------------------------------------------------------------------------------
VOID CSession::PurgeSessionQ( BOOL fRemoveAll )
{
    // Cleanup any registered SessionIDs which are sending 
    // go-away qos responses
    const DWORD QOS_PROBE_REJECT_TIMELIMIT = 15000; // timeout in ms

    const QosQEntry *pItem;
    // Entries in the queue are in reverse chronological order, 
    // so we can stop once we encounter an entry that has not reached
    // the time limit
    while( ( pItem = m_SessionQosQ.Head() ) != NULL )
    {
        if( fRemoveAll || 
            GetTickCount() - pItem->dwStartTick >= QOS_PROBE_REJECT_TIMELIMIT )
        {
            PurgeSessionQHead();
        }
        else
            break;
    }
}




//-------------------------------------------------------------------------------------
// Name: PurgeSessionQHead
// Desc: Cease Qos listening, and unregister, the first session on the queue
//-------------------------------------------------------------------------------------
VOID CSession::PurgeSessionQHead()
{
    const QosQEntry *pItem;
    if ( ( pItem = m_SessionQosQ.Head() ) != NULL )
    {
#ifdef _DEBUG
        Print( L"Qos listening rejection period expired for 0x%x", pItem );
#endif
        INT iQos = XNetQosListen( &pItem->SessionID, NULL, 0, 0, 
                                  XNET_QOS_LISTEN_RELEASE );
        (VOID) iQos;
        // If this SessionID is not in use, unregister it
        if( !m_bKeyRegistered || memcmp( &pItem->SessionID, &m_SessionID, 
            sizeof( XNKID ) ) != 0 )
        {
            INT iResult = XNetUnregisterKey( &pItem->SessionID );
            (VOID) iResult;
        }
        m_SessionQosQ.Dequeue();
    }
}




//-------------------------------------------------------------------------------------
// Name: Listen
// Desc: Control Qos listening
// bEnable     dwBitsPerSec
// -------     ---------
// TRUE        Bandwidth setting (zero for default)
// FALSE       Bandwidth setting (zero is default, NO_WAIT means shut down
//             immediately)
//-------------------------------------------------------------------------------------
VOID CSession::Listen( BOOL bEnable, DWORD dwBitsPerSec )
{
    if( bEnable )
    {
        m_SessionQosQ.Remove( m_SessionID );
        INT iQos = XNetQosListen( &m_SessionID, (BYTE *) m_QosResponse.Data,
            m_QosResponse.Length, dwBitsPerSec, 
            XNET_QOS_LISTEN_ENABLE | XNET_QOS_LISTEN_SET_DATA |
            XNET_QOS_LISTEN_SET_BITSPERSEC );
        (VOID)iQos; // Avoid compiler warning
        m_bListening = TRUE;
    }
    else
    {
        if( m_bListening )
        {
            INT iQos;
            m_bListening = FALSE;
            if( dwBitsPerSec == NO_WAIT ) // Stop listening, and release Qos resources
            {
#ifdef _DEBUG
                Print( L"Qos listening stopped" );
#endif
                iQos = XNetQosListen( &m_SessionID, NULL, 0, 0, 
                                      XNET_QOS_LISTEN_RELEASE );
            }
            else
            {
                // Start rejecting probes for a period of time by sending a "go away"
                // response.  Then, after a certain period of time, actually stop
                // listening by releasing Qos resources
                m_SessionQosQ.Add( m_SessionID, GetTickCount() );
                iQos = XNetQosListen( &m_SessionID, NULL, 0, 
                    dwBitsPerSec, 
                    XNET_QOS_LISTEN_DISABLE | XNET_QOS_LISTEN_SET_BITSPERSEC );
#ifdef _DEBUG
                Print( L"Qos probe rejection period started for 0x%x", 
                       m_SessionQosQ.Tail() );
#endif
            }
            (VOID)iQos; // Avoid compiler warning
        }
    }
}




//-------------------------------------------------------------------------------------
// Name: GetQosResponse
// Desc: Return the value of the Qos Response
//-------------------------------------------------------------------------------------
CBlob CSession::GetQosResponse()
{
    return m_QosResponse;
}




//-------------------------------------------------------------------------------------
// Name: SetQosResponse
// Desc: Set data to be sent in response to Qos probes
//-------------------------------------------------------------------------------------
VOID  CSession::SetQosResponse( CBlob Value )
{
    m_QosResponse = Value;
    if( m_bListening )
    {
        // Call XNetQosListen to set the new value
        INT iQos = XNetQosListen( &m_SessionID , (BYTE *) m_QosResponse.Data,
            m_QosResponse.Length, 0, XNET_QOS_LISTEN_SET_DATA  );
        (VOID)iQos; // Avoid compiler warning
    }
}




//-------------------------------------------------------------------------------------
// Name: SetupAttributes
// Desc: Initialize the m_Attributes array and related string/blob buffers
//-------------------------------------------------------------------------------------
VOID CSession::SetupAttributes()
{
    ZeroMemory( &m_Attributes, sizeof( m_Attributes ) );
    m_Attributes[GAME_TYPE_INDEX].dwAttributeID = XATTRIB_GAME_TYPE;
    m_Attributes[PLAYER_LEVEL_INDEX].dwAttributeID = XATTRIB_PLAYER_LEVEL;
    m_Attributes[SESSION_NAME_INDEX].dwAttributeID = XATTRIB_SESSION_NAME;
    m_Attributes[SESSION_NAME_INDEX].info.string.lpValue = m_strSessionName;
    m_strSessionName[0] = L'\0';
    m_Attributes[GAME_STYLE_INDEX].dwAttributeID = XATTRIB_GAME_STYLE;
    m_Attributes[OWNER_NAME_INDEX].dwAttributeID = XATTRIB_OWNER_NAME;
    m_Attributes[OWNER_NAME_INDEX].info.string.lpValue = m_strOwnerName;
    m_strOwnerName[0] = L'\0';
    m_Attributes[CONFIG_INFO_INDEX].dwAttributeID = XATTRIB_CONFIG_INFO;
    ZeroMemory( &m_rbBlobConfigInfo, XATTRIB_CONFIG_INFO_MAX_LEN );
    m_Attributes[CONFIG_INFO_INDEX].info.blob.pvValue = m_rbBlobConfigInfo;
}



//-------------------------------------------------------------------------------------
// Name: GetGameType
// Desc: Return the value of the 'GameType' attribute
//-------------------------------------------------------------------------------------
ULONGLONG CSession::GetGameType()
{
    return m_Attributes[GAME_TYPE_INDEX].info.integer.qwValue;
}




//-------------------------------------------------------------------------------------
// Name: SetGameType
// Desc: Set the 'GameType' attribute
//-------------------------------------------------------------------------------------
VOID  CSession::SetGameType( ULONGLONG Value )
{
    m_Attributes[GAME_TYPE_INDEX].info.integer.qwValue = Value;
    m_Attributes[GAME_TYPE_INDEX].fChanged = TRUE;
}




//-------------------------------------------------------------------------------------
// Name: GetPlayerLevel
// Desc: Return the value of the 'PlayerLevel' attribute
//-------------------------------------------------------------------------------------
ULONGLONG CSession::GetPlayerLevel()
{
    return m_Attributes[PLAYER_LEVEL_INDEX].info.integer.qwValue;
}




//-------------------------------------------------------------------------------------
// Name: SetPlayerLevel
// Desc: Set the 'PlayerLevel' attribute
//-------------------------------------------------------------------------------------
VOID  CSession::SetPlayerLevel( ULONGLONG Value )
{
    m_Attributes[PLAYER_LEVEL_INDEX].info.integer.qwValue = Value;
    m_Attributes[PLAYER_LEVEL_INDEX].fChanged = TRUE;
}




//-------------------------------------------------------------------------------------
// Name: GetSessionName
// Desc: Return the value of the 'SessionName' attribute
//-------------------------------------------------------------------------------------
const WCHAR * CSession::GetSessionName()
{
    return m_strSessionName;
}




//-------------------------------------------------------------------------------------
// Name: SetSessionName
// Desc: Set the 'SessionName' attribute
//-------------------------------------------------------------------------------------
VOID  CSession::SetSessionName( const WCHAR * Value )
{
    wcscpy( m_strSessionName, Value );
    m_Attributes[SESSION_NAME_INDEX].fChanged = TRUE;
}




//-------------------------------------------------------------------------------------
// Name: GetGameStyle
// Desc: Return the value of the 'GameStyle' attribute
//-------------------------------------------------------------------------------------
ULONGLONG CSession::GetGameStyle()
{
    return m_Attributes[GAME_STYLE_INDEX].info.integer.qwValue;
}




//-------------------------------------------------------------------------------------
// Name: SetGameStyle
// Desc: Set the 'GameStyle' attribute
//-------------------------------------------------------------------------------------
VOID  CSession::SetGameStyle( ULONGLONG Value )
{
    m_Attributes[GAME_STYLE_INDEX].info.integer.qwValue = Value;
    m_Attributes[GAME_STYLE_INDEX].fChanged = TRUE;
}




//-------------------------------------------------------------------------------------
// Name: GetOwnerName
// Desc: Return the value of the 'OwnerName' attribute
//-------------------------------------------------------------------------------------
const WCHAR * CSession::GetOwnerName()
{
    return m_strOwnerName;
}




//-------------------------------------------------------------------------------------
// Name: SetOwnerName
// Desc: Set the 'OwnerName' attribute
//-------------------------------------------------------------------------------------
VOID  CSession::SetOwnerName( const WCHAR * Value )
{
    wcscpy( m_strOwnerName, Value );
    m_Attributes[OWNER_NAME_INDEX].fChanged = TRUE;
}




//-------------------------------------------------------------------------------------
// Name: GetConfigInfo
// Desc: Return the value of the 'ConfigInfo' attribute
//-------------------------------------------------------------------------------------
CBlob CSession::GetConfigInfo()
{
    return CBlob( (WORD) m_Attributes[CONFIG_INFO_INDEX].info.blob.dwLength, 
                  m_rbBlobConfigInfo );
}




//-------------------------------------------------------------------------------------
// Name: SetConfigInfo
// Desc: Set the 'ConfigInfo' attribute
//-------------------------------------------------------------------------------------
VOID  CSession::SetConfigInfo( CBlob Value )
{
    assert( Value.Length <= XATTRIB_CONFIG_INFO_MAX_LEN ); 
    memcpy( m_rbBlobConfigInfo, Value.Data, Value.Length );
    m_Attributes[CONFIG_INFO_INDEX].info.blob.dwLength = Value.Length;
    m_Attributes[CONFIG_INFO_INDEX].fChanged = TRUE;
}




//-------------------------------------------------------------------------------------
// Name: CSessionQosQ
// Desc: Constructor
//-------------------------------------------------------------------------------------
CSession::CSessionQosQ::CSessionQosQ()
{
    m_pHead = m_pTail = NULL;
}




//-------------------------------------------------------------------------------------
// Name: Add
// Desc: Add a session id to the qos go-away response queue
//-------------------------------------------------------------------------------------

VOID CSession::CSessionQosQ::Add( XNKID& SessionID, DWORD dwStartTick )
{
    assert( m_pHead && m_pTail || !m_pHead && !m_pTail );
    QosQEntry *pItem = new QosQEntry;
    pItem->SessionID = SessionID;
    pItem->dwStartTick = dwStartTick;
    pItem->pNext = NULL;
    if( m_pTail )
        m_pTail->pNext = pItem;
    m_pTail = pItem;
    if( !m_pHead )
        m_pHead = pItem;
}




//-------------------------------------------------------------------------------------
// Name: Remove
// Desc: Remove a session from the qos go-away response queue
//-------------------------------------------------------------------------------------
VOID CSession::CSessionQosQ::Remove( XNKID& SessionID )
{
    assert( m_pHead && m_pTail || !m_pHead && !m_pTail );
    QosQEntry *pPrev = NULL;

    for( QosQEntry *pItem = m_pHead; pItem != NULL; pItem = pItem->pNext )
    {
        if( memcmp(&pItem->SessionID, &SessionID, sizeof( XNKID ) ) == 0 )
        {
            if( pPrev )
            {
                pPrev->pNext = pItem->pNext;
            }
            else
            {
                m_pHead = pItem->pNext;
            }
            if( pItem == m_pTail )
            {
                m_pTail = pPrev;
            }
            delete pItem;
            break;
        }
        pPrev = pItem;
    }
}




//-------------------------------------------------------------------------------------
// Name: Dequeue
// Desc: Remove the head of the qos go-away response queue
//-------------------------------------------------------------------------------------
VOID CSession::CSessionQosQ::Dequeue()
{
    assert( m_pHead && m_pTail || !m_pHead && !m_pTail );
    QosQEntry *pItem = m_pHead;
    if( pItem )
    {
        m_pHead = pItem->pNext;
        delete pItem;
        if( !m_pHead )
            m_pTail = NULL;
    }
}



// Attribute layout for OptiMatch return attributes.
// This must match the order of both the query return attributes
// and the fields specified in the COptiMatchResult class.
static const
XONLINE_ATTRIBUTE_SPEC OptiMatchAttributeSpec[]=
{
    { X_ATTRIBUTE_DATATYPE_INTEGER, sizeof( ULONGLONG ) },      // GameType
    { X_ATTRIBUTE_DATATYPE_INTEGER, sizeof( ULONGLONG ) },      // GameStyle
    { X_ATTRIBUTE_DATATYPE_INTEGER, sizeof( ULONGLONG ) },      // PlayerLevel
    { X_ATTRIBUTE_DATATYPE_STRING, ( XATTRIB_SESSION_NAME_MAX_LEN + 1 ) 
      * sizeof( WCHAR ) },                                      // SessionName
    { X_ATTRIBUTE_DATATYPE_STRING, ( XATTRIB_OWNER_NAME_MAX_LEN + 1 ) 
      * sizeof( WCHAR ) },                                      // OwnerName
    { X_ATTRIBUTE_DATATYPE_BLOB, XATTRIB_CONFIG_INFO_MAX_LEN }, // ConfigInfo
};




//-------------------------------------------------------------------------------------
// Name: COptiMatchQuery
// Desc: Constructor
//-------------------------------------------------------------------------------------
COptiMatchQuery::COptiMatchQuery()
{
    m_State = STATE_IDLE;
    m_hrQuery = S_FALSE;
    m_hSearchTask = NULL;
    m_pXnQos = NULL;
}



//-------------------------------------------------------------------------------------
// Name: COptiMatchQuery()
// Desc: Destructor
//-------------------------------------------------------------------------------------
COptiMatchQuery::~COptiMatchQuery()
{
    Cancel();
}



//-------------------------------------------------------------------------------------
// Name: Cancel
// Desc: Cancel a OptiMatch query
//-------------------------------------------------------------------------------------
void COptiMatchQuery::Cancel()
{
    switch( m_State )
    {
    case STATE_IDLE:
       break;
    case STATE_RUNNING:
    case STATE_PROBING_BANDWIDTH:
    case STATE_PROBING_CONNECTIVITY:
    case STATE_DONE:
        if( m_hSearchTask )
        {
            XOnlineTaskClose( m_hSearchTask );
            m_hSearchTask = NULL;
        }
        Clear();
        m_State = STATE_IDLE;
        m_hrQuery = S_FALSE;
        break;
    default:
        assert(0);
    }
}



//-------------------------------------------------------------------------------------
// Name: Clear
// Desc: Release resources
//-------------------------------------------------------------------------------------
void COptiMatchQuery::Clear()
{
    if( m_pXnQos )
    {
        XNetQosRelease( m_pXnQos );
        m_pXnQos = NULL;
    }
    Results.Clear();
}



//-------------------------------------------------------------------------------------
// Name: Query
// Desc: Execute the OptiMatch query (id 0x1)
//-------------------------------------------------------------------------------------
HRESULT COptiMatchQuery::Query(
                  ULONGLONG GameType, // Optional: X_MATCH_NULL_INTEGER to omit
                  ULONGLONG PlayerLevel, // Optional: X_MATCH_NULL_INTEGER to omit
                  ULONGLONG GameStyle // Optional: X_MATCH_NULL_INTEGER to omit
                  )
{
    const DWORD SEARCH_PROC_ID = 0x1;

    if( m_State == STATE_DONE ) // Clear existing results
    {
        Clear();
        m_State = STATE_IDLE;
    }
    assert( m_State == STATE_IDLE );
    if (m_State != STATE_IDLE)
        return E_UNEXPECTED;

    XONLINE_ATTRIBUTE QueryParameters[3] = { 0 };
    QueryParameters[0].dwAttributeID = ( GameType == X_MATCH_NULL_INTEGER ) ? 
        X_ATTRIBUTE_DATATYPE_NULL : X_ATTRIBUTE_DATATYPE_INTEGER;
    QueryParameters[0].info.integer.qwValue = GameType;
    QueryParameters[1].dwAttributeID = ( PlayerLevel == X_MATCH_NULL_INTEGER ) ? 
        X_ATTRIBUTE_DATATYPE_NULL : X_ATTRIBUTE_DATATYPE_INTEGER;
    QueryParameters[1].info.integer.qwValue = PlayerLevel;
    QueryParameters[2].dwAttributeID = ( GameStyle == X_MATCH_NULL_INTEGER ) ? 
        X_ATTRIBUTE_DATATYPE_NULL : X_ATTRIBUTE_DATATYPE_INTEGER;
    QueryParameters[2].info.integer.qwValue = GameStyle;

    // Calculate maximum space required to hold results
    DWORD dwResultsLen = XOnlineMatchSearchResultsLen( MAX_OPTI_MATCH_RESULTS, 
                          sizeof( OptiMatchAttributeSpec ) / 
                          sizeof( OptiMatchAttributeSpec[0] ), 
                          OptiMatchAttributeSpec );

    HRESULT hrSearch = XOnlineMatchSearch( SEARCH_PROC_ID, MAX_OPTI_MATCH_RESULTS,
                                     3, QueryParameters, dwResultsLen, NULL, 
                                     &m_hSearchTask );

    if( SUCCEEDED( hrSearch ) )
        m_State = STATE_RUNNING;

    return hrSearch;

}



//-------------------------------------------------------------------------------------
// Name: Probe
// Desc: Initiate bandwidth probing
//-------------------------------------------------------------------------------------
HRESULT COptiMatchQuery::Probe()
{
    assert( m_State == STATE_DONE );
    if (m_State != STATE_DONE)
       return E_UNEXPECTED;

    // Clean up any earlier probes
    if( m_pXnQos )
    {
        XNetQosRelease( m_pXnQos );
        m_pXnQos = NULL;
    }

    DWORD dwNumSessions = Results.Size();
    if( dwNumSessions )
    {
        for( DWORD i = 0; i < dwNumSessions; ++i )
        {
            Results[i].m_pQosInfo = NULL;
            m_rgpXnAddr[i] = &Results[i].m_HostAddress;
            m_rgpXnKid[i]  = &Results[i].m_SessionID;
            m_rgpXnKey[i]  = &Results[i].m_KeyExchangeKey;
        }
        INT iQos = XNetQosLookup( dwNumSessions, m_rgpXnAddr,
            m_rgpXnKid, m_rgpXnKey, 0, NULL, NULL,
            NUM_QOS_PROBES, QOS_BITS_PER_SEC, 0, NULL,
            &m_pXnQos );

        if( iQos == 0 )
            m_State = STATE_PROBING_BANDWIDTH;
        else
            return E_FAIL;
    }

    return S_OK;

}



//-------------------------------------------------------------------------------------
// Name: Process
// Desc: Continue servicing the query task
//-------------------------------------------------------------------------------------
HRESULT COptiMatchQuery::Process()
{

    if( m_State == STATE_IDLE )
        return S_OK;

    if( m_State == STATE_DONE )
        return m_hrQuery;

    if( m_State == STATE_PROBING_BANDWIDTH )
    {
        // Update any completed Qos info for sessions
        // as they finish
        for( DWORD iResult = 0; iResult < Results.Size(); ++iResult )
        {
            if( !Results[iResult].m_pQosInfo && 
               ( m_pXnQos->axnqosinfo[iResult].bFlags & XNET_XNQOSINFO_COMPLETE ) )
            {
                Results[iResult].m_pQosInfo = &m_pXnQos->axnqosinfo[iResult];
            }
        }

        // Check if all probing is complete
        if( m_pXnQos->cxnqosPending == 0 )
        {
            m_State = STATE_DONE;
            return m_hrQuery;
        }

        return XONLINETASK_S_RUNNING;
    }

    if (m_State == STATE_PROBING_CONNECTIVITY ) 
    {
        // Check if probing is complete
        if( m_pXnQos->cxnqosPending == 0 )
        {
            // Copy QoS info into individual result objects, removing
            // any entries which are not reachable
            DWORD dwNumQosResults = Results.Size();
            DWORD iResult = 0;
            for( DWORD iQos = 0; iQos < dwNumQosResults; ++iQos )
            {
                if( ( m_pXnQos->axnqosinfo[iQos].bFlags & XNET_XNQOSINFO_TARGET_CONTACTED ) &&
                    !( m_pXnQos->axnqosinfo[iQos].bFlags & XNET_XNQOSINFO_TARGET_DISABLED ) )
                {
                    Results[iResult].m_pQosInfo = &m_pXnQos->axnqosinfo[iQos];
                    iResult++;
                }
                else
                {
#ifdef _DEBUG
                    Print( L"Removing Matching Session %lu (host not reachable or disabled)\n", iResult );
#endif
                    // Target not contacted, or is disabled, so remove result
                    Results.Remove( iResult );
                }
            }
            m_State = STATE_DONE;
            return m_hrQuery;
        }

        return XONLINETASK_S_RUNNING;
    }

    HRESULT hr = XOnlineTaskContinue( m_hSearchTask );
    if( hr != XONLINETASK_S_RUNNING )
    {
        m_hrQuery = hr;
        m_State = STATE_DONE;
        if( SUCCEEDED( hr ) )
        {
            // Fetch results
            XONLINE_MATCH_SEARCHRESULT** ppSearchResults;
            DWORD dwNumSessions;
            hr= XOnlineMatchSearchGetResults( m_hSearchTask,
                &ppSearchResults, &dwNumSessions );
            if( SUCCEEDED( hr ) )
            {
                Results.SetSize( dwNumSessions );
                for( DWORD i=0; i < dwNumSessions; ++i )
                {
                    XONLINE_MATCH_SEARCHRESULT* pxms = ppSearchResults[i];

                    Results.m_v[i].m_SessionID = pxms->SessionID;
                    Results.m_v[i].m_KeyExchangeKey = pxms->KeyExchangeKey;
                    Results.m_v[i].m_HostAddress = pxms->HostAddress;
                    Results.m_v[i].m_dwPublicOpen = pxms->dwPublicOpen;
                    Results.m_v[i].m_dwPrivateOpen = pxms->dwPrivateOpen;
                    Results.m_v[i].m_dwPublicFilled = pxms->dwPublicFilled;
                    Results.m_v[i].m_dwPrivateFilled = pxms->dwPrivateFilled;
                    hr = XOnlineMatchSearchParse( ppSearchResults[i],
                        sizeof( OptiMatchAttributeSpec ) / sizeof( OptiMatchAttributeSpec[0] ),
                        OptiMatchAttributeSpec, &Results.m_v[i] );
                    assert(SUCCEEDED(hr));
                   // Save data for Qos probing
                    Results.m_v[i].m_pQosInfo = NULL;
                    m_rgpXnAddr[i] = &Results.m_v[i].m_HostAddress;
                    m_rgpXnKid[i]  = &Results.m_v[i].m_SessionID;
                    m_rgpXnKey[i]  = &Results.m_v[i].m_KeyExchangeKey;
                }
                if( dwNumSessions )
                {
                    INT iQos = XNetQosLookup( dwNumSessions, m_rgpXnAddr,
                        m_rgpXnKid, m_rgpXnKey, 0, NULL, NULL,
                        0, QOS_BITS_PER_SEC, 0, NULL,
                        &m_pXnQos );

                    if( iQos == 0 )
                        m_State = STATE_PROBING_CONNECTIVITY;
                }
            }
        }

        XOnlineTaskClose( m_hSearchTask );
        m_hSearchTask = NULL;
    }

    return hr;
}



//-------------------------------------------------------------------------------------
// Name: CFindSessionByIDQuery
// Desc: Constructor
//-------------------------------------------------------------------------------------
CFindSessionByIDQuery::CFindSessionByIDQuery()
{
    m_State = STATE_IDLE;
    m_hrQuery = S_FALSE;
    m_hSearchTask = NULL;
}



//-------------------------------------------------------------------------------------
// Name: CFindSessionByIDQuery()
// Desc: Destructor
//-------------------------------------------------------------------------------------
CFindSessionByIDQuery::~CFindSessionByIDQuery()
{
    Cancel();
}



//-------------------------------------------------------------------------------------
// Name: Cancel
// Desc: Cancel a FindSessionByID query
//-------------------------------------------------------------------------------------
void CFindSessionByIDQuery::Cancel()
{
    switch( m_State )
    {
    case STATE_IDLE:
       break;
    case STATE_RUNNING:
    case STATE_DONE:
        if( m_hSearchTask )
        {
            XOnlineTaskClose( m_hSearchTask );
            m_hSearchTask = NULL;
        }
        Clear();
        m_State = STATE_IDLE;
        m_hrQuery = S_FALSE;
        break;
    default:
        assert(0);
    }
}



//-------------------------------------------------------------------------------------
// Name: Clear
// Desc: Release resources
//-------------------------------------------------------------------------------------
void CFindSessionByIDQuery::Clear()
{
    Results.Clear();
}



//-------------------------------------------------------------------------------------
// Name: Query
// Desc: Execute the FindSessionByID query (id 0x2)
//-------------------------------------------------------------------------------------
HRESULT CFindSessionByIDQuery::Query( ULONGLONG SessionID )
{
    const DWORD SEARCH_PROC_ID = 0x2;

    if( m_State == STATE_DONE ) // Clear existing results
    {
        Clear();
        m_State = STATE_IDLE;
    }
    assert( m_State == STATE_IDLE );
    if (m_State != STATE_IDLE)
        return E_UNEXPECTED;

    XONLINE_ATTRIBUTE QueryParameters[1] = { 0 };
    QueryParameters[0].dwAttributeID = X_ATTRIBUTE_DATATYPE_INTEGER;
    QueryParameters[0].info.integer.qwValue = SessionID;

    // Calculate maximum space required to hold results
    DWORD dwResultsLen = XOnlineMatchSearchResultsLen( MAX_FIND_SESSION_BY_ID_RESULTS, 0, NULL );

    HRESULT hr = XOnlineMatchSearch( SEARCH_PROC_ID, MAX_FIND_SESSION_BY_ID_RESULTS,
                                    1, QueryParameters, dwResultsLen, NULL, &m_hSearchTask );
    if( SUCCEEDED( hr ) )
        m_State = STATE_RUNNING;

    return hr;

}



//-------------------------------------------------------------------------------------
// Name: Process
// Desc: Continue servicing the query task
//-------------------------------------------------------------------------------------
HRESULT CFindSessionByIDQuery::Process()
{

    if( m_State == STATE_IDLE )
        return S_OK;

    if( m_State == STATE_DONE )
        return m_hrQuery;

    HRESULT hr = XOnlineTaskContinue( m_hSearchTask );
    if( hr != XONLINETASK_S_RUNNING )
    {
        m_hrQuery = hr;
        m_State = STATE_DONE;
        if( SUCCEEDED( hr ) )
        {
            // Fetch results
            XONLINE_MATCH_SEARCHRESULT** ppSearchResults;
            DWORD dwNumSessions;
            hr= XOnlineMatchSearchGetResults( m_hSearchTask,
                &ppSearchResults, &dwNumSessions );
            if( SUCCEEDED( hr ) )
            {
                Results.SetSize( dwNumSessions );
                for( DWORD i=0; i < dwNumSessions; ++i )
                {
                    XONLINE_MATCH_SEARCHRESULT* pxms = ppSearchResults[i];

                    Results.m_v[i].SessionID = pxms->SessionID;
                    Results.m_v[i].KeyExchangeKey = pxms->KeyExchangeKey;
                    Results.m_v[i].HostAddress = pxms->HostAddress;
                    Results.m_v[i].dwPublicOpen = pxms->dwPublicOpen;
                    Results.m_v[i].dwPrivateOpen = pxms->dwPrivateOpen;
                    Results.m_v[i].dwPublicFilled = pxms->dwPublicFilled;
                    Results.m_v[i].dwPrivateFilled = pxms->dwPrivateFilled;
                }
            }
        }

        XOnlineTaskClose( m_hSearchTask );
        m_hSearchTask = NULL;
    }

    return hr;
}


//-------------------------------------------------------------------------------------
// Name: SessionInfo()
// Desc: Default Constructor
//-------------------------------------------------------------------------------------
SessionInfo::SessionInfo()
{
    ZeroMemory( &m_SessionID, sizeof( m_SessionID ) );
    ZeroMemory( &m_KeyExchangeKey, sizeof( m_KeyExchangeKey ) );
    ZeroMemory( &m_HostAddress, sizeof( m_HostAddress ) );
    m_dwPublicOpen = 0;
    m_qwGameType = TYPE_ANY;
    m_qwGameStyle = STYLE_ANY;
    m_qwPlayerLevel = LEVEL_ANY;
    *m_strOwnerName   = 0;
    *m_strSessionName = 0;
    m_ConfigInfo = NULL_BLOB;
}

//-------------------------------------------------------------------------------------
// Name: SessionInfo()
// Desc: Constructor
//-------------------------------------------------------------------------------------
SessionInfo::SessionInfo( COptiMatchResult& Result )
{
    m_SessionID      = Result.m_SessionID;
    m_KeyExchangeKey = Result.m_KeyExchangeKey;
    m_HostAddress    = Result.m_HostAddress;
    m_dwPublicOpen   = Result.m_dwPublicOpen;
    m_qwGameType     = Result.GameType;
    m_qwGameStyle    = Result.GameStyle;
    m_qwPlayerLevel  = Result.PlayerLevel;
    SetOwnerName( Result.OwnerName );
    SetSessionName( Result.SessionName );
    m_ConfigInfo = Result.ConfigInfo;
}

//-------------------------------------------------------------------------------------
// Name: SessionInfo()
// Desc: Constructor
//-------------------------------------------------------------------------------------
SessionInfo::SessionInfo( CFindSessionByIDResult& Result )
{
    m_SessionID      = Result.SessionID;
    m_KeyExchangeKey = Result.KeyExchangeKey;
    m_HostAddress    = Result.HostAddress;
    m_dwPublicOpen   = Result.dwPublicOpen;
    m_qwGameType = TYPE_ANY;
    m_qwGameStyle = STYLE_ANY;
    m_qwPlayerLevel = LEVEL_ANY;
    *m_strOwnerName   = 0;
    *m_strSessionName = 0;
    m_ConfigInfo = NULL_BLOB;

}

//-------------------------------------------------------------------------------------
// Name: SetGameType()
// Desc: Set session game type
//-------------------------------------------------------------------------------------
VOID SessionInfo::SetGameType( ULONGLONG qwGameType )
{
    m_qwGameType = qwGameType;
}

//-------------------------------------------------------------------------------------
// Name: SetPlayerLevel()
// Desc: Set session player level
//-------------------------------------------------------------------------------------
VOID SessionInfo::SetPlayerLevel( ULONGLONG qwPlayerLevel )
{
    m_qwPlayerLevel = qwPlayerLevel;
}

//-------------------------------------------------------------------------------------
// Name: SetSessionName()
// Desc: Set session name
//-------------------------------------------------------------------------------------
VOID SessionInfo::SetSessionName( const WCHAR* strSessionName )
{
    assert( strSessionName != NULL );
    lstrcpynW( m_strSessionName, strSessionName, XATTRIB_SESSION_NAME_MAX_LEN );
}

//-------------------------------------------------------------------------------------
// Name: SetOwnerName()
// Desc: Set owner name
//-------------------------------------------------------------------------------------
VOID SessionInfo::SetOwnerName( const WCHAR* strOwnerName )
{
    assert( strOwnerName != NULL );
    lstrcpynW( m_strOwnerName, strOwnerName, XONLINE_GAMERTAG_SIZE );
}

//-------------------------------------------------------------------------------------
// Name: SetGameStyle()
// Desc: Set game style
//-------------------------------------------------------------------------------------
VOID SessionInfo::SetGameStyle( ULONGLONG qwGameStyle )
{
    m_qwGameStyle = qwGameStyle;
}

//-------------------------------------------------------------------------------------
// Name: GenRandSessionName()
// Desc: Set name of session to randomly generated value
//-------------------------------------------------------------------------------------
VOID SessionInfo::GenRandSessionName()
{
    XBRandName_GetRandomName( m_strSessionName, XATTRIB_SESSION_NAME_MAX_LEN );
}

//-------------------------------------------------------------------------------------
// Name: SetConfigInfo()
// Desc: Set "configuration" blob
//-------------------------------------------------------------------------------------
VOID SessionInfo::SetConfigInfo( const CBlob & Value )
{
    m_ConfigInfo = Value;
}
