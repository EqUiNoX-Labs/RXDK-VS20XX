vs.1.1

//Mesh:
m4x4 r7, v0, c4

m4x4 r0,r7,c0

mov  oPos,r0 

//Water:
mul r8.xy,r7.xz,c16.x
add oT0.xy,r8,c16.y


mad r1,r0.wwww,c15.xzxx,r0

//For Nvidia cards:
mul r2,r1,c15.xyxx
rcp r2.w,r2.w
mul oT1,r2,r2.w
mul oT2,r2,r2.w
sge oT1.w,r2,r2
sge OT2.w,r2,r2


//Fresnel factor
sub r3,c17,r7

dp3 r3.w,r3,r3
rsq r3.w,r3.w
mul r3.xyz,r3,r3.w

add r4,r3,c95
dp3 r4.w,r4,r4						
rsq r4.w,r4.w						
mul r4.y,r4,r4.w
mov r4.xw,c95.w

mul r3,r3,c17.w
add oT3.xy,r3.xz,c17.w

mov oD0,c94

lit r4,r4
mov oD1,r4.zzzz

mad oFog,r0.w,c18.x,c18.y
