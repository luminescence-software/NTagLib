#include "stdafx.h"

#include <taglib.h>
#include <tstring.h>
#include <tpropertymap.h>
#include <flacfile.h>
#include <flacproperties.h>
#include <xiphcomment.h>
#include <mpegfile.h>
#include <id3v1genres.h>
#include <id3v1tag.h>
#include <id3v2tag.h>
#include <vorbisfile.h>
#include <opusfile.h>
#include <asffile.h>
#include <mp4file.h>
#include <tvariant.h>

#include <msclr/marshal.h>
#include <msclr/marshal_cppstd.h>

using namespace System;
using namespace System::IO;
using namespace System::Linq;
using namespace System::Text;
using namespace System::Collections::Generic;
using namespace System::Collections::ObjectModel;
using namespace System::Reflection;
using namespace System::Resources;

namespace
{
    #pragma region Interop functions

    void throw_out_of_memory_exception()
    {
        throw gcnew OutOfMemoryException();
    }

    List<String^>^ PropertyMapToManagedList(const TagLib::PropertyMap& properties)
    {
        auto tags = gcnew List<String^>(properties.size());

        for (auto it = properties.begin(); it != properties.end(); ++it)
            tags->Add(gcnew String(it->first.toCWString()));

        return tags;
    }

    array<byte>^ ByteVectorToManagedArray(const TagLib::ByteVector& data, bool trimZero)
    {
        if (data.size() == 0)
            return Array::Empty<byte>();

        int offset = 0;
        if (trimZero)
        {
            for (auto it = data.begin(); it != data.end(); ++it)
            {
                if (*it == 0)
                    offset++;
                else
                    break;
            }

            if (data.size() == offset)
                return Array::Empty<byte>();
        }

        auto buffer = gcnew array<byte>(data.size() - offset);
        for (int i = 0; i < buffer->Length; i++)
            buffer[i] = data[i + offset];

        return buffer;
    }

    TagLib::ByteVector ManagedArrayToByteVector(array<byte>^ data)
    {
        if (data->Length == 0)
        {
            TagLib::ByteVector buffer;
            return buffer;
        }

        const pin_ptr<byte> p = &data[0];
        unsigned char* pby = p;
        const char* pch = reinterpret_cast<char*>(pby);
        TagLib::ByteVector buffer(pch, data->Length);
        return buffer;
    }

    #pragma endregion
}

namespace NTagLib
{
    #pragma region Cover support

    public ref class PictureTypes abstract sealed
    {
    private:
        static initonly array<String^>^ types = gcnew array<String^> {
            Other, FileIcon, OtherFileIcon, FrontCover, BackCover, LeafletPage, Media, LeadArtist,
                Artist, Conductor, Band, Composer, Lyricist, RecordingLocation, DuringRecording, DuringPerformance,
                MovieScreenCapture, ColouredFish, Illustration, BandLogo, PublisherLogo
        };

    public:
        literal String^ Other = "Other";
        literal String^ FileIcon = "File Icon";
        literal String^ OtherFileIcon = "Other File Icon";
        literal String^ FrontCover = "Front Cover";
        literal String^ BackCover = "Back Cover";
        literal String^ LeafletPage = "Leaflet Page";
        literal String^ Media = "Media";
        literal String^ LeadArtist = "Lead Artist";
        literal String^ Artist = "Artist";
        literal String^ Conductor = "Conductor";
        literal String^ Band = "Band";
        literal String^ Composer = "Composer";
        literal String^ Lyricist = "Lyricist";
        literal String^ RecordingLocation = "Recording Location";
        literal String^ DuringRecording = "During Recording";
        literal String^ DuringPerformance = "During Performance";
        literal String^ MovieScreenCapture = "Movie Screen Capture";
        literal String^ ColouredFish = "Coloured Fish";
        literal String^ Illustration = "Illustration";
        literal String^ BandLogo = "Band Logo";
        literal String^ PublisherLogo = "Publisher Logo";

        static ReadOnlyCollection<String^>^ GetAllTypes()
        {
            return Array::AsReadOnly(types);
        }

        static String^ Normalize(String^ pictureType)
        {
            int index = Array::IndexOf(types, pictureType);
            if (index != -1)
                return types[index];

            return FrontCover;
        }
    };

    public ref class MimeTypes abstract sealed
    {
    public:
        literal String^ JPEG = "image/jpeg";
        literal String^ PNG = "image/png";
        literal String^ GIF = "image/gif";
        literal String^ BMP = "image/bmp";
        literal String^ Unknown = "application/octet-stream";

        static String^ Normalize(String^ mimeType, array<byte>^ data)
        {
            if (mimeType != nullptr)
            {
                if (mimeType->Contains("jpg", StringComparison::OrdinalIgnoreCase) || mimeType->Contains("jpeg", StringComparison::OrdinalIgnoreCase))
                    return JPEG;

                if (mimeType->Contains("png", StringComparison::OrdinalIgnoreCase))
                    return JPEG;

                if (mimeType->Contains("gif", StringComparison::OrdinalIgnoreCase))
                    return GIF;

                if (mimeType->Contains("bmp", StringComparison::OrdinalIgnoreCase))
                    return BMP;
            }

            if (data != nullptr)
                return GetMimeTypeFromData(data);

            return Unknown;
        }

        static String^ GetMimeTypeFromData(array<byte>^ data)
        {
            // https://en.wikipedia.org/wiki/List_of_file_signatures

            if (data->Length > 3 &&
                data[0] == 0xFF &&
                data[1] == 0xD8 &&
                data[2] == 0xFF)
                return JPEG;

            if (data->Length > 8 &&
                data[0] == 0x89 &&
                data[1] == 0x50 &&
                data[2] == 0x4E &&
                data[3] == 0x47 &&
                data[4] == 0x0D &&
                data[5] == 0x0A &&
                data[6] == 0x1A &&
                data[7] == 0x0A)
                return PNG;

            if (data->Length > 4 &&
                data[0] == 0x47 &&
                data[1] == 0x49 &&
                data[2] == 0x46 &&
                data[3] == 0x38)
                return GIF;

            if (data->Length > 2 &&
                data[0] == 0x42 &&
                data[1] == 0x4D)
                return BMP;

            return Unknown;
        }
    };

    public ref class Picture
    {
    private:
        initonly array<byte>^ _data;
        initonly String^ _mimeType;
        initonly String^ _pictureType;
        initonly String^ _description;

    internal:
        Picture(const TagLib::ByteVector& data, const TagLib::String& mimeType, const TagLib::String& pictureType, const TagLib::String& description)
        {
            if (data.isEmpty())
                throw gcnew ArgumentException("The picture data is empty.");

            _data = ByteVectorToManagedArray(data, true);
            _mimeType = MimeTypes::Normalize(gcnew String(mimeType.toCString()), _data);
            _pictureType = PictureTypes::Normalize(gcnew String(pictureType.toCString()));
            _description = description.isEmpty() ? String::Empty : gcnew String(description.toCWString());
        }

    public:
        Picture(array<byte>^ data, String^ mimeType, String^ pictureType, String^ description)
        {
            ArgumentNullException::ThrowIfNull(data, "data");

            if (data->Length == 0)
                throw gcnew ArgumentException("The picture data is empty.");

            _data = data;
            _mimeType = MimeTypes::Normalize(mimeType, data);
            _pictureType = PictureTypes::Normalize(pictureType);
            _description = description != nullptr ? description : String::Empty;
        }

        property array<byte>^ Data { array<byte>^ get() { return _data; } }
        property String^ MimeType { String^ get() { return _mimeType; } }
        property String^ PictureType { String^ get() { return _pictureType; } }
        property String^ Description { String^ get() { return _description; } }
    };

    #pragma endregion

    #pragma region Helper classes

    class ID3StringHandler final : public TagLib::ID3v2::Latin1StringHandler, public TagLib::ID3v1::StringHandler
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
        explicit ID3StringHandler(int cp = 0) : codepage(cp) { }

        TagLib::String parse(const TagLib::ByteVector& data) const override
        {
            return ConvertEncoding(data.data(), data.size(), codepage);
        }

        int getCodepage() const { return codepage; }
    };

    private ref class ResourceStrings abstract sealed
    {
    private:
        static initonly ResourceManager^ rm = gcnew ResourceManager("NTagLib.NTagLibStrings", Assembly::GetExecutingAssembly());

    public:
        static String^ GetString(String^ keyword) { return rm->GetString(keyword); }
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

    public ref class TagNameKey abstract sealed
    {
    private:
        static initonly array<String^>^ names = gcnew array<String^> {
            AcoustidFingerprint, AcoustidID, Album, AlbumArtist, AlbumArtistSort, AlbumSort, Arranger, Artist, ArtistSort, ArtistWebPage, AudioSourceWebPage,
                BPM,
                Comment, Compilation, Composer, ComposerSort, Conductor, Copyright, CopyrightURL,
                Date, DiscNumber, DiscSubtitle, DJMixer,
                EncodedBy, Encoding, EncodingTime, Engineer,
                FileType, FileWebPage,
                Genre,
                InitialKey, ISRC,
                Label, Language, Length, Lyricist, Lyrics,
                Media, Mixer, Mood, MusicBrainzAlbumArtistID, MusicBrainzAlbumID, MusicBrainzArtistID, MusicBrainzReleaseGroupID, MusicBrainzReleaseTrackID, MusicBrainzTrackID, MusicBrainzWorkID, MusicipPUID,
                OriginalAlbum, OriginalArtist, OriginalDate, OriginalFilename, OriginalLyricist, Owner,
                PaymentWebPage, Performer, PlaylistDelay, ProducedNotice, Producer, PublisherWebPage,
                RadioStation, RadioStationOwner, RadioStationWebPage, ReleaseCountry, ReleaseDate, ReleaseStatus, ReleaseType, Remixer,
                Subtitle,
                TaggingDate, Title, TitleSort, TrackNumber,
                URL,
                Work
        };

    public:
        literal String^ AcoustidFingerprint = "ACOUSTID_FINGERPRINT"; // TXXX
        literal String^ AcoustidID = "ACOUSTID_ID"; // TXXX
        literal String^ Album = "ALBUM"; // TALB
        literal String^ AlbumArtist = "ALBUMARTIST"; // TPE2
        literal String^ AlbumArtistSort = "ALBUMARTISTSORT"; // TSO2, non-standard, used by iTunes
        literal String^ AlbumSort = "ALBUMSORT"; // TSOA
        literal String^ Arranger = "ARRANGER"; // TIPL
        literal String^ Artist = "ARTIST"; // TPE1
        literal String^ ArtistSort = "ARTISTSORT"; // TSOP
        literal String^ ArtistWebPage = "ARTISTWEBPAGE"; // WOAR
        literal String^ AudioSourceWebPage = "AUDIOSOURCEWEBPAGE"; // WOAS
        literal String^ BPM = "BPM"; // TBPM
        literal String^ Comment = "COMMENT"; // COMM
        literal String^ Compilation = "COMPILATION"; //TCMP
        literal String^ Composer = "COMPOSER"; // TCOM
        literal String^ ComposerSort = "COMPOSERSORT"; // TSOC
        literal String^ Conductor = "CONDUCTOR"; // TPE3
        literal String^ Copyright = "COPYRIGHT"; // TCOP
        literal String^ CopyrightURL = "COPYRIGHTURL"; // WCOP
        literal String^ Date = "DATE"; // TDRC
        literal String^ DiscNumber = "DISCNUMBER"; // TPOS
        literal String^ DiscSubtitle = "DISCSUBTITLE"; // TSST
        literal String^ DJMixer = "DJMIXER"; // TIPL
        literal String^ EncodedBy = "ENCODEDBY"; // TENC
        literal String^ Encoding = "ENCODING"; // TSSE
        literal String^ EncodingTime = "ENCODINGTIME"; // TDEN
        literal String^ Engineer = "ENGINEER"; // TIPL
        literal String^ FileType = "FILETYPE"; // TFLT
        literal String^ FileWebPage = "FILEWEBPAGE"; // WOAF
        literal String^ Genre = "GENRE"; // TCON
        literal String^ InitialKey = "INITIALKEY"; // TKEY
        literal String^ ISRC = "ISRC"; // TSRC
        literal String^ Label = "LABEL"; // TPUB
        literal String^ Language = "LANGUAGE"; // TLAN
        literal String^ Length = "LENGTH"; // TLEN
        literal String^ Lyricist = "LYRICIST"; // TEXT
        literal String^ Lyrics = "LYRICS"; // USLT
        literal String^ Media = "MEDIA"; // TMED
        literal String^ Mixer = "MIXER"; // TIPL
        literal String^ Mood = "MOOD"; // TMOO
        literal String^ MusicBrainzAlbumArtistID = "MUSICBRAINZ_ALBUMARTISTID"; // TXXX
        literal String^ MusicBrainzAlbumID = "MUSICBRAINZ_ALBUMID"; // TXXX
        literal String^ MusicBrainzArtistID = "MUSICBRAINZ_ARTISTID"; // TXXX
        literal String^ MusicBrainzReleaseGroupID = "MUSICBRAINZ_RELEASEGROUPID"; // TXXX
        literal String^ MusicBrainzReleaseTrackID = "MUSICBRAINZ_RELEASETRACKID"; // TXXX
        literal String^ MusicBrainzTrackID = "MUSICBRAINZ_TRACKID"; // UFID
        literal String^ MusicBrainzWorkID = "MUSICBRAINZ_WORKID"; // TXXX
        literal String^ MusicipPUID = "MUSICIP_PUID"; // TXXX
        literal String^ OriginalAlbum = "ORIGINALALBUM"; // TOAL
        literal String^ OriginalArtist = "ORIGINALARTIST"; // TOPE
        literal String^ OriginalDate = "ORIGINALDATE"; // TDOR
        literal String^ OriginalFilename = "ORIGINALFILENAME"; // TOFN
        literal String^ OriginalLyricist = "ORIGINALLYRICIST"; // TOLY
        literal String^ Owner = "OWNER"; // TOWN
        literal String^ PaymentWebPage = "PAYMENTWEBPAGE"; // WPAY
        literal String^ Performer = "PERFORMER:<instrument>"; // TMCL
        literal String^ PlaylistDelay = "PLAYLISTDELAY"; // TDLY
        literal String^ ProducedNotice = "PRODUCEDNOTICE"; // TPRO
        literal String^ Producer = "PRODUCER"; // TIPL
        literal String^ PublisherWebPage = "PUBLISHERWEBPAGE"; // WPUB
        literal String^ RadioStation = "RADIOSTATION"; // TRSN
        literal String^ RadioStationOwner = "RADIOSTATIONOWNER"; // TRSO
        literal String^ RadioStationWebPage = "RADIOSTATIONWEBPAGE"; // WORS
        literal String^ ReleaseCountry = "RELEASECOUNTRY"; // TXXX
        literal String^ ReleaseDate = "RELEASEDATE"; // TDRL
        literal String^ ReleaseStatus = "RELEASESTATUS"; // TXXX
        literal String^ ReleaseType = "RELEASETYPE"; // TXXX
        literal String^ Remixer = "REMIXER"; // TPE4
        literal String^ Subtitle = "SUBTITLE"; // TIT3
        literal String^ TaggingDate = "TAGGINGDATE"; // TDTG
        literal String^ Title = "TITLE"; // TIT2
        literal String^ TitleSort = "TITLESORT"; // TSOT
        literal String^ TrackNumber = "TRACKNUMBER"; // TRCK
        literal String^ URL = "URL"; // WXXX
        literal String^ Work = "WORK"; // TIT1

        static ReadOnlyCollection<String^>^ GetAllNames()
        {
            return Array::AsReadOnly(names);
        }
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

    // FileFormatException has been moved from System.IO.dll to System.IO.Packaging.dll, and is no longer .NET built-in.
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
}

namespace
{
    #pragma region Helper functions

    TagLib::PropertyMap GetPropertiesFromTags(Dictionary<String^, List<String^>^>^ tags)
    {
        TagLib::PropertyMap properties;

        for each (auto kvp in tags)
        {
            TagLib::String key = msclr::interop::marshal_as<std::wstring>(kvp.Key);
            TagLib::StringList values;

            for each (auto value in kvp.Value)
                values.append(msclr::interop::marshal_as<std::wstring>(value));

            properties.insert(key, values);
        }

        return properties;
    }

    Dictionary<String^, List<String^>^>^ GetTagsFromProperties(const TagLib::PropertyMap& properties)
    {
        auto tags = gcnew Dictionary<String^, List<String^>^>(properties.size());

        for (auto it = properties.begin(); it != properties.end(); ++it)
        {
            TagLib::String tag = it->first;
            TagLib::StringList values = it->second;

            auto ntag = gcnew String(tag.toCWString());
            tags->Add(ntag, gcnew List<String^>(values.size()));

            for (auto it2 = values.begin(); it2 != values.end(); ++it2)
                tags[ntag]->Add(gcnew String(it2->toCWString()));
        }

        return tags;
    }

    List<NTagLib::Picture^>^ GetCoversFromComplexProperties(const TagLib::List<TagLib::VariantMap>& properties)
    {
        auto pictures = gcnew List<NTagLib::Picture^>(properties.size());
        for (const auto& cover : properties)
        {
            auto data = cover.value("data").value<TagLib::ByteVector>();
            if (data.isEmpty())
                continue;

            auto mimeType = cover.value("mimeType").value<TagLib::String>();
            auto pictureType = cover.value("pictureType").value<TagLib::String>();
            auto description = cover.value("description").value<TagLib::String>();

            pictures->Add(gcnew NTagLib::Picture(data, mimeType, pictureType, description));
        }

        return pictures;
    }

    TagLib::List<TagLib::VariantMap> GetComplexPropertiesFromCovers(List<NTagLib::Picture^>^ covers)
    {
        TagLib::List<TagLib::VariantMap> pictures;
        for each (auto cover in covers)
        {
            TagLib::VariantMap picture;
            picture.insert("data", ManagedArrayToByteVector(cover->Data));
            picture.insert("mimeType", TagLib::String(msclr::interop::marshal_as<std::string>(cover->MimeType)));
            picture.insert("pictureType", TagLib::String(msclr::interop::marshal_as<std::string>(cover->PictureType)));
            picture.insert("description", TagLib::String(msclr::interop::marshal_as<std::wstring>(cover->Description)));

            pictures.append(picture);
        }

        return pictures;
    }

    #pragma endregion
}

namespace NTagLib
{
    #pragma region TaglibTagger class

    constexpr auto PICTURE_KEY = "PICTURE";

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
                const ID3StringHandler id3StringHandler(TaglibSettings::ID3Latin1Encoding->CodePage);
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
            const TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::FLAC::File file(fileName, true, TagLib::AudioProperties::ReadStyle::Average);
            VerifyFile(file, false);
            if (!file.hasXiphComment())
                throw gcnew InvalidFileFormatException(ResourceStrings::GetString("InvalidAudioFile"));

            const TagLib::FLAC::Properties* properties = file.audioProperties();
            const TagLib::Ogg::XiphComment* xiph = file.xiphComment();

            TagLib::StringList buffer = xiph->vendorID().split();
            codecVersion = buffer.size() > 2 ? "FLAC " + gcnew String(buffer[2].toCWString()) : "FLAC";
            codec = "FLAC";

            bitrate = properties->bitrate(); // in kb/s
            duration = TimeSpan::FromMilliseconds(properties->lengthInMilliseconds());
            sampleRate = properties->sampleRate(); // in Hertz
            channels = static_cast<byte>(properties->channels()); // number of audio channels
            bitsPerSample = properties->bitsPerSample(); // in bits

            tags = GetTagsFromProperties(file.properties());
            pictures = GetCoversFromComplexProperties(file.complexProperties(PICTURE_KEY));
        }

        List<String^>^ WriteFlacFile()
        {
            String^ path = fullPath;
            const TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::FLAC::File file(fileName, false);
            VerifyFile(file, true);

            TagLib::Ogg::XiphComment* xiph = file.xiphComment(true);
            const TagLib::PropertyMap map = xiph->setProperties(GetPropertiesFromTags(tags));
            file.setComplexProperties(PICTURE_KEY, GetComplexPropertiesFromCovers(pictures));

            file.strip(TagLib::FLAC::File::ID3v1 | TagLib::FLAC::File::ID3v2);
            file.save();
            return PropertyMapToManagedList(map);
        }

        void ReadMp3File(String^ path)
        {
            const TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            const TagLib::MPEG::File file(fileName, true, TagLib::AudioProperties::ReadStyle::Average);
            VerifyFile(file, false);

            const TagLib::MPEG::Properties* properties = file.audioProperties();
            if (properties->bitrate() == 0)
                throw gcnew InvalidFileFormatException(ResourceStrings::GetString("InvalidAudioFile"));

            codec = codecVersion = "MP3";

            bitrate = properties->bitrate(); // in kb/s
            duration = TimeSpan::FromMilliseconds(properties->lengthInMilliseconds());
            sampleRate = properties->sampleRate(); // in Hertz
            channels = static_cast<byte>(properties->channels()); // number of audio channels
            bitsPerSample = 0; // in bits

            tags = GetTagsFromProperties(file.properties());

            if (file.hasID3v2Tag())
                pictures = GetCoversFromComplexProperties(file.complexProperties(PICTURE_KEY));
            else
                pictures = gcnew List<Picture^>();
        }

        List<String^>^ WriteMp3File()
        {
            String^ path = fullPath;
            const TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::MPEG::File file(fileName, false);
            VerifyFile(file, true);

            TagLib::ID3v2::Tag* id3 = file.ID3v2Tag(true);
            id3->setProperties(GetPropertiesFromTags(tags)); // ID3 implements the complete PropertyMap interface, so id3->setProperties() always returns an empty PropertyMap.
            id3->setComplexProperties(PICTURE_KEY, GetComplexPropertiesFromCovers(pictures));

            auto tagVersion = static_cast<TagLib::ID3v2::Version>(id3->header()->majorVersion());

            const auto minVersion = static_cast<TagLib::ID3v2::Version>(TaglibSettings::MinId3Version);
            const auto maxVersion = static_cast<TagLib::ID3v2::Version>(TaglibSettings::MaxId3Version);

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
            return gcnew List<String^>();
        }

        void ReadOggFile(String^ path)
        {
            const TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            const TagLib::Ogg::Vorbis::File file(fileName, true, TagLib::AudioProperties::ReadStyle::Average);
            VerifyFile(file, false);

            const TagLib::Vorbis::Properties* properties = file.audioProperties();
            const TagLib::Ogg::XiphComment* xiph = file.tag();

            auto vendor = gcnew String(xiph->vendorID().toCWString());
            codecVersion = GetVorbisVersionFromVendor(vendor);
            codec = "Vorbis";

            bitrate = properties->bitrate(); // in kb/s
            duration = TimeSpan::FromMilliseconds(properties->lengthInMilliseconds());
            sampleRate = properties->sampleRate(); // in Hertz
            channels = static_cast<byte>(properties->channels()); // number of audio channels

            tags = GetTagsFromProperties(file.properties());
            pictures = GetCoversFromComplexProperties(file.complexProperties(PICTURE_KEY));
        }

        List<String^>^ WriteOggFile()
        {
            String^ path = fullPath;
            const TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::Ogg::Vorbis::File file(fileName, false);
            VerifyFile(file, true);

            TagLib::Ogg::XiphComment* xiph = file.tag();
            const TagLib::PropertyMap map = xiph->setProperties(GetPropertiesFromTags(tags));
            xiph->setComplexProperties(PICTURE_KEY, GetComplexPropertiesFromCovers(pictures));

            file.save();
            return PropertyMapToManagedList(map);
        }

        void ReadOpusFile(String^ path)
        {
            const TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            const TagLib::Ogg::Opus::File file(fileName, true, TagLib::AudioProperties::ReadStyle::Average);
            VerifyFile(file, false);

            const TagLib::Ogg::Opus::Properties* properties = file.audioProperties();
            const TagLib::Ogg::XiphComment* xiph = file.tag();

            auto vendor = gcnew String(xiph->vendorID().toCWString());
            const int endIndex = vendor->IndexOf(',');
            codecVersion = endIndex != -1 ? vendor->Substring(0, endIndex)->Replace("libopus", "Opus") : "Opus";
            codec = "Opus";

            bitrate = properties->bitrate(); // in kb/s
            duration = TimeSpan::FromMilliseconds(properties->lengthInMilliseconds());
            sampleRate = properties->sampleRate(); // in Hertz
            channels = static_cast<byte>(properties->channels()); // number of audio channels

            tags = GetTagsFromProperties(file.properties());
            pictures = GetCoversFromComplexProperties(file.complexProperties(PICTURE_KEY));
        }

        List<String^>^ WriteOpusFile()
        {
            String^ path = fullPath;
            const TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::Ogg::Opus::File file(fileName, false);
            VerifyFile(file, true);

            TagLib::Ogg::XiphComment* xiph = file.tag();
            const TagLib::PropertyMap map = xiph->setProperties(GetPropertiesFromTags(tags));
            xiph->setComplexProperties(PICTURE_KEY, GetComplexPropertiesFromCovers(pictures));

            file.save();
            return PropertyMapToManagedList(map);
        }

        void ReadWmaFile(String^ path)
        {
            const TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            const TagLib::ASF::File file(fileName, true, TagLib::AudioProperties::ReadStyle::Average);
            VerifyFile(file, false);

            const TagLib::ASF::Properties* properties = file.audioProperties();

            codecVersion = !properties->codecName().isEmpty() ? gcnew String(properties->codecName().toCString()) : "WMA";
            codec = "WMA";

            bitrate = properties->bitrate(); // in kb/s
            duration = TimeSpan::FromMilliseconds(properties->lengthInMilliseconds());
            sampleRate = properties->sampleRate(); // in Hertz
            channels = static_cast<byte>(properties->channels()); // number of audio channels
            bitsPerSample = properties->bitsPerSample(); // in bits

            tags = GetTagsFromProperties(file.properties());
            pictures = GetCoversFromComplexProperties(file.complexProperties(PICTURE_KEY));
        }

        List<String^>^ WriteWmaFile()
        {
            String^ path = fullPath;
            const TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::ASF::File file(fileName, false);
            VerifyFile(file, true);

            const TagLib::PropertyMap map = file.setProperties(GetPropertiesFromTags(tags));
            file.setComplexProperties(PICTURE_KEY, GetComplexPropertiesFromCovers(pictures));

            file.save();
            return PropertyMapToManagedList(map);
        }

        void ReadM4aFile(String^ path)
        {
            const TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            const TagLib::MP4::File file(fileName, true, TagLib::AudioProperties::ReadStyle::Average);
            VerifyFile(file, false);

            const TagLib::MP4::Properties* properties = file.audioProperties();

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
            channels = static_cast<byte>(properties->channels()); // number of audio channels
            bitsPerSample = properties->bitsPerSample(); // in bits

            tags = GetTagsFromProperties(file.properties());
            pictures = GetCoversFromComplexProperties(file.complexProperties(PICTURE_KEY));
        }

        List<String^>^ WriteM4aFile()
        {
            String^ path = fullPath;
            const TagLib::FileName fileName(msclr::interop::marshal_as<std::wstring>(path).c_str());

            TagLib::MP4::File file(fileName, false);
            VerifyFile(file, true);

            const TagLib::PropertyMap map = file.setProperties(GetPropertiesFromTags(tags));
            file.setComplexProperties(PICTURE_KEY, GetComplexPropertiesFromCovers(pictures));

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

        static initonly Version^ TagLibVersion = gcnew Version(TAGLIB_MAJOR_VERSION, TAGLIB_MINOR_VERSION, TAGLIB_PATCH_VERSION);

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
                auto values = gcnew List<String^>();
                values->Add(value);
                tags->Add(tag, values);
            }
        }
    };

    #pragma endregion
}
