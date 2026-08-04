//-----------------------------------------------------------------------------
// File: Common.h
//
// Desc: ContentDownload global header
//
// Hist: 09.07.01 - New for Nov release
//       04.05.02 - Updated for May release; Added billable content and content
//                                           details.  Updated for the new
//                                           HD/DVD content enumeration API
//       06.05.02 - Updated for June release; Updated billing stuctures
//                                            Added removal of "bad" content
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef CONTENTDOWNLOAD_COMMON_H
#define CONTENTDOWNLOAD_COMMON_H

#include "xtl.h"
#include "xonline.h"
#include <string>
#include <vector>
#include <assert.h>



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
// Desc: Content information from the online functions 
//-----------------------------------------------------------------------------
class ContentInfo
{
    XOFFERING_ID                m_ID;
    DWORD                       m_dwRating;
    DWORD                       m_dwOfferingType;
    DWORD                       m_dwBitFlags;
    WORD                        m_wOfferingFlags;
    
    DWORD                       m_dwPackageSize;   // package size (bytes)
    DWORD                       m_dwInstallSize;   // size of package on disk
                                                   //   (blocks)
    FILETIME                    m_ftActivationDate;// When this content was
                                                   // activated
    FILETIME                    m_ftDownloadDate;  // When this content was
                                                   // downloaded
    // RXDK: was basic_string<BYTE> (libc++ has no char_traits<unsigned char>)
    std::vector< BYTE >         m_EnumBlob;        // Title-specific enum data
    std::vector< BYTE >         m_DetailsBlob;     // Title-specific details data
    DWORD                       m_dwNumInstances;  // Number of instances already owned
    XONLINE_PRICE               m_Price;
    DWORD                       m_dwFreeMonths;     // free months before charge
    XONLINE_OFFERING_FREQUENCY  m_Frequency;        // how often charges are made
    DWORD                       m_dwDuration;       // duration of recuring charge
    std::basic_string< CHAR >   m_ContentDirectory; // directory content was
                                                    // installed to
    std::basic_string< WCHAR >  m_DisplayName;      // content display name


public:

    ContentInfo()
    :   m_ID( XOFFERING_ID(0) ),
        m_dwRating( 0 ),
        m_dwOfferingType( 0 ),
        m_dwBitFlags( 0 ),
        m_wOfferingFlags( 0 ),
        m_dwPackageSize ( 0 ),
        m_dwInstallSize ( 0 ),
        m_ftActivationDate(),
        m_ftDownloadDate(),
        m_EnumBlob(),   
        m_DetailsBlob(),
        m_dwNumInstances( 0 ),
        m_Price ( XONLINE_PRICE() ),
        m_dwFreeMonths( 0 ),
        m_Frequency (XONLINE_OFFERING_FREQUENCY() ),
        m_dwDuration( 0 ),
        m_ContentDirectory(),
        m_DisplayName()
    {
    }

    VOID InitFromContentFindData( const XCONTENT_FIND_DATA& xContentFindData)
    {
        m_ID                = xContentFindData.qwOfferingId;
        m_dwBitFlags        = xContentFindData.dwFlags;
        m_ftDownloadDate    = xContentFindData.wfd.ftCreationTime;
        m_dwInstallSize     = XGetDisplayBlocks( xContentFindData.szContentDirectory );
        m_ContentDirectory.assign(xContentFindData.szContentDirectory, strlen(xContentFindData.szContentDirectory) + 1);
        m_DisplayName.assign(xContentFindData.szDisplayName, wcslen(xContentFindData.szDisplayName) + 1);
    }

 
    VOID InitFromEnumInfo( const XONLINEOFFERING_INFO& xOnInfo )
    {
        m_dwPackageSize    = xOnInfo.dwPackageSize;
        m_dwInstallSize    = xOnInfo.dwInstallSize; 
        m_dwOfferingType   = xOnInfo.dwOfferingType;
        m_dwBitFlags       = xOnInfo.dwBitFlags;
        m_wOfferingFlags   = xOnInfo.fOfferingFlags;
        m_dwRating         = xOnInfo.dwRating; 
        m_ftActivationDate = xOnInfo.ftActivationDate; 
        m_ID               = xOnInfo.OfferingId; 
        m_EnumBlob.assign( xOnInfo.pbTitleSpecificData, xOnInfo.pbTitleSpecificData + xOnInfo.dwTitleSpecificData);
    }

    VOID InitFromDetails( const XONLINEOFFERING_DETAILS& xOnDetails)
    {
        m_dwNumInstances = xOnDetails.dwInstances;
        m_Price          = xOnDetails.Price;
        m_dwFreeMonths   = xOnDetails.dwFreeMonthsBeforeCharge;
        m_dwDuration     = xOnDetails.dwDuration;
        m_Frequency      = xOnDetails.Frequency;
        m_DetailsBlob.assign( xOnDetails.pbDetailsBuffer, xOnDetails.pbDetailsBuffer + xOnDetails.dwDetailsBuffer);
    }
    

    DWORD GetPackageSize() const       { return m_dwPackageSize; }
    DWORD GetInstallSize() const       { return m_dwInstallSize; }
    DWORD GetOfferingType() const      { return m_dwOfferingType; }
    DWORD GetBitFlags() const          { return m_dwBitFlags; }
    WORD GetOfferingFlags() const      { return m_wOfferingFlags;}

    DWORD GetRating() const            { return m_dwRating; }
    FILETIME GetActivationDate() const { return m_ftActivationDate; }
    FILETIME GetDownloadDate() const   { return m_ftDownloadDate; }
    XOFFERING_ID GetId() const         { return m_ID; }
    const BYTE* GetEnumBlob() const    { return &(*m_EnumBlob.begin());}
    DWORD GetEnumBlobSize() const      { return m_EnumBlob.size();}

    DWORD GetNumInstances() const               { return m_dwNumInstances; }
    XONLINE_PRICE& GetPrice()                   { return m_Price; }
    DWORD GetFreeMonths() const                 { return m_dwFreeMonths; }
    DWORD GetDuration() const                   { return m_dwDuration; }
    const XONLINE_OFFERING_FREQUENCY& GetFrequency()  { return m_Frequency; }
    const BYTE* GetDetailsBlob() const          { return &m_DetailsBlob[0];}
    DWORD GetDetailsBlobSize() const            { return m_DetailsBlob.size();}

    const CHAR* GetContentDirectory() { return &(*m_ContentDirectory.begin()); }
    const WCHAR* GetDisplayName()     { return &(*m_DisplayName.begin()); }
};


//-----------------------------------------------------------------------------
// Typedefs
//-----------------------------------------------------------------------------
typedef std::vector< ContentInfo > ContentList;




#endif // CONTENTDOWNLOAD_COMMON_H
