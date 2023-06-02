#include "stdafx.h"

#include <id3v2tag.h>
#include <id3v1tag.h>
#include <flacpicture.h>
#include <id3v1genres.h>
#include <mp4coverart.h>

#include "Interop.h"

using namespace System;
using namespace System::Text;
using namespace System::Collections::ObjectModel;

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
        // A type not enumerated below
        Other = TagLib::FLAC::Picture::Type::Other,
        // 32x32 PNG image that should be used as the file icon
        FileIcon = TagLib::FLAC::Picture::Type::FileIcon,
        // File icon of a different size or format
        OtherFileIcon = TagLib::FLAC::Picture::Type::OtherFileIcon,
        // Front cover image of the album
        FrontCover = TagLib::FLAC::Picture::Type::FrontCover,
        // Back cover image of the album
        BackCover = TagLib::FLAC::Picture::Type::BackCover,
        // Inside leaflet page of the album
        LeafletPage = TagLib::FLAC::Picture::Type::LeafletPage,
        // Image from the album itself
        Media = TagLib::FLAC::Picture::Type::Media,
        // Picture of the lead artist or soloist
        LeadArtist = TagLib::FLAC::Picture::Type::LeadArtist,
        // Picture of the artist or performer
        Artist = TagLib::FLAC::Picture::Type::Artist,
        // Picture of the conductor
        Conductor = TagLib::FLAC::Picture::Type::Conductor,
        // Picture of the band or orchestra
        Band = TagLib::FLAC::Picture::Type::Band,
        // Picture of the composer
        Composer = TagLib::FLAC::Picture::Type::Composer,
        // Picture of the lyricist or text writer
        Lyricist = TagLib::FLAC::Picture::Type::Lyricist,
        // Picture of the recording location or studio
        RecordingLocation = TagLib::FLAC::Picture::Type::RecordingLocation,
        // Picture of the artists during recording
        DuringRecording = TagLib::FLAC::Picture::Type::DuringRecording,
        // Picture of the artists during performance
        DuringPerformance = TagLib::FLAC::Picture::Type::DuringPerformance,
        // Picture from a movie or video related to the track
        MovieScreenCapture = TagLib::FLAC::Picture::Type::MovieScreenCapture,
        // Picture of a large, coloured fish
        ColouredFish = TagLib::FLAC::Picture::Type::ColouredFish,
        // Illustration related to the track
        Illustration = TagLib::FLAC::Picture::Type::Illustration,
        // Logo of the band or performer
        BandLogo = TagLib::FLAC::Picture::Type::BandLogo,
        // Logo of the publisher (record company)
        PublisherLogo = TagLib::FLAC::Picture::Type::PublisherLogo
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
}
