//-----------------------------------------------------------------------------
// Name: Volume Fog Xbox Sample
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The VoluemFog sample demonstrates a technique for rendering geometric fog 
volumes.  The technique is similar to the one used by the DirectX VolumeFog 
sample.  The basic idea is very simple, the depth value of the front faces of 
the fog volume are subtracted from the depth value of the back faces yielding
distance through the fog volume.  Some additional complexity is required in 
order to correctly fog objects inside the fog volume and to handle cases where 
the viewer is inside the fog volume.  First the scene is rendered normally.  
The fog volume is then done in five steps.  The first step is to render the fog 
volume into the stencil with front faces decrementing the stencil value and 
backfaces incrementing the stencil value and the z-test set to always.  The net 
result of this step is to set the stencil to one if the viewpoint is inside the 
fog volume.  The second step draws the visible front faces of the fog volume 
adding the depth values to the subtractive buffer and incrementing the stencil 
value.  The third step draws the visible back faces of the fog volume adding 
the depth values to the additive buffer and decrementing the stencil value.  
Any object in the fog volume can now be identified by having a stencil value 
not equal to zero.  The fourth step draws objects potentially in the fog volume 
with the stencil test set to not equal to zero and adding their depth values to 
the additive buffer.  The final step draws a full screen quad with a pixel 
shader that subtracts the value in the subtractive buffer from the value in the 
additive buffer in order to get distance through the fog.

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
	In order to have more than 8-bits of precision in the fog buffers a 12-bit
value is split up across the red, green, and blue channels of a 32-bit buffer.
The lower 4-bits of each channel are 4-bits of the fog value and the upper 
4-bits are reserved for carry (allowing up to 16 levels of overdraw).  The 
pixel shader combines the red, green, and blue channels using appropriate scale
values to get the final fog value.  In order to change the density of the fog
the scale factors in the pixel shader would have to be modified.
