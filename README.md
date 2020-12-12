# NTagLib

NTagLib is a C++/CLI library for tagging audio files with [TagLib](https://github.com/taglib/taglib) in managed code.

**Like all C++/CLI libraries, NTagLib requires the Visual C++ 2015-2019 runtime (x86 or x64).**

This library has been designed to be used with my tags editor, [Metatogger](https://www.luminescence-software.org/metatogger).

Here is a screenshot of that software:
![Screenshot of Metatogger](https://www.luminescence-software.org/user/pages/02.metatogger/metatogger_main.png)

Feel free to fork the code and [contribute](https://guides.github.com/activities/contributing-to-open-source/) to NTagLib.

## How to use the tagging API in C♯

```C#
using NTagLib;

// supports the following audio formats: MP3 (*.mp3), FLAC (*.flac), Ogg Vorbis (*.ogg), WMA (*.wma) and AAC/ALAC (*.m4a)
var tagger = new TaglibTagger(@"C:\MyFolder\MyAudioFile.flac");

// getting technical informations

string codecName = tagger.Codec; // "FLAC"
string codecVersion = tagger.CodecVersion; // "1.2.1"
byte channelsCount = tagger.Channels; // 2
TimeSpan duration = tagger.Duration;
int sampleRateInHertz = tagger.SampleRate; // 44100
int bitsPerSample = tagger.BitsPerSample; // 16

// getting or setting textual tags

// textual tags is stored in a dictionary where the key is the tag name in uppercase
Dictionary<string, List<string>> tagsRepository = tagger.Tags;
string artist = tagger.GetTagValues(TagNameKey.Artist).FirstOrDefault();
tagger.AddTag(TagNameKey.Artist, "Artist 2", "Artist 3");
tagger.ReplaceTag(TagNameKey.Title, "All You Need Is Love");
tagger.RemoveTag(TagNameKey.Comment);
tagger.AddTag("MY_KEY_NAME", "Custom Tag Value");

// getting or setting embedded covers

List<Picture> pictures = tagger.Pictures;
if (pictures != null && pictures.Count != 0)
{
   Picture mainPicture = pictures.FirstOrDefault(p => p.Type == PictureType.FrontCover);
   if (mainPicture == null) mainPicture = pictures[0];

   byte[] data = mainPicture.Data;
   Format pictureFormat = mainPicture.PictureFormat; // Format.JPEG
}

pictures.Clear();

tagger.Save(); // save changes
```

## Licence

NTagLib is distributed under the same licensing conditions as [TagLib](https://github.com/taglib/taglib).

_Sylvain Rougeaux (Cyber Sinh)_