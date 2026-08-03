;------------------------------------------------------------------------------
; Vertex shader for doing shadow-volumes
;------------------------------------------------------------------------------
xvs.1.1
#pragma screenspace


;------------------------------------------------------------------------------
; Vertex components (as specified in the vertex DECL)
;    v0.xyz = Position
;    v0.w   = Offset bool (0 or 1)
;
; Constants specified by the app
;    c0-c4   = Composite transform matrix
;    c4.xyzw = Light position (may be infinite: w=0)
;    c5.x    = Offset scale
;------------------------------------------------------------------------------

; Offset vertex along light direction.
; Directional light math could be simpler.

mad r0.xyz, v0.xyz, c4.w, -c4.xyz   ; V * Lw - L

dp3 r0.w, r0.xyz, r0.xyz            ; Length squared
rsq r1.x, r0.w                      ; 1/sqrt(length^2)
mul r0, r0, r1.x                    ; Normalize (w = length)

mul r2.x, v0.w, c5.x                ; Offset bool * offset scale

mad r0.xyz, r0.xyz, r2.x, v0.xyz    ; Add offset to pos


;------------------------------------------------------------------------------
; Transform offset vertex.
;------------------------------------------------------------------------------
dph oPos.x, r0, c0
dph oPos.y, r0, c1
dph oPos.z, r0, c2
dph oPos.w, r0, c3

; Multiply by reciprocal and add viewport offset.
; r12 is a read-only alias for oPos.
rcc r1.x, r12.w
mad oPos.xyz, r12, r1.x, c95
