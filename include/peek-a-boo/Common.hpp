#pragma once

#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <string>
#include <vector>

// NTAPI functions return NTSTATUS instead of BOOL or HRESULT.
typedef LONG NTSTATUS;
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

#ifndef THREAD_CREATE_FLAGS_CREATE_SUSPENDED
#define THREAD_CREATE_FLAGS_CREATE_SUSPENDED 0x00000001
#endif

// winternl.h only exposes a stub of RTL_USER_PROCESS_PARAMETERS (just ImagePathName
// and CommandLine). The full layout is needed to rebase all embedded string pointers
// when the params block must be copied to a different VA in the remote process.
typedef struct _CURDIR
{
    UNICODE_STRING DosPath;
    HANDLE Handle;
} CURDIR;

typedef struct _RTL_USER_PROCESS_PARAMETERS_FULL
{
    ULONG MaximumLength;
    ULONG Length;
    ULONG Flags;
    ULONG DebugFlags;
    HANDLE ConsoleHandle;
    ULONG ConsoleFlags;
    HANDLE StandardInput;
    HANDLE StandardOutput;
    HANDLE StandardError;
    CURDIR CurrentDirectory;
    UNICODE_STRING DllPath;
    UNICODE_STRING ImagePathName;
    UNICODE_STRING CommandLine;
    PVOID Environment;
    ULONG StartingX;
    ULONG StartingY;
    ULONG CountX;
    ULONG CountY;
    ULONG CountCharsX;
    ULONG CountCharsY;
    ULONG FillAttribute;
    ULONG WindowFlags;
    ULONG ShowWindowFlags;
    UNICODE_STRING WindowTitle;
    UNICODE_STRING DesktopInfo;
    UNICODE_STRING ShellInfo;
    UNICODE_STRING RuntimeData;
} RTL_USER_PROCESS_PARAMETERS_FULL, *PRTL_USER_PROCESS_PARAMETERS_FULL;

// We define these as types so we can assign them to function pointers.
typedef NTSTATUS(NTAPI *pNtCreateSection)(
    PHANDLE SectionHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PLARGE_INTEGER MaximumSize,
    ULONG SectionPageProtection,
    ULONG AlocationAttributes,
    HANDLE FileHandle);

typedef NTSTATUS(NTAPI *pNtCreateProcessEx)(
    PHANDLE ProcessHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    HANDLE ParentProcess,
    ULONG Flags,
    HANDLE SectionHandle,
    HANDLE DebugPort,
    HANDLE ExceptionPort,
    BOOLEAN InJob);

typedef NTSTATUS(NTAPI *pNtCreateThreadEx)(
    PHANDLE ThreadHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    HANDLE ProcessHandle,
    PVOID StartRoutine,
    PVOID Argument,
    ULONG CreateFlags,
    ULONG ZeroBits,
    SIZE_T StackSize,
    SIZE_T MaximumStackSize,
    PVOID AttributeList);

typedef NTSTATUS(NTAPI *pNtResumeThread)(
    HANDLE ThreadHandle,
    PULONG NumSuspend);

typedef NTSTATUS(NTAPI *pNtQueryInformationProcess)(
    HANDLE ProcessHandle,
    PROCESSINFOCLASS ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength);

typedef NTSTATUS(NTAPI *pRtlCreateProcessParametersEx)(
    PRTL_USER_PROCESS_PARAMETERS_FULL *pProcessParameters,
    PUNICODE_STRING ImagePathName,
    PUNICODE_STRING DllPath,
    PUNICODE_STRING CurrentDirectory,
    PUNICODE_STRING CommandLine,
    PVOID Environment,
    PUNICODE_STRING WindowTitle,
    PUNICODE_STRING DesktopInfo,
    PUNICODE_STRING ShellInfo,
    PUNICODE_STRING RuntimeData,
    ULONG Flags);

typedef NTSTATUS(NTAPI *pRtlDestroyProcessParameters)(
    PRTL_USER_PROCESS_PARAMETERS_FULL ProcessParameters);

class CommonUtils
{
public:
    // Fills a UNICODE_STRING pointing into `str`. The wstring must outlive the UNICODE_STRING.
    static void ToUnicodeString(const std::wstring &str, UNICODE_STRING &out)
    {
        out.Buffer = const_cast<PWCH>(str.c_str());
        out.Length = static_cast<USHORT>(str.length() * sizeof(wchar_t));
        out.MaximumLength = out.Length + sizeof(wchar_t);
    }

    // Basic logging helper to print NTSTATUS errors in a readable way.
    static void LogStatus(const std::string &operation, NTSTATUS status)
    {
        if (!NT_SUCCESS(status))
        {
            std::cerr << "[!] " << operation << " failed. NTSTATUS: 0x"
                      << std::hex << status << std::dec << std::endl;
        }
        else
        {
            std::cout << "[+] " << operation << " succeeded." << std::endl;
        }
    }
};

class DynamicNT
{
public:
    pNtCreateSection NtCreateSection = nullptr;
    pNtCreateProcessEx NtCreateProcessEx = nullptr;
    pNtCreateThreadEx NtCreateThreadEx = nullptr;
    pNtResumeThread NtResumeThread = nullptr;
    pNtQueryInformationProcess NtQueryInformationProcess = nullptr;
    pRtlCreateProcessParametersEx RtlCreateProcessParametersEx = nullptr;
    pRtlDestroyProcessParameters RtlDestroyProcessParameters = nullptr;

    // Singleton instance
    static DynamicNT &Instance()
    {
        static DynamicNT instance;
        return instance;
    }

    // Initialize by resolving addresses from ntdll.dll
    bool Initialize()
    {
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (!hNtdll)
            return false;

        NtCreateSection = (pNtCreateSection)GetProcAddress(hNtdll, "NtCreateSection");
        NtCreateProcessEx = (pNtCreateProcessEx)GetProcAddress(hNtdll, "NtCreateProcessEx");
        NtCreateThreadEx = (pNtCreateThreadEx)GetProcAddress(hNtdll, "NtCreateThreadEx");
        NtResumeThread = (pNtResumeThread)GetProcAddress(hNtdll, "NtResumeThread");
        NtQueryInformationProcess = (pNtQueryInformationProcess)GetProcAddress(hNtdll, "NtQueryInformationProcess");
        RtlCreateProcessParametersEx = (pRtlCreateProcessParametersEx)GetProcAddress(hNtdll, "RtlCreateProcessParametersEx");
        RtlDestroyProcessParameters = (pRtlDestroyProcessParameters)GetProcAddress(hNtdll, "RtlDestroyProcessParameters");

        return (NtCreateSection && NtCreateProcessEx && NtCreateThreadEx &&
                NtResumeThread && NtQueryInformationProcess &&
                RtlCreateProcessParametersEx && RtlDestroyProcessParameters);
    }

private:
    DynamicNT() {} // Private constructor for singleton
};
