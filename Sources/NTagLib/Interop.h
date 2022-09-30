#pragma once

#include <taglib.h>
#include <tstring.h>
#include <tpropertymap.h>

#include <msclr\marshal.h>
#include <msclr\marshal_cppstd.h>

using namespace System;
using namespace System::Collections::Generic;

static array<byte>^ PictureByteVectorToManagedArray(const TagLib::ByteVector& data)
{
    int offset = 0;
    for (auto it = data.begin(); it != data.end(); it++)
    {
        if (*it == 0)
            offset++;
        else
            break;
    }

    if (data.size() == offset)
        return gcnew array<byte>(0);

    array<byte>^ buffer = gcnew array<byte>(data.size() - offset);
    for (int i = 0; i < buffer->Length; i++)
        buffer[i] = data[i];

    return buffer;
}

static List<String^>^ PropertyMapToManagedList(const TagLib::PropertyMap& map)
{
    auto tags = gcnew List<String^>(map.size());

    for (auto it = map.begin(); it != map.end(); it++)
        tags->Add(gcnew String(it->first.toCWString()));

    return tags;
}

static TagLib::ByteVector ManagedArrayToByteVector(array<byte>^ data)
{
    if (data->Length == 0)
    {
        TagLib::ByteVector buffer;
        return buffer;
    }

    pin_ptr<byte> p = &data[0];
    unsigned char* pby = p;
    const char* pch = reinterpret_cast<char*>(pby);
    TagLib::ByteVector buffer(pch, data->Length);
    return buffer;
}

static TagLib::PropertyMap ManagedDictionaryToPropertyMap(Dictionary<String^, List<String^>^>^ dic)
{
    TagLib::PropertyMap map;

    for each (auto kvp in dic)
    {
        TagLib::String key = msclr::interop::marshal_as<std::wstring>(kvp.Key);
        TagLib::StringList values;

        for each (auto value in kvp.Value)
            values.append(msclr::interop::marshal_as<std::wstring>(value));

        map.insert(key, values);
    }

    return map;
}

static Dictionary<String^, List<String^>^>^ PropertyMapToManagedDictionary(const TagLib::PropertyMap& map)
{
    auto tags = gcnew Dictionary<String^, List<String^>^>(map.size());

    for (auto it = map.begin(); it != map.end(); it++)
    {
        TagLib::String tag = it->first;
        TagLib::StringList values = it->second;

        auto ntag = gcnew String(tag.toCWString());
        tags->Add(ntag, gcnew List<String^>(values.size()));

        for (auto it2 = values.begin(); it2 != values.end(); it2++)
            tags[ntag]->Add(gcnew String(it2->toCWString()));
    }

    return tags;
}

static void throw_out_of_memory_exception()
{
    throw gcnew OutOfMemoryException();
}