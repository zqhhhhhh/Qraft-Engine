#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "rapidjson/document.h"
#include "engine/Actor.h"

class ComponentDB;

class TemplateDB 
{
public:
    TemplateDB() = default;
    void SetComponentDB(ComponentDB* db);
    void LoadTemplates(const std::string& template_dir);
    std::shared_ptr<Actor> Instantiate(const std::string& name) const;

private:
    ComponentDB* component_db = nullptr;
    static std::unordered_map<std::string, rapidjson::Document> template_docs;
};