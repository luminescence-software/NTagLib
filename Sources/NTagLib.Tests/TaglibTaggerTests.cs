using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Xunit;

namespace NTagLib.Tests;

public class TaglibTaggerTests
{
   //private readonly ITestOutputHelper output;
   //public TaglibTaggerTests(ITestOutputHelper outputHelper)
   //{
   //    output = outputHelper;
   //}

   [Theory]
   [InlineData("flac.flac")]
   [InlineData("mp3.mp3")]
   [InlineData("vorbis.ogg")]
   [InlineData("wma.wma")]
   [InlineData("aac.m4a")]
   [InlineData("opus.opus")]
   public void WriteTags(string filename)
   {
      AudioFileFormat format = AudioFileFormats.FromExtension(filename);
      if (format == AudioFileFormat.Mp3)
      {
         TaglibSettings.MinId3Version = Id3Version.id3v23;
         TaglibSettings.MaxId3Version = Id3Version.id3v24;
         //TaglibSettings.ID3Latin1Encoding = Encoding.GetEncoding(1252);
      }

      const string folder = @"E:\Home\Important\Development\Toolkit\Tests\Audio\Tags";
      string path = WorkOnCopy(Path.Combine(folder, filename));
      var tagger = new TaglibTagger(path);
      tagger.ReplaceTag(TagNameKey.Artist, "artist 1", "artist 2");
      tagger.AddTag("FAKE", "toto");

      byte[] data = File.ReadAllBytes(Path.Combine(folder, "cover.jpg"));
      tagger.Pictures.Clear();
      tagger.Pictures.Add(new Picture(data, MimeTypes.JPEG, PictureTypes.FrontCover, String.Empty));
      IEnumerable<string> rejectedTags = tagger.SaveTags();

      tagger.ReloadTags();
      Picture? picture = tagger.Pictures.FirstOrDefault();

      if (format == AudioFileFormat.Wma)
         Assert.True(tagger.Tags.TryGetValue(TagNameKey.Artist, out var artists) && artists.Count == 1 && artists.Contains("artist 1 artist 2"));
      else
         Assert.True(tagger.Tags.TryGetValue(TagNameKey.Artist, out var artists) && artists.Count == 2 && artists.Contains("artist 1") && artists.Contains("artist 2"));

      if (!rejectedTags.Contains("FAKE"))
         Assert.True(tagger.Tags.TryGetValue("FAKE", out var fakes) && fakes.Contains("toto"));

      Assert.True(picture != null && picture.Data.SequenceEqual(data) && picture is { PictureType: PictureTypes.FrontCover, MimeType: MimeTypes.JPEG, Description: "" });

      File.Delete(path);
   }

   [Theory]
   [InlineData("flac.flac")]
   [InlineData("mp3.mp3")]
   [InlineData("vorbis.ogg")]
   [InlineData("wma.wma")]
   [InlineData("aac.m4a")]
   [InlineData("opus.opus")]
   public void ReadAudioProperties(string filename)
   {
      const string folder = @"E:\Home\Important\Development\Toolkit\Conception\NTagLib\Test";
      var tagger = new TaglibTagger(Path.Combine(folder, filename));

      Assert.True(tagger.Bitrate > 0);
      Assert.True(tagger.BitsPerSample is 0 or 16);
      Assert.True(tagger.Channels == 2);
      Assert.True(tagger.SampleRate is 22_050 or 44_100 or 48_000);
      Assert.True(tagger.Duration > TimeSpan.Zero);
   }

   private static string WorkOnCopy(string path)
   {
      while (true)
      {
         string newPath = Path.Combine(Path.GetDirectoryName(path)!, Path.ChangeExtension(Path.GetRandomFileName(), Path.GetExtension(path)));
         if (File.Exists(newPath)) continue;

         File.Copy(path, newPath, false);
         return newPath;
      }
   }
}
