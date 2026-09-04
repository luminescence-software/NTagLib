using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace NTagLib.Tests;

[TestClass]
[DoNotParallelize]
public class TaglibTaggerTests
{
   private const string TestAudioFilesDirectory = @"D:\Home\Important\Development\Toolkit\Tests\Audio\Tags";

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

      string path = WorkOnCopy(Path.Combine(TestAudioFilesDirectory, filename));
      var tagger = new TaglibTagger(path);
      tagger.ReplaceTag(TagNameKey.Artist, "artist 1", "artist 2");
      tagger.AddTag("FAKE", "toto");

      byte[] data = File.ReadAllBytes(Path.Combine(TestAudioFilesDirectory, "cover.jpg"));
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
   [DataRow("artist")]
   [DataRow("FAKE")]
   [DataRow("MUSICBRAINZ_ALBUMID")]
   [DataRow("ARRANGER")]
   [DataRow("DJMIXER")]
   [DataRow("ENGINEER")]
   [DataRow("MIXER")]
   [DataRow("PRODUCER")]
   [DataRow("PERFORMER:GUITAR")]
   public void MultipleTextValuesForceId3v24(string tag)
   {
      string path = WorkOnCopy(Path.Combine(TestAudioFilesDirectory, "mp3.mp3"));
      Id3Version oldMinVersion = TaglibSettings.MinId3Version;
      Id3Version oldMaxVersion = TaglibSettings.MaxId3Version;

      try
      {
         TaglibSettings.MinId3Version = Id3Version.id3v23;
         TaglibSettings.MaxId3Version = Id3Version.id3v24;

         var tagger = new TaglibTagger(path);
         tagger.Tags.Clear();
         tagger.AddTag(tag, "value 1", "value 2");
         tagger.SaveTags();

         Assert.AreEqual(4, ReadId3MajorVersion(path));

         tagger.ReloadTags();
         Assert.IsTrue(tagger.Tags.TryGetValue(tag, out List<string>? values), $"Available tags: {String.Join(", ", tagger.Tags.Keys)}");
         Assert.AreSequenceEqual(["value 1", "value 2"], values);
      }
      finally
      {
         TaglibSettings.MinId3Version = oldMinVersion;
         TaglibSettings.MaxId3Version = oldMaxVersion;
         File.Delete(path);
      }
   }

   [TestMethod]
   public void MultipleGenresKeepId3v23()
   {
      string path = WorkOnCopy(Path.Combine(TestAudioFilesDirectory, "mp3.mp3"));

      try
      {
         var tagger = new TaglibTagger(path);
         tagger.Tags.Clear();
         tagger.AddTag(TagNameKey.Genre, "Rock", "Pop");
         tagger.SaveTags();

         Assert.AreEqual(3, ReadId3MajorVersion(path));

         tagger.ReloadTags();
         Assert.IsTrue(tagger.Tags.TryGetValue(TagNameKey.Genre, out List<string>? values));
         Assert.AreSequenceEqual(["Rock", "Pop"], values);
      }
      finally
      {
         File.Delete(path);
      }
   }

   [TestMethod]
   public void DistinctInvolvedRolesKeepId3v23()
   {
      string path = WorkOnCopy(Path.Combine(TestAudioFilesDirectory, "mp3.mp3"));

      try
      {
         var tagger = new TaglibTagger(path);
         tagger.Tags.Clear();
         tagger.AddTag(TagNameKey.Arranger, "arranger");
         tagger.AddTag(TagNameKey.Producer, "producer");
         tagger.AddTag("PERFORMER:GUITAR", "performer");
         tagger.SaveTags();

         Assert.AreEqual(3, ReadId3MajorVersion(path));

         tagger.ReloadTags();
         Assert.AreEqual("arranger", tagger.GetTagValues(TagNameKey.Arranger).Single());
         Assert.AreEqual("producer", tagger.GetTagValues(TagNameKey.Producer).Single());
         Assert.AreEqual("performer", tagger.GetTagValues("PERFORMER:GUITAR").Single());
      }
      finally
      {
         File.Delete(path);
      }
   }

   [TestMethod]
   [DataRow("DISCSUBTITLE")]
   [DataRow("ENCODINGTIME")]
   [DataRow("COMPOSERSORT")]
   public void Id3v24OnlyTextFrameForcesId3v24WithOneValue(string tag)
   {
      string path = WorkOnCopy(Path.Combine(TestAudioFilesDirectory, "mp3.mp3"));
      Id3Version oldMinVersion = TaglibSettings.MinId3Version;
      Id3Version oldMaxVersion = TaglibSettings.MaxId3Version;

      try
      {
         TaglibSettings.MinId3Version = Id3Version.id3v23;
         TaglibSettings.MaxId3Version = Id3Version.id3v24;

         var tagger = new TaglibTagger(path);
         tagger.Tags.Clear();
         tagger.AddTag(tag, "value");
         tagger.SaveTags();

         Assert.AreEqual(4, ReadId3MajorVersion(path));

         tagger.ReloadTags();
         Assert.IsTrue(tagger.Tags.TryGetValue(tag, out List<string>? values));
         Assert.AreEqual("value", values.Single());
      }
      finally
      {
         TaglibSettings.MinId3Version = oldMinVersion;
         TaglibSettings.MaxId3Version = oldMaxVersion;
         File.Delete(path);
      }
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
      var tagger = new TaglibTagger(Path.Combine(TestAudioFilesDirectory, filename));

      Assert.IsGreaterThan(0, tagger.Bitrate);
      Assert.IsTrue(tagger.BitsPerSample is 0 or 16);
      Assert.AreEqual(2, tagger.Channels);
      Assert.IsTrue(tagger.SampleRate is 22_050 or 44_100 or 48_000);
      Assert.IsGreaterThan(TimeSpan.Zero, tagger.Duration);
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

   private static int ReadId3MajorVersion(string path)
   {
      using var stream = File.OpenRead(path);
      Span<byte> header = stackalloc byte[4];
      Assert.AreEqual(header.Length, stream.Read(header));
      Assert.AreSequenceEqual("ID3"u8.ToArray(), header[..3].ToArray());
      return header[3];
   }
}
