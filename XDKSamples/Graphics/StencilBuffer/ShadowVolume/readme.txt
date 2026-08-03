//-----------------------------------------------------------------------------
// Name: ShadowVolume Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The ShadowVolume sample uses stencil buffers to implement real-time shadows.
   In the sample, a complex object is rendered and used as a shadow-caster, to
   cast real-time shadows on itself and on the terrain below.

   Stencil buffers are a depth buffer technique that can be updated as
   geometry is rendered, and used again as a mask for drawing more geometry.
   Common effects include mirrors, shadows (an advanced technique), dissolves,
   etc..


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   Real-time shadows is a fairly advanced technique. Each frame, or as the
   geometry or lights in the scene are moves, an object called a shadow volume
   is computed. A shadow volume is an actual 3D object which is the silhouette
   of the shadowcasting object, as protruded away from the light source. The 
   shadow volumes are closed and are drawn using the "zfail" method and depth 
   clamping so that they are fully robust (the view may be inside the shadow
   volume and the shadow volume may be clipped without any artifacts).

   In this sample, the 3D object which casts shadows is a bi-plane. Each frame,
   the silhouette of the plane is computed (using an edge detection algorithm,
   in which silhouette edges are found because the normals of adjacent polygons
   will have opposing normals with respect to the light vector). The resulting
   edge list (the silhouette) is protruded into a 3D object away from the light
   source. The faces frontfacing to the light are used to cap one end of the
   volume and the faces backfacing to the light are used to cap the other end.
   This 3D object is known as the shadow volume, as every point inside the 
   volume is inside a shadow.
   
   Next, the shadow volume is rendering into the stencil buffer twice. First,
   only forward-facing polygons are rendering, and the stencil buffer values 
   are decremented each time. Then the back-facing polygons of the shadow
   volume are drawn, incrementing values in the stencil buffer. Normally, all
   incremented and decremented values would cancel each other out. However,
   because the scene was already rendered with normal geometry (the plane and
   the terrain, in this case), some pixels will fail the zbuffer test as the 
   shadowvolume is rendered. Any values left in the stencil buffer correspond
   to pixels that are in the shadow.

   Finally, these remaining stencil buffer contents are used as a mask, as a
   large full screen black quad is alpha-blended into the scene. With the
   stencil buffer as a mask, only pixels in shadow are darkened. An alternative
   implementation would be to light any pixels that are not in the shadow 
   volume.

   The shadow volume can also be generated using the GPU. GPU generation of the
   shadow volume is accomplished by having three vertices for every triangle in
   the mesh and a quad for every edge of the mesh. The vertices contain the 
   position of the vertex and the plane equation of the triangle the vertex 
   belongs to. The quads are defined using two vertices on an edge from each 
   triangle that shares the edge. When rendering the vertex shader offsets any
   vertices that belong to triangles that are backfacing to the light away from
   the light. The net effect is that any quads that are on a silhouette edge are
   expanded and any quads that are not on a silhouette edge are culled because
   they have zero area. The tradeoff between CPU vs. GPU generation of the 
   shadow volume is simple. CPU generation of the shadow volume consumes more
   CPU time and less GPU time. GPU generation of the shadow volume consumes
   less CPU time and more GPU time.

   A one-pass solution is also implemented that uses two-sided lighting and maps
   the depthbuffer to an A8R8G8B8 surface so that we can render into the blue
   channel to effectively write to the stencil buffer. With the two-sided
   lighting, the front faces are colored "negative blue" (ARGB = 0x000000ff) 
   and the back faces are colored "positive blue" (ARGB = 0x00000001) and the
   values are combined by using a signed add blend operation. Then, the stencil
   buffer contains a shadow mask just as in the two-pass solution.

   Keep in mind that rendering shadow volumes is typically a fill-bound
   operation and the single-pass solution uses different pixel operations which
   turn out to be slightly slower. Therefore, the one-pass solution will be
   slightly slower than the two-pass solution unless the shadow geometry is
   severely transform bound.
