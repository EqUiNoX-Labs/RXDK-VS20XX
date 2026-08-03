//-----------------------------------------------------------------------------
// File: Common.h
//
// Desc: DownloadManager global header
//
// Hist: 10.16.02 - New for Nov release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef DOWNLOAD_COMMON_H
#define DOWNLOAD_COMMON_H

#include "xtl.h"
#include "xonline.h"
#include <vector>


//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

const DWORD MAX_ACCOUNTS_DISPLAYED = 3;

//-----------------------------------------------------------------------------
// Typedefs
//-----------------------------------------------------------------------------
typedef std::vector< XONLINE_SERVICE_INFO > ServiceInfoList;




#endif // DOWNLOAD_COMMON_H
