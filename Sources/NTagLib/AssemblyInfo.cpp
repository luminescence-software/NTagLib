#include "stdafx.h"

using namespace System;
using namespace System::Reflection;
using namespace System::Runtime::CompilerServices;
using namespace System::Runtime::InteropServices;
using namespace System::Security::Permissions;

[assembly:AssemblyTitle("NTagLib")];
[assembly:AssemblyDescription("A C++/CLI assembly for tagging audio files with TagLib in managed code.")]
[assembly:AssemblyConfiguration("")];
[assembly:AssemblyCompany("Sylvain Rougeaux")];
[assembly:AssemblyProduct("NTagLib")];
[assembly:AssemblyCopyright("Sylvain Rougeaux, © 2019-2022")];
[assembly:AssemblyTrademark("")];
[assembly:AssemblyCulture("")];
[assembly:ComVisible(false)];
[assembly:CLSCompliant(true)];

[assembly:AssemblyVersion("1.13.39508.0")]; // commit taglib 9a54a47de214e9a487a22f32279f12802a29c358
// Major.Minor.x.x is the version number of taglib
// x.x.Build.x is the four first digits of taglib git commit
// x.x.x.Revision is the version number of NTagLib
