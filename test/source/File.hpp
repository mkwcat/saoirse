#pragma once

/* ~~Maybe we could even define the functions as wrappers for IPC~~ */
#define DIR FF_DIR
#include "../../title_loader/lib/ff.h"
#undef DIR

#include <gctypes.h>

/* 
 * The struct must be 4 byte aligned due to a hardware bug and a memcpy
 * implementation that doesn't work around it
 */
static_assert((sizeof(FIL) & 3) == 0, "sizeof(FIL) must be 4 byte aligned");

namespace irse
{

class file
{
public:
    static void init();

    file(FIL& f) {
        /* [FIXME] Maybe the old file should be invalidated? Unsure, it should
         * be fine with multiple instances */
        this->m_f = f;
    }
    file(const TCHAR* path, BYTE mode) {
        open(path, mode);
    }

    ~file() {
        close();
    }
    
    FRESULT read(void* data, u32 len, u32& read);
    FRESULT write(const void* data, u32 len, u32& wrote);
    FRESULT lseek(u32 offset);
    FRESULT truncate();
    FRESULT sync();

    /* Macro functions */

    u32 tell() {
        return static_cast<u32>(f_tell(&this->m_f));
    }
    /* Returns true if the end of the file has been reached */
    bool eof() {
        return static_cast<bool>(f_eof(&this->m_f));
    }
    u32 size() {
        return static_cast<u32>(f_size(&this->m_f));
    }
    /* Returns true if an error has occurred */
    bool error() {
        return static_cast<bool>(f_error(&this->m_f));
    }
    FRESULT result() {
        return this->m_result;
    }

protected:
    FRESULT open(const TCHAR* path, BYTE mode);
    FRESULT close();
    FIL m_f;
    FRESULT m_result;
};

} // namespace irse