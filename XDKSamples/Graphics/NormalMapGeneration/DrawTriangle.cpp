//-----------------------------------------------------------------------------
// File: DrawTraingle.cpp
//
// Desc: Very odd rasterization routine that samples normals by computing 
//       intersections with a mesh.  Since the results of the rasterization
//       will be used as a texture map the usual rasterization rules don't
//       work.
//
// Hist: 08.05.02 - New
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>
#include "RayMesh.h"
#include "DrawTriangle.h"



//-----------------------------------------------------------------------------
// Name: Swap()
// Desc: Simple templated swap function.
//-----------------------------------------------------------------------------
template<class T>
inline void Swap(T& a, T& b)
{
    T temp = a;
    a = b;
    b = temp;
}



//-----------------------------------------------------------------------------
// Name: iRound()
// Desc: Fast float to integer rounding (asuming SSE is set to round).
//-----------------------------------------------------------------------------
inline int iRound(float f)
{
    __asm cvtss2si eax,[f]
}



//-----------------------------------------------------------------------------
// Name: iTrunc()
// Desc: Fast float to integer truncation (using SSE).
//-----------------------------------------------------------------------------
inline int iTrunc(float f)
{
    __asm cvttss2si eax,[f]
}



//-----------------------------------------------------------------------------
// Name: struct ClipVertex
// Desc: Texture vertex used for clipping.
//-----------------------------------------------------------------------------
struct ClipVertex
{
    float u, v;
};



//-----------------------------------------------------------------------------
// Name: struct ClipPolygon
// Desc: Trivial polygon struct for clipping.
//-----------------------------------------------------------------------------
struct ClipPolygon
{
    int iNumVerts;
    ClipVertex Verts[8];
};



//-----------------------------------------------------------------------------
// Name: InterpolateVert()
// Desc: Interpolate between two vertices based on a paramter. t=0 is at V1.
//       t=1 is at V2.
//-----------------------------------------------------------------------------
inline void InterpolateVert( ClipVertex *pOut, const ClipVertex* pV1, 
                             const ClipVertex* pV2, float t )
{
    pOut->u = (1-t) * pV1->u + t * pV2->u;
    pOut->v = (1-t) * pV1->v + t * pV2->v;
}



//-----------------------------------------------------------------------------
// Name: [Left,Right,Top,Bottom]Edge
// Desc: Edge classes for clipping routine.
//-----------------------------------------------------------------------------
class LeftEdge
{
public:
    LeftEdge( float val ) 
    { 
        m_Val = val; 
    }

    inline float Distance( float x, float y )
    {
        return (x - m_Val);
    }

private:
    float m_Val;
};


class RightEdge
{
public:
    RightEdge( float val ) 
    { 
        m_Val = val; 
    }

    inline float Distance( float x, float y )
    {
        return (m_Val - x);
    }

private:
    float m_Val;
};


class TopEdge
{
public:
    TopEdge( float val ) 
    { 
        m_Val = val; 
    }

    inline float Distance( float x, float y )
    {
        return (y - m_Val);
    }

private:
    float m_Val;
};


class BottomEdge
{
public:
    BottomEdge( float val ) 
    { 
        m_Val = val; 
    }

    inline float Distance( float x, float y )
    {
        return (m_Val - y);
    }

private:
    float m_Val;
};



//-----------------------------------------------------------------------------
// Name: SHClip()
// Desc: Sutherland-Hodgman clipping routine.  Templated based on edge.
//-----------------------------------------------------------------------------
template < class Edge >
inline void SHClip( const ClipPolygon& InPoly, ClipPolygon& OutPoly, Edge edge )
{
    // Initialize output polygon.
    OutPoly.iNumVerts = 0;

    const ClipVertex* v1 = InPoly.Verts + (InPoly.iNumVerts - 1);
    float d1 = edge.Distance( v1->u, v1->v );
    bool v1_in = (d1 > 0.0f);

    // Clip each edge of InPoly against this edge.
    for ( int i = 0; i < InPoly.iNumVerts; i++ ) 
    {
        const ClipVertex* v2 = InPoly.Verts + i;
        float d2 = edge.Distance( v2->u, v2->v );
        bool v2_in = (d2 > 0.0f);

        if (v2_in) 
        {
            if (v1_in) 
            {
                // (in->in) add endpoint
                OutPoly.Verts[OutPoly.iNumVerts++] = *v2;
            }
            else 
            {
                // (out->in) add intersection point
                float t = d1 / (d1 - d2);
                if (t < 0.0f)
                    t = 0.0f; 
                else if (t > 1.0f)
                    t = 1.0f;

                InterpolateVert( OutPoly.Verts + OutPoly.iNumVerts++, v1, v2, t );

                // add endpoint
                OutPoly.Verts[OutPoly.iNumVerts++] = *v2;
            }
        }
        else 
        {
            if (v1_in) 
            {
                // (in->out) add intersection point
                float t = d1 / (d1 - d2);
                if (t < 0.0f)
                    t = 0.0f; 
                else if (t > 1.0f)
                    t = 1.0f;

                InterpolateVert( OutPoly.Verts + OutPoly.iNumVerts++, v1, v2, t );
            }
        }

        // End of previous edge is now start of next edge.
        v1 = v2;
        v1_in = v2_in;
        d1 = d2;
    }
}



//-----------------------------------------------------------------------------
// Name: ClipTriangle()
// Desc: Clip a triangle to a rectangle.
//-----------------------------------------------------------------------------
void ClipTriangle( ClipPolygon* pOutPoly,
                   const SourceVert* v0, const SourceVert* v1, const SourceVert* v2, 
                   float top, float bottom, float left, float right )
{
    ClipPolygon PolyA, PolyB;

    // Copy triangle into polygon.
    PolyA.iNumVerts = 3;
    PolyA.Verts[0].u = v0->u;
    PolyA.Verts[0].v = v0->v;
    PolyA.Verts[1].u = v1->u;
    PolyA.Verts[1].v = v1->v;
    PolyA.Verts[2].u = v2->u;
    PolyA.Verts[2].v = v2->v;

    // Clip against each edge.
    SHClip( PolyA, PolyB, TopEdge(top) );
    SHClip( PolyB, PolyA, BottomEdge(bottom) );
    SHClip( PolyA, PolyB, LeftEdge(left) );
    SHClip( PolyB, *pOutPoly, RightEdge(right) );
}



//-----------------------------------------------------------------------------
// Name: ComputePolygonAreaAndCenter()
// Desc: Compute the center of a ClipPolygon.  We used doubles in the 
//       calculation becuause numerical accuracy can be a problem for small
//       polygons.
//-----------------------------------------------------------------------------
float ComputePolygonAreaAndCenter( ClipVertex* pCenter, const ClipPolygon* pPoly )
{
    double fArea = 0.0f;
    double fU = 0.0f;
    double fV = 0.0f;

    int i = pPoly->iNumVerts - 1;
    for ( int j = 0; j < pPoly->iNumVerts; j++ ) 
    {
        double ai = pPoly->Verts[i].u * pPoly->Verts[j].v - 
                    pPoly->Verts[j].u * pPoly->Verts[i].v;

        fArea += ai;

        fU += (pPoly->Verts[i].u + pPoly->Verts[j].u) * ai;
        fV += (pPoly->Verts[i].v + pPoly->Verts[j].v) * ai;
        
        i = j;
    }

    fArea *= 0.5f;

    fU *= 1.0f / (6.0f * fArea);
    fV *= 1.0f / (6.0f * fArea);

    pCenter->u = float(fU);
    pCenter->v = float(fV);

    return float(fabs(fArea));
}



//-----------------------------------------------------------------------------
// Name: BarycentricCoords()
// Desc: Compute the the barycentric coordinates for a point.
//-----------------------------------------------------------------------------
void BarycentricCoordinates( float& t0, float& t1, float& t2, 
                             const SourceVert* v0, const SourceVert* v1, 
                             const SourceVert* v2, float u, float v )
{
    float b = (v1->u - v0->u) * (v2->v - v0->v) - (v2->u - v0->u) * (v1->v - v0->v);

    t0 = ((v1->u - u) * (v2->v - v) - (v2->u - u) * (v1->v - v)) / b;
    t1 = ((v2->u - u) * (v0->v - v) - (v0->u - u) * (v2->v - v)) / b;
    t2 = ((v0->u - u) * (v1->v - v) - (v1->u - u) * (v0->v - v)) / b;

}



//-----------------------------------------------------------------------------
// Name: SamplePositionAndNormalAtPoint()
// Desc: Given a triangle and a 2D point inside the triangle (u,v) return 
//       the 3D position and normal at the point.
//-----------------------------------------------------------------------------
void SamplePositionAndNormalAtPoint( D3DXVECTOR3* p, D3DXVECTOR3* n, 
                                     const SourceVert* v0, const SourceVert* v1, 
                                     const SourceVert* v2, float u, float v )
{
    float t0, t1, t2;

    // Compute the barycentric coordinates of the point.
    BarycentricCoordinates( t0, t1, t2, v0, v1, v2, u, v );

    // Make sure the point is inside the triangle (within tolerance).
    assert( t0 >= -1e-4f );
    assert( t1 >= -1e-4f );
    assert( t2 >= -1e-4f );

    // Make sure the barycentric coordinates make sense.
    assert( fabs(t0 + t1 + t2 - 1.0f) < 1e-4f );

    // Compute postion.
    *p = v0->pos * t0 + v1->pos * t1 + v2->pos * t2;

    // Compute normal.
    *n = v0->norm * t0 + v1->norm * t1 + v2->norm * t2;
}



//-----------------------------------------------------------------------------
// Name: DrawTriangle()
// Desc: Very odd rasterization routine.
//-----------------------------------------------------------------------------
void DrawTriangle( D3DXVECTOR3* pDestNorms, int iPitch, const SourceVert* v0, 
                   const SourceVert* v1, const SourceVert* v2, RayMesh* pMesh )
{
    // Find box that bounds the triangle.
    float fLeft = v0->u;
    float fRight = v0->u;
    float fTop = v0->v;
    float fBottom = v0->v;

    if ( v1->u < fLeft )
        fLeft = v1->u;

    if ( v1->u > fRight )
        fRight = v1->u;

    if ( v1->v < fTop )
        fTop = v1->v;

    if ( v1->v > fBottom )
        fBottom = v1->v;

    if ( v2->u < fLeft )
        fLeft = v2->u;

    if ( v2->u > fRight )
        fRight = v2->u;

    if ( v2->v < fTop )
        fTop = v2->v;

    if ( v2->v > fBottom )
        fBottom = v2->v;

    // Convert to integer values.
    int iLeft = iRound(floorf(fLeft));
    int iRight = iRound(ceilf(fRight));
    int iTop = iRound(floorf(fTop));
    int iBottom = iRound(ceilf(fBottom));

    pDestNorms += iPitch * iTop;

    for ( int iv = iTop; iv < iBottom; iv++ )
    {
        fTop = float(iv);
        fBottom = float(iv+1);

        for ( int iu = iLeft; iu < iRight; iu++ )
        {
            fLeft = float(iu);
            fRight = float(iu+1);

            ClipPolygon Poly;

            // Clip the triangle to the current texel.
            ClipTriangle( &Poly, v0, v1, v2, fTop, fBottom, fLeft, fRight );

            if ( Poly.iNumVerts > 0 )
            {
                ClipVertex vCenter;

                float fArea = ComputePolygonAreaAndCenter( &vCenter, &Poly );
                
                D3DXVECTOR3 p;
                D3DXVECTOR3 n;

                if (fArea > 0.0f)
                {
                    SamplePositionAndNormalAtPoint( &p, &n, v0, v1, v2, vCenter.u, vCenter.v );
                }
                else
                {
                    p = (v0->pos + v1->pos + v2->pos) * 1.0f/3.0f;
                    n = (v0->norm + v1->norm + v2->norm) * 1.0f/3.0f;
                }

                float fDist1, fDist2;
                D3DXVECTOR3 vPos1, vPos2;
                D3DXVECTOR3 vNorm1, vNorm2;

                bool i1 = pMesh->RayIntersection( p, n, &fDist1, &vPos1, &vNorm1 );
                bool i2 = pMesh->RayIntersection( p, -n, &fDist2, &vPos2, &vNorm2 );
                
                float dot1 = D3DXVec3Dot( &n, &vNorm1 );
                float dot2 = D3DXVec3Dot( &n, &vNorm2 );

                D3DXVECTOR3 vNorm = n;
                
                // Choose the intersection that points in the right direction
                // or the nearest intersection.
                if (i1 && i2)
                {
                    if ( (dot1 > 0.0f) && !(dot2 > 0.0f) )
                    {
                        vNorm = vNorm1;
                    }
                    else if ( !(dot1 > 0.0f) && (dot2 > 0.0f) )
                    {
                        vNorm = vNorm2;
                    }
                    else
                    {
                        if ( fDist1 <= fDist2 )
                            vNorm = vNorm1;
                        else
                            vNorm = vNorm2;
                    }
                }
                else if (i1)
                {
                    vNorm = vNorm1;
                }
                else if (i2)
                {
                    vNorm = vNorm2;
                }
                
                pDestNorms[iu] += vNorm * fArea;
            }
        }

        pDestNorms += iPitch;
    }
}
