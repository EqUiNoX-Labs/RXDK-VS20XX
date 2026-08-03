//-----------------------------------------------------------------------------
// Name: Elo Ranking Statistics Sample
// 
// Copyright (c) 2002-2003 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The Elo Sample demonstrates the Elo Ranking system. The ELO service is
   a way for a title to rate and rank players in a meaningful way.
   Elo scoring is accurate at reflecting true skill than other rating systems.
   By using this service your game's ranking is less prone to exploits.
   Since the Elo service is an extension of the Arbitration service cheating
   exploits are avoided.


Title Notes
===========
   This sample uses a "Leaderboard" posted to PartnerNet. A new leaderboard
   will have to be created for your title ID. Leaderboards are created and
   submitted to PartnerNet using the Xbox Live Authoring and Submission Tool
   (aka XLAST). If you have questions about the Leaderboard creation process
   please contact your Xbox account manager.


Programming Notes
=================

   This sample is structured into three implementation files:

   match.cpp     - Used to find and join a match a host already exists.
                   Handles network messaging.
 
         
   GameMsg.cpp   - Handles network blobs (aka messages) and hooks into
                   callbacks provided by the Elo sample class.

                  
   EloDemo.cpp   - Contains the main logic to demonstrate Elo Rankings.


Running the Sample
==================

   Two Xboxes and a personal computer with the Matchmaking Simulation tool
   installed.
   
   First open the provided MatchMaking simulation file (MatchMaking.xms) with the
   MatchMaking Simulation Tool.
   
   Copy the Elo Sample XBE to each Xbox.
   
   Using the Xbox Neighborhood tool, set one Xbox as the default box. Then using the MatchMaking
   Simulation Tool, choose TOOLS->CONNECT XBOX to route the Xbox's QuickSearch through your computer.
   
   Repeat the previous step for your second Xbox.
   
   Using the MatchMaking Simulator tool select MATCH->START SERVER to start the MatchMaking simulation.
   
   Start the Elo sample on both Xboxes and log into the Xbox live service using different accounts.
   
   You may now create a match using one Xbox and join the match with the other (using QuickMatch)
   
   

