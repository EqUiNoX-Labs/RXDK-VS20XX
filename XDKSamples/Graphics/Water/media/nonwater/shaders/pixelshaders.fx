PixelShader NORM =//normal
asm
{
	ps.1.1
		tex t0
		mul r0,t0,v0
};

PixelShader RFLE = //reflection
asm
{
	ps.1.1
		tex t0
		texkill t1
		mul r0.rgb,t0,v0
		+sub r0.a,t0,t0
};

PixelShader RFRA = //refraction
asm
{
	ps.1.1
		tex t0
		texkill t1
		tex t2
		mul r0,t0,v0
		lrp r0.rgb,t2,r0,c0
		+mov r0.a,c0.a
};

PixelShader RFWA = //refractionAndWriteAlpha
asm
{
	ps.1.1
		tex t0
		texkill t1
		tex t2
		mul r0,t0,v0
		lrp r0.rgb,t2,r0,c0
		+mov r0.a,t2.a
};

technique t0
{
	pass p0
	{
	}
}

