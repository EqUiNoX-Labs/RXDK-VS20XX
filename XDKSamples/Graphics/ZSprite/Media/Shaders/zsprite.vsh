;------------------------------------------------------------------------------
; Vertex shader for ZSprite quad
;------------------------------------------------------------------------------
vs.1.0
#pragma screenspace

;------------------------------------------------------------------------------
; Vertex stream
;    v0  - Vertex position
;
; Constants
;    c0  - Offsets for the screen space position
;    c1  - Stage 2 texture coords (1, 0, 0)
;    c2  - Stage 3 texture coords (0, 0, 1/D3DZ_MAX_D24S8)
;------------------------------------------------------------------------------

; Transform vertex position to screen space (Using supplied coordinates plus
; the half-pixel shift to align pixel centers with texel centers)
add oPos, v0, c0

; Texture coords
mov oT0, v0   ; Texcoords for the zsprite image (same as screenspace pos)
mov oT1, v0   ; Texcoords for the zsprite z-buffer (same as screenspace pos)
mov oT2, c1   ; Texcoords for pixel shader dot product program
mov oT3, c2   ; Texcoords for pixel shader "dot product zw" program
