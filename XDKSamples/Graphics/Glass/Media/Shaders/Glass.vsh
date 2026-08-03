;------------------------------------------------------------------------------
; Vertex shader to perform glass effect
;------------------------------------------------------------------------------
xvs.1.1

;------------------------------------------------------------------------------
; Vertex streams expected by this shader
;    struct VERTEX
;    {
;       D3DXVECTOR3 p;        // v0.xyz  = Vertex position
;       D3DXVECTOR3 n;        // v2.xyz  = Vertex normal
;       ...
;    };
;
; Expected vertex shaders constants
;    c0-c3    = Transpose of world*view*proj matrix
;    c4-c7    = Transpose of world*view matrix
;------------------------------------------------------------------------------

; Transform position
m4x4 oPos, v0, c0

; Transform normal and output as a texcoord
m3x3 oT0, v2, c4

; Write out 3x3 per-pixel transform as tex coords
mov oT1, c20
mov oT2, c21
mov oT3, c22
