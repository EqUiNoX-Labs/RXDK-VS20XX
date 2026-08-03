xvs.1.1

;
; v0 = position
; v1 = normals
; v2 = texture coords
; v3 = matrix indices
; v4 = weights
; v5 = S basis vector
; v6 = T basis vector
; v7 = SxT basis vector
;
;
; c0-c3 = world*view*projection matrix
; c4 = local space light direction
; c5 = local space view position
;
; c6+ = bone matrices
;

; First bone
mov		a0.x,v3.x
mul		r2,v4.x,c[a0.x+0]
mul	 	r3,v4.x,c[a0.x+1]
mul		r4,v4.x,c[a0.x+2]

; Second bone
mov		a0.x,v3.y
mad		r2,v4.y,c[a0.x+0],r2
mad		r3,v4.y,c[a0.x+1],r3
mad		r4,v4.y,c[a0.x+2],r4

; Third bone
mov		a0.x,v3.z
mad		r2,v4.z,c[a0.x+0],r2
mad		r3,v4.z,c[a0.x+1],r3
mad		r4,v4.z,c[a0.x+2],r4

; Transform weighted position
dp4		r5.x,v0,r2
dp4		r5.y,v0,r3
dp4		r5.z,v0,r4

; Rotate light vector into bone space
mul		r6,c4.x,r2
mad		r6,c4.y,r3,r6
mad		r6,c4.z,r4,r6

; Rotate light vector into texture space
dp3		oT2.x,r6,v5
dp3		oT2.y,r6,v6
dp3		oT2.z,r6,v7

; Compute vector to eye (V)
add		r0.xyz,c5,-r5

; Normliaze vector to eye
dp3		r0.w,r0.xyz,r0.xyz
rsq		r1.x,r0.w
mul		r0,r0,r1.x

; Rotate eye vector into bone space
mul		r7,r0.x,r2
mad		r7,r0.y,r3,r7
mad		r7,r0.z,r4,r7

; compute H = L + V
add		r7,r6,r7

; Rotate half angle vector into texture space
dp3		oT3.x,r7,v5
dp3		oT3.y,r7,v6
dp3		oT3.z,r7,v7

; Texture co-ordinates
mov		oT0,v2
mov		oT1,v2

; World, view, projection
dph		oPos.x,r5,c0
dph		oPos.y,r5,c1
dph		oPos.z,r5,c2
dph		oPos.w,r5,c3
