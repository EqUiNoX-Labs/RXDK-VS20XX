;------------------------------------------------------------------------------
; Terrain vertex shader
; Copyright (C) 2001 Microsoft Corporation
; All rights reserved.
;------------------------------------------------------------------------------
vs.1.0

;------------------------------------------------------------------------------
;vertex streams
;	v0	= verts
;
;inputs
;	c80-c83	= world*view*proj matrix
;	c84	= texture scale and offset for detail texture
;	c85	= texture scale and offset for shadow texture
;------------------------------------------------------------------------------

	; vertex->screen
	dp4	oPos.x,v0,c80
	dp4	oPos.y,v0,c81
	dp4	oPos.z,v0,c82
	dp4	oPos.w,v0,c83

	; texture coordinate generation
	mad	oT0.xy, v0.xz, c84.xy, c84.zw
	mad	oT1.xy, v0.xz, c85.xy, c85.zw
