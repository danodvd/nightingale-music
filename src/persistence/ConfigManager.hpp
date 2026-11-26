#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

enum class RepeatMode {
    None,
    One,
    All
};

struct AppConfig {
    std::filesystem::path databasePath;
    std::filesystem::path coverCacheDir;
    std::vector<std::filesystem::path> libraryPaths;
    bool scanOnStartup = true;

    float defaultVolume = 0.8f; 
    bool shuffleEnabled = false;
    RepeatMode repeatMode = RepeatMode::None;
    std::string preferredAudioDevice;

    bool fetchOnlineCovers = true;
    bool fetchLyrics = true;
};

class ConfigManager {
public:
    using ConfigChangedCallback = std::function<void(const AppConfig&)>;

    explicit ConfigManager(std::filesystem::path configFilePath);

    bool load();
    bool save() const;
    void resetToDefaults();

    [[nodiscard]] AppConfig getConfig() const;
    void updateConfig(const std::function<void(AppConfig&)>& updater);

    void addLibraryPath(const std::filesystem::path& path);
    void removeLibraryPath(const std::filesystem::path& path);
    [[nodiscard]] std::vector<std::filesystem::path> getLibraryPaths() const;

    void setOnConfigChanged(ConfigChangedCallback callback);

private:
    std::filesystem::path configFilePath_;
    AppConfig config_;
    mutable std::mutex mutex_;
    ConfigChangedCallback onConfigChanged_;

    void applyPlatformDefaults();
};