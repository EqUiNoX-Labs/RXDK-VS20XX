//-----------------------------------------------------------------------------
// File: Common.h
//
// Desc: SimpleContentDownload global header
//
// Hist: 11.10.01 - New for Nov release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef SIMPLECONTENTDOWNLOAD_COMMON_H
#define SIMPLECONTENTDOWNLOAD_COMMON_H

#include "xtl.h"
#include "xonline.h"
#include <string>
#include <vector>




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
enum
{
    // Confirm menu
    CONFIRM_YES = 0,
    CONFIRM_NO,
    CONFIRM_MAX
};




//-----------------------------------------------------------------------------
// Name: class ContentInfo
// Desc: Content information from the online catalog; simplifies access to
//       XONLINECATALOG data.
//-----------------------------------------------------------------------------
class ContentInfo
{
    DWORD                     m_dwPackageSize;
    DWORD                     m_dwInstallSize;
    DWORD                     m_dwRating;
    DWORD                     m_dwOfferingType;
    DWORD                     m_dwBitFlags;   
    FILETIME                  m_ftCreationDate;
    XOFFERING_ID        m_ID;
    // RXDK: was basic_string<BYTE> (libc++ has no char_traits<unsigned char>)
    std::vector< BYTE > m_Data;             // Title-specific data

public:

    ContentInfo()
    :   
        m_dwPackageSize ( 0 ),
        m_dwInstallSize ( 0 ),
        m_dwRating( 0 ),
        m_dwOfferingType( 0 ),
        m_dwBitFlags( 0 ),
        m_ftCreationDate( FILETIME() ),
        m_ID            ( XOFFERING_ID(0) ),
        m_Data          ()
    {
    }

    explicit ContentInfo( const XONLINEOFFERING_INFO& xOnInfo )
    :   
        m_dwPackageSize ( xOnInfo.dwPackageSize ),
        m_dwInstallSize ( xOnInfo.dwInstallSize ),
        m_dwOfferingType( xOnInfo.dwOfferingType ),
        m_dwBitFlags    ( xOnInfo.dwBitFlags ),
        m_dwRating      ( xOnInfo.dwRating ),
        m_ftCreationDate( xOnInfo.ftActivationDate ),
        m_ID            ( xOnInfo.OfferingId ),
        m_Data          ( xOnInfo.pbTitleSpecificData,
                          xOnInfo.pbTitleSpecificData + xOnInfo.dwTitleSpecificData )
    {
    }

    DWORD GetPackageSize() const     { return m_dwPackageSize; }
    DWORD GetInstallSize() const     { return m_dwInstallSize; }
    DWORD GetOfferingType() const    { return m_dwOfferingType; }
    DWORD GetBitFlags() const        { return m_dwBitFlags; }
    DWORD GetRating() const          { return m_dwRating; }
    FILETIME GetCreationDate() const { return m_ftCreationDate; }
    XOFFERING_ID GetId() const { return m_ID; }

};




//-----------------------------------------------------------------------------
// Typedefs
//-----------------------------------------------------------------------------
typedef std::vector< ContentInfo > ContentList;




#endif // SIMPLECONTENTDOWNLOAD_COMMON_H