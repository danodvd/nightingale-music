#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>

struct RawTrackMetadata {
    std::filesystem::path filePath;
    std::string title;
    std::string albumTitle;
    std::vector<std::string> artists;
    std::vector<std::string> albumArtists;
    std::string genre;
    uint32_t year = 0;
    uint32_t trackNumber = 0;
    uint32_t discNumber = 0;
    uint32_t durationMs = 0;
    uint32_t bitrateKbps = 0;
    uint32_t sampleRateHz = 0;
    int64_t lastModifiedTimestamp = 0;
};

enum class CoverArtType {
    Other,
    FileIcon,
    OtherFileIcon,
    FrontCover,
    BackCover,
    Leaflet,
    Media,
    LeadArtist,
    Artist,
    Conductor,
    Band,
    Composer,
    Lyricist,
    RecordingLocation,
    DuringRecording,
    DuringPerformance,
    MovieCapture,
    BrightColouredFish,
    Illustration,
    BandLogo,
    PublisherLogo,
    Unknown
};

struct RawCoverArt {
    CoverArtType type;
    std::string mimeType;
    std::vector<unsigned char> data;
    std::string description;
};


class TagReader {
public:
    TagReader() = default;
    ~TagReader() = default;

    [[nodiscard]] std::optional<RawTrackMetadata> readMetadata(const std::filesystem::path& filePath) const;

    [[nodiscard]] std::vector<RawCoverArt> extractCoverArt(const std::filesystem::path& filePath) const;

    static CoverArtType convertPictureType(TagLib::ID3v2::AttachedPictureFrame::Type type);

    [[nodiscard]] static bool isSupportedFormat(const std::filesystem::path& filePath);
};