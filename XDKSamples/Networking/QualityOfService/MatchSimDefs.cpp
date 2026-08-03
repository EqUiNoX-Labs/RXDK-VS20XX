// MatchSim generated file ///////////////////////////////////////////////////
//

//
// Xbox Matchmaking Functions for Title: QualityOfService [0xFFFF0114]
//

#include "xtl.h"
#include "xonline.h"
#include "MatchSimDefs.h"


//////////////////////////////////////////////////////////////////////
//
// Query Definition - 
//
//       Name:    Default
//       ProcNum: 1
//       Params:  0
//       Returns: 0
//
//////////////////////////////////////////////////////////////////////

const DWORD c_cReturnsDefault = 0;

//////////////////////////////////////////////////////////////////////
HRESULT StartMatchQueryDefault(
                    HANDLE hWorkEvent,
                    PXONLINETASK_HANDLE phTask )
{
    const DWORD dwProcNum = 1;
    const DWORD cMaxResults = 6;
    const DWORD cParams = 0;

    DWORD cbResultsLen = XOnlineMatchSearchResultsLen( cMaxResults, c_cReturnsDefault, NULL );


    //
    // Create the async matchmaking search task handle
    //

    HRESULT hr = XOnlineMatchSearch(
                    dwProcNum,
                    cMaxResults,
                    cParams,
                    NULL,
                    cbResultsLen,
                    hWorkEvent,
                    phTask );

    return( hr );
}

//////////////////////////////////////////////////////////////////////

