//-----------------------------------------------------------------------------
// Name: Marketplace Xbox Sample
// 
// Copyright (c) 2003 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
    
    The Marketplace demo sample demonstrates proximity-based conversations for large
    numbers of users over speaker and headset.   It shows a method for dynamically 
    allocating bandwidth using simple peer prediction for a scalable mix of 
    peer-to-peer and player-server voice traffic.                   
                 
    Note that this is more of a demo than a sample, to demonstrate the concepts
    in the upcoming voice partitioning whitepaper.  Because of the flexibility in
    network architecture required (support for both peer-to-peer and full server-
    forward for demonstration purposes), there is not much plug-and-play code here.
    As well, because of the added complexity in making a visual demonstration 
    ( bots, sorted sprites, soundbit editor for the bots ), there is a lot of 
    code here which is not directly applicable to voice partitioning, and the
    full voice path code is spread throughout the sample.  ( Most of the partitioning
    code is in VoiceMgr.cpp, but some other important concepts, like 3d sound buffer
    caching are implemented in Audio.cpp )
                     
              
Required files and media
========================
    
    Doing a full build in Visual Studio will copy the media files to your xbox.        

Using the sample
================

    This sample requires a controller with communicator plugged in to port 0.

    After you log on, you will be presented with 4 options.  
        
        * You can host a game with bots 
                - bots simulate other players 
        * You can host a game without bots
                - play with your friends                
        * You can join an existing game
                - finds the best game based on QoS- no selection
        * You can edit bot voices
                - record some voice clips for the bots to use

    Main game
    ---------

    Left analog stick       -   move around
    X                       -   join private channel (through headset)
    A                       -   talk on private channel (hold to talk)
    Y                       -   host menu (if you are the host)
           The host menu lets you set:
                Server forward or peer-to-peer voice mode
                Network send rate
                Voice send rate
                Speech timeout
                Max number of people you can listen to (downstream # of peers)
                Max number of people you can send to (upstream # of peers)
    
    Select                  -  help menu
    Black button            -  show names
    White button            -  show network stats
    
    In the upper right corner of the marketplace is a podium.  If you stand 
    behind it, voice will go through the server out to everyone in the marketplace.
                        
                
    Key
    ---
    
    Overhead icons:
            V:      Players
            .)):    Talking player
            ((.)):  Player talking at the podium (voice routed through server)
            P))):   Player talking on the private channel
            
    Side icons:
            P:          Player is on the private channel
            computer:   Player is a bot             
    
    Icon colors:
            Yellow:     Local player
            Dark blue:  Remote player
            Light blue: Remote player I can currently hear 
                            (getting network packets from)
    
    Player colors:
            Light:      Players who I would hear me if I talked                
            Dark:       Players who wouldn't hear me if I talked
                           
    
    Bot voice editor
    ----------------
           
    A - play an existing sound bite
    X - hold to record a sound bite
    Y - save database
    B - cancel out
    
    The letters (A,B,C...H) represent the different bot voices
    You can have 9 samples per voice.
    

Sound around the marketplace
============================

    * There is a localized 3d source at the fountain
    * There is an ambient crowd sound that is scaled based on the total
        number of people talking.
    * If someone is talking but you can't hear them (because of network 
        allocation), you will hear a walla-walla-walla sound 
        eminating from their position    
    * If someone is talking and is sending traffic to you, you will
        hear their voice.    
    * When a new person enters the marketplace, you will hear a chime                  
    
Programming Notes
=================   
    
    A whitepaper will follow with more detail on how this sample works.
   
    The technical areas this sample covers are:
        
        * Speaker and headset output through XHV
        * Coalescing voice packets to save bandwidth
        * Proximity-based voice partitioning with low server overhead
        
    This sample does not comply with logon and other Live TCRs, because it is 
    demonstrating a way to do voice partitioning, not a full live game.
    
    Basic problem
    -------------
                    
    
        Partitioning voice is hard; not only do you have to limit how many other 
    players you are sending to, but how many other players are sending to them.  
    For instance, lets say you solely partitioned based on the n closest other 
    players.  If you have a player standing in the middle of a circle of people, 
    and several other players in the circle are talking, the person in the 
    center will have his bandwidth saturated with voice packets; since he is
    close to them all, he'll be on all of their receiving lists.  
    
           P    P!
       P!           P
           
      P       *       P!
      
       P!           P
           P    P!  
                     
        
        You could have the server calculate all of the player to player routes, 
    but not only does this cost in bandwidth and CPU cycles, but there will be a 
    significant delay once I start talking while the routes are recalculated.  
    If they are precalculated, I will be unable to hear someone who isn't in the
    closest 4 people to me, even if none of those closest 4 are talking.
    
    Presented solution
    ------------------
    
        The solution demonstrated in marketplace involves a single bit per player 
    being sent on the update packet from the server which specifies whether a given
    player is talking or not.  The player-to-player voice routes are completely
    calculated on each player based on the talking bits, so oversaturating a player
    can be avoided.  Even if the update comes a little late, oversaturating a player
    will not last more than a single network update (50 ms by default) which gives 
    adequate time to recover.
       
        A priority is determined for each proximal player-to-player pair, and then
    bandwidth is allocated round-robin, with a player being removed from the send
    or receive set as soon as its bandwidth quota is used up.  This quota could
    be scaled based on their QoS bandwidth numbers easily, so some players would
    automatically receive or send more data than others based on what their
    connection would allow.
    
        Priority is given to players near other players, players that aren't moving
    (and thus are likely to be in an established conversation), and players facing 
    other players.  Additional priority modifiers could be implemented, including
    higher priority for friends, etc.               
                               
    Summary of files
    ----------------
    
    Audio.cpp       - sound buffer cache management code, and xact and xhv interfaces
    Draw2D.cpp      - simple sprite drawing class
    Marketplace.cpp - core app class with network message pump and state machine
    Match.cpp       - autogenerated matchmaking query class (from matchsim)
    Object.cpp      - manages a single sprite with sound sources, collision, etc
    Player.cpp      - player and playerbot classes
    SoundbitDB.cpp  - holds forged voice packets for bots
    VoiceMgr.cpp    - voice packet routing and priority calculation
                    
    
Network stats notes
===================

    The network stats (accessed by pressing the white button) only shows the traffic
    used for voice.  If voice is piggybacked on a data packet, the 44 bytes for the
    packet header are not counted.  If the voice is sent by itself (as in peer-to-peer),
    the 44 bytes are counted.  
    
    In peer-to-peer mode, the host bandwidth shows only the stats for voice going 
    through the server;  there would be no players locally in a massively-multiplayer
    game, and the stats are to show the impact on server bandwidth for using this method 
    of voice.
        
