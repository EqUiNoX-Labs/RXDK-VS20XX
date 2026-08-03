//-----------------------------------------------------------------------------
// File: LogFile.h
//
// This file defines the format of strings in the log file used by the
// CallStackLog and CallStackLookup samples.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once




//-----------------------------------------------------------------------------
// At the beginning of the log, modules are listed in a format like this:
//    Module [ModuleName.exe, 0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC]
// where 0xAAAAAAAA is the base address of the module, 0xBBBBBBBB is the
// module size, and 0xCCCCCCCC is its timestamp.
//-----------------------------------------------------------------------------

const CHAR g_strModuleOutputStart[] = "Module [";
const INT  g_iCharCountModuleOutputStart = sizeof(g_strModuleOutputStart) - 1;

const CHAR g_strModuleOutputEnd[] = "]\r\n";
const INT  g_iCharCountModuleOutputEnd = sizeof(g_strModuleOutputEnd) - 1;

const CHAR g_strModuleOutputSeparator[] = ", ";
const INT  g_iCharCountModuleOutputSeparator = sizeof(g_strModuleOutputSeparator) - 1;

const INT  g_iModuleOutputMaxChars = g_iCharCountModuleOutputStart +
                                     MAX_PATH +       // module name
                                     10 * 3 +         // three hex values
                                     g_iCharCountModuleOutputSeparator * 2 +
                                     g_iCharCountModuleOutputEnd +
                                     1;               // null terminator




//-----------------------------------------------------------------------------
// Each stack dump has a format like the following:
//    Stack [0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, ..., 0xFFFFFFFF]
//-----------------------------------------------------------------------------

// Maximum number of levels of the call stack to collect.
// 32 is the most that can be collected via DmCaptureStackBackTrace.
const ULONG g_ulNumFramesToCapture = 32;

const CHAR g_strStackOutputStart[] = "Stack [";
const INT  g_iCharCountStackOutputStart = sizeof(g_strStackOutputStart) - 1;

const CHAR g_strStackOutputEnd[] = "]\r\n";
const INT  g_iCharCountStackOutputEnd = sizeof(g_strStackOutputEnd) - 1;

const CHAR g_strStackOutputSeparator[] = ", ";
const INT  g_iCharCountStackOutputSeparator = sizeof(g_strStackOutputSeparator) - 1;

const INT  g_iStackOutputMaxChars = g_iCharCountStackOutputStart +
                                    g_ulNumFramesToCapture *
                                        (10 + g_iCharCountStackOutputSeparator) +
                                    sizeof(g_strStackOutputEnd) +
                                    1;
