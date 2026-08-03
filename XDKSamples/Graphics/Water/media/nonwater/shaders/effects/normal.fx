matrix  mTot;
texture BsTx;
vector  LtDr;


technique tech
{
	pass p0
	{
		ALPHABLENDENABLE=FALSE;
		CullMode=CCW;
		FOGENABLE=FALSE;
        	ZENABLE=TRUE;
		ZWRITEENABLE=TRUE;

		VertexShaderConstant[0] = <mTot>;
		VertexShaderConstant[6] = <LtDr>;

		Texture[0]   = <BsTx>;
		MINFILTER[0]=LINEAR;
		MAGFILTER[0]=LINEAR;
		mipfilter[0]=POINT;
	}
}

