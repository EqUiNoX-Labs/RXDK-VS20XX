;------------------------------------------------------------------------------
; Vertex shader to do high quality bump mapping.
;------------------------------------------------------------------------------
xvs.1.1

;------------------------------------------------------------------------------
; Vertex streams expected by this shader
;
; v0 = Vertex position
; v1 = Vertex normal
; v2 = Vertex tex coords
; v3 = Vertex S basis vector
; v4 = Vertex T basis vector
; v5 = Vertex SxT basis vector
;
; Expected vertex shaders constants
;
; c0-c3  = world*view*projection matrix
; c4     = local space light position
; c5     = local space eye position
; c6     = 0.5,0.5,0.5,0.5
;------------------------------------------------------------------------------

; Transform position by world*view*projection
dp4 oPos.x, v0, c0
dp4 oPos.y, v0, c1
dp4 oPos.z, v0, c2
dp4 oPos.w, v0, c3

; Copy base texture coords
mov oT0, v2

; Compute vector to the light (L)
add r0.xyz, c4, -v0
dp3 r0.w, r0.xyz, r0.xyz        ; Normalize
rsq r1.x, r0.w
mul r0, r0, r1.x                ; leave it in r0.

; Rotate L into texture space.
dp3 r2.x, r0, v3
dp3 r2.y, r0, v4
dp3 r2.z, r0, v5

mad oD0.xyz, r2, c6, c6			; scale and bias to the range [0,1]

; Compute vector to eye (V)
add r2.xyz, c5, -v0            
dp3 r2.w, r2.xyz, r2.xyz        ; Normalize V
rsq r1.x, r2.w
mul r2, r2, r1.x                ; leave it in r2.

; Compute H = L + V
add r3, r0, r2                  
dp3 r3.w, r3.xyz, r3.xyz        ; Normalize H
rsq r1.x, r3.w
mul r3, r3, r1.x				; result in r3

; Rotate H into texture space.
dp3 r4.x, r3, v3
dp3 r4.y, r3, v4
dp3 r4.z, r3, v5

mov oT1.xyz, r4
mov oT2.xyz, r4
mov oT3.xyz, r4
