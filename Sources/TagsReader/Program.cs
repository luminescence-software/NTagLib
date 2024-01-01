using System;
using System.IO;
using System.Text;
using NTagLib;

namespace TagsReader;

public class Program
{
   public static void Main(string[] args)
   {
      Console.OutputEncoding = Encoding.UTF8;
      //TaglibSettings.ID3Latin1Encoding = Encoding.GetEncoding(1252);

      Version taglibVersion = TaglibTagger.TagLibVersion;
      Console.WriteLine(taglibVersion.ToString());

      foreach (string path in args)
      {
         if (File.Exists(path))
         {
            ReadAudioFile(path);
            Console.WriteLine(new string('_', 40));
         }
      }
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
            Console.WriteLine("PICTURE:");
            Console.WriteLine($"   PictureType={cover.PictureType}");
            Console.WriteLine($"   Description={cover.Description}");
            Console.WriteLine($"   MimeType={cover.MimeType}");
            Console.WriteLine($"   Size={cover.Data.Length} bytes");

            Console.WriteLine();
         }
      }
   }
}
