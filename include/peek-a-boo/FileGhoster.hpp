#pragma once

#include "Common.hpp"
#include <string>
#include <vector>

// FileGhoster
// Responsibility: Handles the file operations required for the ghosting technique,
// specifically creating a file and marking it for deletion so it disappears
// once the handle is closed.
class FileGhoster
{
public:
    // Creates a file and immediately marks it for deletion
    // Returns the handle to the ghost file, or INVALID_HANDLE_VALUE on failure
    static HANDLE CreateGhostFile(const std::wstring &filePath);

    // Writes the provided payload buffer to the ghost file
    static bool WritePayloadToFile(HANDLE fileHandle, const std::vector<uint8_t> &payload);

    // Closes the file handle, which triggers the actual deletion of the file from disk
    static void CloseGhostFile(HANDLE &fileHandle);
};
