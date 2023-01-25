// Riivolution.hpp
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once

#include "Handler.hpp"
#include <memory>
#include <string>
#include <vector>

class Riivolution : public DeviceHandler
{
public:
    Riivolution();

    void OnDeviceInsertion(u8 id) override;
    void OnDeviceRemoval(u8 id) override;

private:
    struct XmlFile {
        std::string m_path;
        std::unique_ptr<char[]> m_fileData;

        struct Choice {
            std::string m_name;
            std::vector<std::string> m_patches;
        };

        struct Option {
            std::string m_name;
            std::string m_id;
            std::string m_default;
            std::vector<Choice> m_choices;
        };

        struct Section {
            std::string m_name;
            std::vector<Option> m_options;
        };

        struct {
            bool m_valid;
            std::string m_root;

            struct {
                std::string m_game;
                std::string m_developer;
                std::string m_disc;
                std::string m_version;
                std::vector<std::string> m_regions;
            } m_id;

            std::vector<Section> m_sections;
        } m_wiidisc;
    };

    std::vector<XmlFile> m_xmls;

    void LoadXml(std::string& path, u64 size);
    void ParseXml(XmlFile& xml);
};
