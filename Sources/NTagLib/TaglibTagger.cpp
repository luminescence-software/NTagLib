#include "stdafx.h"

#include <taglib.h>
#include <tstring.h>
#include <tpropertymap.h>
#include <flacfile.h>
#include <flacproperties.h>
#include <xiphcomment.h>
#include <mpegfile.h>
#include <attachedpictureframe.h>
#include <id3v1genres.h>
#include <id3v1tag.h>
#include <id3v2tag.h>
#include <vorbisfile.h>
#include <opusfile.h>
#include <asffile.h>
#include <mp4file.h>

#include "Interop.h"
#include "ResourceStrings.h"

#include "Helper.cpp"

using namespace System;
using namespace System::IO;
using namespace System::Linq;

namespace NTagLib
{
    [FileFormat("MP3 (MPEG-1/2 Audio Layer 3)", ".mp3")]
    [FileFormat("FLAC (Free Lossless Audio Codec)", ".flac")]
    [FileFormat("Ogg Vorbis", ".ogg")]
    [FileFormat("Opus", ".opus")]
    [FileFormat("WMA (Windows Media Audio)", ".wma")]
    [FileFormat("AAC (Advanced Audio Coding) - ALAC (Apple Lossless Audio Codec)", ".m4a")]
    public ref class TaglibTagger
    {
    private:
        String^ fullPath;

        String^ codec;
        String^ codecVersion;
        byte channels;
        TimeSpan duration;
        int bitrate;
        int sampleRate;
        byte bitsPerSample;

        Dictionary<String^, List<String^>^>^ tags;
        List<Picture^>^ pictures;

        static void VerifyFile(const TagLib::File& file, bool writeAccessRequired)
        {
            if (!file.isOpen())
                throw gcnew IOException(ResourceStrings::GetString("CannotOpenFile"));

            if (!file.isValid() || (!writeAccessRequired && file.audioProperties() == nullptr))
            {
                // HACK : Audio files larger than 2GB cannot be opened with taglib 1.x. https://github.com/taglib/taglib/issues/1089
                auto fi = gcnew FileInfo(gcnew String(file.name().wstr().c_str()));
                if (fi->Length > Int32::MaxValue)
                    throw gcnew NotSupportedException("Audio files larger than 2GB cannot be opened with taglib.");

                throw gcnew InvalidFileFormatException(ResourceStrings::GetString("InvalidAudioFile"));
            }

            if (writeAccessRequired && file.readOnly())
                throw gcnew IOException(ResourceStrings::GetString("CannotWriteFile"));
        }

        void ReadTags(String^ path)
        {
            if (!File::Exists(path))
                throw gcnew FileNotFoundException(ResourceStrings::GetString("FileNotFound"), path);

            String^ extension = Path::GetExtension(path);
            if (String::Equals(extension, ".mp3", StringComparison::OrdinalIgnoreCase))
            {
                ID3StringHandler id3StringHandler(TaglibSettings::ID3Latin1Encoding->CodePage);
                if (TaglibSettings::OverrideID3Latin1EncodingCodepage)
                {
                    TagLib::ID3v2::Tag::setLatin1StringHandler(&id3StringHandler);
                    TagLib::ID3v1::Tag::setStringHandler(&id3StringHandler);
                }

                try
                {
                    ReadMp3File(path);
                }
                finally
                {
                    TagLib::ID3v2::Tag::setLatin1StringHandler(nullptr);
                    TagLib::ID3v1::Tag::setStringHandler(nullptr);
                }
            }
            else if (String::Equals(extension, ".flac", StringComparison::OrdinalIgnoreCase))
            {
                ReadFlacFile(path);
            }
            else if (String::Equals(extension, ".ogg", StringComparison::OrdinalIgnoreCase))
            {
                ReadOggFile(path);
            }
            else if (String::Equals(extension, ".opus", StringComparison::OrdinalIgnoreCase))
            {
                ReadOpusFile(path);
            }
            else if (String::Equals(extension, ".wma", StringComparison::OrdinalIgnoreCase))
            {
                ReadWmaFile(path);
            }
            else if (String::Equals(extension, ".m4a", StringComparison::OrdinalIgnoreCase))
            {
                ReadM4aFile(path);
            }
            else
            {
                throw gcnew NotSupportedFileFormatException(ResourceStrings::GetString("FileFormatNotSupported"));
            }

            fullPath = path;
        }

        void ReadFlacFile(String^ path)
        {
            TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::FLAC::File file(fileName, true, TagLib::AudioProperties::ReadStyle::Average);
            VerifyFile(file, false);
            if (!file.hasXiphComment())
                throw gcnew InvalidFileFormatException(ResourceStrings::GetString("InvalidAudioFile"));

            TagLib::FLAC::Properties* properties = file.audioProperties();
            TagLib::Ogg::XiphComment* xiph = file.xiphComment();

            TagLib::StringList buffer = xiph->vendorID().split();
            codecVersion = buffer.size() > 2 ? "FLAC " + gcnew String(buffer[2].toCWString()) : "FLAC";
            codec = "FLAC";

            bitrate = properties->bitrate(); // in kb/s
            duration = TimeSpan::FromMilliseconds(properties->lengthInMilliseconds());
            sampleRate = properties->sampleRate(); // in Hertz
            channels = (byte)properties->channels(); // number of audio channels
            bitsPerSample = properties->bitsPerSample(); // in bits

            tags = PropertyMapToManagedDictionary(file.properties());

            TagLib::List<TagLib::FLAC::Picture*> arts = file.pictureList();
            pictures = gcnew List<Picture^>(arts.size());
            for (auto it = arts.begin(); it != arts.end(); it++)
            {
                TagLib::FLAC::Picture* pic = *it;
                pictures->Add(gcnew Picture(pic->data(), (PictureType)pic->type(), pic->description()));
            }
        }

        List<String^>^ WriteFlacFile()
        {
            String^ path = fullPath;
            TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::FLAC::File file(fileName, false);
            VerifyFile(file, true);

            TagLib::Ogg::XiphComment* xiph = file.xiphComment(true);
            TagLib::PropertyMap map = xiph->setProperties(ManagedDictionaryToPropertyMap(tags));

            file.removePictures();
            for each (auto picture in pictures)
            {
                TagLib::FLAC::Picture* pic = new TagLib::FLAC::Picture();
                pic->setData(ManagedArrayToByteVector(picture->Data));
                pic->setMimeType(msclr::interop::marshal_as<std::string>(picture->GetMimeType()).c_str());
                pic->setType((TagLib::FLAC::Picture::Type)picture->Type);
                if (picture->Description != nullptr)
                    pic->setDescription(msclr::interop::marshal_as<std::wstring>(picture->Description).c_str());

                file.addPicture(pic); //The file takes ownership of the picture and will handle freeing its memory.
            }

            file.strip(TagLib::FLAC::File::ID3v1 | TagLib::FLAC::File::ID3v2);
            file.save();
            return PropertyMapToManagedList(map);
        }

        void ReadMp3File(String^ path)
        {
            TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::MPEG::File file(fileName, true, TagLib::AudioProperties::ReadStyle::Average);
            VerifyFile(file, false);

            TagLib::MPEG::Properties* properties = file.audioProperties();
            if (properties->bitrate() == 0)
                throw gcnew InvalidFileFormatException(ResourceStrings::GetString("InvalidAudioFile"));

            codec = codecVersion = "MP3";

            bitrate = properties->bitrate(); // in kb/s
            duration = TimeSpan::FromMilliseconds(properties->lengthInMilliseconds());
            sampleRate = properties->sampleRate(); // in Hertz
            channels = (byte)properties->channels(); // number of audio channels
            bitsPerSample = 0; // in bits

            tags = PropertyMapToManagedDictionary(file.properties());

            if (file.hasID3v2Tag())
            {
                TagLib::ID3v2::Tag* tag = file.ID3v2Tag(false);
                TagLib::ID3v2::FrameList arts = tag->frameListMap()["APIC"];
                pictures = gcnew List<Picture^>(arts.size());
                for (auto it = arts.begin(); it != arts.end(); it++)
                {
                    TagLib::ID3v2::AttachedPictureFrame* pic = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(*it);
                    pictures->Add(gcnew Picture(pic->picture(), (PictureType)pic->type(), pic->description()));
                }
            }
            else
                pictures = gcnew List<Picture^>(1);
        }

        List<String^>^ WriteMp3File()
        {
            String^ path = fullPath;
            TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::MPEG::File file(fileName, false);
            VerifyFile(file, true);

            TagLib::ID3v2::Tag* id3 = file.ID3v2Tag(true);
            id3->setProperties(ManagedDictionaryToPropertyMap(tags));
            // ID3 implements the complete PropertyMap interface, so id3->setProperties() always returns an empty PropertyMap.

            id3->removeFrames("APIC");
            for each (auto picture in pictures)
            {
                TagLib::ID3v2::AttachedPictureFrame* frame = new TagLib::ID3v2::AttachedPictureFrame();
                frame->setPicture(ManagedArrayToByteVector(picture->Data));
                frame->setMimeType(msclr::interop::marshal_as<std::string>(picture->GetMimeType()).c_str());
                frame->setType((TagLib::ID3v2::AttachedPictureFrame::Type)picture->Type);
                if (picture->Description != nullptr)
                    frame->setDescription(msclr::interop::marshal_as<std::wstring>(picture->Description).c_str());

                id3->addFrame(frame); // At this point, the tag takes ownership of the frame and will handle freeing its memory.
            }

            TagLib::ID3v2::Version tagVersion = (TagLib::ID3v2::Version)id3->header()->majorVersion();

            TagLib::ID3v2::Version minVersion = (TagLib::ID3v2::Version)TaglibSettings::MinId3Version;
            TagLib::ID3v2::Version maxVersion = (TagLib::ID3v2::Version)TaglibSettings::MaxId3Version;

            if (tagVersion < minVersion)
                tagVersion = minVersion;

            if (tagVersion > maxVersion)
                tagVersion = maxVersion;

            array<String^>^ unsupportedFramesId3v23 = { TagNameKey::ReleaseDate, TagNameKey::TaggingDate, TagNameKey::Mood, TagNameKey::ProducedNotice, TagNameKey::AlbumSort, TagNameKey::TitleSort, TagNameKey::ArtistSort };
            if (tagVersion < TagLib::ID3v2::Version::v4 && Enumerable::Any<String^>(unsupportedFramesId3v23, gcnew Func<String^, bool>(tags, &Dictionary<String^, List<String^>^>::ContainsKey)))
                tagVersion = TagLib::ID3v2::Version::v4;

            List<String^>^ genres;
            if (tagVersion < TagLib::ID3v2::Version::v4 && tags->TryGetValue(TagNameKey::Genre, genres) && Enumerable::Any<String^>(Enumerable::Except<String^>(genres, gcnew ID3v1GenresCollection())))
                tagVersion = TagLib::ID3v2::Version::v4;

            file.save(2, TagLib::File::StripTags::StripOthers, tagVersion, TagLib::File::DuplicateTags::DoNotDuplicate);
            return nullptr;
        }

        void ReadOggFile(String^ path)
        {
            TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::Ogg::Vorbis::File file(fileName, true, TagLib::AudioProperties::ReadStyle::Average);
            VerifyFile(file, false);

            TagLib::Vorbis::Properties* properties = file.audioProperties();
            TagLib::Ogg::XiphComment* xiph = file.tag();

            String^ vendor = gcnew String(xiph->vendorID().toCWString());
            codecVersion = GetVorbisVersionFromVendor(vendor);
            codec = "Vorbis";

            bitrate = properties->bitrate(); // in kb/s
            duration = TimeSpan::FromMilliseconds(properties->lengthInMilliseconds());
            sampleRate = properties->sampleRate(); // in Hertz
            channels = (byte)properties->channels(); // number of audio channels

            TagLib::List<TagLib::FLAC::Picture*> arts = xiph->pictureList();
            pictures = gcnew List<Picture^>(arts.size());
            for (auto it = arts.begin(); it != arts.end(); it++)
            {
                TagLib::FLAC::Picture* pic = *it;
                pictures->Add(gcnew Picture(pic->data(), (PictureType)pic->type(), pic->description()));
            }

            tags = PropertyMapToManagedDictionary(file.properties());
        }

        List<String^>^ WriteOggFile()
        {
            String^ path = fullPath;
            TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::Ogg::Vorbis::File file(fileName, false);
            VerifyFile(file, true);

            TagLib::Ogg::XiphComment* xiph = file.tag();
            TagLib::PropertyMap map = xiph->setProperties(ManagedDictionaryToPropertyMap(tags));

            xiph->removeAllPictures();
            for each (auto picture in pictures)
            {
                TagLib::FLAC::Picture* pic = new TagLib::FLAC::Picture();
                pic->setData(ManagedArrayToByteVector(picture->Data));
                pic->setMimeType(msclr::interop::marshal_as<std::string>(picture->GetMimeType()).c_str());
                pic->setType((TagLib::FLAC::Picture::Type)picture->Type);
                if (picture->Description != nullptr)
                    pic->setDescription(msclr::interop::marshal_as<std::wstring>(picture->Description).c_str());

                xiph->addPicture(pic); //The file takes ownership of the picture and will handle freeing its memory.
            }

            file.save();
            return PropertyMapToManagedList(map);
        }

        void ReadOpusFile(String^ path)
        {
            TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::Ogg::Opus::File file(fileName, true, TagLib::AudioProperties::ReadStyle::Average);
            VerifyFile(file, false);

            TagLib::Ogg::Opus::Properties* properties = file.audioProperties();
            TagLib::Ogg::XiphComment* xiph = file.tag();

            String^ vendor = gcnew String(xiph->vendorID().toCWString());
            int endIndex = vendor->IndexOf(',');
            codecVersion = endIndex != -1 ? vendor->Substring(0, endIndex)->Replace("libopus", "Opus") : "Opus";
            codec = "Opus";

            bitrate = properties->bitrate(); // in kb/s
            duration = TimeSpan::FromMilliseconds(properties->lengthInMilliseconds());
            sampleRate = properties->sampleRate(); // in Hertz
            channels = (byte)properties->channels(); // number of audio channels

            TagLib::List<TagLib::FLAC::Picture*> arts = xiph->pictureList();
            pictures = gcnew List<Picture^>(arts.size());
            for (auto it = arts.begin(); it != arts.end(); it++)
            {
                TagLib::FLAC::Picture* pic = *it;
                pictures->Add(gcnew Picture(pic->data(), (PictureType)pic->type(), pic->description()));
            }

            tags = PropertyMapToManagedDictionary(file.properties());
        }

        List<String^>^ WriteOpusFile()
        {
            String^ path = fullPath;
            TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::Ogg::Opus::File file(fileName, false);
            VerifyFile(file, true);

            TagLib::Ogg::XiphComment* xiph = file.tag();
            TagLib::PropertyMap map = xiph->setProperties(ManagedDictionaryToPropertyMap(tags));

            xiph->removeAllPictures();
            for each (auto picture in pictures)
            {
                TagLib::FLAC::Picture* pic = new TagLib::FLAC::Picture();
                pic->setData(ManagedArrayToByteVector(picture->Data));
                pic->setMimeType(msclr::interop::marshal_as<std::string>(picture->GetMimeType()).c_str());
                pic->setType((TagLib::FLAC::Picture::Type)picture->Type);
                if (picture->Description != nullptr)
                    pic->setDescription(msclr::interop::marshal_as<std::wstring>(picture->Description).c_str());

                xiph->addPicture(pic); //The file takes ownership of the picture and will handle freeing its memory.
            }

            file.save();
            return PropertyMapToManagedList(map);
        }

        void ReadWmaFile(String^ path)
        {
            TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::ASF::File file(fileName, true, TagLib::AudioProperties::ReadStyle::Average);
            VerifyFile(file, false);

            TagLib::ASF::Properties* properties = file.audioProperties();

            codecVersion = !properties->codecName().isEmpty() ? gcnew String(properties->codecName().toCString()) : "WMA";
            codec = "WMA";

            bitrate = properties->bitrate(); // in kb/s
            duration = TimeSpan::FromMilliseconds(properties->lengthInMilliseconds());
            sampleRate = properties->sampleRate(); // in Hertz
            channels = (byte)properties->channels(); // number of audio channels
            bitsPerSample = properties->bitsPerSample(); // in bits

            tags = PropertyMapToManagedDictionary(file.properties());

            TagLib::ASF::Tag* asf = file.tag();
            TagLib::ASF::AttributeList& arts = asf->attributeListMap()["WM/Picture"];
            pictures = gcnew List<Picture^>(arts.size());
            for (auto it = arts.begin(); it != arts.end(); it++)
            {
                TagLib::ASF::Attribute attr = *it;
                TagLib::ASF::Picture pic = attr.toPicture();
                pictures->Add(gcnew Picture(pic.picture(), (PictureType)pic.type(), pic.description()));
            }
        }

        List<String^>^ WriteWmaFile()
        {
            String^ path = fullPath;
            TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::ASF::File file(fileName, false);
            VerifyFile(file, true);

            TagLib::PropertyMap map = file.setProperties(ManagedDictionaryToPropertyMap(tags));

            TagLib::ASF::Tag* asf = file.tag();
            asf->removeItem("WM/Picture");
            for each (auto picture in pictures)
            {
                TagLib::ASF::Picture pic;
                pic.setPicture(ManagedArrayToByteVector(picture->Data));
                pic.setMimeType(msclr::interop::marshal_as<std::string>(picture->GetMimeType()).c_str());
                pic.setType((TagLib::ASF::Picture::Type)picture->Type);
                if (picture->Description != nullptr)
                    pic.setDescription(msclr::interop::marshal_as<std::wstring>(picture->Description).c_str());

                asf->addAttribute("WM/Picture", pic);
            }

            file.save();
            return PropertyMapToManagedList(map);
        }

        void ReadM4aFile(String^ path)
        {
            TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::MP4::File file(fileName, true, TagLib::AudioProperties::ReadStyle::Average);
            VerifyFile(file, false);

            TagLib::MP4::Properties* properties = file.audioProperties();

            switch (properties->codec())
            {
            case TagLib::MP4::Properties::Codec::AAC:
                codec = codecVersion = "AAC";
                break;
            case TagLib::MP4::Properties::Codec::ALAC:
                codec = codecVersion = "ALAC";
                break;
            case TagLib::MP4::Properties::Codec::Unknown:
                throw gcnew NotSupportedFileFormatException(ResourceStrings::GetString("FileFormatNotSupported"));
            }

            bitrate = properties->bitrate(); // in kb/s
            duration = TimeSpan::FromMilliseconds(properties->lengthInMilliseconds());
            sampleRate = properties->sampleRate(); // in Hertz
            channels = (byte)properties->channels(); // number of audio channels
            bitsPerSample = properties->bitsPerSample(); // in bits

            tags = PropertyMapToManagedDictionary(file.properties());

            TagLib::MP4::Tag* tag = file.tag();
            TagLib::MP4::CoverArtList arts = tag->item("covr").toCoverArtList();
            pictures = gcnew List<Picture^>(arts.size());
            for (auto it = arts.begin(); it != arts.end(); it++)
            {
                TagLib::MP4::CoverArt pic = *it;
                pictures->Add(gcnew Picture(pic.data(), (PictureFormat)pic.format()));
            }
        }

        List<String^>^ WriteM4aFile()
        {
            String^ path = fullPath;
            TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::MP4::File file(fileName, false);
            VerifyFile(file, true);

            TagLib::PropertyMap map = file.setProperties(ManagedDictionaryToPropertyMap(tags));

            TagLib::MP4::Tag* atom = file.tag();
            atom->removeItem("covr");
            if (pictures->Count != 0)
            {
                TagLib::MP4::CoverArtList arts;
                for each (auto picture in pictures)
                {
                    TagLib::MP4::CoverArt pic((TagLib::MP4::CoverArt::Format)picture->Format, ManagedArrayToByteVector(picture->Data));
                    arts.append(pic);
                }

                atom->setItem("covr", arts);
            }

            file.save();
            return PropertyMapToManagedList(map);
        }

    public:
        static TaglibTagger()
        {
            std::set_new_handler(throw_out_of_memory_exception);
        }

        TaglibTagger(String^ path)
        {
            ReadTags(path);
        }

        property String^ Source { String^ get() { return fullPath; } }
        property String^ Codec { String^ get() { return codec; } }
        property String^ CodecVersion { String^ get() { return codecVersion; } }
        property byte Channels { byte get() { return channels; } }
        property TimeSpan Duration { TimeSpan get() { return duration; } }
        property int Bitrate { int get() { return bitrate; } }
        property int SampleRate { int get() { return sampleRate; } }
        property byte BitsPerSample { byte get() { return bitsPerSample; } }

        property Dictionary<String^, List<String^>^>^ Tags { Dictionary<String^, List<String^>^>^ get() { return tags; } }
        property List<Picture^>^ Pictures { List<Picture^>^ get() { return pictures; } } // return nullptr if the format doesn't support cover, return empty collection if there is no cover

        static property Version^ TagLibVersion { Version^ get() { return gcnew Version(TAGLIB_MAJOR_VERSION, TAGLIB_MINOR_VERSION, TAGLIB_PATCH_VERSION); } }

        void ReloadTags()
        {
            ReadTags(fullPath);
        }

        void ChangeSource(String^ newPath, bool reloadTags)
        {
            if (!String::Equals(Path::GetExtension(fullPath), Path::GetExtension(newPath), StringComparison::OrdinalIgnoreCase))
                throw gcnew ArgumentException("Changing the extension of the source file is not allowed.", "newPath");

            fullPath = newPath;
            if (reloadTags) ReloadTags();
        }

        List<String^>^ SaveTags()
        {
            if (!File::Exists(fullPath))
                throw gcnew FileNotFoundException(ResourceStrings::GetString("FileNotFound"), fullPath);

            if ((File::GetAttributes(fullPath) & FileAttributes::ReadOnly) == FileAttributes::ReadOnly)
                File::SetAttributes(fullPath, FileAttributes::Normal);

            String^ extension = Path::GetExtension(fullPath);
            if (String::Equals(extension, ".mp3", StringComparison::OrdinalIgnoreCase))
                return WriteMp3File();

            if (String::Equals(extension, ".flac", StringComparison::OrdinalIgnoreCase))
                return WriteFlacFile();

            if (String::Equals(extension, ".ogg", StringComparison::OrdinalIgnoreCase))
                return WriteOggFile();

            if (String::Equals(extension, ".opus", StringComparison::OrdinalIgnoreCase))
                return WriteOpusFile();

            if (String::Equals(extension, ".wma", StringComparison::OrdinalIgnoreCase))
                return WriteWmaFile();

            if (String::Equals(extension, ".m4a", StringComparison::OrdinalIgnoreCase))
                return WriteM4aFile();

            throw gcnew NotSupportedFileFormatException(ResourceStrings::GetString("FileFormatNotSupported"));
        }

        bool RemoveTag(String^ tag)
        {
            return tags->Remove(tag);
        }

        IEnumerable<String^>^ GetTagValues(String^ tag)
        {
            List<String^>^ tagValues;
            if (tags->TryGetValue(tag, tagValues))
                return tagValues;

            return Enumerable::Empty<String^>();
        }

        void ReplaceTag(String^ tag, ...array<String^>^ values)
        {
            RemoveTag(tag);
            AddTag(tag, values);
        }

        void ReplaceTag(String^ tag, String^ value)
        {
            RemoveTag(tag);
            AddTag(tag, value);
        }

        void AddTag(String^ tag, ...array<String^>^ values)
        {
            List<String^>^ tagValues;
            if (tags->TryGetValue(tag, tagValues))
            {
                for each (String ^ value in values)
                {
                    if (!tagValues->Contains(value))
                        tagValues->Add(value);
                }
            }
            else
                tags->Add(tag, gcnew List<String^>(Enumerable::Distinct<String^>(values)));
        }

        void AddTag(String^ tag, String^ value)
        {
            List<String^>^ tagValues;
            if (tags->TryGetValue(tag, tagValues))
            {
                if (!tagValues->Contains(value))
                    tagValues->Add(value);
            }
            else
            {
                auto values = gcnew List<String^>(1);
                values->Add(value);
                tags->Add(tag, values);
            }
        }
    };
}
