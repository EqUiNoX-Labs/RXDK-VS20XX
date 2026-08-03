//-----------------------------------------------------------------------------
// File: XBProfiling.cpp
//
// Desc: Helper classes to support easier profiling, as describe in
//       the Holistic Guide to Xbox Profilers whitepaper.
//
// Hist: 10.22.03 - New for December 2003 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include "XBApp.h"
#include "XBProfiling.h"
#include <xbdm.h>

// This entire file compiles to nothing if PROFILE is not defined.
#ifdef PROFILE

// The command prefix is compatible with the DebugConsole sample
// so we can use DebugConsole to trigger profiles as well as using
// the TriggerProfile tool.
static const char* s_CommandPrefix = "XCMD";

// Static global flag to indicate whether profiling has been
// requested.
static BOOL s_FastCAPRequested = FALSE;




//-----------------------------------------------------------------------------
// Name: dbgtolower()
// Desc: Returns lowercase of char. Used by dbgstrnicmp
//-----------------------------------------------------------------------------
static inline CHAR dbgtolower( CHAR ch )
{
    if( ch >= 'A' && ch <= 'Z' )
        return ch - ( 'A' - 'a' );
    else
        return ch;
}




//-----------------------------------------------------------------------------
// Name: dbgstrnicmp()
// Desc: Case-insensitive string compare that avoids potential deadlocks from
// the CRT strnicmp function.
// Note that this function returns TRUE if the strings match, which is the
// reverse of the CRT strnicmp function.
//-----------------------------------------------------------------------------
static BOOL dbgstrnicmp( const CHAR* str1, const CHAR* str2, int n )
{
    while( ( dbgtolower( *str1 ) == dbgtolower( *str2 ) ) && *str1 && n > 0 )
    {
        --n;
        ++str1;
        ++str2;
    }

    return( n == 0 || dbgtolower( *str1 ) == dbgtolower( *str2 ) );
}




//-----------------------------------------------------------------------------
// Name: ProfileCommandHandler()
// Desc: Command handler to hook in to the Debug Manager command system.
// This function is called from the Debug Manager DPC (Deferred Procedure Call)
// which means there are many CRT and XTL functions that cannot be used.
// It also means you cannot set breakpoints in this function using VC++.
// This function just looks for the command StartFastCap and sets a flag when
// it sees it.
//-----------------------------------------------------------------------------
static HRESULT __stdcall ProfileCommandHandler( const CHAR* strCommand,
                                        CHAR* strResponse, DWORD dwResponseLen,
                                        PDM_CMDCONT pdmcc )
{
    // Skip over the command prefix and the exclamation mark
    strCommand += strlen( s_CommandPrefix ) + 1;

    // Check if this is the initial connect signal
    if( dbgstrnicmp ( strCommand, "__connect__", 11 ) )
    {
        // If so, respond that we're connected
        lstrcpynA( strResponse, "RemoteTrigger connected.", dwResponseLen );
        return XBDM_NOERR;
    }

    const char* command = "StartFastCap";
    const char* response = 0;
    if( dbgstrnicmp( strCommand, command, strlen( command ) ) )
    {
        // Handle this command differently depending on whether this
        // module was compiled with /fastcap
#ifdef _CAP_PROFILING
        if( DmIsFastCAPEnabled() )
        {
            s_FastCAPRequested = TRUE;
            response = "Begin profiling request received.";
        }
        else
        {
            response = "Begin profiling request received when DmIsFastCAPEnabled() returned FALSE.";
        }
#else
        response = "Begin profiling request received on non-fastcap build.";
#endif
    }

    // Copy the response to the output buffer.
    if( response )
    {
        OutputDebugStringA( response );
        OutputDebugStringA( "\n" );
        lstrcpynA( strResponse, response, dwResponseLen );
    }

    return XBDM_NOERR;
}




//-----------------------------------------------------------------------------
// Name: IsFastCapProfileRequested()
// Desc: Returns TRUE if a fastcap profile was requested, and resets the
// internal variable so it will return FALSE next time.
//-----------------------------------------------------------------------------
BOOL XBProfiling_IsFastCAPRequested()
{
    // Initialize the command processor if this hasn't been done yet.
    static BOOL s_Initialized = FALSE;
    if( !s_Initialized )
    {
        s_Initialized = TRUE;
        HRESULT hr = DmRegisterCommandProcessor( s_CommandPrefix, ProfileCommandHandler );
        if( FAILED(hr) )
            OutputDebugStringA( "Error registering command processor.\n" );
    }

    BOOL Result = s_FastCAPRequested;
    s_FastCAPRequested = FALSE;
    return Result;
}




//-----------------------------------------------------------------------------
// Name: BeginProfiling()
// Desc: Start /fastcap profiling. You can specify how much memory to use, or
// you can accept the default and let the function adjust as needed if less
// memory is available. 
//     Call EndProfiling() or let the destructor call it at the end of the
// frame.
//-----------------------------------------------------------------------------
void CXBSimpleFastCapProfile::BeginProfiling( DWORD amountOfMemory )
{
    if ( DmIsFastCAPEnabled() != XBDM_FASTCAPENABLED )
    {
        OutputDebugStringA( "Fast cap not enabled on this build.\n" );
        return;
    }

    HRESULT result = S_OK;
    for (;;)    // Loop until we successfully start profiling or completely fail
    {
        const DWORD decreaseAmount = 1024 * 1024;
        result = DmStartProfiling( "d:\\results.cap", amountOfMemory );
        // 0x80070008 is the error return code for not enough memory - ERROR_NOT_ENOUGH_MEMORY
        if ( result == 0x80070008 && amountOfMemory > decreaseAmount )
        {
            // Try again with a smaller amount of memory.
            amountOfMemory -= decreaseAmount;
        }
        else
        {
            // Either we succeeded, failed due to something other than lack of memory,
            // or we have already tried with the minimum amount of memory.
            break;
        }
    }

    if ( result != XBDM_NOERR )
    {
        char    buffer[1000];
        sprintf( buffer, "DmStartProfiling failed with %08lx.\n", result );
        OutputDebugStringA( buffer );
    }
    else
    {
        // Record the amount of memory used so we can display it later.
        // Also, m_amountOfMemory indicates that profiling was successfully started
        m_amountOfMemory = amountOfMemory;
    }
}




//-----------------------------------------------------------------------------
// Name: EndProfiling()
// Desc: End /fastcap profiling. Then you can manually copy the resulting
// .cap file to your PC and use xbperfview, together with the .exe and .pdb
// files, to analyze your performance.
//-----------------------------------------------------------------------------
void CXBSimpleFastCapProfile::EndProfiling()
{
    // If we weren't doing profiling, just exit.
    if ( !m_amountOfMemory )
        return;

    DmStopProfiling();
    // You should check to see whether the file size is smaller than the
    // buffer size used. If it is the same size then that means that the
    // capture ran out of memory, and you should try to free more memory
    // for a more complete capture.
    OutputDebugStringA( "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" );
    char    buffer[1000];
    sprintf( buffer, "\t\tFinished profiling with a %d Mbyte buffer.\n", m_amountOfMemory / ( 1024 * 1024 ) );
    OutputDebugStringA( buffer );
    OutputDebugStringA( "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n" );

    // Make sure we don't stop profiling twice.
    m_amountOfMemory = 0;
}
#endif
