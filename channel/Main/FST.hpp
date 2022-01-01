#pragma once
#include <System/Types.hpp>
#include <System/Util.hpp>
#include <cassert>
#include <string>
#include <vector>

struct FSTEntry {
    bool isDir : 8;
    u32 nameOffset : 24;
    union {
        struct {
            u32 wordOffset;
            u32 byteLength;
        } file;
        struct {
            u32 parentEntry;
            u32 lastEntry;
        } dir;
    };
} ATTRIBUTE_PACKED;

static_assert(sizeof(FSTEntry) == 0xC, "sizeof FSTEntry != 0xC");

class FSTBuilder
{
public:
    struct Entry;
    struct FileEntry;
    struct DirEntry;

    struct Entry {
        explicit Entry(const char* name) : m_name(name), m_parent(nullptr)
        {
        }
        virtual ~Entry()
        {
        }
        virtual bool isDir() const = 0;
        DirEntry* dir()
        {
            assert(isDir());
            return reinterpret_cast<DirEntry*>(this);
        }
        FileEntry* file()
        {
            assert(!isDir());
            return reinterpret_cast<FileEntry*>(this);
        }

        const std::string m_name;
        DirEntry* m_parent;
    };

    struct FileEntry : public Entry {
        using Entry::Entry;
        FileEntry(const char* name, u32 wordOffset, u32 byteLength)
            : Entry::Entry(name), m_wordOffset(wordOffset),
              m_byteLength(byteLength)
        {
        }
        bool isDir() const
        {
            return false;
        }

        u32 m_wordOffset;
        u32 m_byteLength;
    };

    struct DirEntry : public Entry {
        using Entry::Entry;
        DirEntry(const DirEntry&) = delete;
        ~DirEntry()
        {
            for (auto it : m_children)
                delete it;
        }
        bool isDir() const
        {
            return true;
        }

        DirEntry& operator+=(DirEntry&& entry)
        {
            for (auto it : entry.m_children)
                this->m_children.push_back(it);
            entry.m_children.clear();
            return *this;
        }

        DirEntry& operator+=(Entry* entry)
        {
            m_children.push_back(entry);
            return *this;
        }

        void remove(Entry* entry)
        {
            auto it = std::find(m_children.begin(), m_children.end(), entry);
            if (it == m_children.end())
                return;
            m_children.erase(it);
            entry->m_parent = nullptr;
        }

        Entry* find(const char* path);

        std::vector<Entry*> m_children;
    };

    void write(void* fst);
    /* Returns total FST size */
    u32 build(const DirEntry* root);

private:
    void addDirEntry(const DirEntry* dir);

    std::vector<FSTEntry> m_entries;
    std::vector<const std::string*> m_names;
    u32 m_namesLen;
};

class FSTReader
{
public:
    static constexpr u32 maxEntries = 10000;
    static constexpr u32 maxDirDepth = 64;

    FSTBuilder::DirEntry* process(const FSTEntry* fst, u32 fstLength);

private:
    bool processDirEntry(FSTBuilder::DirEntry* dir, u32 entry);
    const char* entryName(u32 entry);

    FSTBuilder::DirEntry* m_root;
    union {
        const void* m_fstData;
        const FSTEntry* m_fst;
    };
    u32 m_fstLen;
    u32 m_numEntries;
    const char* m_names;

    u32 m_curEntry;
    u32 m_dirDepth;
};