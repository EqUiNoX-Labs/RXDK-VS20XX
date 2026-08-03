//-----------------------------------------------------------------------------
// Name: HighDynamicRange Xbox Sample
// 
// Copyright (c) 2002 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
The HighDynamicRange sample demonstrates a technique for simulating high
dynamic range.  A dimmed and blurred version of the scene is added to the
original scene to simulate saturation and blooming.

Required files and media
========================
Copy the media tree to the target machine before running this sample.

Programming Notes
=================
The original scene is first dimmed and down sampled.  The dimmed version is
then blurred in two passes using a four tap vertical Gaussian filter followed
by a four tap horizontal Gaussian filter.  The blurring is required to
smooth out the blooming.
  
This technique is suited for integration into an existing engine since it
is a post processing technique and adds a constant amount of GPU overhead.

This sample also shows how to add motion blur to the blooming light sources.
A drastically darkened final bloomed image is added to the slightly darkened
previous frame's bloomed image.  The darkening constant for the current frame 
controls how bright the blooms get when accumulating them and the darkening 
constant for the previous frame controls how long the "tails" or "streaks" 
are for the motion blur.

There is one small issue with the motion blur.  Since it accumulates the
previous frames and slowly darkens them, you would expect the accumulation to
eventually return to zero.  Unfortunately, this is not the case because we
only have 8 bits per color.  So, looking at the 0.95 darkening case, over many
iterations the color values approach zero but when they reach a floating
point value of 0.07 there isn't enough precision left to get any smaller.
0.95 * 0.07 = 0.0665.  Since our pecision is limited to 0.004 (1/255), this
number is actually 0.07!  Thus 0.95 * 0.07 = 0.07!  You could get back to zero
with another pass but you can also just clear the accumulation buffer to the
lowest value we can get back down to.  This just brightens the image slightly 
when using motion blur.

Note also that whenever we change the blur value or the draw mode, we must
clear the accumulation buffer to remove any of the previous accumulations.  
This causes the image to "flash" when setting the parameters.  Since these
changes would be fixed at certain values in a real game, this isn't an issue.

