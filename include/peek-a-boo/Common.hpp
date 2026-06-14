#pragma once

#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

// NTAPI functions return NTSTATUS instead of BOOL or HRESULT.
typedef LONG NTSTATUS;
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

// These are often missing or inclomplete in winternl.h
typedef struct _UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLegth;
    PWCH Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

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
    HANDLE ParentProcessHandle,
    HANDLE SectionHandle,
    ULONG ProcessFlags,
    ULONG CreateFlags,
    PVOID ProcessParameters,
    ULONG ProcessParametersSize,
    SIZE_T ProcessSize,
    PVOID ProcessPageOrderAlignment,
    PVOID ProcessPageOrder);

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

class CommonUtils
{
public:
    // Converts a standard Windows wide string (wstring) into a UNICODE_STRING required by NTAPI.
    static UNICODE_STRING ToUnicodeString(const std::wstring &str)
    {
        UNICODE_STRING us;
        us.Buffer = const_cast<PWCH>(str.c_str());
        us.Length = static_cast<USHORT>(str.length() * sizeof(wchar_t));
        us.MaximumLength = us.Length + sizeof(wchar_t);
        return us;
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

        return (NtCreateSection && NtCreateProcessEx && NtCreateThreadEx && NtResumeThread);
    }

private:
    DynamicNT() {} // Private constructor for singleton
};
