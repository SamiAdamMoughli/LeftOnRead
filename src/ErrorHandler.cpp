#include "ErrorHandler.hpp"
#include <iostream>

void ErrorHandler::LogWin32Error(const std::string &operation)
{
    DWORD error = GetLastError();

    // Ask Windows to format the error code into a human-readable string.
    LPSTR messageBuffer = nullptr;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&messageBuffer),
        0,
        NULL);

    if (size > 0 && messageBuffer)
    {
        // Strip the trailing newline FormatMessage appends.
        while (size > 0 && (messageBuffer[size - 1] == '\n' || messageBuffer[size - 1] == '\r'))
            messageBuffer[--size] = '\0';

        std::cerr << "[-] " << operation << " failed. Error " << error
                  << ": " << messageBuffer << std::endl;
        LocalFree(messageBuffer);
    }
    else
    {
        std::cerr << "[-] " << operation << " failed. Error: 0x"
                  << std::hex << error << std::dec << std::endl;
    }
}

void ErrorHandler::LogNTStatus(const std::string &operation, NTSTATUS status)
{
    if (NT_SUCCESS(status))
    {
        std::cout << "[+] " << operation << " succeeded." << std::endl;
        return;
    }

    // NTSTATUS values come from ntdll, so we ask FormatMessage to look there.
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    LPSTR messageBuffer = nullptr;
    DWORD size = 0;

    if (hNtdll)
    {
        size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
            hNtdll,
            static_cast<DWORD>(status),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&messageBuffer),
            0,
            NULL);
    }

    if (size > 0 && messageBuffer)
    {
        while (size > 0 && (messageBuffer[size - 1] == '\n' || messageBuffer[size - 1] == '\r'))
            messageBuffer[--size] = '\0';

        std::cerr << "[-] " << operation << " failed. NTSTATUS 0x"
                  << std::hex << static_cast<DWORD>(status) << std::dec
                  << ": " << messageBuffer << std::endl;
        LocalFree(messageBuffer);
    }
    else
    {
        std::cerr << "[-] " << operation << " failed. NTSTATUS: 0x"
                  << std::hex << static_cast<DWORD>(status) << std::dec << std::endl;
    }
}

void ErrorHandler::SafeCloseHandle(HANDLE &handle)
{
    if (handle != NULL && handle != INVALID_HANDLE_VALUE)
    {
        if (!CloseHandle(handle))
            LogWin32Error("CloseHandle");
        handle = INVALID_HANDLE_VALUE;
    }
}

void ErrorHandler::CleanupHandles(std::vector<HANDLE *> handles)
{
    for (HANDLE *h : handles)
    {
        if (h)
            SafeCloseHandle(*h);
    }
}
