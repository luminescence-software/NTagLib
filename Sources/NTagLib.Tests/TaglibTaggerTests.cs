using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace NTagLib.Tests;

[TestClass]
public class TaglibTaggerTests
{
   [TestMethod]
   [DataRow("flac.flac")]
   [DataRow("mp3.mp3")]
   [DataRow("vorbis.ogg")]
   [DataRow("wma.wma")]
   [DataRow("aac.m4a")]
   [DataRow("opus.opus")]
   public void WriteTags(string filename)
   {
      AudioFileFormat format = AudioFileFormats.FromExtension(filename);
      if (format == AudioFileFormat.Mp3)
      {
         TaglibSettings.MinId3Version = Id3Version.id3v23;
         TaglibSettings.MaxId3Version = Id3Version.id3v24;
         //TaglibSettings.ID3Latin1Encoding = Encoding.GetEncoding(1252);
      }

      const string folder = @"D:\Home\Important\Development\Toolkit\Tests\Audio\Tags";
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
         Assert.IsTrue(tagger.Tags.TryGetValue(TagNameKey.Artist, out var artists) && artists.Count == 1 && artists.Contains("artist 1 artist 2"));
      else
         Assert.IsTrue(tagger.Tags.TryGetValue(TagNameKey.Artist, out var artists) && artists.Count == 2 && artists.Contains("artist 1") && artists.Contains("artist 2"));

      if (!rejectedTags.Contains("FAKE"))
         Assert.IsTrue(tagger.Tags.TryGetValue("FAKE", out var fakes) && fakes.Contains("toto"));

      Assert.IsTrue(picture != null && picture.Data.SequenceEqual(data) && picture is { PictureType: PictureTypes.FrontCover, MimeType: MimeTypes.JPEG, Description: "" });

      File.Delete(path);
   }

   [TestMethod]
   [DataRow("flac.flac")]
   [DataRow("mp3.mp3")]
   [DataRow("vorbis.ogg")]
   [DataRow("wma.wma")]
   [DataRow("aac.m4a")]
   [DataRow("opus.opus")]
   public void ReadAudioProperties(string filename)
   {
      const string folder = @"D:\Home\Important\Development\Toolkit\Conception\NTagLib\Test";
      var tagger = new TaglibTagger(Path.Combine(folder, filename));

      Assert.IsGreaterThan(0, tagger.Bitrate);
      Assert.IsTrue(tagger.BitsPerSample is 0 or 16);
      Assert.AreEqual(2, tagger.Channels);
      Assert.IsTrue(tagger.SampleRate is 22_050 or 44_100 or 48_000);
      Assert.IsTrue(tagger.Duration > TimeSpan.Zero);
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
