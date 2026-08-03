//-----------------------------------------------------------------------------
// Name: Stats Xbox Sample
// 
// Copyright (c) 2002 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   This sample demonstrates the XOnlineStat APIs. The sample models a
   ficticious first person shooter.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================

This sample demonstrates the XOnlineStat APIs. The sample models a fictitious
game that maintains a leaderboard for each of the four levels supported by the 
game.  Each leaderboard, or scoreboard, tracks the following player stats:

    Kills       The number of other players killed by a player
    Deaths      The number of times a player had died
    Assists     The number of assist this player provided
    Started     The number of times the player had started a level
    Completed   The number of times the player has completed a level
    Rating      A player rating (larger is better) used by the
                stats service to rank users
    Rank        The player rank (computed by the stats service) for a
                leaderboard

When a "game" is completed, the stats for a particular player is written to the
leaderboard associated with the game level.  If there are existing statistics
for that player on the leaderboard, the new are added to the existing values.
In addition, an overall leaderboard is maintained so that the all-time number
 of kills, etc. can be recorded.  

When writing stats, a "rating" (XONLINE_STAT_RATING) is computed by the game
title.  For demonstration purposes, the sample (in CXBoxSample::CalculateRating)
computes the rating as:  100*kills + 10*assist -5*deaths.   It’s up to the title
to decide how the rating is computed, but the bigger the number, the better the
rating.    Using this rating, the Stats service will rank each player on a
leaderboard.

Note: In addition to the currently signed on user, other "players" are selected
from existing user accounts stored locally on the machine as well as any 
Memory Units which have been inserted.   These additional players are also
signed on as the XOnline Stat APIs require that they be signed on before
their stats can be updated.

