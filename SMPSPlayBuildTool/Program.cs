﻿using IniFile;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using IniFileClass = IniFile.IniFile;

namespace SMPSPlayBuildTool
{
	static class Program
	{
		static void Main(string[] args)
		{
			bool skc = File.Exists(@"Data\songs_SKC.ini");
			Dictionary<string, Dictionary<string, string>> ini = IniFileClass.Load(@"Data\songs.ini");
			if (skc)
				ini = IniFileClass.Combine(ini, IniFileClass.Load(@"Data\songs_SKC.ini"));
			SongList songdata = IniSerializer.Deserialize<SongList>(ini);
			List<KeyValuePair<string, SongInfo>> songlist = new List<KeyValuePair<string, SongInfo>>(songdata.songs);
			using (StreamWriter sw = File.CreateText("resource.gen.h"))
			{
				sw.WriteLine("// This file was automatically generated by SMPSPlayBuildTool.");
				sw.WriteLine();
				for (int i = 0; i < songlist.Count; i++)
					sw.WriteLine("#define IDR_MUSIC_{0} {1}", (i + 1).ToString(NumberFormatInfo.InvariantInfo).PadRight(3), i + songdata.residstart);
			}
			using (StreamWriter sw = File.CreateText("SMPSPlay.gen.rc"))
			{
				sw.WriteLine("// This file was automatically generated by SMPSPlayBuildTool.");
				sw.WriteLine();
				sw.WriteLine("#include <windows.h>");
				sw.WriteLine("#include \"resource.gen.h\"");
				sw.WriteLine();
				for (int i = 0; i < songlist.Count; i++)
				{
					sw.WriteLine("LANGUAGE LANG_NEUTRAL, SUBLANG_NEUTRAL");
					sw.WriteLine("IDR_MUSIC_{0}      MUSIC          \"{1}\"", (i + 1).ToString(NumberFormatInfo.InvariantInfo).PadRight(3), songlist[i].Value.File.Replace(@"\", @"\\"));
					sw.WriteLine();
					sw.WriteLine();
				}
			}
			using (StreamWriter sw = File.CreateText("musicid.gen.h"))
			{
				sw.WriteLine("// This file was automatically generated by SMPSPlayBuildTool.");
				sw.WriteLine();
				sw.WriteLine("enum MusicID {");
				for (int i = 0; i < songlist.Count; i++)
					sw.WriteLine("\tMusicID_{0},", songlist[i].Key);
				sw.WriteLine("\tSongCount");
				sw.WriteLine("};");
			}
			using (StreamWriter sw = File.CreateText("songinfo.gen.cpp"))
			{
				sw.WriteLine("// This file was automatically generated by SMPSPlayBuildTool.");
				sw.WriteLine();
				sw.WriteLine("#include \"songinfo.h\"");
				sw.WriteLine("#include \"musicid.gen.h\"");
				sw.WriteLine();
				sw.WriteLine("const musicentry MusicFiles[] = {");
				for (int i = 0; i < songlist.Count; i++)
					sw.WriteLine("\t{{ 0x{0}, TrackMode_{1}, \"{2}\" }},", songlist[i].Value.Offset, songlist[i].Value.Type, songlist[i].Key);
				sw.WriteLine("};");
			}
		}
	}

	class SongList
	{
		public int residstart { get; set; }
		[IniCollection(IniCollectionMode.IndexOnly)]
		public Dictionary<string, SongInfo> songs { get; set; }
	}

	class SongInfo
	{
		public string Type { get; set; }
		public string Offset { get; set; }
		public string File { get; set; }
	}
}