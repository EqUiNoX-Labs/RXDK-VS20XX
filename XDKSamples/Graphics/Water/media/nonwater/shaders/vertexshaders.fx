VertexShader NORM = decl //normal
{
	stream 0;
	float v0[3];
	float v3[3];
	float v7[2];
}
asm
{
	vs.1.1
		m4x4 oPos, v0, c0
		dp3 r3,v3,c6
		max r3,r3,c7.x
		add oD0,r3,c6.w
		mov oT0,v7
};

VertexShader RFLE = decl //reflection
{
	stream 0;
	float v0[3];
	float v3[3];
	float v7[2];
}
asm
{
		vs.1.1
		m4x4 r0, v0, c0
		mov oPos,r0
		mad oFog,r0.w,c8.x,c8.y
		dp3 r3,v3,c6
		max r3,r3,c4.y
		add oD0,r3,c6.w
		mov oT0,v7
		add oT1.xyz,v0.y,c4.x
};


VertexShader RFRA = decl //refraction
{
	stream 0;
	float v0[3];
	float v3[3];
	float v7[2];
}
asm
{

		vs.1.1
		mul r0,v0,c4.zyzz
		m4x4 oPos, r0, c0
		dp3 r3,v3,c6
		max r3,r3,c4.w
		add oD0,r3,c6.w
		mov oT0,v7
		add oT1.xyz,-v0.y,c4.x
		mov r2.y,-v0.y
		mov r1,v0
		sub r1.y,r1.x,r1.x
		dp3 r1.w,r1, r1
		rsq r1.w,r1.w
		rcp r2.x,r1.w
		mul oT2.xy,r2,c5		
		
};


technique t0
{
	pass p0
	{
	}
}

