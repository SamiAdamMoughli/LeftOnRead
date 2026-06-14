#include "PayloadManager.hpp"
#include <iostream>
#include <fstream>
#include <windows.h>

/**
 * Loads a file from disk into a byte buffer.
 * Used to read the initial payload file before it is written to the "ghost" file.
 */
std::vector<uint8_t> PayloadManager::LoadPayload(const std::string &filePath)
{
    std::vector<uint8_t> buffer;

    // Open file in binary mode and move pointer to the end to determine size
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);

    if (!file.is_open())
    {
        std::cerr << "[-] Failed to open payload file: " << filePath << std::endl;
        return buffer;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Resize buffer to fit the file size
    buffer.resize(static_cast<size_t>(size));

    if (!file.read(reinterpret_cast<char *>(buffer.data()), size))
    {
        std::cerr << "[-] Failed to read payload file content." << std::endl;
        buffer.clear();
    }

    return buffer;
}

/**
 * Validates the PE signatures (MZ and PE headers).
 * This ensures that the loaded buffer is actually a valid Windows executable.
 */
bool PayloadManager::ValidatePE(const std::vector<uint8_t> &buffer)
{
    // 1. Basic size check (Must at least hold the DOS header)
    if (buffer.size() < sizeof(IMAGE_DOS_HEADER))
    {
        return false;
    }

    const auto *dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER *>(buffer.data());

    // 2. Check for "MZ" signature (Magic Number)
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        std::cerr << "[-] Invalid DOS signature (Not an MZ executable)." << std::endl;
        return false;
    }

    // 3. Check if the PE header offset is within buffer bounds
    uint32_t peOffset = dosHeader->e_lfanew;
    if (peOffset + sizeof(IMAGE_NT_HEADERS64) > buffer.size())
    {
        std::cerr << "[-] PE header offset points outside of buffer bounds." << std::endl;
        return false;
    }

    // 4. Check for "PE\0\0" signature at the offset provided by e_lfanew
    const auto *ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(buffer.data() + peOffset);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
    {
        std::cerr << "[-] Invalid PE signature." << std::endl;
        return false;
    }

    return true;
}

/**
 * Extracts the Entry Point from the PE optional header.
 * The entry point is the Relative Virtual Address (RVA) where execution begins.
 */
uint32_t PayloadManager::GetEntryPoint(const std::vector<uint8_t> &buffer)
{
    if (buffer.empty())
        return 0;

    const auto *dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER *>(buffer.data());

    // Calculate the position of the NT Headers using the e_lfanew offset from DOS header
    uint32_t peOffset = dosHeader->e_lfanew;

    // Ensure we don't read past the end of the buffer
    if (peOffset + sizeof(IMAGE_NT_HEADERS64) > buffer.size())
    {
        std::cerr << "[-] Error: Buffer too small to extract entry point." << std::endl;
        return 0;
    }

    const auto *ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(buffer.data() + peOffset);

    // AddressOfEntryPoint is the RVA of the entry point
    return ntHeaders->OptionalHeader.AddressOfEntryPoint;
}
