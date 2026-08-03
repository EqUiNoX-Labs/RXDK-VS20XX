//-----------------------------------------------------------------------------
// Name: MatchMaking
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   This sample illustrates online matchmaking on Xbox. Code shows how
   to handle both QuickMatch and CustomMatch methods of findind games.
   Shows how to use the MatchMaking API, using session attributes. 
   Shows how to get search results, add and remove players from a session.

   Once a session has been created or found, enters game state and
   shows how players connect using the secure Xbox network state and
   how they communicate using standard Winsock calls.

   With Live 3.0, expected in early 2004, queries can return sessions
   grouped by an integer attribute.  When grouped by a attribute, the
   Average, Sum, Minimum, Maximum, or Count of that attribute may be returned.
   Any titles which ship before Live 3.0 will fail certification if these
   features are used.
   


Programming Notes
=================
  The MatchSim tool is used to create queries which are then submitted
  to XBox Developer Support.  The MatchMaking.xms file was used to generate
  the query used by the MatchMaking sample.
