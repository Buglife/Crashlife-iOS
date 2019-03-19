//
//  CLKSCrashReportFields.h
//
//  Created by Karl Stenerud on 2012-10-07.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//


#ifndef HDR_CLKSCrashReportFields_h
#define HDR_CLKSCrashReportFields_h


#pragma mark - Report Types -

#define CLKSCrashReportType_Minimal          "minimal"
#define CLKSCrashReportType_Standard         "standard"
#define CLKSCrashReportType_Custom           "custom"


#pragma mark - Memory Types -

#define CLKSCrashMemType_Block               "objc_block"
#define CLKSCrashMemType_Class               "objc_class"
#define CLKSCrashMemType_NullPointer         "null_pointer"
#define CLKSCrashMemType_Object              "objc_object"
#define CLKSCrashMemType_String              "string"
#define CLKSCrashMemType_Unknown             "unknown"


#pragma mark - Exception Types -

#define CLKSCrashExcType_CPPException        "cpp_exception"
#define CLKSCrashExcType_Deadlock            "deadlock"
#define CLKSCrashExcType_Mach                "mach"
#define CLKSCrashExcType_NSException         "nsexception"
#define CLKSCrashExcType_Signal              "signal"
#define CLKSCrashExcType_User                "user"


#pragma mark - Common -

#define CLKSCrashField_Address               "address"
#define CLKSCrashField_Contents              "contents"
#define CLKSCrashField_Exception             "exception"
#define CLKSCrashField_FirstObject           "first_object"
#define CLKSCrashField_Index                 "index"
#define CLKSCrashField_Ivars                 "ivars"
#define CLKSCrashField_Language              "language"
#define CLKSCrashField_Name                  "name"
#define CLKSCrashField_UserInfo              "userInfo"
#define CLKSCrashField_ReferencedObject      "referenced_object"
#define CLKSCrashField_Type                  "type"
#define CLKSCrashField_UUID                  "uuid"
#define CLKSCrashField_Value                 "value"

#define CLKSCrashField_Error                 "error"
#define CLKSCrashField_JSONData              "json_data"


#pragma mark - Notable Address -

#define CLKSCrashField_Class                 "class"
#define CLKSCrashField_LastDeallocObject     "last_deallocated_obj"


#pragma mark - Backtrace -

#define CLKSCrashField_InstructionAddr       "instruction_addr"
#define CLKSCrashField_LineOfCode            "line_of_code"
#define CLKSCrashField_ObjectAddr            "object_addr"
#define CLKSCrashField_ObjectName            "object_name"
#define CLKSCrashField_SymbolAddr            "symbol_addr"
#define CLKSCrashField_SymbolName            "symbol_name"


#pragma mark - Stack Dump -

#define CLKSCrashField_DumpEnd               "dump_end"
#define CLKSCrashField_DumpStart             "dump_start"
#define CLKSCrashField_GrowDirection         "grow_direction"
#define CLKSCrashField_Overflow              "overflow"
#define CLKSCrashField_StackPtr              "stack_pointer"


#pragma mark - Thread Dump -

#define CLKSCrashField_Backtrace             "backtrace"
#define CLKSCrashField_Basic                 "basic"
#define CLKSCrashField_Crashed               "crashed"
#define CLKSCrashField_CurrentThread         "current_thread"
#define CLKSCrashField_DispatchQueue         "dispatch_queue"
#define CLKSCrashField_NotableAddresses      "notable_addresses"
#define CLKSCrashField_Registers             "registers"
#define CLKSCrashField_Skipped               "skipped"
#define CLKSCrashField_Stack                 "stack"


#pragma mark - Binary Image -

#define CLKSCrashField_CPUSubType            "cpu_subtype"
#define CLKSCrashField_CPUType               "cpu_type"
#define CLKSCrashField_ImageAddress          "image_addr"
#define CLKSCrashField_ImageVmAddress        "image_vmaddr"
#define CLKSCrashField_ImageSize             "image_size"
#define CLKSCrashField_ImageMajorVersion     "major_version"
#define CLKSCrashField_ImageMinorVersion     "minor_version"
#define CLKSCrashField_ImageRevisionVersion  "revision_version"


#pragma mark - Memory -

#define CLKSCrashField_Free                  "free"
#define CLKSCrashField_Usable                "usable"


#pragma mark - Error -

#define CLKSCrashField_Backtrace             "backtrace"
#define CLKSCrashField_Code                  "code"
#define CLKSCrashField_CodeName              "code_name"
#define CLKSCrashField_CPPException          "cpp_exception"
#define CLKSCrashField_ExceptionName         "exception_name"
#define CLKSCrashField_Mach                  "mach"
#define CLKSCrashField_NSException           "nsexception"
#define CLKSCrashField_Reason                "reason"
#define CLKSCrashField_Signal                "signal"
#define CLKSCrashField_Subcode               "subcode"
#define CLKSCrashField_UserReported          "user_reported"


#pragma mark - Process State -

#define CLKSCrashField_LastDeallocedNSException "last_dealloced_nsexception"
#define CLKSCrashField_ProcessState             "process"


#pragma mark - App Stats -

#define CLKSCrashField_ActiveTimeSinceCrash  "active_time_since_last_crash"
#define CLKSCrashField_ActiveTimeSinceLaunch "active_time_since_launch"
#define CLKSCrashField_AppActive             "application_active"
#define CLKSCrashField_AppInFG               "application_in_foreground"
#define CLKSCrashField_BGTimeSinceCrash      "background_time_since_last_crash"
#define CLKSCrashField_BGTimeSinceLaunch     "background_time_since_launch"
#define CLKSCrashField_LaunchesSinceCrash    "launches_since_last_crash"
#define CLKSCrashField_SessionsSinceCrash    "sessions_since_last_crash"
#define CLKSCrashField_SessionsSinceLaunch   "sessions_since_launch"


#pragma mark - Report -

#define CLKSCrashField_Crash                 "crash"
#define CLKSCrashField_Debug                 "debug"
#define CLKSCrashField_Diagnosis             "diagnosis"
#define CLKSCrashField_ID                    "id"
#define CLKSCrashField_ProcessName           "process_name"
#define CLKSCrashField_Report                "report"
#define CLKSCrashField_Timestamp             "timestamp"
#define CLKSCrashField_Version               "version"

#pragma mark Minimal
#define CLKSCrashField_CrashedThread         "crashed_thread"

#pragma mark Standard
#define CLKSCrashField_AppStats              "application_stats"
#define CLKSCrashField_BinaryImages          "binary_images"
#define CLKSCrashField_System                "system"
#define CLKSCrashField_Memory                "memory"
#define CLKSCrashField_Threads               "threads"
#define CLKSCrashField_User                  "user"
#define CLKSCrashField_ConsoleLog            "console_log"

#pragma mark Incomplete
#define CLKSCrashField_Incomplete            "incomplete"
#define CLKSCrashField_RecrashReport         "recrash_report"

#pragma mark System
#define CLKSCrashField_AppStartTime          "app_start_time"
#define CLKSCrashField_AppUUID               "app_uuid"
#define CLKSCrashField_BootTime              "boot_time"
#define CLKSCrashField_BundleID              "CFBundleIdentifier"
#define CLKSCrashField_BundleName            "CFBundleName"
#define CLKSCrashField_BundleShortVersion    "CFBundleShortVersionString"
#define CLKSCrashField_BundleVersion         "CFBundleVersion"
#define CLKSCrashField_CPUArch               "cpu_arch"
#define CLKSCrashField_CPUType               "cpu_type"
#define CLKSCrashField_CPUSubType            "cpu_subtype"
#define CLKSCrashField_BinaryCPUType         "binary_cpu_type"
#define CLKSCrashField_BinaryCPUSubType      "binary_cpu_subtype"
#define CLKSCrashField_DeviceAppHash         "device_app_hash"
#define CLKSCrashField_Executable            "CFBundleExecutable"
#define CLKSCrashField_ExecutablePath        "CFBundleExecutablePath"
#define CLKSCrashField_Jailbroken            "jailbroken"
#define CLKSCrashField_KernelVersion         "kernel_version"
#define CLKSCrashField_Machine               "machine"
#define CLKSCrashField_Model                 "model"
#define CLKSCrashField_OSVersion             "os_version"
#define CLKSCrashField_ParentProcessID       "parent_process_id"
#define CLKSCrashField_ProcessID             "process_id"
#define CLKSCrashField_ProcessName           "process_name"
#define CLKSCrashField_Size                  "size"
#define CLKSCrashField_Storage               "storage"
#define CLKSCrashField_SystemName            "system_name"
#define CLKSCrashField_SystemVersion         "system_version"
#define CLKSCrashField_TimeZone              "time_zone"
#define CLKSCrashField_BuildType             "build_type"

#endif
