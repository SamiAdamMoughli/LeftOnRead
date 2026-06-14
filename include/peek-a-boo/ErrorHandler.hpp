#pragma once

#include "Common.hpp"
#include <string>
#include <vector>

// ErrorHandler
// Responsibility: Centralized error logging and resource cleanup.
// All modules should route their error reporting through here so output
// format stays consistent and handles are never silently leaked.
class ErrorHandler
{
public:
    // Logs a Win32 error (GetLastError) with context.
    static void LogWin32Error(const std::string &operation);

    // Logs an NTSTATUS result. No-op if NT_SUCCESS(status).
    static void LogNTStatus(const std::string &operation, NTSTATUS status);

    // Safely closes a Win32 HANDLE and sets it to INVALID_HANDLE_VALUE.
    static void SafeCloseHandle(HANDLE &handle);

    // Closes a list of handles in one call (order: front to back).
    static void CleanupHandles(std::vector<HANDLE *> handles);
};
