//-----------------------------------------------------------------------------
// Name: Arbitration Xbox Sample
// 
// Copyright (c) 2002-2003 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
    Arbitration is an attempt to minimize online cheating. Xbox Live prevents
    many kinds of cheating by encrypting all packetts, and preventing modified
    code from running.
    Arbitration helps to deal with other types of cheating, such as pulling the
    network cable, blocking ports, and packet flooding.

    After creating a session and having at least one other player join the session,
    the session create can start an arbitrated session. This will trigger the
    other boxes to register for arbitration, and then the 'game' appears. The
    game menu looks like:
    
    player1 - 0 points
    player2 - 0 points

    Score Point
    Score Point (not sent)
    End Game

    From the game menu players can score points, which should show up on the
    other players' screens. They can also score points that are not broadcast,
    to intentionally cause results discrepancies.

    Eventually the game is ended (by any player) which triggers reporting of
    results by all players. If there are any discrepancies in the results
    then this will be reported. Note that the first box to submit results
    will never find any discrepancies, because there is no data to differ from.
    However subsequent boxes will report any discrepancies they find.

Required files and media
========================
    Copy the media tree to the target machine before running this sample.


Programming Notes
=================

    This sample is based on the MatchMaking sample, and most of the code that
    is not in ArbitrationManager.cpp is not related to arbitration.

    This sample demonstrates the normal flow of arbitration. Matchmaking is
    used to find a game, players join the game and wait in a lobby. At some
    point a decision is made to start an arbitrated game. The host tells
    the other players to register, and the host registers last.
    
    When the host registers they get the XUIDS of all of the players and
    send them to all of the players. Any players that don't register in time
    are disconnected (not currently demonstrated).
    
    Then a regular game is played. Points are scored, with the option to
    score points that aren't sent to the other players (simulating a
    dropped packet that the game doesn't handle well). Players can choose to
    quit the game, in which case they will suffer a drop penalty. Eventually
    somebody ends the game, and all players submit their results.
    
    Some things that are not demonstrated are host migration, robust network
    protocols, and other features that would complicate the sample without
    adding value related to demonstrating arbitration.
