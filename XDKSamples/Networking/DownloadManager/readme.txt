//-----------------------------------------------------------------------------
// Name: DownloadManager Xbox Sample
// 
// Copyright (c) 2002 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The DownloadManager sample demonstrates how a title can save and restore
logon state after launching another image.  For demonstration purposes, the
Xbox content downloader is launched after allowing multiple users to sign on.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.
   Be sure to rename the DownloadManager.xbe to Default.xbe so that
   it will be launched when the Xbox Downloader is exited.  Also, 
   Downloader.xbe must be stamped with the title id of the sample:
   (xbecert /TESTID:0xffff010C Downloader.xbe) and copied into the
   sample location as DownloadManager sample.


Programming Notes
=================
    A title saves the current logon state using the XOnlineSaveLogonState
    function.  This saves the current logon state into a XONLINE_LOGON_STATE
    structure.  To restore the saved logon state the XOnlineRetrieveLogonState
    function is used.  When the sample launches the Xbox downloader using
    XLaunchNewImage, it passes the current logon state as launch data.  When
    the downloader exits, it uses XLaunchNewImage to load default.xbe and
    passes that launch data back to it.  When the sample starts it checks for
    launch data, if it is determined to be saved logon state, it will attempt
    to restore the users from that logon state. 

    See CXBoxSample::Initialize in DownloadManager.cpp for details on how
    to check for launch data.  See RestoreUsersFromLogonState() for details
    on how the users are restored (take note of the additional checks being
    made for unplugges controllers, etc.)  See CXBoxSample::UpdateStateMainMenu()
    for details on how the current logon state is obtained and passed to the
    the Xbox downloader.


     
