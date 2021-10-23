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

using namespace System;
using namespace System::IO;
using namespace System::Linq;
using namespace System::Text;

static String^ GetVorbisVersionFromVendor(String^ vendor)
{
    if (vendor->StartsWith("AO; aoTuV")) return "Vorbis aoTuV";

    if (vendor == "Xiphophorus libVorbis I 20000508") return "Vorbis 1.0 Beta 1/2";
    if (vendor == "Xiphophorus libVorbis I 20001031") return "Vorbis 1.0 Beta 3";
    if (vendor == "Xiphophorus libVorbis I 20010225") return "Vorbis 1.0 Beta 4";
    if (vendor == "Xiphophorus libVorbis I 20010615") return "Vorbis 1.0 RC1";
    if (vendor == "Xiphophorus libVorbis I 20010813") return "Vorbis 1.0 RC2";
    if (vendor == "Xiphophorus libVorbis I 20011217") return "Vorbis 1.0 RC3";
    if (vendor == "Xiphophorus libVorbis I 20011231") return "Vorbis 1.0 RC3";

    // https://gitlab.xiph.org/xiph/vorbis/-/blob/master/CHANGES
    if (vendor == "Xiph.Org libVorbis I 20020717") return "Vorbis 1.0.0";
    if (vendor == "Xiph.Org libVorbis I 20030909") return "Vorbis 1.0.1";
    if (vendor == "Xiph.Org libVorbis I 20040629") return "Vorbis 1.1.0";
    if (vendor == "Xiph.Org libVorbis I 20050304") return "Vorbis 1.1.1/2";
    if (vendor == "Xiph.Org libVorbis I 20070622") return "Vorbis 1.2.0";
    if (vendor == "Xiph.Org libVorbis I 20080501") return "Vorbis 1.2.1";
    if (vendor == "Xiph.Org libVorbis I 20090624") return "Vorbis 1.2.2";
    if (vendor == "Xiph.Org libVorbis I 20090709") return "Vorbis 1.2.3";
    if (vendor == "Xiph.Org libVorbis I 20100325 (Everywhere)") return "Vorbis 1.3.1";
    if (vendor == "Xiph.Org libVorbis I 20101101 (Schaufenugget)") return "Vorbis 1.3.2";
    if (vendor == "Xiph.Org libVorbis I 20120203 (Omnipresent)") return "Vorbis 1.3.3";
    if (vendor == "Xiph.Org libVorbis I 20140122 (Turpakäräjiin)") return "Vorbis 1.3.4";
    if (vendor == L"Xiph.Org libVorbis I 20150105 (⛄⛄⛄⛄)") return "Vorbis 1.3.5";
    if (vendor == L"Xiph.Org libVorbis I 20180316 (Now 100% fewer shells)") return "Vorbis 1.3.6";
    if (vendor == L"Xiph.Org libVorbis I 20200704 (Reducing Environment)") return "Vorbis 1.3.7";

    return nullptr;
}

namespace NTagLib
{
#pragma region Helper

    class ID3StringHandler : public TagLib::ID3v2::Latin1StringHandler, public TagLib::ID3v1::StringHandler
    {
    private:
        int codepage;

        static std::wstring ConvertEncoding(const char* data, unsigned int size, int codepage)
        {
            std::wstring utf16;
            const int utf16Length = MultiByteToWideChar(codepage, 0, data, size, nullptr, 0);
            utf16.resize(utf16Length);
            MultiByteToWideChar(codepage, 0, data, size, &utf16[0], utf16Length);
            return utf16;
        }

    public:
        ID3StringHandler(int cp = 0) : codepage(cp) { }

        TagLib::String parse(const TagLib::ByteVector& data) const override
        {
            return ConvertEncoding(data.data(), data.size(), codepage);
        }

        int getCodepage() const { return codepage; }
    };

    public enum class Id3Version
    {
        id3v23 = 3,
        id3v24 = 4
    };

    public ref class TaglibSettings abstract sealed
    {
    public:
        static TaglibSettings()
        {
            Encoding::RegisterProvider(CodePagesEncodingProvider::Instance);
            ID3Latin1Encoding = Encoding::GetEncoding(0);
        }

        static Id3Version MinId3Version = Id3Version::id3v23;
        static Id3Version MaxId3Version = Id3Version::id3v24;
        static bool OverrideID3Latin1EncodingCodepage = true;
        static Encoding^ ID3Latin1Encoding;
    };

    public enum class PictureType
    {
        //! A type not enumerated below
        Other = TagLib::FLAC::Picture::Type::Other,
        //! 32x32 PNG image that should be used as the file icon
        FileIcon = TagLib::FLAC::Picture::Type::FileIcon,
        //! File icon of a different size or format
        OtherFileIcon = TagLib::FLAC::Picture::Type::OtherFileIcon,
        //! Front cover image of the album
        FrontCover = TagLib::FLAC::Picture::Type::FrontCover,
        //! Back cover image of the album
        BackCover = TagLib::FLAC::Picture::Type::BackCover,
        //! Inside leaflet page of the album
        LeafletPage = TagLib::FLAC::Picture::Type::LeafletPage,
        //! Image from the album itself
        Media = TagLib::FLAC::Picture::Type::Media,
        //! Picture of the lead artist or soloist
        LeadArtist = TagLib::FLAC::Picture::Type::LeadArtist,
        //! Picture of the artist or performer
        Artist = TagLib::FLAC::Picture::Type::Artist,
        //! Picture of the conductor
        Conductor = TagLib::FLAC::Picture::Type::Conductor,
        //! Picture of the band or orchestra
        Band = TagLib::FLAC::Picture::Type::Band,
        //! Picture of the composer
        Composer = TagLib::FLAC::Picture::Type::Composer,
        //! Picture of the lyricist or text writer
        Lyricist = TagLib::FLAC::Picture::Type::Lyricist,
        //! Picture of the recording location or studio
        RecordingLocation = TagLib::FLAC::Picture::Type::RecordingLocation,
        //! Picture of the artists during recording
        DuringRecording = TagLib::FLAC::Picture::Type::DuringRecording,
        //! Picture of the artists during performance
        DuringPerformance = TagLib::FLAC::Picture::Type::DuringPerformance,
        //! Picture from a movie or video related to the track
        MovieScreenCapture = TagLib::FLAC::Picture::Type::MovieScreenCapture,
        //! Picture of a large, coloured fish
        ColouredFish = TagLib::FLAC::Picture::Type::ColouredFish,
        //! Illustration related to the track
        Illustration = TagLib::FLAC::Picture::Type::Illustration,
        //! Logo of the band or performer
        BandLogo = TagLib::FLAC::Picture::Type::BandLogo,
        //! Logo of the publisher (record company)
        PublisherLogo = TagLib::FLAC::Picture::Type::PublisherLogo
    };

    public ref class TagNameKey abstract sealed
    {
    public:
        literal String^ Artist = "ARTIST";
        literal String^ Title = "TITLE";
        literal String^ Date = "DATE";
        literal String^ Album = "ALBUM";
        literal String^ Genre = "GENRE";
        literal String^ TrackNumber = "TRACKNUMBER";

        literal String^ AlbumArtist = "ALBUMARTIST";
        literal String^ DiscNumber = "DISCNUMBER";
        literal String^ Lyricist = "LYRICIST";
        literal String^ Composer = "COMPOSER";
        literal String^ Language = "LANGUAGE";
        literal String^ OriginalAlbum = "ORIGINALALBUM";
        literal String^ OriginalArtist = "ORIGINALARTIST";
        literal String^ OriginalDate = "ORIGINALDATE";
        literal String^ AlbumSort = "ALBUMSORT";
        literal String^ ArtistSort = "ARTISTSORT";
        literal String^ AlbumArtistSort = "ALBUMARTISTSORT";
        literal String^ TitleSort = "TITLESORT";
        literal String^ Label = "LABEL";
        literal String^ Comment = "COMMENT";
        literal String^ Lyrics = "LYRICS";
    };

    public enum class PictureFormat
    {
        Unknown = TagLib::MP4::CoverArt::Format::Unknown,
        JPEG = TagLib::MP4::CoverArt::Format::JPEG,
        PNG = TagLib::MP4::CoverArt::Format::PNG,
        GIF = TagLib::MP4::CoverArt::Format::GIF,
        BMP = TagLib::MP4::CoverArt::Format::BMP
    };

    public ref class Picture
    {
    internal:
        Picture(const TagLib::ByteVector& data, PictureType type, const TagLib::String& description)
        {
            Data = PictureByteVectorToManagedArray(data);
            Format = GetFormatFromData(Data);
            Type = type;
            Description = gcnew String(description.toCString());
        }

        Picture(const TagLib::ByteVector& data, PictureFormat format) :
            Picture(PictureByteVectorToManagedArray(data), format, PictureType::FrontCover, nullptr) { }

    public:
        Picture(array<byte>^ data, PictureFormat format, PictureType type, String^ description)
        {
            Data = data;
            Format = format;
            Type = type;
            Description = description;
        }

        Picture(array<byte>^ data, PictureType type, String^ description) :
            Picture(data, GetFormatFromData(data), type, description) { }

        property array<byte>^ Data;
        property PictureType Type;
        property PictureFormat Format;
        property String^ Description;

        String^ GetMimeType() { return GetMimeTypeFromFormat(Format); }

        static String^ GetMimeTypeFromFormat(PictureFormat format)
        {
            switch (format)
            {
            case PictureFormat::JPEG:
                return "image/jpeg";
            case PictureFormat::PNG:
                return "image/png";
            case PictureFormat::GIF:
                return "image/gif";
            case PictureFormat::BMP:
                return "image/bmp";
            default:
                return "application/octet-stream";
            }
        }

        static PictureFormat GetFormatFromData(array<byte>^ data)
        {
            // https://en.wikipedia.org/wiki/List_of_file_signatures

            if (data->Length > 3 &&
                data[0] == 0xFF &&
                data[1] == 0xD8 &&
                data[2] == 0xFF)
                return PictureFormat::JPEG;

            if (data->Length > 8 &&
                data[0] == 0x89 &&
                data[1] == 0x50 &&
                data[2] == 0x4E &&
                data[3] == 0x47 &&
                data[4] == 0x0D &&
                data[5] == 0x0A &&
                data[6] == 0x1A &&
                data[7] == 0x0A)
                return PictureFormat::PNG;

            if (data->Length > 4 &&
                data[0] == 0x47 &&
                data[1] == 0x49 &&
                data[2] == 0x46 &&
                data[3] == 0x38)
                return PictureFormat::GIF;

            if (data->Length > 2 &&
                data[0] == 0x42 &&
                data[1] == 0x4D)
                return PictureFormat::BMP;

            return PictureFormat::Unknown;
        }

        byte GetAtomDataType() { return (byte)Format; }
    };

    public ref class ID3v1GenresCollection sealed : List<String^>
    {
    public:
        ID3v1GenresCollection(bool sort)
        {
            TagLib::StringList genres = TagLib::ID3v1::genreList();
            for each (auto genre in genres)
                Add(gcnew String(genre.toCWString()));

            if (sort)
                Sort();
        }

        ID3v1GenresCollection() : ID3v1GenresCollection(true) { }
    };

    [AttributeUsage(AttributeTargets::Class, AllowMultiple = true)]
    public ref class FileFormatAttribute sealed : Attribute
    {
    private:
        initonly String^ _extension;
        initonly String^ _file;

    public:
        FileFormatAttribute(String^ file, String^ extension)
        {
            _file = file;
            _extension = extension;
        }

        property String^ Extension { String^ get() { return _extension; } }
        property String^ File { String^ get() { return _file; } }
    };

    // FileFormatException has been moved from System.IO.dll to System.IO.Packaging.dll, and is not .NET 5.0 builtin. 
    // By the way, you can't install it as package nuget:
    // Impossible d’installer le package « System.IO.Packaging 5.0.0 ». Vous essayez d’installer ce package dans un projet ciblant « native,Version=v0.0 », mais le package ne contient aucun fichier de contenu ou référence d’assembly compatible avec ce framework.
    [Serializable]
    public ref class InvalidFileFormatException : /*File*/FormatException
    {
    public:
        InvalidFileFormatException(String^ message) : FormatException(message) { }
    };

    [Serializable]
    public ref class NotSupportedFileFormatException : /*File*/FormatException
    {
    public:
        NotSupportedFileFormatException(String^ message) : FormatException(message) { }
    };

#pragma endregion

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
                throw gcnew InvalidFileFormatException(ResourceStrings::GetString("InvalidAudioFile"));

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

            array<String^>^ unsupportedFramesId3v23 = { "RELEASEDATE", "TAGGINGDATE", "MOOD", "PRODUCEDNOTICE", "ALBUMSORT", "TITLESORT", "ARTISTSORT" };
            if (tagVersion < TagLib::ID3v2::Version::v4 && Enumerable::Any<String^>(unsupportedFramesId3v23, gcnew Func<String^, bool>(tags, &Dictionary<String^, List<String^>^>::ContainsKey)))
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