#pragma once
// FPKArchive — Reader for Firaxis Package (.FPK) archives
//
// FPK format: simple uncompressed archive with Caesar+1 encoded filenames.
// Used by Civ4 to bundle DDS textures, NIF models, and animation files.
// Art0.FPK in the base game contains ~7,500 files (317 MB).
//
// Files are stored raw (no compression). The only "encryption" is that
// each byte of the filename is incremented by 1 (Caesar cipher).

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

struct FPKEntry {
    std::string filename;   // decoded filename (lowercase, backslash separators)
    uint64_t timestamp;     // Windows FILETIME
    uint32_t dataSize;      // file size in bytes
    uint32_t dataOffset;    // absolute offset in the FPK file
};

class FPKArchive {
public:
    FPKArchive() = default;
    ~FPKArchive();

    // Open and index an FPK file. Returns true on success.
    bool open(const char* fpkPath);

    // Check if a file exists in the archive (case-insensitive, uses forward slashes)
    bool hasFile(const std::string& path) const;

    // Read a file from the archive into a buffer. Returns empty vector on failure.
    std::vector<uint8_t> readFile(const std::string& path) const;

    // Get the number of entries
    int entryCount() const { return (int)m_entries.size(); }

    // Get all entries (for debugging/listing)
    const std::vector<FPKEntry>& entries() const { return m_entries; }

private:
    std::string m_fpkPath;
    std::vector<FPKEntry> m_entries;
    // Map from normalized path (lowercase, forward slashes) to entry index
    std::unordered_map<std::string, int> m_lookup;

    // Normalize a path: lowercase, convert backslash to forward slash
    static std::string normalizePath(const std::string& path);
};
