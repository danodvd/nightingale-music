#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <unordered_set>
#include <vector>

struct ScanStats {
    size_t filesScanned = 0;
    size_t audioFilesFound = 0;
    size_t errorsSkipped = 0;
};

class FileScanner {
public:
    using FileFoundCallback = std::function<void(const std::filesystem::path&)>;
    using ProgressCallback = std::function<void(size_t scannedCount, const std::filesystem::path& currentFile)>;

    explicit FileScanner(std::unordered_set<std::string> supportedExtensions = {
        ".mp3", ".flac", ".ogg", ".opus", ".wav", ".m4a", ".mp4"
    });

    [[nodiscard]] std::vector<std::filesystem::path> scanDirectories(
        const std::vector<std::filesystem::path>& rootPaths,
        const std::atomic<bool>& cancelFlag
    ) const;

    ScanStats scanDirectoriesWithCallback(
        const std::vector<std::filesystem::path>& rootPaths,
        const FileFoundCallback& onFileFound,
        const ProgressCallback& onProgress,
        const std::atomic<bool>& cancelFlag
    ) const;

private:
    std::unordered_set<std::string> supportedExtensions_;

    [[nodiscard]] bool hasSupportedExtension(const std::filesystem::path& path) const;
};