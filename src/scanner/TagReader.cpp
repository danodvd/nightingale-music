#include "TagReader.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

#include <taglib/attachedpictureframe.h>
#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mpegfile.h>
#include <taglib/tag.h>
#include <taglib/xiphcomment.h>

std::optional<RawTrackMetadata> TagReader::readMetadata(const std::filesystem::path& filePath) const {
    TagLib::FileRef file(filePath.string().c_str());

    if(file.isNull() || !file.tag()) {
        return std::nullopt;
    }

    auto* tag = file.tag();
    auto* properties = file.audioProperties();

    RawTrackMetadata metadata;

    metadata.filePath = filePath;
    metadata.title = tag->title().to8Bit(true);
    metadata.albumTitle = tag->album().to8Bit(true);
    metadata.artists.emplace_back(tag->artist().to8Bit(true));
    metadata.genre = tag->genre().to8Bit(true);
    metadata.year = tag->year();
    metadata.trackNumber = tag->track();
    
    if(properties) {
        metadata.durationMs = properties->lengthInMilliseconds();
        metadata.bitrateKbps = properties->bitrate();
        metadata.sampleRateHz = properties->sampleRate();
    }

    return metadata;
}

std::vector<RawCoverArt> TagReader::extractCoverArt(
    const std::filesystem::path& filePath
) const {
    std::vector<RawCoverArt> result;

    TagLib::FileRef ref(filePath.string().c_str());
    if (ref.isNull() || !ref.file()) {
        return result;
    }

    auto appendFlacPictures = [&](const TagLib::List<TagLib::FLAC::Picture*>& pictures) {
        for (auto* pic : pictures) {
            if (!pic) continue;
            const auto& data = pic->data();
            if (data.isEmpty()) continue;

            RawCoverArt art;
            art.type = convertPictureType(
                static_cast<TagLib::ID3v2::AttachedPictureFrame::Type>(pic->type())
            );
            art.mimeType = pic->mimeType().to8Bit(true);
            art.description = pic->description().to8Bit(true);
            art.data.assign(data.begin(), data.end());
            result.push_back(std::move(art));
        }
    };

    if (auto* mpegFile = dynamic_cast<TagLib::MPEG::File*>(ref.file())) {
        if (auto* tag = mpegFile->ID3v2Tag()) {
            for (auto* frame : tag->frameList("APIC")) {
                auto* picture = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frame);
                if (!picture) continue;

                const auto& imageData = picture->picture();
                if (imageData.isEmpty()) continue;

                RawCoverArt art;
                art.type = convertPictureType(picture->type());
                art.mimeType = picture->mimeType().to8Bit(true);
                art.description = picture->description().to8Bit(true);
                art.data.assign(imageData.begin(), imageData.end());
                result.push_back(std::move(art));
            }
        }
    }
    else if (auto* flacFile = dynamic_cast<TagLib::FLAC::File*>(ref.file())) {
        appendFlacPictures(flacFile->pictureList());
    }
    else if (auto* mp4File = dynamic_cast<TagLib::MP4::File*>(ref.file())) {
        if (auto* tag = mp4File->tag(); tag && tag->contains("covr")) {
            for (const auto& cover : tag->item("covr").toCoverArtList()) {
                const auto& data = cover.data();
                if (data.isEmpty()) continue;

                RawCoverArt art;
                art.type = CoverArtType::FrontCover;
                switch (cover.format()) {
                    case TagLib::MP4::CoverArt::PNG:  art.mimeType = "image/png"; break;
                    case TagLib::MP4::CoverArt::BMP:  art.mimeType = "image/bmp"; break;
                    case TagLib::MP4::CoverArt::GIF:  art.mimeType = "image/gif"; break;
                    case TagLib::MP4::CoverArt::JPEG:
                    default:                          art.mimeType = "image/jpeg"; break;
                }
                art.data.assign(data.begin(), data.end());
                result.push_back(std::move(art));
            }
        }
    }
    else if (ref.file()->tag()) {
        if (auto* xiph = dynamic_cast<TagLib::Ogg::XiphComment*>(ref.file()->tag())) {
            appendFlacPictures(xiph->pictureList());
        }
    }

    return result;
}

CoverArtType TagReader::convertPictureType(
    TagLib::ID3v2::AttachedPictureFrame::Type type
) {
    using TagLibType = TagLib::ID3v2::AttachedPictureFrame;

    switch (type) {
        case TagLibType::Other:
            return CoverArtType::Other;

        case TagLibType::FileIcon:
            return CoverArtType::FileIcon;

        case TagLibType::OtherFileIcon:
            return CoverArtType::OtherFileIcon;

        case TagLibType::FrontCover:
            return CoverArtType::FrontCover;

        case TagLibType::BackCover:
            return CoverArtType::BackCover;

        case TagLibType::LeafletPage:
            return CoverArtType::Leaflet;

        case TagLibType::Media:
            return CoverArtType::Media;

        case TagLibType::LeadArtist:
            return CoverArtType::LeadArtist;

        case TagLibType::Artist:
            return CoverArtType::Artist;

        case TagLibType::Conductor:
            return CoverArtType::Conductor;

        case TagLibType::Band:
            return CoverArtType::Band;

        case TagLibType::Composer:
            return CoverArtType::Composer;

        case TagLibType::Lyricist:
            return CoverArtType::Lyricist;

        case TagLibType::RecordingLocation:
            return CoverArtType::RecordingLocation;

        case TagLibType::DuringRecording:
            return CoverArtType::DuringRecording;

        case TagLibType::DuringPerformance:
            return CoverArtType::DuringPerformance;

        case TagLibType::MovieScreenCapture:
            return CoverArtType::MovieCapture;

        case TagLibType::Illustration:
            return CoverArtType::Illustration;

        case TagLibType::BandLogo:
            return CoverArtType::BandLogo;

        case TagLibType::PublisherLogo:
            return CoverArtType::PublisherLogo;

        default:
            return CoverArtType::Unknown;
    }
}

bool TagReader::isSupportedFormat(const std::filesystem::path& filePath) {
    static const std::unordered_set<std::string> supportedFormats = {
        ".mp3",
        ".flac",
        ".ogg",
        ".opus",
        ".m4a",
        ".wav"
    };

    std::string extension = filePath.extension().string();

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );

    return supportedFormats.contains(extension);
}