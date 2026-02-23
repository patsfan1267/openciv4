// FPKArchive.cpp — Reader for Firaxis Package (.FPK) archives
//
// FPK binary format (little-endian):
//   Header:
//     uint32  version       (always 4)
//     char[4] magic         "FPK_"
//     uint8   encoding      1 = Caesar+1 encoded filenames
//     uint32  entryCount
//   Per entry:
//     uint32  filenameLen
//     char[]  encodedFilename   (each byte incremented by 1)
//     padding to 4-byte boundary
//     uint64  timestamp         (Windows FILETIME)
//     uint32  dataSize
//     uint32  dataOffset        (absolute offset in the FPK file)
//   File data stored raw (uncompressed) at the given offsets.

#include "FPKArchive.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

// Helper: read N bytes from a FILE*
static bool readBytes(FILE* fp, void* buf, size_t n) {
    return fread(buf, 1, n, fp) == n;
}

// Helper: read a little-endian uint32
static bool readU32(FILE* fp, uint32_t& out) {
    return readBytes(fp, &out, 4);
}

// Helper: read a little-endian uint64
static bool readU64(FILE* fp, uint64_t& out) {
    return readBytes(fp, &out, 8);
}

FPKArchive::~FPKArchive() = default;

bool FPKArchive::open(const char* fpkPath) {
    m_entries.clear();
    m_lookup.clear();
    m_fpkPath = fpkPath;

    FILE* fp = fopen(fpkPath, "rb");
    if (!fp) {
        fprintf(stderr, "FPKArchive: cannot open '%s'\n", fpkPath);
        return false;
    }

    // --- Read header ---
    uint32_t version;
    char magic[4];
    uint8_t encoding;
    uint32_t entryCount;

    if (!readU32(fp, version) || !readBytes(fp, magic, 4) ||
        !readBytes(fp, &encoding, 1) || !readU32(fp, entryCount)) {
        fprintf(stderr, "FPKArchive: failed to read header from '%s'\n", fpkPath);
        fclose(fp);
        return false;
    }

    if (memcmp(magic, "FPK_", 4) != 0) {
        fprintf(stderr, "FPKArchive: bad magic in '%s'\n", fpkPath);
        fclose(fp);
        return false;
    }

    m_entries.reserve(entryCount);

    // --- Read entries ---
    std::vector<char> nameBuf;
    for (uint32_t i = 0; i < entryCount; i++) {
        uint32_t fnameLen;
        if (!readU32(fp, fnameLen)) {
            fprintf(stderr, "FPKArchive: truncated entry %u\n", i);
            fclose(fp);
            return false;
        }

        // Read encoded filename
        nameBuf.resize(fnameLen);
        if (!readBytes(fp, nameBuf.data(), fnameLen)) {
            fprintf(stderr, "FPKArchive: truncated filename in entry %u\n", i);
            fclose(fp);
            return false;
        }

        // Decode Caesar+1: each byte was incremented by 1
        std::string filename(fnameLen, '\0');
        if (encoding == 1) {
            for (uint32_t j = 0; j < fnameLen; j++)
                filename[j] = (char)((unsigned char)nameBuf[j] - 1);
        } else {
            filename.assign(nameBuf.data(), fnameLen);
        }

        // Skip padding to 4-byte alignment
        uint32_t pad = (4 - (fnameLen % 4)) % 4;
        if (pad > 0) {
            char padBuf[3];
            if (!readBytes(fp, padBuf, pad)) {
                fclose(fp);
                return false;
            }
        }

        // Read timestamp, data size, data offset
        FPKEntry entry;
        entry.filename = filename;
        if (!readU64(fp, entry.timestamp) ||
            !readU32(fp, entry.dataSize) ||
            !readU32(fp, entry.dataOffset)) {
            fprintf(stderr, "FPKArchive: truncated metadata in entry %u\n", i);
            fclose(fp);
            return false;
        }

        // Build lookup with normalized path
        std::string key = normalizePath(entry.filename);
        m_lookup[key] = (int)m_entries.size();
        m_entries.push_back(std::move(entry));
    }

    fclose(fp);
    fprintf(stderr, "FPKArchive: opened '%s' — %d entries\n", fpkPath, (int)m_entries.size());
    return true;
}

bool FPKArchive::hasFile(const std::string& path) const {
    std::string key = normalizePath(path);
    return m_lookup.find(key) != m_lookup.end();
}

std::vector<uint8_t> FPKArchive::readFile(const std::string& path) const {
    std::string key = normalizePath(path);
    auto it = m_lookup.find(key);
    if (it == m_lookup.end())
        return {};

    const FPKEntry& entry = m_entries[it->second];

    FILE* fp = fopen(m_fpkPath.c_str(), "rb");
    if (!fp)
        return {};

    // Seek to data offset and read
    if (fseek(fp, (long)entry.dataOffset, SEEK_SET) != 0) {
        fclose(fp);
        return {};
    }

    std::vector<uint8_t> data(entry.dataSize);
    if (fread(data.data(), 1, entry.dataSize, fp) != entry.dataSize) {
        fclose(fp);
        return {};
    }

    fclose(fp);
    return data;
}

std::string FPKArchive::normalizePath(const std::string& path) {
    std::string result = path;
    // Convert backslashes to forward slashes
    for (char& c : result) {
        if (c == '\\')
            c = '/';
    }
    // Lowercase
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return result;
}
