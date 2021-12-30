#include "fst.h"
#include "irse.h"
#include <cctype>
#include <cstring>

static inline long pathCompare(const char* path, const char* entryName)
{
    long i = 0;
    while (true) {
        if (entryName[i] == '\0') {
            if (path[i] == '\0' || path[i] == '/')
                return i;
            return 0;
        }
        if (std::toupper(path[i]) != std::toupper(entryName[i]))
            return 0;
        i++;
    }
}

FSTBuilder::Entry* FSTBuilder::DirEntry::find(const char* path)
{
    for (auto it : m_children) {
        long res = pathCompare(path, it->m_name.c_str());
        if (res == 0)
            continue;
        if (path[res] == 0)
            return it;

        /* Assuming path[res] is '/' */
        if (path[res + 1] == 0)
            return nullptr;
        if (!it->isDir())
            return nullptr;

        return it->dir()->find(path + res + 1);
    }
    return nullptr;
}

void FSTBuilder::write(void* fst)
{
    memcpy(fst, reinterpret_cast<void*>(m_entries.data()),
           m_entries.size() * sizeof(FSTEntry));

    char* names =
        reinterpret_cast<char*>(fst) + m_entries.size() * sizeof(FSTEntry);
    memset(names, 0, m_namesLen);

    for (auto str : m_names) {
        std::strcpy(names, str->c_str());
        names += str->size() + 1;
    }
}

void FSTBuilder::addDirEntry(const FSTBuilder::DirEntry* dir)
{
    const u32 curEntry = m_entries.size() - 1;

    for (auto it : dir->m_children) {
        if (it->isDir()) {
            /* Directory entry */
            FSTEntry entry = {.isDir = true,
                              .nameOffset = m_namesLen,
                              .dir = {.parentEntry = curEntry, .lastEntry = 0}};
            m_names.push_back(&it->m_name);
            m_namesLen += it->m_name.size() + 1;
            m_entries.push_back(entry);
            addDirEntry(it->dir());
        } else {
            /* File entry */
            FSTEntry entry = {.isDir = false,
                              .nameOffset = m_namesLen,
                              .file = {.wordOffset = it->file()->m_wordOffset,
                                       .byteLength = it->file()->m_byteLength}};
            m_names.push_back(&it->m_name);
            m_namesLen += it->m_name.size() + 1;
            m_entries.push_back(entry);
        }
    }

    m_entries[curEntry].dir.lastEntry = m_entries.size();
}

u32 FSTBuilder::build(const DirEntry* root)
{
    /* Root entry */
    FSTEntry entry = {.isDir = true,
                      .nameOffset = 0,
                      .dir = {.parentEntry = 0, .lastEntry = 0}};
    m_entries.push_back(entry);
    m_namesLen = 0;

    addDirEntry(root);
    return m_entries.size() * sizeof(FSTEntry) + m_namesLen;
}

const char* FSTReader::entryName(u32 entry)
{
    const u32 nameOffset = m_fst[entry].nameOffset;
    const char* name = m_names + nameOffset;

    if (!check_bounds(m_fst, m_fstLen, name, 1)) {
        irse::Log(LogS::FST, LogL::ERROR, "Invalid name offset (entry %u)",
                  entry);
        return nullptr;
    }

    const u32 maxLen =
        reinterpret_cast<u32>(m_fst) + m_fstLen - reinterpret_cast<u32>(name);
    if (strnlen(name, maxLen) == maxLen) {
        irse::Log(LogS::Loader, LogL::ERROR,
                  "Name extends past the end of the file (entry %u)", entry);
        return nullptr;
    }
    return name;
}

bool FSTReader::processDirEntry(FSTBuilder::DirEntry* dir, u32 entry)
{
    assert(m_fst[entry].isDir);
    assert(m_curEntry < m_numEntries);

    if (m_curEntry + 1 >= m_fst[entry].dir.lastEntry)
        return true;
    m_curEntry++;

    const char* name = entryName(m_curEntry);
    if (name == nullptr)
        return false;

    if (m_fst[m_curEntry].isDir) {
        /* New directory */
        if (m_dirDepth + 1 >= maxDirDepth) {
            irse::Log(LogS::FST, LogL::ERROR,
                      "Directory depth exceeds max (%u)", maxDirDepth);
            return false;
        }

        if (m_fst[m_curEntry].dir.lastEntry > m_fst[entry].dir.lastEntry) {
            irse::Log(LogS::FST, LogL::ERROR,
                      "Subdirectory contains too many entries (entry %u)",
                      m_curEntry);
            return false;
        }

        FSTBuilder::DirEntry* newDir = new FSTBuilder::DirEntry(name);
        newDir->m_parent = dir;
        m_dirDepth++;
        if (!processDirEntry(newDir, m_curEntry))
            return false;
        m_dirDepth--;
        *dir += newDir;
    } else {
        /* New file */
        FSTBuilder::FileEntry* newFile =
            new FSTBuilder::FileEntry(name, m_fst[m_curEntry].file.wordOffset,
                                      m_fst[m_curEntry].file.byteLength);
        newFile->m_parent = dir;
        *dir += newFile;
    }

    return processDirEntry(dir, entry);
}

FSTBuilder::DirEntry* FSTReader::process(const FSTEntry* fst, u32 fstLength)
{
    m_fst = fst;
    m_fstLen = fstLength;

    if (!m_fst->isDir) {
        irse::Log(LogS::FST, LogL::ERROR, "Root entry is not a directory");
        return nullptr;
    }
    if (!check_bounds(m_fst, m_fstLen, m_fst, sizeof(FSTEntry))) {
        irse::Log(LogS::FST, LogL::ERROR, "Not enough space for root entry");
        return nullptr;
    }

    m_numEntries = m_fst->dir.lastEntry;

    if (m_numEntries >= maxEntries) {
        irse::Log(LogS::FST, LogL::ERROR, "Entry count exceeds max (%u > %u)",
                  m_numEntries, maxEntries);
    }
    if (!check_bounds(m_fst, m_fstLen, m_fst,
                      sizeof(FSTEntry) * m_numEntries)) {
        irse::Log(LogS::FST, LogL::ERROR,
                  "Entry count extends past the end of the file");
        return nullptr;
    }

    m_dirDepth = 0;
    m_names = reinterpret_cast<const char*>(m_fst + m_numEntries);
    m_curEntry = 0;
    m_root = new FSTBuilder::DirEntry("data");
    bool ret = processDirEntry(m_root, 0);
    if (!ret) {
        delete m_root;
        return nullptr;
    }
    return m_root;
}
