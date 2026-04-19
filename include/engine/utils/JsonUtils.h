#pragma once

#include <string>
#include <rapidjson/document.h>

class JsonUtils {
public:
    static void ReadJsonFile(const std::string& path, rapidjson::Document& out_document);
};