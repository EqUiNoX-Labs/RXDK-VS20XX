//-----------------------------------------------------------------------------
// Name: Lensflare Xbox Sample
// 
// Copyright (c) 2002 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
    The Lensflare sample shows how to use Xbox specific features to create a
    convincing effect.  The Xbox's visibility processing and GPU texture copying
    are both used.


Required files and media
========================
    Copy the media tree to the target machine before running this sample.


Programming Notes
=================
    The sample first renders a light sprite into the back buffer.  It then
    uses the visibility of the light sprite as well as a "stretched" version
    of the back buffer alpha to achieve the light "flare out" and element
    rendering effects. The algorithm for drawing the lens flare is explained in
    detail below.



    Given a light sprite (alpha texture and color), a number of flare element
    sprites (an alpha texture, distance from light, and color per element), the
    light direction, and the eye vector, 


    1) clear the back buffer alpha at the light source (render the light sprite
       with the z buffer and color write disabled, and alpha write enabled)
        * back buffer alpha = 0

    2) render the light sprite (z buffer enabled) to determine visibility and
       fill in the back buffer alpha
        * back buffer alpha =
               { light texture alpha where the light is visible (passes z test) 
               { 0 where the light is not visible (fails z test)
        * visibility ratio  = number of visible pixels / max visible pixels

    3) copy the portion of the back buffer rendered to by the light sprite to a
       texture
        * transform the light sprite quad into screen space to find the
          source rectangle
        * be sure to account for sampling artifacts and clipping of the sprite

    4) render the light "flare out"
        * enabling alpha blending
        * use the back buffer copy for the light sprite texture
        * render the light sprite n times while increasing the sprite's size on
          each pass
        * light intensity = dotproduct(eye vector, light direction)
        * sprite rgb      = f( light intensity, light color.rgb )
        * sprite alpha    = f( light intensity, light color.a, back buffer alpha, n )

    5) render the flare element sprites along the element vector
        * element vector  = ( screen space light position - screen space light
                              center (usually screen center) )
        * distance ratio  = length( element vector ) / max distance
        * position        = element vector * element distance from light
        * scale           = f( distance ratio )
        * sprite rgb      = element color.rgb
        * sprite alpha    = f( distance ratio, visibly ratio, element color.a,
                               light intensity )



    The sample encapsulates this algorithm in the CLensFlare class.  A struct
    defining the light and an array of structs defining each flare element is
    used to create an instance of the class.  After the parameters have been
    filled in, the sample simply calls CLensFlare::Render() each frame. The
    creation parameters can be tuned to achieve a wide variety of lens flares.

    The sample implements the following optimizations:

    * the effect is culled if it is outside of the view frustum
    * if the visibility ratio is 0, the effect is occluded
    * IDirect3DDevice8::CopyRects is used for the back buffer copy
    * alpha textures are used for the light and elements
    * the back buffer is not copied if the light is fully visible

    Further optimizations for an "in-game" implementation are possible. The sample
    stalls while it waits for the completion of the first rendering of the light
    sprite (it must wait for the sprite to be rendered to copy the back buffer and
    get the results of the visibility test).  An in-game implementation could render
    other objects during this period to avoid the stall (as long as they could not 
    occlude the light or render into the alpha channel of the color buffer).  The
    light source could also be moved further overhead to avoid rendering the effect
    with other "expensive" objects near the ground plane.  Finally, the code could
    be re-written for speed, not clarity.

