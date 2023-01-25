// Riivolution.cpp
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#include "Riivolution.hpp"
#include <Debug/Log.hpp>
#include <Main/IOSBoot.hpp>
#include <System/ISFS.hpp>
#include <System/OS.hpp>
#include <array>
#include <cassert>
#include <cstring>
#include <malloc.h>
#include <mxml.h>
#include <string>

Riivolution::Riivolution()
{
    HandlerMgr::AddHandler(this);
}

void Riivolution::OnDeviceInsertion(u8 id)
{
    PRINT(Riivo, INFO, "Searching patches on device %u", id);

    IOSBoot::WaitForIOS();

    std::array<std::string, 2> pathList = {
        std::string(1, '0' + id) + ":/riivolution",
        std::string(1, '0' + id) + ":/apps/riivolution",
    };

    for (auto path : pathList) {
        IOS::ResourceCtrl<ISFSIoctl> dir("/dev/saoirse/file");
        assert(dir.fd() >= 0);

        IOS::IVector<1> openVec alignas(32) = {
            .in = {{.data = path.c_str(), .len = path.size() + 1}},
        };

        if (dir.ioctlv(ISFSIoctl::Direct_DirOpen, openVec) != ISFSError::OK) {
            PRINT(Riivo, INFO, "Directory '%s' doesn't exist", path.c_str());
            continue;
        }

        while (true) {
            s32 ret = ISFSError::OK;
            ISFSDirect_Stat stat alignas(32) = {};

            IOS::OVector<1> nextVec alignas(32) = {
                .out = {{.data = &stat, .len = sizeof(stat)}},
            };

            if (ret = dir.ioctlv(ISFSIoctl::Direct_DirNext, nextVec),
                ret != ISFSError::OK) {
                PRINT(Riivo, ERROR, "DirNext failed: %d", ret);
                break;
            }

            if (stat.name[0] == '\0') {
                PRINT(Riivo, INFO, "Reached end of directory");
                break;
            }

            if ((stat.attribute & ISFSDirect_Stat::SYS) ||
                (stat.attribute & ISFSDirect_Stat::DIR)) {
                continue;
            }

            if (stat.size > 0x01000000) {
                continue;
            }

            std::string xmlPath = path + "/" + stat.name;
            if (!xmlPath.ends_with(".xml")) {
                continue;
            }

            LoadXml(xmlPath, stat.size);
        }
    }
}

void Riivolution::OnDeviceRemoval(u8 id)
{
    PRINT(Riivo, INFO, "Clearing out patches from device %u", id);

    char dev = '\0' + id;

    for (auto xml = m_xmls.begin(); xml != m_xmls.end(); xml++) {
        if (xml->m_path[0] != dev)
            continue;

        m_xmls.erase(xml);
    }
}

void Riivolution::LoadXml(std::string& path, u64 size)
{
    XmlFile xml;
    xml.m_path = path;

    xml.m_fileData =
        std::unique_ptr<char[]>((char*)memalign(32, round_up(size, 32)));
    if (xml.m_fileData.get() == nullptr) {
        PRINT(Riivo, ERROR, "%s: Not enough memory for size: 0x%llX",
              path.c_str(), size);
        return;
    }

    {
        IOS::ResourceCtrl<ISFSIoctl> file("/dev/saoirse/file");
        assert(file.fd() >= 0);

        u32 openMode = IOS::Mode::Read;
        IOS::IVector<2> openVec alignas(32) = {
            .in = {{.data = path.c_str(), .len = path.size() + 1},
                   {.data = &openMode, .len = sizeof(openMode)}},
        };

        if (file.ioctlv(ISFSIoctl::Direct_Open, openVec) != ISFSError::OK) {
            PRINT(Riivo, INFO, "%s: Failed Direct_Open", path.c_str());
            return;
        }

        auto ret = file.read(xml.m_fileData.get(), size);
        if (ret != s32(size)) {
            PRINT(Riivo, ERROR, "%s: Failed to read: %d", path.c_str(), ret);
            return;
        }
    }

    xml.m_fileData.get()[size] = '\0';

    PRINT(Riivo, INFO, "Successfully read xml: %s", path.c_str());

    ParseXml(xml);
}

void Riivolution::ParseXml(XmlFile& xml)
{
    auto xmlTree =
        mxmlLoadString(nullptr, xml.m_fileData.get(), MXML_TEXT_CALLBACK);
    if (xmlTree == nullptr) {
        PRINT(Riivo, ERROR, "%s: Failed to create XML tree",
              xml.m_path.c_str());
        return;
    }

    auto wiidisc = xmlTree;
    if (strcmp(mxmlGetElement(wiidisc), "wiidisc") != 0) {
        wiidisc = mxmlFindElement(xmlTree, xmlTree, "wiidisc", nullptr, nullptr,
                                  MXML_DESCEND_FIRST);
        if (wiidisc == nullptr) {
            PRINT(Riivo, WARN, "%s: Failed to find wiidisc element",
                  xml.m_path.c_str());
            return;
        }
    }

    auto makeString = [](const char* str) -> std::string {
        if (str == nullptr)
            return "";

        return std::string(str);
    };

    auto version = makeString(mxmlElementGetAttr(wiidisc, "version"));
    if (version != "1") {
        PRINT(Riivo, ERROR, "%s: Unrecognized version: %s", version.c_str());
        return;
    }

    xml.m_wiidisc.m_root = makeString(mxmlElementGetAttr(wiidisc, "root"));

    auto xmlId = mxmlFindElement(wiidisc, wiidisc, "id", nullptr, nullptr,
                                 MXML_DESCEND_FIRST);
    if (xmlId == nullptr) {
        PRINT(Riivo, WARN, "%s: Failed to find id element", xml.m_path.c_str());
        return;
    }

    xml.m_wiidisc.m_id.m_game = makeString(mxmlElementGetAttr(xmlId, "game"));
    xml.m_wiidisc.m_id.m_developer =
        makeString(mxmlElementGetAttr(xmlId, "developer"));
    xml.m_wiidisc.m_id.m_disc = makeString(mxmlElementGetAttr(xmlId, "disc"));
    xml.m_wiidisc.m_id.m_version =
        makeString(mxmlElementGetAttr(xmlId, "version"));

    auto xmlGetArray = [](mxml_node_t* root,
                          const char* element) -> std::vector<mxml_node_t*> {
        std::vector<mxml_node_t*> vec;

        for (auto node = mxmlFindElement(root, root, element, nullptr, nullptr,
                                         MXML_DESCEND_FIRST);
             node != nullptr;
             node = mxmlFindElement(node, root, element, nullptr, nullptr,
                                    MXML_NO_DESCEND)) {
            vec.push_back(node);
        }

        return vec;
    };

    for (auto xmlRegion : xmlGetArray(xmlId, "region")) {
        auto region = makeString(mxmlElementGetAttr(xmlRegion, "type"));
        if (region != "") {
            xml.m_wiidisc.m_id.m_regions.push_back(region);
        }
    }

    auto xmlOptions = mxmlFindElement(wiidisc, wiidisc, "options", nullptr,
                                      nullptr, MXML_DESCEND_FIRST);
    if (xmlOptions == nullptr) {
        PRINT(Riivo, WARN, "%s: Failed to find options element",
              xml.m_path.c_str());
        return;
    }

    for (auto xmlSection : xmlGetArray(xmlOptions, "section")) {
        XmlFile::Section section = {};
        section.m_name = makeString(mxmlElementGetAttr(xmlSection, "name"));

        for (auto xmlOption : xmlGetArray(xmlSection, "option")) {
            XmlFile::Option option = {};
            option.m_name = makeString(mxmlElementGetAttr(xmlOption, "name"));
            option.m_id = makeString(mxmlElementGetAttr(xmlOption, "id"));
            option.m_default =
                makeString(mxmlElementGetAttr(xmlOption, "default"));

            for (auto xmlChoice : xmlGetArray(xmlOption, "choice")) {
                XmlFile::Choice choice = {};
                choice.m_name =
                    makeString(mxmlElementGetAttr(xmlChoice, "name"));

                for (auto xmlPatch : xmlGetArray(xmlChoice, "patch")) {
                    choice.m_patches.push_back(
                        makeString(mxmlElementGetAttr(xmlPatch, "id")));
                }

                option.m_choices.push_back(choice);
            }

            section.m_options.push_back(option);
        }

        xml.m_wiidisc.m_sections.push_back(section);
    }

    m_xmls.push_back(std::move(xml));
}
