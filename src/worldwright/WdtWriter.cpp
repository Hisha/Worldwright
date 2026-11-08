#include "worldwright/WdtWriter.h"

#include <atomic>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace worldwright {
namespace {

// WoW chunk identifiers are stored as little-endian 32-bit values. Building the
// constant with the first character in the most significant byte means that
// appendU32() writes the characters in reverse order on disk ("REVM" for MVER),
// matching real 3.3.5a files.
constexpr std::uint32_t fourcc(char a, char b, char c, char d) {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(a)) << 24) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 8) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(d));
}

constexpr std::uint32_t kMver = fourcc('M', 'V', 'E', 'R');
constexpr std::uint32_t kMphd = fourcc('M', 'P', 'H', 'D');
constexpr std::uint32_t kMain = fourcc('M', 'A', 'I', 'N');

constexpr std::uint32_t kTileExistsFlag = 0x0001u;
constexpr std::uint32_t kMphdFlags = 0u;
constexpr std::size_t kMphdReservedSize = WdtWriter::kMphdPayloadSize - 4;

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

void appendChunk(std::vector<std::uint8_t>& out, std::uint32_t id,
                 const std::vector<std::uint8_t>& payload) {
    appendU32(out, id);
    appendU32(out, static_cast<std::uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
}

std::filesystem::path parentDirectory(const std::filesystem::path& outputPath) {
    const auto parent = outputPath.parent_path();
    return parent.empty() ? std::filesystem::path(".") : parent;
}

// Builds a temporary name in the destination directory. The name is not part of
// the output and therefore does not affect determinism.
std::filesystem::path makeTempPath(const std::filesystem::path& outputPath) {
    static std::atomic<unsigned long long> counter{0};

    const auto directory = parentDirectory(outputPath);
    const std::string base = outputPath.filename().string();

    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto serial =
            counter.fetch_add(1, std::memory_order_relaxed);
        const auto candidate =
            directory / ("." + base + ".worldwright-" +
                         std::to_string(serial) + ".tmp");

        std::error_code error;
        if (!std::filesystem::exists(candidate, error)) {
            return candidate;
        }
    }

    // Refuse to guess rather than risk truncating an unrelated existing file.
    return {};
}

std::string describePath(const std::filesystem::path& path) {
    return path.empty() ? std::string("<empty path>") : path.string();
}

}  // namespace

std::vector<std::uint8_t> WdtWriter::serialize(
    const WorldDefinition& world) {
    std::vector<std::uint8_t> output;
    output.reserve(kSerializedSize);

    std::vector<std::uint8_t> mverPayload;
    mverPayload.reserve(4);
    appendU32(mverPayload, kVersion);
    appendChunk(output, kMver, mverPayload);

    std::vector<std::uint8_t> mphdPayload;
    mphdPayload.reserve(kMphdPayloadSize);
    appendU32(mphdPayload, kMphdFlags);
    mphdPayload.insert(mphdPayload.end(), kMphdReservedSize, 0);
    appendChunk(output, kMphd, mphdPayload);

    std::vector<std::uint8_t> mainPayload;
    mainPayload.reserve(kMainPayloadSize);
    for (int y = 0; y < WorldDefinition::GridHeight; ++y) {
        for (int x = 0; x < WorldDefinition::GridWidth; ++x) {
            appendU32(mainPayload,
                      world.hasTile(x, y) ? kTileExistsFlag : 0u);
            // The second uint32 is the runtime-only async field. Real 3.3.5a
            // terrain WDTs store zero here, and the client regenerates it.
            appendU32(mainPayload, 0u);
        }
    }
    appendChunk(output, kMain, mainPayload);

    return output;
}

bool WdtWriter::save(const WorldDefinition& world,
                     const std::filesystem::path& outputPath,
                     const WdtWriteOptions& options) {
    reset();

    if (outputPath.empty() || !outputPath.has_filename()) {
        return fail(WdtWriteError::InvalidPath,
                    "output path does not name a file: " +
                        describePath(outputPath));
    }

    std::error_code error;
    const auto directory = parentDirectory(outputPath);

    const bool directoryExists =
        std::filesystem::exists(directory, error);
    if (error) {
        return fail(WdtWriteError::InvalidPath,
                    "could not inspect output directory: " +
                        describePath(directory) + ": " + error.message());
    }
    if (!directoryExists) {
        return fail(WdtWriteError::InvalidPath,
                    "output directory does not exist: " +
                        describePath(directory));
    }

    error.clear();
    const bool directoryIsDirectory =
        std::filesystem::is_directory(directory, error);
    if (error) {
        return fail(WdtWriteError::InvalidPath,
                    "could not inspect output directory: " +
                        describePath(directory) + ": " + error.message());
    }
    if (!directoryIsDirectory) {
        return fail(WdtWriteError::InvalidPath,
                    "output parent is not a directory: " +
                        describePath(directory));
    }

    error.clear();
    const bool destinationExists =
        std::filesystem::exists(outputPath, error);
    if (error) {
        return fail(WdtWriteError::InvalidPath,
                    "could not inspect output path: " +
                        describePath(outputPath) + ": " + error.message());
    }
    if (destinationExists) {
        error.clear();
        const bool destinationIsDirectory =
            std::filesystem::is_directory(outputPath, error);
        if (error) {
            return fail(WdtWriteError::InvalidPath,
                        "could not inspect output path: " +
                            describePath(outputPath) + ": " + error.message());
        }
        if (destinationIsDirectory) {
            return fail(WdtWriteError::InvalidPath,
                        "output path is a directory: " +
                            describePath(outputPath));
        }
    }
    if (destinationExists && !options.overwrite) {
        return fail(WdtWriteError::OutputExists,
                    "output file already exists (use --overwrite to replace "
                    "it): " +
                        describePath(outputPath));
    }

    // Serialize completely before touching the destination directory. A
    // serialization failure can therefore never truncate an existing file.
    const auto bytes = serialize(world);
    const auto tempPath = makeTempPath(outputPath);
    if (tempPath.empty()) {
        return fail(WdtWriteError::OpenFailed,
                    "could not allocate a temporary file name in: " +
                        describePath(directory));
    }

    {
        std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            return fail(WdtWriteError::OpenFailed,
                        "could not create temporary file: " +
                            describePath(tempPath));
        }

        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        file.flush();

        if (!file) {
            file.close();
            std::error_code removeError;
            std::filesystem::remove(tempPath, removeError);
            return fail(WdtWriteError::WriteFailed,
                        "could not write complete WDT data to: " +
                            describePath(tempPath));
        }

        file.close();
        if (!file) {
            std::error_code removeError;
            std::filesystem::remove(tempPath, removeError);
            return fail(WdtWriteError::WriteFailed,
                        "could not close temporary file: " +
                            describePath(tempPath));
        }
    }

#if defined(_WIN32)
    // std::filesystem::rename does not replace an existing destination on
    // Windows. Removing first is not atomic; see docs/WDT_WRITER.md.
    if (destinationExists) {
        std::filesystem::remove(outputPath, error);
        if (error) {
            std::error_code removeError;
            std::filesystem::remove(tempPath, removeError);
            return fail(WdtWriteError::ReplaceFailed,
                        "could not remove existing output file: " +
                            describePath(outputPath) + ": " + error.message());
        }
    }
#endif

    std::filesystem::rename(tempPath, outputPath, error);
    if (error) {
        std::error_code removeError;
        std::filesystem::remove(tempPath, removeError);
        return fail(WdtWriteError::ReplaceFailed,
                    "could not replace output file: " +
                        describePath(outputPath) + ": " + error.message());
    }

    return true;
}

WdtWriteError WdtWriter::error() const noexcept {
    return m_error;
}

const std::string& WdtWriter::errorMessage() const noexcept {
    return m_errorMessage;
}

const char* WdtWriter::errorName(WdtWriteError error) noexcept {
    switch (error) {
        case WdtWriteError::None:
            return "None";
        case WdtWriteError::InvalidPath:
            return "InvalidPath";
        case WdtWriteError::OutputExists:
            return "OutputExists";
        case WdtWriteError::OpenFailed:
            return "OpenFailed";
        case WdtWriteError::WriteFailed:
            return "WriteFailed";
        case WdtWriteError::ReplaceFailed:
            return "ReplaceFailed";
    }
    return "Unknown";
}

bool WdtWriter::fail(WdtWriteError error, std::string message) {
    m_error = error;
    m_errorMessage = std::move(message);
    return false;
}

void WdtWriter::reset() noexcept {
    m_error = WdtWriteError::None;
    m_errorMessage.clear();
}

}  // namespace worldwright
