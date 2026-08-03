//-----------------------------------------------------------------------------
// Name: XMVCompare Xbox Sample
// 
// Copyright (c) 2003 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The XMVCompare sample compares two XMV videos side-by-side for visual
   quality differences.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Usage
=================

   Perform two different encodings of a video that you want to compare the
   visual quality of encoder's settings (encode video only with no audio using
   -a_setting 0). Copy the files to media\videos directory.
   
   The sample will enumerate videos from media\videos directory. Cycle through
   the enumerated list with the A and B buttons. A button will select the video
   on the left pane, and B button will select the video on the right pane. 
     
   Press the X button to start both videos. Pause playback with the Start
   button then use the left thumb stick to move the splitter left-right. If
   the videos get out of sync, click on the left or right thumb sticks to
   advance the left or right video one video frame, respectively.
   
   Left or right triggers will display left or right pane  "full screen". The
   splitter actually moves to far left or right. Use the left thumbstick to
   scroll it back to viewable position.
   
   Other options:
   
   Black and white buttons - select and change display modes supported with the
      current AV pack
   Back button - displays help screen.
   Start button - pause both videos
   Dpad left/right - changes splitter color
   Dpad up/down - changes splitter scroll rate
   Y button - toggles on-screen text display
          
   
   The example videos have the following encodings:
   
   VideoA: -v_mode 1 -v_quality 100 -v_bitrate 2000000 -v_buffer 5000
   VideoB: -v_mode 1 -v_quality 50 -v_bitrate 500000 -v_buffer 5000
   
   
   