//-----------------------------------------------------------------------------
// Name: SimpleConnect Xbox Sample
// 
// Copyright (c) 2002-2003 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The SimpleConnect sample demonstrates how to form a security exchange manually rather than on the first sent packet.  This allows much better control over the delays NAT traversal and security exchange introduce.  It also demonstrates how to attempt to recover when a security exchange is lost.
   
   The first instance of simpleconnect you run will create a 'host' and loop indefinately.  The second instance is the one you will want to run with a debugger or xbwatson to see the output messages.


Programming Notes
=================

Full technical information on these APIs is available in the SDK help under XNetConnect