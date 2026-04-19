#include <iostream>
#include <filesystem>
#include "rapidjson/document.h"
#include "engine/utils/JsonUtils.h"
#include "engine/TemplateDB.h"
#include "engine/ComponentDB.h"

std::unordered_map<std::string, rapidjson::Document> TemplateDB::template_docs;

void TemplateDB::SetComponentDB(ComponentDB* db)
{
    component_db = db;
}

void TemplateDB::LoadTemplates(const std::string& template_dir) 
{
    if (std::filesystem::exists(template_dir)) {
        template_docs.clear();

        for (auto& entry : std::filesystem::directory_iterator(template_dir)) {
            if (entry.path().extension() != ".template") continue;

            std::string name = entry.path().stem().string();

            rapidjson::Document template_doc;
            JsonUtils::ReadJsonFile(entry.path().string(), template_doc);

            template_docs.emplace(std::move(name), std::move(template_doc));
        }
    }
}

std::shared_ptr<Actor> TemplateDB::Instantiate(const std::string& name) const
{
    auto it = template_docs.find(name);
    if (it == template_docs.end()) {
        std::cout << "error: template " << name << " is missing";
        exit(0);
    }

    auto instance = std::make_shared<Actor>();
    if (component_db != nullptr)
    {
        component_db->ApplyActorJson(*instance, it->second);
    }
    return instance;
}