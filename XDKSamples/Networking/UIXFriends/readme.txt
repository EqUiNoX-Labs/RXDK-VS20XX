//-----------------------------------------------------------------------------
// Name: UIXFriends Xbox Sample
// 
// Copyright (c) 2002-2003 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The UIXFriends sample shows how to implement Live friends using the
   UIX library to handle most of the work.
   The UIXFriends sample also demonstrates how to create a "Live Aware" title
   that supports automatically logging in the user and receiving game invites.
   The Live Aware version of this sample can be created by defining LIVE_AWARE
   in UIXFriends.h.


Required files and media
========================
   Run copytoxb.bat to copy the required fonts and skin files to the Xbox prior
   to running this sample.


Programming Notes
=================

   UIXFriends is designed to be a simple demonstration of how to implement Xbox
   Live friends using the UIX engine. Requesting that somebody be your friend is
   not supported because this is a feature that is trivial when you have finished
   a game, but difficult or artifical otherwise. Thus, this sample demonstrates
   how to manage your friends list, including sending and receiving invites,
   and receiving friend requests.
   
   This sample also demonstrates proper handling of exit codes, including asynchronous
   exit codes that come from network failures or duplicate logons.
