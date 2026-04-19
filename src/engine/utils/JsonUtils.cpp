#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <rapidjson/filereadstream.h>
#include "engine/utils/JsonUtils.h"

void JsonUtils::ReadJsonFile(const std::string& path, rapidjson::Document& out_document)
{
    FILE* file_pointer = nullptr;

#ifdef _WIN32
    fopen_s(&file_pointer, path.c_str(), "rb");
#else
    file_pointer = fopen(path.c_str(), "rb");
#endif

    if (!file_pointer) {
        std::cout << "error parsing json at [" << path << "]";
        exit(0);
    }

    char buffer[65536];
    rapidjson::FileReadStream stream(file_pointer, buffer, sizeof(buffer));
    out_document.ParseStream(stream);
    std::fclose(file_pointer);

    if (out_document.HasParseError()) {
        std::cout << "error parsing json at [" << path << "]";
        exit(0);
    }
}