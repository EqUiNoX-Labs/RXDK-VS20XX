matrix mTot;
texture BsTx;
texture FogT;

vector FogS;
vector WFgC;
vector LtDr;




technique t0
{
	pass p0
	{
		ALPHABLENDENABLE=FALSE;
		CullMode=CCW;
		FOGENABLE=FALSE;
		ALPHATESTENABLE=FALSE;

		VertexShaderConstant[0] = <mTot>;
		VertexShaderConstant[5] = <FogS>;
		VertexShaderConstant[6] = <LtDr>;
		
		PixelShaderConstant[0] = <WFgC>;	

		Texture[0]   = <BsTx>;
		ADDRESSU[0]=wrap;
		ADDRESSV[0]=wrap;
		MINFILTER[0]=LINEAR;
		MAGFILTER[0]=LINEAR;
		mipfilter[0]=POINT;

		Texture[1]   = <FogT>;
		ADDRESSU[1]=wrap;
		ADDRESSV[1]=wrap;

		Texture[2]   = <FogT>;
		MINFILTER[2]=LINEAR;
		MAGFILTER[2]=LINEAR;
		ADDRESSU[2]=clamp;
		ADDRESSV[2]=clamp;
		ADDRESSW[2]=clamp;
		mipfilter[2]=none;

	}
}
