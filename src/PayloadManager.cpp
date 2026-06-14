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

    // 3. Check if the PE header offset is within buffer bounds (use the smaller
    //    IMAGE_NT_HEADERS32 size here — both 32 and 64 share the same Signature field).
    uint32_t peOffset = dosHeader->e_lfanew;
    if (peOffset + sizeof(IMAGE_NT_HEADERS32) > buffer.size())
    {
        std::cerr << "[-] PE header offset points outside of buffer bounds." << std::endl;
        return false;
    }

    // 4. Check for "PE\0\0" signature at the offset provided by e_lfanew
    const auto *ntHeaders32 = reinterpret_cast<const IMAGE_NT_HEADERS32 *>(buffer.data() + peOffset);
    if (ntHeaders32->Signature != IMAGE_NT_SIGNATURE)
    {
        std::cerr << "[-] Invalid PE signature." << std::endl;
        return false;
    }

    // 5. Confirm the optional header magic matches a supported architecture.
    WORD magic = ntHeaders32->OptionalHeader.Magic;
    if (magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC && magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        std::cerr << "[-] Unrecognised optional header magic: 0x" << std::hex << magic << std::dec << std::endl;
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

    // Minimum check: IMAGE_NT_HEADERS32 is the smaller of the two layouts and
    // contains the Magic field we need to branch on.
    if (peOffset + sizeof(IMAGE_NT_HEADERS32) > buffer.size())
    {
        std::cerr << "[-] Error: Buffer too small to extract entry point." << std::endl;
        return 0;
    }

    const auto *ntHeaders32 = reinterpret_cast<const IMAGE_NT_HEADERS32 *>(buffer.data() + peOffset);

    if (ntHeaders32->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        if (peOffset + sizeof(IMAGE_NT_HEADERS64) > buffer.size())
        {
            std::cerr << "[-] Error: Buffer too small for PE64 headers." << std::endl;
            return 0;
        }
        const auto *ntHeaders64 = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(buffer.data() + peOffset);
        return ntHeaders64->OptionalHeader.AddressOfEntryPoint;
    }

    return ntHeaders32->OptionalHeader.AddressOfEntryPoint;
}
