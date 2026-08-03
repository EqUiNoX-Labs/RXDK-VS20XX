//-----------------------------------------------------------------------------
// Name: ContentDownload
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
    This sample illustrates Xbox Online content enumeration for the hard
    disk, and online servers. The sample demonstrates content download,
    online content details, content purchase, subscription purchase, 
    subscription cancellation, installation, verification, HD content metadata
    and content removal.

Required files and media
========================
    Copy the media tree to the target machine before running this sample.

Programming Notes
=================
    * If installation is cancelled, it is up to the title to handle removing 
      the content that was installed. This sample shows how to properly handle
      installation cancellation.
    * The sample assumes that all subscriptions are only needed once, and 
      attempts to cancel them if their instance count is above 0.
    * Once a piece of content has been purchased, it can be downloaded multiple
      times without being re-purchased (even if it has been deleted).  Note
      that content ownership is Xbox specific while subscriptions are user
      specific.  This means that if a user moves to a different Xbox, his
      subscriptions will be valid but any content must be re-purchased 
      for the other Xbox.

   


   

    