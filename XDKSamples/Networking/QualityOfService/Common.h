//-----------------------------------------------------------------------------
// File: Common.h
//
// Desc: QualityOfService global header
//
// Hist: 05.24.02 - New for June release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef QUALITYOFSERVICE_COMMON_H
#define QUALITYOFSERVICE_COMMON_H

#include "xtl.h"
#include "xonline.h"
#include "XBRandName.h"
#include "XBNetMsg.h"
#include "XBOnlineTask.h"

#pragma warning( disable: 4786 )
#include <vector>
#include <string>




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD MAX_SESSION_NAME    = 10;
const DWORD MAX_SAMPLES         = 16;          // Max samples/probe
const DWORD LISTEN_BANDWIDTH_MAX  = 64 * 1000; // Max QoS listen bandwidth (bps)
const DWORD LOOKUP_BANDWIDTH_MAX  = 64 * 1000; // Max QoS lookup bandwidth (bps)

enum
{
    // Mode menu
    MENU_SYSLINK = 0,
    MENU_ONLINE,
    MENU_MODE_MAX,

    // Listen params menu
    MENU_START_LOOKUPS = 0,
    MENU_SET_LISTEN_STATE,
    MENU_NUM_SAMPLES,
    MENU_LISTEN_BANDWIDTH,
    MENU_LOOKUP_BANDWIDTH,
    MENU_LISTEN_PARAMS_MAX,

};




//-----------------------------------------------------------------------------
// Name: class SessionList
// Desc: List of sessions to be queried for QoS
//-----------------------------------------------------------------------------
class SessionList
{
    struct SessionData
    {
        XNADDR xnAddr;
        XNKID  xnKid;
        XNKEY  xnKey;
    };

    // List of sessions to probe
    std::vector< SessionData > m_SessionList;

    // The address of each critical item is maintained in separate vectors to match
    // the expected parameters of XNetQosLookup
    std::vector< const XNADDR* > m_xnAddrList;
    std::vector< const XNKID* >  m_xnKidList;
    std::vector< const XNKEY* >  m_xnKeyList;

public:

    SessionList();

    bool empty() const;
    DWORD size() const;
    void push_back( const XNADDR&, const XNKID&, const XNKEY& );
    void clear();

    // Direct access to internal data for XNetQosLookup
    const XNADDR** GetXnAddrs();
    const XNKID** GetXnKids();
    const XNKEY** GetXnKeys();

    XNADDR GetXnAddr( DWORD ) const;

private:

    // Disabled
    SessionList( const SessionList& );
    SessionList& operator=( const SessionList& );

};




#endif // QUALITYOFSERVICE_COMMON_H
