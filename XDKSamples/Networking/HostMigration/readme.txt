//-----------------------------------------------------------------------------
// Name: HostMigration Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The HostMigration sample demonstrates a simple way a title can migrate
the host of a game session when the original host leaves.  A single 
ID is used as a place holder for game state data which is maintained
when reconnecting to the new host as part of host migration.       
  
  
Required files and media
========================
   Copy the media tree to the target machine before running this sample.

Programming Notes
=================

   This implementation adds 3 new messages and 2 states to a basic networking 
application.  The functions to look at are:

FrameMoveFindNewHost          - the 'meat' of host migration
FrameMoveResumeGame           - resumes the game as the new host 
                                ( mirrors the StartGame state )
SendConnectToMigratedHost     - closely mirrors the standard 'JoinGame' message
SendMigrateHostApproved       - closely mirrors the standard 'JoinApproved' message
SendPlayerRejoin              - closely mirrors the standard 'PlayerJoined' message
ProcessConnectToMigratedHost
ProcessMigrateHostApproved
ProcessPlayerRejoin

   Since reconnecting to a new host is closely related to connecting to a new 
game, the messages and state transitions are very similar to those for initial
connection; the main difference is that the player game state is maintained
rather than initialized.  

In this sample, rather than the new host keeping track of all of the connections
it is waiting for, the client resubmits its state info upon connection to the 
new host.

The algorithm used for host migration is as follows:
  
Each Xbox has a matching list of current players.  The player in the first 
element of the list is the host.

< Step 1 is in ProcessDropouts >
< Steps 2-7 are implemented in FrameMoveFindNewHost >
< Step 8 is in ProcessMigrateHostApproved >

(1) If the host disconnects, remove them from the player list, and try
    to connect to the new first player.
(2) If I'm the first player, start hosting.
(3) If I'm not the first player:
   
   (4) Start a timeout timer  (2 seconds in this case) 
   (5) Start a response delay timer       (0.3 seconds in this case)
   (6) Try to connect to who I think is host
   (7) If the response delay timer has triggered, go to (5)
       If I recieve a reply, go to (8)
       If my timeout timer has triggered, remove the player from the player list
           and go to step (2)

(8) Connection was accepted, start the rejoin handshaking
   (If for some reason the new host IS a host, but is full, for this sample
   a JoinDenied message is sent so it will notify the player the game is full)   