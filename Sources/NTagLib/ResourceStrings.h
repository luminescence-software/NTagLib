#pragma once

using namespace System;
using namespace System::Reflection;
using namespace System::Resources;

private ref class ResourceStrings abstract sealed
{
private:
   static initonly ResourceManager^ rm = gcnew ResourceManager("NTagLib.NTagLibStrings", Assembly::GetExecutingAssembly());

public:
   static String^ GetString(String^ keyword) { return rm->GetString(keyword); }
};