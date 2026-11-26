
#include "FileScanner.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

FileScanner::FileScanner(
    std::unordered_set<std::string> supportedExtensions
) : supportedExtensions_(std::move(supportedExtensions)) {

    // Normalize extensions to lowercase.
    std::unordered_set<std::string> normalized;

    for (auto extension : supportedExtensions_) {
        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            }
        );

        normalized.insert(std::move(extension));
    }

    supportedExtensions_ = std::move(normalized);
}

bool FileScanner::hasSupportedExtension(
    const std::filesystem::path& path
) const {

    std::string extension = path.extension().string();

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );

    return supportedExtensions_.find(extension) != supportedExtensions_.end();
}

std::vector<std::filesystem::path> FileScanner::scanDirectories(
    const std::vector<std::filesystem::path>& rootPaths,
    const std::atomic<bool>& cancelFlag
) const {

    std::vector<std::filesystem::path> results;

    scanDirectoriesWithCallback(
        rootPaths,
        [&results](const std::filesystem::path& file) {
            results.push_back(file);
        },
        nullptr,
        cancelFlag
    );

    return results;
}

ScanStats FileScanner::scanDirectoriesWithCallback(
    const std::vector<std::filesystem::path>& rootPaths,
    const FileFoundCallback& onFileFound,
    const ProgressCallback& onProgress,
    const std::atomic<bool>& cancelFlag
) const {

    ScanStats stats;

    for (const auto& root : rootPaths) {

        if (cancelFlag.load()) {
            break;
        }

        std::error_code ec;

        if (!std::filesystem::exists(root, ec) || ec) {
            ++stats.errorsSkipped;
            continue;
        }

        if (!std::filesystem::is_directory(root, ec) || ec) {
            ++stats.errorsSkipped;
            continue;
        }

        std::filesystem::recursive_directory_iterator it(
            root,
            std::filesystem::directory_options::skip_permission_denied,
            ec
        );

        std::filesystem::recursive_directory_iterator end;

        if (ec) {
            ++stats.errorsSkipped;
            continue;
        }

        while (it != end) {

            if (cancelFlag.load()) {
                return stats;
            }

            const auto currentPath = it->path();

            std::error_code fileEc;

            if (it->is_regular_file(fileEc) && !fileEc) {

                ++stats.filesScanned;

                if (hasSupportedExtension(currentPath)) {

                    ++stats.audioFilesFound;

                    if (onFileFound) {
                        onFileFound(currentPath);
                    }
                }

                if (onProgress) {
                    onProgress(stats.filesScanned, currentPath);
                }

            } else if (fileEc) {
                ++stats.errorsSkipped;
            }

            it.increment(ec);

            if (ec) {
                ++stats.errorsSkipped;
                ec.clear();
            }
        }
    }

    return stats;
}
