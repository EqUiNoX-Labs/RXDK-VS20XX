;
; Draw a single object.
; (one directional light)
;
; v0 = position
; v1 = normal
; v2 = texture coord
;
; c[0,1,2,3] = composite matrix
; c[4] = local light direction vector
; c[95] = viewport offset.
;

#pragma screenspace

xvs.1.1

; transform and project position
dp4 oPos.x,v0,c[0]
dp4 oPos.y,v0,c[1]
dp4 oPos.z,v0,c[2]
dp4 oPos.w,v0,c[3]

; compute directional light
dp3 oD0,v1,c[4]

; copy texture co-ordinate
mov oT0,v2

; multiply by 1/w and add viewport offset.
; r12 is a read-only alias for oPos.
rcc r1.x, r12.w
mad oPos.xyz, r12.xyz, r1.x, c95.xyz
