#pragma once

#include "Common.hpp"
#include <vector>
#include <string>
#include <fstream>

// PayloadManager
class PayloadManager
{
public:
    // Loads a file from disk into a byte buffer
    static std::vector<uint8_t> LoadPayload(const std::string &filePath);

    // Validates the PE signatures (MZ and PE headers)
    static bool ValidatePE(const std::vector<uint8_t> &buffer);

    // Extracts the Entry Point from the PE optional header
    static uint32_t GetEntryPoint(const std::vector<uint8_t> &buffer);
};
