;------------------------------------------------------------------------------
; Vertex shader for lighting a skybox with time-of-day texture-based lighting
;------------------------------------------------------------------------------
vs.1.0


;------------------------------------------------------------------------------
; Vertex type expected by this shader
;    v0 = Vertex position - a point on a unit sphere
;
; Expected vertex shaders constants
;    c10      = Constants for computing the color texture's tex coords
;    c20-c23  = View transform
;    c24-c27  = Proj transform
;------------------------------------------------------------------------------


;------------------------------------------------------------------------------
; Vertex transformation
;------------------------------------------------------------------------------

; Transform the vertex
m4x4 r0, v0, c20             ; [r0]   = [v0] * | c20..c23 |
m4x4 oPos, r0, c24           ; [oPos] = [r0] * | c24..c27 |


;------------------------------------------------------------------------------
; Diffuse color
;------------------------------------------------------------------------------

; Output pure white
mov oD0, c10.w


;------------------------------------------------------------------------------
; Texture coordinates
;------------------------------------------------------------------------------

; Use the current time value (c10.x) as the x-texture coordinate,
; and use the vertex y position multiplied by c10.y (-.5) and added to
; c10.z (.5) as the y-texture coordinate. This translates v0.y from the
; -1.0 to 1.0 range to .5 to -.5 range and thence to 1.0 to 0.0 range.
mov oT0.x, c10.x
mad oT0.y, v0.y, c10.y, c10.z

; Use the vertex position as the texture coordinates for luminance.
mov oT1, v0
