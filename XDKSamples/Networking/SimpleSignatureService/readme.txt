//-----------------------------------------------------------------------------
// Name: SimpleSignatureService Xbox Sample
// 
// Copyright (c) 2002-2003 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The SimpleSignatureService sample demonstrates how to use the extra security
of the online signature service to validate content.
   

Xbox Live Signature Service 

To protect customers and optimize the online gaming experience, the Xbox Live
service seeks to maximize the security of content that is distributed through 
Xbox Live. Whether the data consists of bonus levels, game statistics and 
rankings, game replays, or even user-created content such as custom tracks, 
fields, or team emblems, a user who downloads data from Xbox Live should feel 
confident that the data is accurate, reliable, and non-malicious.

The Xbox Live Signature Service APIs allow you to get a signature for the 
digest of any piece of shared online content.  It is more secure than the 
signatures for harddrive content because it requires an Xbox Live key that
is only available to titles while they are connected to Xbox Live.  Signatures
can be expired or denied in the future based on time, title, Xbox, or users
in response to a discovered security issue or other policy.
