//-----------------------------------------------------------------------------
// File: P3HardwareCounters.h
//
// Desc: Stuctures and functions to handle the P3 hardware counters
//
//       See the "IA-32 Intel Architecture Software Developer's Manual
//       Volume 3: System Programming Guide" for details on P3 hardware
//       counters.
//
// Hist: 1.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef P3_HARDWARE_COUNTERS
#define P3_HARDWARE_COUNTERS

#include <xtl.h>
#include <assert.h>

    
//-----------------------------------------------------------------------------
// P3 Counter Events
//-----------------------------------------------------------------------------
enum P3Event
{
    DATA_MEM_REFS = 0,
    DCU_LINES_IN,
    DCU_M_LINES_IN,
    DCU_M_LINES_OUT,
    DCU_MISS_OUTSTANDING,
    IFU_IFETCH,
    IFU_IFETCH_MISS,
    ITLB_MISS,
    IFU_MEM_STALL,
    ILD_STALL,
    L2_IFETCH_M,
    L2_IFETCH_E,
    L2_IFETCH_S,
    L2_IFETCH_I,
    L2_IFETCH_MESI,
    L2_LD_M,
    L2_LD_E,
    L2_LD_S,
    L2_LD_I,
    L2_LD_MESI,
    L2_ST_M,
    L2_ST_E,
    L2_ST_S,
    L2_ST_I,
    L2_ST_MESI,
    L2_LINES_IN,
    L2_LINES_OUT,
    L2_M_LINES_INM,
    L2_M_LINES_OUTM,
    L2_RQSTS_M,
    L2_RQSTS_E,
    L2_RQSTS_S,
    L2_RQSTS_I,
    L2_RQSTS_MESI,
    L2_ADS,
    L2_DBUS_BUSY,
    L2_DBUS_BUSY_RD,
    BUS_DRDY_CLOCKS_SELF,
    BUS_DRDY_CLOCKS_ANY,
    BUS_LOCK_CLOCKS_SELF,
    BUS_LOCK_CLOCKS_ANY,
    BUS_REQ_OUTSTANDING_SELF,
    BUS_TRAN_BRD_SELF,
    BUS_TRAN_BRD_ANY,
    BUS_TRAN_RFO_SELF,
    BUS_TRAN_RFO_ANY,
    BUS_TRANS_WB_SELF,
    BUS_TRANS_WB_ANY,
    BUS_TRAN_IFETCH_SELF,
    BUS_TRAN_IFETCH_ANY,
    BUS_TRAN_INVAL_SELF,
    BUS_TRAN_INVAL_ANY,
    BUS_TRAN_PWR_SELF,
    BUS_TRAN_PWR_ANY,
    BUS_TRANS_P_SELF,
    BUS_TRANS_P_ANY,
    BUS_TRANS_IO_SELF,
    BUS_TRANS_IO_ANY,
    BUS_TRAN_DEF_SELF,
    BUS_TRAN_DEF_ANY,
    BUS_TRAN_BURST_SELF,
    BUS_TRAN_BURST_ANY,
    BUS_TRAN_ANY_SELF,
    BUS_TRAN_ANY_ANY,
    BUS_TRAN_MEM_SELF,
    BUS_TRAN_MEM_ANY,
    BUS_DATA_RCV_SELF,
    BUS_BNR_DRV_SELF,
    BUS_HIT_DRV_SELF,
    BUS_HITM_DRV_SELF,
    BUS_SNOOP_STALL_SELF,
    FLOPS,
    FP_COMP_OPS_EXE,
    FP_ASSIST,
    MUL,
    DIV,
    CYCLES_DIV_BUSY,
    LD_BLOCKS,
    SB_DRAINS,
    MISALIGN_MEM_REF,
    EMON_KNI_PREF_DISPATCHED_PREFETCHNTA,
    EMON_KNI_PREF_DISPATCHED_PREFETCHT1,
    EMON_KNI_PREF_DISPATCHED_PREFETCHT2,
    EMON_KNI_PREF_DISPATCHED_WEAKLY_ORDERED_STORES,
    EMON_KNI_PREF_MISS_PREFETCHNTA,
    EMON_KNI_PREF_MISS_PREFETCHT1,
    EMON_KNI_PREF_MISS_PREFETCHT2,
    EMON_KNI_PREF_MISS_WEAKLY_ORDERED_STORES,
    INST_RETIRED,
    UOPS_RETIRED,
    INST_DECODED,
    EMON_KNI_INST_RETIRED_PACKED_AND_SCALER,
    EMON_KNI_INST_RETIRED_SCALER,
    EMON_KNI_COMP_INST_RET_PACKED_AND_SCALER,
    EMON_KNI_COMP_INST_RET_SCALER,
    HW_INT_RX,
    CYCLES_INT_MASKED,
    CYCLES_INT_PENDING_AND_MASKED,
    BR_INST_RETIRED,
    BR_MISS_PRED_RETIRED,
    BR_TAKEN_RETIRED,
    BR_MISS_PRED_TAKEN_RET,
    BR_INST_DECODED,
    BTB_MISSES,
    BR_BOGUS,
    BACLEARS,
    RESOURCE_STALLS,
    PARTIAL_RAT_STALLS,
    SEGMENT_REG_LOADS,
    CPU_CLK_UNHALTED,
    MMX_INSTR_EXEC,
    MMX_SAT_INSTR_EXEC,
    MMX_UOPS_EXEC,
    MMX_INSTR_TYPE_EXEC_PACKED_MULTIPLY,
    MMX_INSTR_TYPE_EXEC_PACKED_SHIFT,
    MMX_INSTR_TYPE_EXEC_PACKED_PACK,
    MMX_INSTR_TYPE_EXEC_PACKED_UNPACK,
    MMX_INSTR_TYPE_EXEC_PACKED_LOGICAL,
    MMX_INSTR_TYPE_EXEC_PACKED_ARITHMATIC,
    MMX_ASSIST,
    MMX_INSTR_RET,
    SEG_RENAME_STALLS_ES,
    SEG_RENAME_STALLS_DS,
    SEG_RENAME_STALLS_FS,
    SEG_RENAME_STALLS_GS,
    SEG_RENAME_STALLS_ES_DS_FS_GS,
    SEG_RENAMES_ES,
    SEG_RENAMES_DS,
    SEG_RENAMES_FS,
    SEG_RENAMES_GS,
    SEG_RENAMES_ES_DS_FS_GS,
    RET_SEG_RENAMES,
    P3EVENT_MAX,
    FORCE_DWORD = 0xFFFFFFFF
};


//-----------------------------------------------------------------------------
// Name: P3EventCtrlInfo
// Desc: stucture holding information about each kind of event
//-----------------------------------------------------------------------------
struct P3EventCtrlInfo
{
    const CHAR* szName;             // event name
    BYTE        EventSelect;        // selection number
    BYTE        UnitMask;           // unit mask
    BYTE        CountersAllowed;    // what counter can count this event
                                    // 0x00 : Counter 0
                                    // 0x01 : Counter 1
                                    // 0x10 : Counter 1 or Counter 0
};


//-----------------------------------------------------------------------------
// Name: Event infos for each event type
//-----------------------------------------------------------------------------
extern P3EventCtrlInfo g_P3EventInfos[P3EVENT_MAX];


//-----------------------------------------------------------------------------
// P3 Hardware Counter Event Control Model Specific Register (MSR) bits
//-----------------------------------------------------------------------------
struct P3EventCtrlMSR
{
    DWORD EventSelect: 8;
    DWORD UnitMask: 8;
    DWORD UserMode: 1;
    DWORD OperatingSystemMode: 1;
    DWORD EdgeDetect: 1;
    DWORD PinControl: 1;
    DWORD APICInterruptTable: 1;
    DWORD Reserved:1;
    DWORD EnableCounter: 1;
    DWORD InvertCounterMask: 1;
    DWORD CounterMask: 8;
};


//-----------------------------------------------------------------------------
// Name: P3SetEventCtrl
// Desc: Sets an event control MSR
//-----------------------------------------------------------------------------
VOID P3SetEventCtrl( DWORD dwEventCtrlNum, P3EventCtrlMSR EventCtrl);


//-----------------------------------------------------------------------------
// Name: P3GetEventCtrl
// Desc: Gets an event control MSR
//-----------------------------------------------------------------------------
P3EventCtrlMSR P3GetEventCtrl( DWORD dwEventCtrlNum );


//-----------------------------------------------------------------------------
// Name: P3SetEventCtrlSimple
// Desc: Sets an event control MSR from an event with other flags set to a
//       common configuration
//-----------------------------------------------------------------------------
VOID P3SetEventCtrlSimple( DWORD dwEventCtrlNum, P3Event Event );


//-----------------------------------------------------------------------------
// Name: P3EnableCounters
// Desc: Both counters are either enabled or disabled by seting the 
//       enable bit in the first event control MSR
//-----------------------------------------------------------------------------
VOID P3EnableCounters( BOOL bEnable );


//-----------------------------------------------------------------------------
// Name: P3SetCounter
// Desc: Sets for value of one of the counters
//-----------------------------------------------------------------------------
VOID P3SetCounter( DWORD dwCounterNum, __int64 Value );


//-----------------------------------------------------------------------------
// Name: P3SetCounter
// Desc: Gets for value of one of the counters
//-----------------------------------------------------------------------------
__int64 P3GetCounter( DWORD dwCounterNum );


//-----------------------------------------------------------------------------
// Name: P3GetCycles
// Desc: Gets for cycles count
//-----------------------------------------------------------------------------
__int64 P3GetCycles();


//-----------------------------------------------------------------------------
// Name: P3SetCycles
// Desc: Sets the cycles counter MSR
//       *** Many functions and profiling tools use this value.  Don't
//           set it unless you know what your doing!***
//-----------------------------------------------------------------------------
VOID P3SetCycles( __int64 Value );


//-----------------------------------------------------------------------------
// Name: P3EnableInterups
// Desc: Enables and disabled maskable interrupts
//-----------------------------------------------------------------------------
VOID P3EnableInterrupts( BOOL bEnable );


//-----------------------------------------------------------------------------
// Name: P3InvalidateCache
// Desc: Write back Invalides L1 and L2
//-----------------------------------------------------------------------------
VOID P3WriteBackInvalidateCache();


//-----------------------------------------------------------------------------
// Name: P3WarmCache
// Desc: prefectches data into L1 and L2
//-----------------------------------------------------------------------------
VOID P3WarmCache( const VOID* pMem, DWORD dwCount);




//-----------------------------------------------------------------------------
// inlines
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Name: P3SetCounter
// Desc: Gets for value of one of the counters
//-----------------------------------------------------------------------------
__forceinline __int64 P3GetCounter( DWORD dwCounterNum )
{
    assert( dwCounterNum < 2 );

    __asm mov ecx, dwCounterNum
    __asm rdpmc  // read performance monitoring counters
}


//-----------------------------------------------------------------------------
// Name: P3GetCycles
// Desc: Gets for cycles count
//-----------------------------------------------------------------------------
__forceinline __int64 P3GetCycles()
{
    __asm rdtsc // read time stamp counter
}
        

#endif // P3_HARDWARE_COUNTERS

