matrix mTot;
texture BsTx;

vector LtDr;
vector REFF;

 
technique t0
{
	pass p0
	{
		ALPHABLENDENABLE=FALSE;
		ALPHATESTENABLE=FALSE;
		CullMode=NONE;
		FOGENABLE=TRUE;

		VertexShaderConstant[0] = <mTot>;
		VertexShaderConstant[6] = <LtDr>;
		VertexShaderConstant[8] = <REFF>;

		Texture[0]   = <BsTx>;
		MINFILTER[0]=LINEAR;
		MAGFILTER[0]=LINEAR;
		mipfilter[0]=POINT;

	}
}
