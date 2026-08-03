//-----------------------------------------------------------------------------
// Name: Sample Game
// 
// Copyright (c) 2001 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

Description
===========
    A psuedo game that meets all technical certification requirements.
    Key requirements include:

    * No access of ROMs, SMC, USB, IDE or other off-limits hardware
    * No change of global settings
    * Displays local time-zone adjusted current time
    * Memory unit requirements, including read/write speed, roaming,
      persistence, capacity issues, and dynamic updates
    * Gamer-friendly save game names and default names
    * Game directory limit
    * Title ID, publisher data and game representative image in XE file
    * Game stability
    * Saving to hard drive
    * Rebooting into the Dash for save game management
    * Storage space display requirements
    * Menu buttons and controller navigation
    * In-game pause and resume requirements
    * Controller selection and discovery
    * Loss of controller
    * Xbox naming standards and terminology
    * Display of unsupported characters
    * Attract mode cycling, stability and interrupt

    The game is not designed to meet the more subjective non-technical
    gameplay requirements. In other words, the game is not necessarily fun!

Required files and media
========================
   Copy the media tree to the target machine before running this sample.

How to play the game
====================
    The game menus follow standard convention for navigation.

    From within the game, the controller works as follows:

        Right joystick:     Controls the buggy
        Back button:        Exits the game and allows game saves
        Start button:       Pauses and resumes the game

Demo game
========================
   To build the demo version of the game, load the TechCertDemo project. This
   project #defines XDEMO, which enables and disables certain portions of the
   games to meet demo Technical Certification Requirements. This project also
   uses the /DONTMODIFYHD ImageBld flag.
   
   To test the demo version with CDX:
   
        1) Copy the CDX.XBE and CDXMEDIA folder to the Xbox development kit.
           CDX is located in the REDIST directory installed with the Xbox SDK.
           Don't forget to include the ArialUni.ttf font file in the CDXMEDIA
           folder.
        2) Build and copy TechCertDemo.XBE and media folder to the same 
           location as CDX.XBE.
        3) Copy the CDX.INX file to the CDXMEDIA folder.
