#include "SectionManager.hpp"
#include <iostream>

/**
 * Creates a memory section object backed by the ghost file.
 * By using SEC_IMAGE, the Windows kernel parses the PE headers and
 * maps the sections (text, data, etc.) into the section object.
 */
HANDLE SectionManager::CreateImageSection(HANDLE fileHandle)
{
    if (fileHandle == INVALID_HANDLE_VALUE || fileHandle == NULL)
    {
        std::cerr << "[-] Invalid file handle provided to SectionManager." << std::endl;
        return NULL;
    }

    HANDLE hSection = NULL;
    OBJECT_ATTRIBUTES objAttr;
    InitializeObjectAttributes(&objAttr, NULL, 0, NULL, NULL);

    // We use the DynamicNT singleton to call the resolved NtCreateSection function
    // Parameters:
    // - SectionHandle: Output handle to the created section
    // - DesiredAccess: SECTION_ALL_ACCESS to allow mapping and process creation
    // - ObjectAttributes: Attributes for the section object
    // - MaximumSize: NULL because SEC_IMAGE determines the size from PE headers
    // - SectionPageProtection: PAGE_EXECUTE_READWRITE (standard for initial image creation)
    // - AllocationAttributes: SEC_IMAGE (Crucial: this tells Windows it's a PE file)
    // - FileHandle: The handle to our ghost file
    NTSTATUS status = DynamicNT::Instance().NtCreateSection(
        &hSection,
        SECTION_ALL_ACCESS,
        &objAttr,
        NULL,
        PAGE_READONLY,
        SEC_IMAGE,
        fileHandle);

    if (!NT_SUCCESS(status))
    {
        CommonUtils::LogStatus("NtCreateSection", status);
        return NULL;
    }

    std::cout << "[+] Image section created successfully." << std::endl;
    return hSection;
}

