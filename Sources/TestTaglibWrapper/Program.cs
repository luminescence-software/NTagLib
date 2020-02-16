using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using NTagLib;

namespace TestTaglibWrapper
{
   class Program
   {
      static void Main()
      {
         Console.OutputEncoding = Encoding.UTF8;

         TaglibSettings.MinId3Version = Id3Version.id3v23;
         TaglibSettings.MaxId3Version = Id3Version.id3v24;
         //TaglibSettings.ID3Latin1Encoding = Encoding.GetEncoding(1252);

         bool workOnCopy = true;
         bool writeTags = false;
         bool addCover = false;
         bool addExoticTag = false;
         var paths = new string[]
         {
            //@"C:\Users\cyber\Downloads\Test\flac.ogg", // should crash because Ogg FLAC is not supported yet

            //@"C:\Users\cyber\Downloads\Test\flac.flac",
            //@"C:\Users\cyber\Downloads\Test\mp3.mp3",
            //@"C:\Users\cyber\Downloads\Test\vorbis.ogg",
            //@"C:\Users\cyber\Downloads\Test\wma.wma",
            //@"C:\Users\cyber\Downloads\Test\aac.m4a",
            @"C:\Users\Sylvain Rougeaux\Downloads\bug_player_chromaprint\access_violation_exception.mp3"
         };

         DoWork(workOnCopy, writeTags, addCover, addExoticTag, paths);

         Console.WriteLine("Press any key to exit...");
         Console.ReadKey();
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

         TaglibTagger tagger = new TaglibTagger(path);

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
            string cover = @"C:\Users\cyber\Downloads\folder.jpg";
            var fi = new FileInfo(cover);
            Console.WriteLine($"Adding a cover : FrontCover, JPEG, {fi.Length} bytes, without description");
            tagger.Pictures.Add(new Picture(File.ReadAllBytes(cover), PictureFormat.JPEG, PictureType.FrontCover, null)); 
         }

         Console.WriteLine("Saving...");
         tagger.SaveTags();
         Console.WriteLine("Saved.");

         Console.WriteLine();
      }

      private static void ReadAudioFile(string path)
      {
         Console.WriteLine($"Reading file {path}");

         TaglibTagger tagger = new TaglibTagger(path);
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
               Console.WriteLine($"   Type={cover.Type}");
               Console.WriteLine($"   Description={cover.Description}");
               Console.WriteLine($"   PictureFormat={cover.Format}");
               Console.WriteLine($"   Size={cover.Data.Length} bytes");

               Console.WriteLine();
            } 
         }
      }

      private static string WorkOnCopy(string path)
      {
         string newPath = Path.Combine(Path.GetDirectoryName(path), Path.ChangeExtension(Path.GetRandomFileName(), Path.GetExtension(path)));
         if (File.Exists(newPath))
            return WorkOnCopy(path);

         File.Copy(path, newPath, false);
         return newPath;
      }
   }
}
