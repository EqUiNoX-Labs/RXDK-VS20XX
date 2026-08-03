//-----------------------------------------------------------------------------
// Name: DynamicGamma Sample
// 
// Copyright (c) 2003 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The Dynamic Gamma sample updates the Xbox gamma ramp based on luminance
   data derived from the frame buffer.
   
   The gamma ramp adjustments can help compensate for games where a scene
   is too dark due to dynamic lighting modulation, dark textures, etc.
   
   The effect can also be used during game play. 
   * Looking at the sun can cause temporary blindness.
   * When someone moves from a bright outdoors scene to a dark indoors
     scene they may have difficulty adjusting to the darkness for a set
     period.
   * Players with night vision may see better in darkness but have
     difficulty adjusting to bright scenes.
   
   The sample allows the user to control numerous effect parameters.
   
   Bias:               Gamma ramp bias
   DarkClamp:          Maximum gamma ramp adjustment for dark scene
					   compensation
   LightClamp:         Maximum gamma ramp adjustment for bright scene
                       compensation
   Dark Adjust Dt:     Gamma ramp darkness adjustment with respect to time
   Light Adjust Dt:    Gamma ramp brightness adjustment with respect to time
   Contrast Adjust Dt: Gamma ramp contrast adjustment with respect to time
   Dark Threshold:	   Area underneath the dark side of the normalized
                       histogram where luminance values are considered 0.
                       Used to filter out dark outliners.
   Light Threshold:	   Area underneath the bright side of the normalized
                       histogram where luminance values are considered 0.
                       Used to filter out bright outliners.
   
   The sample also applies an inverse gamma operation to a HUD element in a
   pixel shader to maintain a linear gamma when it is drawn.  Note that in 
   the sample, the inverse gamma operation is only applied to the single HUD
   element in the lower center of the screen.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   A downsampled version of the frame buffer is rendered using a luminance
   transform implemented in a pixel shader.  A luminance histogram is generated
   from the downsampled luminance texture and used to update the Xbox gamma
   ramp. 
   
   It is often desirable to maintain a constant gamma for HUD elements.  The
   sample applies an inverse gamma adjustment in a pixel shader when rendering
   HUD elements.  It is worth noting that the luminance texture is generated
   from the frame buffer before HUD elements are drawn.
   
   The processing and memory requirements of the effect are minimal.  The
   downsampled luminance texture is small and only needs a single 8 bit channel.
   Also, the effect need not be updated every frame further reducing its cost. 
   
   



