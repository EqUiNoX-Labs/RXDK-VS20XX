;------------------------------------------------------------------------------
; Vertex shader for doing two-sided shadow-volumes on the GPU
;------------------------------------------------------------------------------
xvs.1.1
#pragma screenspace


;------------------------------------------------------------------------------
; Vertex components (as specified in the vertex DECL)
;    v0.xyz  = Position
;    v1.xyzw = Plane equation of face
;
; Constants specified by the app
;    c0-c4   = Composite transform matrix
;    c4.xyzw = Light position (may be infinite: w=0)
;    c5.x    = Offset scale
;    c5.y    = 0.0f
;    c6      = Front side color
;    c7      = Back side color
;------------------------------------------------------------------------------

; Offset vertex along light direction.
; Directional light math could be simpler.

dp4 r2.x, v1, c4                    ; Dot light vector with face plane
slt r2.x, r2.x, c5.y                ; Offset bool = 1 if dot < 0

mad r0.xyz, v0.xyz, c4.w, -c4.xyz   ; V * Lw - L

dp3 r0.w, r0.xyz, r0.xyz            ; Length squared
rsq r1.x, r0.w                      ; 1/sqrt(length^2)
mul r0, r0, r1.x                    ; Normalize (w = length)

mul r2.x, r2.x, c5.x                ; Offset bool * offset scale

mad r0.xyz, r0.xyz, r2.x, v0.xyz    ; Add offset to pos


;------------------------------------------------------------------------------
; Transform offset vertex.
;------------------------------------------------------------------------------
dph oPos.x, r0, c0
dph oPos.y, r0, c1
dph oPos.z, r0, c2
dph oPos.w, r0, c3


;------------------------------------------------------------------------------
; Write out the front and back colors.
;------------------------------------------------------------------------------
mov oD0, c6 
mov oB0, c7


; Multiply by reciprocal and add viewport offset.
; r12 is a read-only alias for oPos.
rcc r1.x, r12.w
mad oPos.xyz, r12, r1.x, c95
