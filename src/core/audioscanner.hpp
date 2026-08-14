#pragma once
#include "databasemanager.hpp"
#include <filesystem>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <fstream>

class AudioScanner {
public:
    std::string extractCoverArt(const std::string& audioPath, const std::string& albumName) {
        TagLib::MPEG::File file(audioPath.c_str());

        if (!file.isValid() || !file.ID3v2Tag())
            return "";

        auto frameList = file.ID3v2Tag()->frameListMap()["APIC"];

        if (frameList.isEmpty())
            return "";

        auto* pictureFrame =
            dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frameList.front());

        if (!pictureFrame)
            return "";

        std::filesystem::create_directories("cache/covers");

        std::string safeAlbumName = albumName;
        std::replace(
            safeAlbumName.begin(),
            safeAlbumName.end(),
            ' ',
            '_'
        );

        std::string extension = ".jpg";
        std::string mimeType = pictureFrame->mimeType().to8Bit(true);

        if (mimeType == "image/png") {
            extension = ".png";
        } else if (mimeType == "image/webp") {
            extension = ".webp";
        } else if (mimeType == "image/jpeg") {
            extension = ".jpg";
        }

        std::string outPath =
            "cache/covers/" + safeAlbumName + extension;

        if (!std::filesystem::exists(outPath)) {
            std::ofstream outFile(outPath, std::ios::binary);

            if (!outFile)
                return "";

            const auto& data = pictureFrame->picture();

            outFile.write(data.data(), data.size());
        }

        return outPath;
    }

    void scanDirectory(const std::string& directory, DatabaseManager& db) {
        db.exec("BEGIN TRANSACTION;"); 

        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                Track track;
                track.path = entry.path().string();
                
                TagLib::FileRef fileRef(track.path.c_str());
                if (!fileRef.isNull() && fileRef.tag()) {
                    auto* tag = fileRef.tag();
                    track.title = tag->title().isEmpty() ? entry.path().stem().string() : tag->title().toCString(true);
                    track.artist = tag->artist().isEmpty() ? "Unknown Artist" : tag->artist().toCString(true);
                    track.album = tag->album().isEmpty() ? "Unknown Album" : tag->album().toCString(true);
                } else {
                    track.title = entry.path().stem().string();
                    track.artist = "Unknown Artist";
                    track.album = "Unknown Album";
                }
                
                track.cover_path = extractCoverArt(track.path, track.album);
                
                db.insertTrack(track);
            }
        }
        db.exec("COMMIT;");
    }
};