//-----------------------------------------------------------------------------
// Name: FastCPU Xbox Sample
// 
// Copyright (c) 2003 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The FastCPU sample implements a library for using the Xbox Pentium 3
   counters which contains functions for setting the event control MSRs,
   querying information about each event type (name, which counter it is
   supported on, unit masks, etc.), and reading the event counters as well
   as the TSC MSR. Helper functions for enabling/disabling interrupts,
   clearing and warming the cache, etc. are also included.

   P3 specific optimizations such as SSE arithmetic, SSE approximations
   with Newton-Raphson iteration, SSE float to long, software prefetch, and
   64 bit MMX reads of write combining memory are benchmarked using the P3
   hardware counter library.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
    The P3 hardware counters library is dived into two modules.  One module
    implements setting and getting the counter controls and values at a low
    level.  This module is easy to integrate into existing profiling code.
    The second module is a class that handles recording events and
    generating a report.  This class is stored in non-cached memory to avoid
    cache pollution.

    The P3 optimized math conversions and approximations are designed for
    use in an existing engine.

    It is worth noting that software prefetch (__asm prefectX [address])
    accounts for over 150 percent of the assembly skinning routines
    performance gain over the C version.  SSE prefetch is extremely easy to
    use and can increase the performance of any streaming operation such as
    skinning, particle system updates, triangle ray tracing, etc. significantly.


       	

