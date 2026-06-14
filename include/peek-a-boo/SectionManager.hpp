#pragma once

#include "Common.hpp"

// SectionManager
// Responsibility: Manages the creation and configuration of memory sections
// backed by the ghost file. This transforms the file on disk into a
// mapped executable image in memory.
class SectionManager
{
public:
    // Creates an image section from the ghost file handle
    // Returns the handle to the section, or NULL on failure
    static HANDLE CreateImageSection(HANDLE fileHandle);

};
