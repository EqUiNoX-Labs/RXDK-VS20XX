//-----------------------------------------------------------------------------
// File: P3HardwareCounters.cpp
//
// Desc: Stuctures and Functions to hand the P3 hardware counters
//
//       See the "IA-32 Intel Architecture Software Developer's Manual
//       Volume 3: System Programming Guide" for details on P3 hardware
//       counters.
//       http://developer.intel.com/design/PentiumIII/manuals
//
// Hist: 1.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "P3HardwareCounters.h"


//-----------------------------------------------------------------------------
// Name: Event infos for each event type
//-----------------------------------------------------------------------------
P3EventCtrlInfo g_P3EventInfos[P3EVENT_MAX] = 
{
    {"DATA_MEM_REFS", 0x43, 0x00, 0x10},
    {"DCU_LINES_IN", 0x45, 0x00, 0x10},
    {"DCU_M_LINES_IN", 0x46, 0x00, 0x10},
    {"DCU_M_LINES_OUT", 0x47, 0x00, 0x10},
    {"DCU_MISS_OUTSTANDING", 0x48, 0x00, 0x10},
    {"IFU_IFETCH", 0x80, 0x00, 0x10},
    {"IFU_IFETCH_MISS", 0x81, 0x00, 0x10},
    {"ITLB_MISS", 0x85, 0x00, 0x10},
    {"IFU_MEM_STALL", 0x86, 0x00, 0x10},
    {"ILD_STALL", 0x87, 0x00, 0x10},
    {"L2_IFETCH_M", 0x28, 0x08, 0x10},
    {"L2_IFETCH_E", 0x28, 0x04, 0x10},
    {"L2_IFETCH_S", 0x28, 0x02, 0x10},
    {"L2_IFETCH_I", 0x28, 0x01, 0x10},
    {"L2_IFETCH_MESI", 0x28, 0x0F, 0x10},
    {"L2_LD_M", 0x29, 0x08, 0x10},
    {"L2_LD_E", 0x29, 0x04, 0x10},
    {"L2_LD_S", 0x29, 0x02, 0x10},
    {"L2_LD_I", 0x29, 0x01, 0x10},
    {"L2_LD_MESI", 0x29, 0x0F, 0x10},
    {"L2_LD_M", 0x29, 0x08, 0x10},
    {"L2_ST_E", 0x2A, 0x04, 0x10},
    {"L2_ST_S", 0x2A, 0x02, 0x10},
    {"L2_ST_I", 0x2A, 0x01, 0x10},
    {"L2_ST_MESI", 0x2A, 0x0F, 0x10},
    {"L2_LINES_IN", 0x24, 0x00, 0x10},
    {"L2_LINES_OUT", 0x26, 0x00, 0x10},
    {"L2_M_LINES_INM", 0x25, 0x00, 0x10},
    {"L2_M_LINES_OUTM", 0x27, 0x00, 0x10},
    {"L2_RQSTS_M", 0x2E, 0x08, 0x10},
    {"L2_RQSTS_E", 0x2E, 0x04, 0x10},
    {"L2_RQSTS_S", 0x2E, 0x02, 0x10},
    {"L2_RQSTS_I", 0x2E, 0x01, 0x10},
    {"L2_RQSTS_MESI", 0x2E, 0x0F, 0x10},
    {"L2_ADS", 0x21, 0x00, 0x10},
    {"L2_DBUS_BUSY", 0x22, 0x00, 0x10},
    {"L2_DBUS_BUSY_RD", 0x23, 0x00, 0x10},
    {"BUS_DRDY_CLOCKS_SELF", 0x62, 0x00, 0x10},
    {"BUS_DRDY_CLOCKS_ANY", 0x62, 0x20, 0x10},
    {"BUS_LOCK_CLOCKS_SELF", 0x63, 0x00, 0x10},
    {"BUS_LOCK_CLOCKS_ANY", 0x63, 0x20, 0x10},
    {"BUS_REQ_OUTSTANDING_SELF", 0x60, 0x00, 0x10},
    {"BUS_TRAN_BRD_SELF", 0x65, 0x00, 0x10},
    {"BUS_TRAN_BRD_ANY", 0x65, 0x20, 0x10},
    {"BUS_TRAN_RFO_SELF", 0x66, 0x00, 0x10},
    {"BUS_TRAN_RFO_ANY", 0x66, 0x20, 0x10},
    {"BUS_TRANS_WB_SELF", 0x67, 0x00, 0x10},
    {"BUS_TRANS_WB_ANY", 0x67, 0x20, 0x10},
    {"BUS_TRAN_IFETCH_SELF", 0x68, 0x00, 0x10},
    {"BUS_TRAN_IFETCH_ANY", 0x68, 0x20, 0x10},
    {"BUS_TRAN_INVAL_SELF", 0x69, 0x00, 0x10},
    {"BUS_TRAN_INVAL_ANY", 0x69, 0x20, 0x10},
    {"BUS_TRAN_PWR_SELF", 0x6A, 0x00, 0x10},
    {"BUS_TRAN_PWR_ANY", 0x6A, 0x20, 0x10},
    {"BUS_TRANS_P_SELF", 0x6B, 0x00, 0x10},
    {"BUS_TRANS_P_ANY", 0x6B, 0x20, 0x10},
    {"BUS_TRANS_IO_SELF", 0x6C, 0x00, 0x10},
    {"BUS_TRANS_IO_ANY", 0x6C, 0x20, 0x10},
    {"BUS_TRAN_DEF_SELF", 0x6D, 0x00, 0x10},
    {"BUS_TRAN_DEF_ANY", 0x6D, 0x20, 0x10},
    {"BUS_TRAN_BURST_SELF", 0x6E, 0x00, 0x10},
    {"BUS_TRAN_BURST_ANY", 0x6E, 0x20, 0x10},
    {"BUS_TRAN_ANY_SELF", 0x70, 0x00, 0x10},
    {"BUS_TRAN_ANY_ANY", 0x70, 0x20, 0x10},
    {"BUS_TRAN_MEM_SELF", 0x6F, 0x00, 0x10},
    {"BUS_TRAN_MEM_ANY", 0x6F, 0x20, 0x10},
    {"BUS_DATA_RCV_SELF", 0x64, 0x00, 0x10},
    {"BUS_BNR_DRV_SELF", 0x61, 0x00, 0x10},
    {"BUS_HIT_DRV_SELF", 0x7A, 0x00, 0x10},
    {"BUS_HITM_DRV_SELF", 0x7B, 0x00, 0x10},
    {"BUS_SNOOP_STALL_SELF", 0x7E, 0x00, 0x10},
    {"FLOPS", 0xC1, 0x00, 0x00},
    {"FP_COMP_OPS_EXE", 0x10, 0x00, 0x00},
    {"FP_ASSIST", 0x11, 0x00, 0x01},
    {"MUL", 0x12, 0x00, 0x01},
    {"DIV", 0x13, 0x00, 0x01},
    {"CYCLES_DIV_BUSY", 0x14, 0x00, 0x00},
    {"LD_BLOCKS", 0x03, 0x00, 0x10},
    {"SB_DRAINS", 0x04, 0x00, 0x10},
    {"MISALIGN_MEM_REF", 0x05, 0x00, 0x10},
    {"EMON_KNI_PREF_DISPATCHED_PREFETCHNTA", 0x07, 0x00, 0x10},
    {"EMON_KNI_PREF_DISPATCHED_PREFETCHT1", 0x07, 0x01, 0x10},
    {"EMON_KNI_PREF_DISPATCHED_PREFETCHT2", 0x07, 0x02, 0x10},
    {"EMON_KNI_PREF_DISPATCHED_WEAKLY_ORDERED_STORES", 0x07, 0x03, 0x10},
    {"EMON_KNI_PREF_MISS_PREFETCHNTA", 0x4B, 0x00, 0x10},
    {"EMON_KNI_PREF_MISS_PREFETCHT1", 0x4B, 0x01, 0x10},
    {"EMON_KNI_PREF_MISS_PREFETCHT2", 0x4B, 0x02, 0x10},
    {"EMON_KNI_PREF_MISS_WEAKLY_ORDERED_STORES", 0x4B, 0x03, 0x10},
    {"INST_RETIRED", 0xC0, 0x00, 0x10},
    {"UOPS_RETIRED", 0xC2, 0x00, 0x10},
    {"INST_DECODED", 0xD0, 0x00, 0x10},
    {"EMON_KNI_INST_RETIRED_PACKED_AND_SCALER", 0xD8, 0x00, 0x10},
    {"EMON_KNI_INST_RETIRED_SCALER", 0xD8, 0x01, 0x10},
    {"EMON_KNI_COMP_INST_RET_PACKED_AND_SCALER", 0xD9, 0x00, 0x10},
    {"EMON_KNI_COMP_INST_RET_SCALER", 0xD9, 0x01, 0x10},
    {"HW_INT_RX", 0xC8, 0x00, 0x10},
    {"CYCLES_INT_MASKED", 0xC6, 0x00, 0x10},
    {"CYCLES_INT_PENDING_AND_MASKED", 0xC7, 0x00, 0x10},
    {"BR_INST_RETIRED", 0xC4, 0x00, 0x10},
    {"BR_MISS_PRED_RETIRED", 0xC5, 0x00, 0x10},
    {"BR_TAKEN_RETIRED", 0xC9, 0x00, 0x10},
    {"BR_MISS_PRED_TAKEN_RET", 0xCA, 0x00, 0x10},
    {"BR_INST_DECODED", 0xE0, 0x00, 0x10},
    {"BTB_MISSES", 0xE2, 0x00, 0x10},
    {"BR_BOGUS", 0xE4, 0x00, 0x10},
    {"BACLEARS", 0xE6, 0x00, 0x10},
    {"RESOURCE_STALLS", 0xA2, 0x00, 0x10},
    {"PARTIAL_RAT_STALLS", 0xD2, 0x00, 0x10},
    {"SEGMENT_REG_LOADS", 0x06, 0x00, 0x10},
    {"CPU_CLK_UNHALTED", 0x79, 0x00, 0x10},
    {"MMX_INSTR_EXEC", 0xB0, 0x00, 0x10},
    {"MMX_SAT_INSTR_EXEC", 0xB1, 0x00, 0x10},
    {"MMX_UOPS_EXEC", 0xB2, 0x0F, 0x10},
    {"MMX_INSTR_TYPE_EXEC_PACKED_MULTIPLY", 0xB3, 0x01, 0x10},
    {"MMX_INSTR_TYPE_EXEC_PACKED_SHIFT", 0xB3, 0x02, 0x10},
    {"MMX_INSTR_TYPE_EXEC_PACKED_PACK", 0xB3, 0x04, 0x10},
    {"MMX_INSTR_TYPE_EXEC_PACKED_UNPACK", 0xB3, 0x08, 0x10},
    {"MMX_INSTR_TYPE_EXEC_PACKED_LOGICAL", 0xB3, 0x10, 0x10},
    {"MMX_INSTR_TYPE_EXEC_PACKED_ARITHMATIC", 0xB3, 0x20, 0x10},
    {"MMX_ASSIST", 0xCD, 0x00, 0x10},
    {"MMX_INSTR_RET", 0xCE, 0x00, 0x10},
    {"SEG_RENAME_STALLS_ES", 0xD4, 0x01, 0x10},
    {"SEG_RENAME_STALLS_DS", 0xD4, 0x02, 0x10},
    {"SEG_RENAME_STALLS_FS", 0xD4, 0x04, 0x10},
    {"SEG_RENAME_STALLS_GS", 0xD4, 0x08, 0x10},
    {"SEG_RENAME_STALLS_ES_DS_FS_GS", 0xD4, 0x0F, 0x10},
    {"SEG_RENAMES_ES", 0xD5, 0x01, 0x10},
    {"SEG_RENAMES_DS", 0xD5, 0x02, 0x10},
    {"SEG_RENAMES_FS", 0xD5, 0x04, 0x10},
    {"SEG_RENAMES_GS", 0xD5, 0x08, 0x10},
    {"SEG_RENAMES_ES_DS_FS_GS", 0xD5, 0x0F, 0x10},
    {"RET_SEG_RENAMES", 0xD6, 0x00, 0x10}
};


//-----------------------------------------------------------------------------
// Name: P3SetEventCtrl
// Desc: Sets an event control MSR
//-----------------------------------------------------------------------------
void P3SetEventCtrl( DWORD dwEventCtrlNum, P3EventCtrlMSR EventCtrl )
{
    assert( dwEventCtrlNum < 2 );

    __asm mov ecx, 0x186  // control 0 is 0x186, control 1 is 0x187
    __asm add ecx, dwEventCtrlNum
    __asm mov eax, DWORD PTR [EventCtrl];
    __asm mov edx, 0
    __asm wrmsr  // set MSR
}


//-----------------------------------------------------------------------------
// Name: P3GetEventCtrl
// Desc: Gets an event control MSR
//-----------------------------------------------------------------------------
P3EventCtrlMSR P3GetEventCtrl( DWORD dwEventCtrlNum )
{
    assert( dwEventCtrlNum < 2 );

    __asm mov ecx, 0x186  // control 0 is 0x186, control 1 is 0x187
    __asm add ecx, dwEventCtrlNum
    __asm rdmsr // read MSR
}       


//-----------------------------------------------------------------------------
// Name: P3SetEventCtrlSimple
// Desc: Sets an event control MSR from an event with other flags set to a
//       common configuration
//-----------------------------------------------------------------------------
VOID P3SetEventCtrlSimple( DWORD dwEventCtrlNum, P3Event Event )
{
    assert( Event < P3EVENT_MAX );
    assert( g_P3EventInfos[Event].CountersAllowed == dwEventCtrlNum ||
            g_P3EventInfos[Event].CountersAllowed == 0x10 );


    P3EventCtrlMSR MSR;
    ZeroMemory( &MSR, sizeof(P3EventCtrlMSR) );
    MSR.UserMode = 1;
    MSR.OperatingSystemMode = 1;

    MSR.EventSelect = g_P3EventInfos[Event].EventSelect;
    MSR.UnitMask = g_P3EventInfos[Event].UnitMask;

    P3SetEventCtrl( dwEventCtrlNum, MSR );
}


//-----------------------------------------------------------------------------
// Name: P3EnableCounters
// Desc: Both counters are either enabled or disabled by setting the 
//       enable bit in the first event control MSR
//-----------------------------------------------------------------------------
VOID P3EnableCounters( BOOL bEnable )
{
    P3EventCtrlMSR EventCtrl0 = P3GetEventCtrl( 0 );
    EventCtrl0.EnableCounter = bEnable ? 0x1 : 0x0;
    P3SetEventCtrl( 0, EventCtrl0 );
}


//-----------------------------------------------------------------------------
// Name: P3SetCounter
// Desc: Sets for value of one of the counters
//-----------------------------------------------------------------------------
VOID P3SetCounter( DWORD dwCounterNum, __int64 Value )
{
    assert( dwCounterNum < 2 );
    
    __asm mov eax, dword ptr[Value]
    __asm mov edx, dword ptr[Value + 4]
    __asm mov ecx, 0xC1 // counter 0 is 0xC1, couter 2 is 0xC2
    __asm add ecx, dwCounterNum
    __asm wrmsr  // set MSR
}


//-----------------------------------------------------------------------------
// Name: P3SetCycles
// Desc: Sets the cycles counter MSR
//       *** Many functions and profiling tools read time stamp counter (TSC)
//           by using the rdtsc instruction.
//           Don't set it unless you know what your doing!***
//-----------------------------------------------------------------------------
VOID P3SetCycles( __int64 Value )
{
    __asm mov eax, dword ptr[Value]
    __asm mov edx, dword ptr[Value + 4]
    __asm mov ecx, 0x10 // TSC msr number
    __asm wrmsr // set MSR
}


//-----------------------------------------------------------------------------
// Name: P3EnableInterupts
// Desc: Enables and disabled maskable iterrupts
//-----------------------------------------------------------------------------
VOID P3EnableInterrupts( BOOL bEnable )
{
    if( bEnable )
        __asm sti // enable maskable interrupts
    else
        __asm cli // disable maskable interrupts
}


//-----------------------------------------------------------------------------
// Name: P3InvalidateCache
// Desc: Write back invalidates L1 and L2
//-----------------------------------------------------------------------------
VOID P3WriteBackInvalidateCache()
{
     __asm wbinvd  // write back invalidate the cache
}


//-----------------------------------------------------------------------------
// Name: P3WarmCache
// Desc: prefetches data into L1 and L2
//-----------------------------------------------------------------------------
VOID P3WarmCache( const VOID* pMem, DWORD dwCount)
{
    __asm mov eax, pMem
    __asm mov ecx, pMem
    __asm add ecx, dwCount;

PREFETCH_LOOP:
    __asm prefetcht0 [eax] // prefetch cache line containing the address in eax
    __asm add eax, 32
    __asm cmp eax, ecx
    __asm jl PREFETCH_LOOP
}
