﻿#include "stdafx.h"

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
[assembly:AssemblyCopyright("Sylvain Rougeaux, © 2019-2023")];
[assembly:AssemblyTrademark("")];
[assembly:AssemblyCulture("")];
[assembly:ComVisible(false)];
[assembly:CLSCompliant(true)];

[assembly:AssemblyVersion("1.13.3.14823")]; // commit taglib 39e712796f2dd7336188ae690bc3837049f29aac
// Major.Minor.x.x is the version number of taglib
// x.x.Build.x is the version number of NTagLib
// x.x.x.Revision is the four first hex digits of taglib git commit, converted in decimal
