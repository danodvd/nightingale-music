#include "ConfigManager.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <locale>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

constexpr std::string_view kAppDirName = "MusicPlayer";

std::string trim(std::string_view sv) {
    size_t start = 0;
    while (start < sv.size() && std::isspace(static_cast<unsigned char>(sv[start]))) {
        ++start;
    }
    size_t end = sv.size();
    while (end > start && std::isspace(static_cast<unsigned char>(sv[end - 1]))) {
        --end;
    }
    return std::string(sv.substr(start, end - start));
}

std::string toLower(std::string_view sv) {
    std::string out(sv);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::optional<bool> parseBool(std::string_view value) {
    const std::string lower = toLower(trim(value));
    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
        return false;
    }
    return std::nullopt;
}

std::optional<float> parseFloatClassic(const std::string& value) {
    std::istringstream iss(trim(value));
    iss.imbue(std::locale::classic());
    float result = 0.0f;
    if (!(iss >> result)) {
        return std::nullopt;
    }
    char leftover = '\0';
    if (iss >> leftover) {
        return std::nullopt;
    }
    return result;
}

std::string formatFloatClassic(float value) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << value;
    return oss.str();
}

std::string_view repeatModeToString(RepeatMode mode) {
    switch (mode) {
        case RepeatMode::None: return "none";
        case RepeatMode::One:  return "one";
        case RepeatMode::All:  return "all";
    }
    return "none";
}

RepeatMode stringToRepeatMode(std::string_view value) {
    const std::string lower = toLower(trim(value));
    if (lower == "one") return RepeatMode::One;
    if (lower == "all") return RepeatMode::All;
    return RepeatMode::None;
}

std::optional<std::string> getEnvVar(const char* name) {
#ifdef _WIN32
    char* raw = nullptr;
    size_t len = 0;
    if (_dupenv_s(&raw, &len, name) != 0 || raw == nullptr) {
        return std::nullopt;
    }
    std::string val(raw);
    std::free(raw);
    return val.empty() ? std::nullopt : std::optional<std::string>(std::move(val));
#else
    const char* raw = std::getenv(name);
    if (raw == nullptr || *raw == '\0') {
        return std::nullopt;
    }
    return std::string(raw);
#endif
}

std::filesystem::path normalizePath(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    return ec ? path.lexically_normal() : canonical;
}

bool writeConfigToFile(const std::filesystem::path& targetPath, const AppConfig& cfg) {
    if (targetPath.empty()) {
        return false;
    }

    std::error_code ec;
    if (targetPath.has_parent_path()) {
        std::filesystem::create_directories(targetPath.parent_path(), ec);
        if (ec) {
            return false;
        }
    }

    std::filesystem::path tempPath = targetPath;
    tempPath += ".tmp";

    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }

        out << "# MusicPlayer Configuration\n\n";
        out << "database_path = " << cfg.databasePath.generic_string() << "\n";
        out << "cover_cache_dir = " << cfg.coverCacheDir.generic_string() << "\n";
        out << "scan_on_startup = " << (cfg.scanOnStartup ? "true" : "false") << "\n\n";

        if (cfg.libraryPaths.empty()) {
            out << "library_path =\n";
        } else {
            for (const auto& dir : cfg.libraryPaths) {
                out << "library_path = " << dir.generic_string() << "\n";
            }
        }

        out << "\n";
        out << "default_volume = " << formatFloatClassic(cfg.defaultVolume) << "\n";
        out << "shuffle_enabled = " << (cfg.shuffleEnabled ? "true" : "false") << "\n";
        out << "repeat_mode = " << repeatModeToString(cfg.repeatMode) << "\n";
        out << "preferred_audio_device = " << cfg.preferredAudioDevice << "\n\n";

        out << "fetch_online_covers = " << (cfg.fetchOnlineCovers ? "true" : "false") << "\n";
        out << "fetch_lyrics = " << (cfg.fetchLyrics ? "true" : "false") << "\n";

        out.flush();
        if (!out.good()) {
            return false;
        }
    }

    std::filesystem::rename(tempPath, targetPath, ec);
    if (ec) {
        // En Windows std::filesystem::rename falla si targetPath ya existe en algunos compiladores antiguos
        std::filesystem::remove(targetPath, ec);
        ec.clear();
        std::filesystem::rename(tempPath, targetPath, ec);
        if (ec) {
            std::filesystem::remove(tempPath, ec);
            return false;
        }
    }

    return true;
}

} // namespace

ConfigManager::ConfigManager(std::filesystem::path configFilePath)
    : configFilePath_(std::move(configFilePath)) {
    applyPlatformDefaults();
}

void ConfigManager::applyPlatformDefaults() {
    config_ = AppConfig{};

#ifdef _WIN32
    std::filesystem::path baseDataDir;
    if (auto localAppData = getEnvVar("LOCALAPPDATA")) {
        baseDataDir = std::filesystem::path(*localAppData) / kAppDirName;
    } else if (auto userProfile = getEnvVar("USERPROFILE")) {
        baseDataDir = std::filesystem::path(*userProfile) / "AppData" / "Local" / kAppDirName;
    } else {
        baseDataDir = std::filesystem::path(".") / "data";
    }

    config_.databasePath = baseDataDir / "library.db";
    config_.coverCacheDir = baseDataDir / "covers";

    if (auto userProfile = getEnvVar("USERPROFILE")) {
        std::filesystem::path defaultMusic = std::filesystem::path(*userProfile) / "Music";
        std::error_code ec;
        if (std::filesystem::exists(defaultMusic, ec)) {
            config_.libraryPaths.push_back(normalizePath(defaultMusic));
        }
    }
#else
    std::filesystem::path dataHome;
    if (auto xdgData = getEnvVar("XDG_DATA_HOME")) {
        dataHome = std::filesystem::path(*xdgData) / kAppDirName;
    } else if (auto home = getEnvVar("HOME")) {
        dataHome = std::filesystem::path(*home) / ".local" / "share" / kAppDirName;
    } else {
        dataHome = std::filesystem::path(".") / "data";
    }

    std::filesystem::path cacheHome;
    if (auto xdgCache = getEnvVar("XDG_CACHE_HOME")) {
        cacheHome = std::filesystem::path(*xdgCache) / kAppDirName;
    } else if (auto home = getEnvVar("HOME")) {
        cacheHome = std::filesystem::path(*home) / ".cache" / kAppDirName;
    } else {
        cacheHome = std::filesystem::path(".") / "cache";
    }

    config_.databasePath = dataHome / "library.db";
    config_.coverCacheDir = cacheHome / "covers";

    if (auto home = getEnvVar("HOME")) {
        std::error_code ec;
        std::filesystem::path musicEn = std::filesystem::path(*home) / "Music";
        std::filesystem::path musicEs = std::filesystem::path(*home) / "Música";
        if (std::filesystem::exists(musicEn, ec)) {
            config_.libraryPaths.push_back(normalizePath(musicEn));
        } else if (std::filesystem::exists(musicEs, ec)) {
            config_.libraryPaths.push_back(normalizePath(musicEs));
        }
    }
#endif
}

bool ConfigManager::load() {
    AppConfig snapshot;
    ConfigChangedCallback callback;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        applyPlatformDefaults();

        std::error_code ec;
        if (!std::filesystem::exists(configFilePath_, ec)) {
            // Primer arranque: persistimos la configuración por defecto
            if (!writeConfigToFile(configFilePath_, config_)) {
                return false;
            }
            snapshot = config_;
            callback = onConfigChanged_;
        } else {
            std::ifstream in(configFilePath_);
            if (!in.is_open()) {
                return false;
            }

            bool libraryKeySeen = false;
            std::string line;

            while (std::getline(in, line)) {
                const std::string trimmed = trim(line);
                if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
                    continue;
                }

                const size_t eqPos = trimmed.find('=');
                if (eqPos == std::string::npos) {
                    continue;
                }

                const std::string key = toLower(trim(std::string_view(trimmed).substr(0, eqPos)));
                const std::string val = trim(std::string_view(trimmed).substr(eqPos + 1));

                if (key == "database_path" && !val.empty()) {
                    config_.databasePath = std::filesystem::path(val);
                } else if (key == "cover_cache_dir" && !val.empty()) {
                    config_.coverCacheDir = std::filesystem::path(val);
                } else if (key == "library_path") {
                    if (!libraryKeySeen) {
                        config_.libraryPaths.clear();
                        libraryKeySeen = true;
                    }
                    if (!val.empty()) {
                        const auto norm = normalizePath(std::filesystem::path(val));
                        if (std::find(config_.libraryPaths.begin(), config_.libraryPaths.end(), norm) == config_.libraryPaths.end()) {
                            config_.libraryPaths.push_back(norm);
                        }
                    }
                } else if (key == "scan_on_startup") {
                    if (auto b = parseBool(val)) config_.scanOnStartup = *b;
                } else if (key == "default_volume") {
                    if (auto f = parseFloatClassic(val)) {
                        config_.defaultVolume = std::clamp(*f, 0.0f, 1.0f);
                    }
                } else if (key == "shuffle_enabled") {
                    if (auto b = parseBool(val)) config_.shuffleEnabled = *b;
                } else if (key == "repeat_mode") {
                    config_.repeatMode = stringToRepeatMode(val);
                } else if (key == "preferred_audio_device") {
                    config_.preferredAudioDevice = val;
                } else if (key == "fetch_online_covers") {
                    if (auto b = parseBool(val)) config_.fetchOnlineCovers = *b;
                } else if (key == "fetch_lyrics") {
                    if (auto b = parseBool(val)) config_.fetchLyrics = *b;
                }
            }

            snapshot = config_;
            callback = onConfigChanged_;
        }
    }

    if (callback) {
        callback(snapshot);
    }
    return true;
}

bool ConfigManager::save() const {
    AppConfig snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot = config_;
    }
    return writeConfigToFile(configFilePath_, snapshot);
}

void ConfigManager::resetToDefaults() {
    AppConfig snapshot;
    ConfigChangedCallback callback;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        applyPlatformDefaults();
        writeConfigToFile(configFilePath_, config_);
        snapshot = config_;
        callback = onConfigChanged_;
    }

    if (callback) {
        callback(snapshot);
    }
}

AppConfig ConfigManager::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void ConfigManager::updateConfig(const std::function<void(AppConfig&)>& updater) {
    if (!updater) {
        return;
    }

    AppConfig snapshot;
    ConfigChangedCallback callback;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        updater(config_);
        config_.defaultVolume = std::clamp(config_.defaultVolume, 0.0f, 1.0f);
        writeConfigToFile(configFilePath_, config_);
        snapshot = config_;
        callback = onConfigChanged_;
    }

    if (callback) {
        callback(snapshot);
    }
}

void ConfigManager::addLibraryPath(const std::filesystem::path& path) {
    if (path.empty()) {
        return;
    }

    const auto normalized = normalizePath(path);
    AppConfig snapshot;
    ConfigChangedCallback callback;
    bool changed = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = std::find(config_.libraryPaths.begin(), config_.libraryPaths.end(), normalized);
        if (it == config_.libraryPaths.end()) {
            config_.libraryPaths.push_back(normalized);
            writeConfigToFile(configFilePath_, config_);
            snapshot = config_;
            callback = onConfigChanged_;
            changed = true;
        }
    }

    if (changed && callback) {
        callback(snapshot);
    }
}

void ConfigManager::removeLibraryPath(const std::filesystem::path& path) {
    if (path.empty()) {
        return;
    }

    const auto normalized = normalizePath(path);
    AppConfig snapshot;
    ConfigChangedCallback callback;
    bool changed = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto oldSize = config_.libraryPaths.size();
        config_.libraryPaths.erase(
            std::remove_if(config_.libraryPaths.begin(), config_.libraryPaths.end(),
                [&](const std::filesystem::path& existing) {
                    return existing == path || normalizePath(existing) == normalized;
                }),
            config_.libraryPaths.end()
        );

        if (config_.libraryPaths.size() != oldSize) {
            writeConfigToFile(configFilePath_, config_);
            snapshot = config_;
            callback = onConfigChanged_;
            changed = true;
        }
    }

    if (changed && callback) {
        callback(snapshot);
    }
}

std::vector<std::filesystem::path> ConfigManager::getLibraryPaths() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.libraryPaths;
}

void ConfigManager::setOnConfigChanged(ConfigChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onConfigChanged_ = std::move(callback);
}