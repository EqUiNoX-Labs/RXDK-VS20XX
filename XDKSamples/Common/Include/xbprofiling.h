//-----------------------------------------------------------------------------
// File: XBProfiling.h
//
// Desc: Prototypes and class declarations for profiling helper functions.
//       See the Holistic Guide to Xbox Profilers for details.
//
// Hist: 11.04.03 - New for December XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef XBPROFILING
#define XBPROFILING

// Work around the structure packing bug in d3d8perf.h.
#pragma pack(push, 8)
#include <d3d8perf.h>
#pragma pack(pop)

#include <stdio.h>




//-----------------------------------------------------------------------------
// Name: XBProfiling_IsFastCAPRequested()
// Desc: Returns TRUE if a FastCAP profile has been requested with the
// TriggerProfile tool on the PC. Typical usage would be to call this once
// frame and trigger a profile when it returns TRUE. This function will return
// TRUE once for each use of TriggerProfile
// Typical usage is like this:
//
//        CXBSimpleFastCapProfile profile;
//        if ( XBProfiling_IsFastCAPRequested() )
//            profile.BeginProfiling();
//
//       In release builds the code compiles away to nothing.
//-----------------------------------------------------------------------------
#ifdef  PROFILE
BOOL XBProfiling_IsFastCAPRequested();
#else
inline BOOL XBProfiling_IsFastCAPRequested() { return FALSE; }
#endif




//-----------------------------------------------------------------------------
// Name: class SimpleFastCapProfile
// Desc: Simple class to manage programmatic /fastcap captures.
//
//       To use it, declare an object of SimpleFastCapProfile type in your
// main loop. When you want to create a /fastcap capture, call
// BeginProfiling(). When the object leaves scope it's destructor will
// automatically call EndProfiling(), or you can call it manually.
//
// /fastcap captures only work if your code was built with the /fastcap
// compiler option.
//-----------------------------------------------------------------------------
class CXBSimpleFastCapProfile
{
public:
    inline CXBSimpleFastCapProfile()
    {
#ifdef  PROFILE
        m_amountOfMemory = 0;
#endif
    }

    inline ~CXBSimpleFastCapProfile()
    {
        EndProfiling();
    }

    //-----------------------------------------------------------------------------
    // Name: BeginProfiling()
    // Desc: Start /fastcap profiling. You can specify how much memory to use, or
    // you can accept the default and let the function adjust as needed if less
    // memory is available. 
    //     Call EndProfiling() or let the destructor call it at the end of the
    // frame.
    //-----------------------------------------------------------------------------
#ifdef  PROFILE
    void BeginProfiling( DWORD amountOfMemory = 32 * 1024 * 1024 );
#else
    // Compile to nothing in release builds.
    inline void BeginProfiling( DWORD amountOfMemory = 32 * 1024 * 1024 ) {}
#endif

    //-----------------------------------------------------------------------------
    // Name: EndProfiling()
    // Desc: End /fastcap profiling. Then you can manually copy the resulting
    // .cap file to your PC and use xbperfview, together with the .exe and .pdb
    // files, to analyze your performance.
    //-----------------------------------------------------------------------------
#ifdef  PROFILE
    void EndProfiling();
#else
    // Compile to nothing in release builds
    inline void EndProfiling() {}
#endif

private:
#ifdef  PROFILE
    DWORD   m_amountOfMemory;
#endif

    // The copy constructor and assignment operator are private and
    // unimplemented to disallow object copying.
    CXBSimpleFastCapProfile& operator=(const CXBSimpleFastCapProfile& rhs);
    CXBSimpleFastCapProfile(const CXBSimpleFastCapProfile& rhs);
};




//-----------------------------------------------------------------------------
// This object encapsulates the D3DPERF_BeginEvent/EndEvent commands, to ensure
// that they are always properly nested, while requiring a minimum of code
// modifications to add these profiling calls - just one line per event.
// This class completely disappears in release builds.
// Example usage:
// BeginEventObject eventObject( "Object %s", object->GetName() );
// BeginEventObject eventObject( D3DCOLOR_XRGB( 0xff,0xff,0x7f ), "FrameMove" );
//-----------------------------------------------------------------------------
class CXBBeginEventObject
{
public:
    //-----------------------------------------------------------------------------
    // Name: CXBBeginEventObject()
    // Desc: The constructors call D3DPERF_BeginEvent and the destructor will
    // call D3DPERF_EndEvent. This constructor takes just a color and a name.
    // The color is optional, and you can specify up to two additional parameters
    // for sprintf style formatting of the event label - the appropriate template
    // constructor will be automatically called.
    //-----------------------------------------------------------------------------
    inline CXBBeginEventObject(D3DCOLOR color, const char* name)
    {
#if defined(PROFILE) || defined(_DEBUG)
        D3DPERF_BeginEvent(color, name);
#endif
    }
    // This constructor is the same as the one above except that it
    // doesn't require the color parameter. A color will be chosen from a table.
    inline CXBBeginEventObject(const char* name)
    {
#if defined(PROFILE) || defined(_DEBUG)
        D3DPERF_BeginEvent(GetNextColor(), name);
#endif
    }



    // Template constructor to support the sprintf style interface to D3DPerf_BeginEvent
    // This constructor is used for one sprintf parameter of any type.
    template <typename T>
    inline CXBBeginEventObject(D3DCOLOR color, const char* name, T data1)
    {
#if defined(PROFILE) || defined(_DEBUG)
        D3DPERF_BeginEvent(color, name, data1);
#endif
    }
    // This constructor is the same as the one above except that it
    // doesn't require the color parameter. A color will be chosen from a table.
    template <typename T>
    inline CXBBeginEventObject(const char* name, T data1)
    {
#if defined(PROFILE) || defined(_DEBUG)
        D3DPERF_BeginEvent(GetNextColor(), name, data1);
#endif
    }



    // Template constructor to support the sprintf style interface to D3DPerf_BeginEvent
    // This constructor is used for two sprintf parameters of any types.
    template <typename T, typename T2>
    inline CXBBeginEventObject(D3DCOLOR color, const char* name, T data1, T2 data2)
    {
#if defined(PROFILE) || defined(_DEBUG)
        D3DPERF_BeginEvent(color, name, data1, data2);
#endif
    }
    // This constructor is the same as the one above except that it
    // doesn't require the color parameter. A color will be chosen from a table.
    template <typename T, typename T2>
    inline CXBBeginEventObject(const char* name, T data1, T2 data2)
    {
#if defined(PROFILE) || defined(_DEBUG)
        D3DPERF_BeginEvent(GetNextColor, name, data1, data2);
#endif
    }



    //-----------------------------------------------------------------------------
    // Name: ~CXBBeginEventObject()
    // Desc: The destructor for CXBBeginEventObject calls D3DPERF_EndEvent to
    // ensure that Begin/End calls are always matched.
    //-----------------------------------------------------------------------------
    inline ~CXBBeginEventObject()
    {
#if defined(PROFILE) || defined(_DEBUG)
        D3DPERF_EndEvent();
#endif
    }

private:
#if defined(PROFILE) || defined(_DEBUG)

    //-----------------------------------------------------------------------------
    // GetNextColor() simplifies the effective usage of BeginEventObject by
    // returning a sequence of colors, so that adjacent events in the timeline
    // are different colors. When the hierarchy is collapsed it may end up that
    // adjacent events are the same color, but with enough colors in the
    // s_colors array this should be rare. Some extra code could avoid this
    // possibility, but it's not really important.
    // The colors will be different on different captures.
    //-----------------------------------------------------------------------------
    inline D3DCOLOR GetNextColor() const
    {
        static int s_currentColor = 0;
        // Add all your favorite colors to this array.
        static D3DCOLOR s_colors[] =
        {
            D3DCOLOR_XRGB(0xFF, 0x00, 0x00),    // Red
            D3DCOLOR_XRGB(0x00, 0xFF, 0x00),    // Green
            D3DCOLOR_XRGB(0x00, 0x00, 0xFF),    // Blue
            D3DCOLOR_XRGB(0xFF, 0xFF, 0x00), // Red+Green makes ???
            D3DCOLOR_XRGB(0xFF, 0x00, 0xFF),    // Purple
            D3DCOLOR_XRGB(0x00, 0xFF, 0xFF),    // Green+Blue makes ???
        };
        ++s_currentColor;
        if ( s_currentColor >= ( sizeof(s_colors) / sizeof( s_colors[0] ) ) )
            s_currentColor = 0;
        return s_colors[s_currentColor];
    }
#endif

    // The copy constructor and assignment operator are private and
    // unimplemented to disallow object copying.
    CXBBeginEventObject(const CXBBeginEventObject& rhs);
    CXBBeginEventObject& operator=(const CXBBeginEventObject& rhs);
};

#endif  // XBPROFILING
