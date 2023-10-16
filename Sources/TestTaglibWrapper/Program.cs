using System;
using System.IO;
using System.Text;
using NTagLib;

namespace TestTaglibWrapper
{
   public class Program
   {
      public static void Main()
      {
         Console.OutputEncoding = Encoding.UTF8;

         TaglibSettings.MinId3Version = Id3Version.id3v23;
         TaglibSettings.MaxId3Version = Id3Version.id3v24;
         //TaglibSettings.ID3Latin1Encoding = Encoding.GetEncoding(1252);

         Version taglibVersion = TaglibTagger.TagLibVersion;
         Console.WriteLine(taglibVersion.ToString());

         const bool workOnCopy = true;
         const bool writeTags = true;
         const bool addCover = true;
         const bool addExoticTag = false;
         var paths = new[]
         {
            @"E:\Home\Important\Development\Toolkit\Conception\NTagLib\Test\flac.flac",
            @"E:\Home\Important\Development\Toolkit\Conception\NTagLib\Test\mp3.mp3",
            @"E:\Home\Important\Development\Toolkit\Conception\NTagLib\Test\vorbis.ogg",
            @"E:\Home\Important\Development\Toolkit\Conception\NTagLib\Test\wma.wma",
            @"E:\Home\Important\Development\Toolkit\Conception\NTagLib\Test\aac.m4a",
            @"E:\Home\Important\Development\Toolkit\Conception\NTagLib\Test\opus.opus"
         };

         DoWork(workOnCopy, writeTags, addCover, addExoticTag, paths);
      }

      private static void DoWork(bool workOnCopy, bool writeTags, bool addCover, bool addExoticTag, string[] paths)
      {
         foreach (string path in paths)
         {
            string realPath = workOnCopy && writeTags ? WorkOnCopy(path) : path;

            ReadAudioFile(realPath);
            if (writeTags)
            {
               WriteAudioFile(realPath, addCover, addExoticTag);
               ReadAudioFile(realPath);
            }

            if (workOnCopy && writeTags)
               File.Delete(realPath);

            Console.WriteLine("----------------------------------------");
         }
      }

      private static void WriteAudioFile(string path, bool addCover, bool addExoticTag)
      {
         Console.WriteLine($"Writing file {path}");

         var tagger = new TaglibTagger(path);

         Console.WriteLine(@"Ajout: ARTIST=""artist 1"" ARTIST=""artist 2""");
         tagger.ReplaceTag(TagNameKey.Artist, "artist 1", "artist 2");

         if (addExoticTag)
         {
            Console.WriteLine(@"Ajout: FAKE=""toto""");
            tagger.AddTag("FAKE", "toto");
         }

         tagger.Pictures.Clear();

         if (addCover)
         {
            const string cover = @"E:\Home\Important\Development\Toolkit\Conception\NTagLib\Test\cover.jpg";
            var fi = new FileInfo(cover);
            Console.WriteLine($"Adding a cover : FrontCover, JPEG, {fi.Length} bytes, without description");
            tagger.Pictures.Add(new Picture(File.ReadAllBytes(cover), MimeTypes.JPEG, PictureTypes.FrontCover, String.Empty));
         }

         Console.WriteLine("Saving...");
         tagger.SaveTags();
         Console.WriteLine("Saved.");

         Console.WriteLine();
      }

      private static void ReadAudioFile(string path)
      {
         Console.WriteLine($"Reading file {path}");

         var tagger = new TaglibTagger(path);
         Console.WriteLine($"Codec = {tagger.Codec}");
         Console.WriteLine($"Codec Version = {tagger.CodecVersion}");
         Console.WriteLine($"Bitrate = {tagger.Bitrate}");
         Console.WriteLine($"Duration = {tagger.Duration} seconds");
         Console.WriteLine($"Sample rate = {tagger.SampleRate} Hertz");
         Console.WriteLine($"Channels = {tagger.Channels} channels");
         Console.WriteLine($"Bits per sample = {tagger.BitsPerSample} bits");
         Console.WriteLine();

         foreach (var tag in tagger.Tags)
         {
            Console.WriteLine($"{tag.Key}:");
            foreach (string value in tag.Value)
               Console.WriteLine($"   {value}");

            Console.WriteLine();
         }

         if (tagger.Pictures != null)
         {
            foreach (var cover in tagger.Pictures)
            {
               Console.WriteLine("COVER ART:");
               Console.WriteLine($"   PictureType={cover.PictureType}");
               Console.WriteLine($"   Description={cover.Description}");
               Console.WriteLine($"   MimeType={cover.MimeType}");
               Console.WriteLine($"   Size={cover.Data.Length} bytes");

               Console.WriteLine();
            }
         }
      }

      private static string WorkOnCopy(string path)
      {
         string newPath = Path.Combine(GetParentDirectory(path), Path.ChangeExtension(Path.GetRandomFileName(), Path.GetExtension(path)));
         if (File.Exists(newPath))
            return WorkOnCopy(path);

         File.Copy(path, newPath, false);
         return newPath;
      }

      private static string GetParentDirectory(string path)
      {
         // Path.GetDirectoryName() returns null if path is null or if the file is in a root path

         ArgumentNullException.ThrowIfNull(path);

         string? parentDirectory = Path.GetDirectoryName(path) ?? Path.GetPathRoot(path);

         if (String.IsNullOrEmpty(parentDirectory))
            throw new ArgumentException("The path does not contain any root directory information.", nameof(path));

         return parentDirectory;
      }
   }
}
