#include "storm/Error.hpp"
#include "storm/File.hpp"
#include "storm/List.hpp"
#include "storm/String.hpp"
#include "storm/thread/CCritSect.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(WHOA_SYSTEM_WIN)
#include <Windows.h>
#else
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE (reinterpret_cast<void*>(-1))

struct FILETIME {
    uint32_t dwLowDateTime, dwHighDateTime;
};
#endif

#define MPQ_SIGNATURE 0x1A51504D    // MPQ\x1A

#define HASH_INDEX 0
#define HASH_CHECK0 1
#define HASH_CHECK1 2
#define HASH_ENCRYPTKEY 3
#define HASH_ENCRYPTDATA 4

struct ARCHIVEHEADER {
    uint32_t signature;     // 0x00
    uint32_t headersize;    // 0x04
    uint32_t archivesize;   // 0x08
    uint16_t version;       // 0x0C
    uint16_t sectorsizeid;  // 0x0E
    uint32_t hashoffset;    // 0x10
    uint32_t blockoffset;   // 0x14
    uint32_t hashcount;     // 0x18
    uint32_t blockcount;    // 0x1C
};

struct MD5 {
    uint32_t val[4];
};

struct BLOCKENTRY {
    uint32_t offset;
    uint32_t sizealloc;
    uint32_t sizefile;
    uint32_t flags;
};

struct HASHENTRY {
    uint32_t hashcheck[2];
    uint16_t languageId;
    uint8_t platformId;
    uint8_t reserved;
    uint32_t block;
};

class ARCHIVEREC;
void RemoveArchiveRef(ARCHIVEREC* rec);

class ARCHIVEREC : public TSLinkedNode<ARCHIVEREC> {
public:
    ARCHIVEREC() {}

    ~ARCHIVEREC() {
        if (this->handle != INVALID_HANDLE_VALUE) {
#if defined(WHOA_SYSTEM_WIN)
            CloseHandle(this->handle);
#else
            std::fclose(static_cast<FILE*>(this->handle));  // Stopgap
#endif
        }

        if (this->sectorbuffer) STORM_FREE(this->sectorbuffer);
        if (this->fileheader) STORM_FREE(this->fileheader);
        if (this->blocktable) STORM_FREE(this->blocktable);
        if (this->hashtable) STORM_FREE(this->hashtable);
    }

public:
    char archivename[MAX_PATH];     // 0x08
    void* handle;                   // 0x10C
    int32_t cdrom;                  // 0x110
    int32_t priority;               // 0x114
    void* sectorfile;               // 0x118
    uint32_t sectorlocation;        // 0x11C
    uint32_t sectorsize;            // 0x120
    uint8_t* sectorbuffer;          // 0x124
    uint32_t sectorbytesread;       // 0x128
    uint32_t startinglocation;      // 0x12C
    ARCHIVEHEADER* fileheader;      // 0x130
    BLOCKENTRY* blocktable;         // 0x134
    HASHENTRY* hashtable;           // 0x138
    uint32_t lastlocation;          // 0x13C
    int32_t refcount;               // 0x140
};

class FILEREC : public TSLinkedNode<FILEREC> {
public:
    FILEREC() {}

    ~FILEREC() {
        if (this->handle != INVALID_HANDLE_VALUE) {
#if defined(WHOA_SYSTEM_WIN)
            CloseHandle(this->handle);
#else
            std::fclose(static_cast<FILE*>(this->handle));  // Stopgap
#endif
        }
        if (this->archive) {
            this->archive->sectorfile = nullptr;
        }

        if (this->sectoroffsettable) STORM_FREE(this->sectoroffsettable);
        if (this->readaheadbuffer) STORM_FREE(this->readaheadbuffer);

        RemoveArchiveRef(this->archive);
    }

public:
    char name[MAX_PATH];            // 0x08
    void* handle;                   // 0x10C
    ARCHIVEREC* archive;            // 0x110
    BLOCKENTRY* block;              // 0x114
    uint32_t key;                   // 0x118
    uint32_t location;              // 0x11C
    uint32_t lastlocation;          // 0x120
    uint32_t sectors;               // 0x124
    uint32_t* sectoroffsettable;    // 0x128
    int32_t sectoroffsettablevalid; // 0x12C
    int32_t dda;                    // 0x130
    void* readaheadbuffer;          // 0x134
    uint32_t readaheadoffset;       // 0x138
    uint32_t readaheadbytes;        // 0x13C
    int32_t refcount;               // 0x140
};

CCritSect s_archivelock;
STORM_LIST(ARCHIVEREC) s_archivelist;
STORM_LIST(FILEREC) s_filelist;
int32_t s_directaccess;
char s_basepath[MAX_PATH];
char s_userDataPath[MAX_PATH];
uint16_t s_languageId;
uint32_t s_ioerrormode = 2;
SFILEERRORPROC s_ioerrorproc;
uint8_t s_platformId;

typedef uint32_t HASHSOURCE[5][256];

HASHSOURCE *s_hashsource = nullptr;
static void InitializeHashSource(uint32_t seed) {
    if (!s_hashsource) {
        s_hashsource = static_cast<HASHSOURCE*>(STORM_ALLOC(sizeof(HASHSOURCE)));
    }

    uint32_t rand1, rand2;
    for (int32_t i = 0; i < 256; i++) {
        for (int32_t j = 0; j < 5; j++) {
            rand1 = seed = (125 * seed + 3) % 0x2AAAAB;
            rand2 = seed = (125 * seed + 3) % 0x2AAAAB;
            (*s_hashsource)[j][i] = (rand2 & 0xFFFF) | (rand1 << 16);
        }
    }
}

void RemoveArchiveRef(ARCHIVEREC* rec) {
    if (rec) {
        rec->refcount--;
        if (rec->refcount == 0) {
            delete rec;
        }
    }
}

void RemoveFileRef(FILEREC* rec) {
    if (rec) {
        rec->refcount--;
        if (rec->refcount == 0) {
            delete rec;
        }
    }
}

void AddArchiveRef(ARCHIVEREC* rec) {
    if (rec) {
        rec->refcount++;
    }
}

void AddFileRef(FILEREC* rec) {
    if (rec) {
        rec->refcount++;
    }
}

static void Initialize() {
    if (s_hashsource) return;

#if defined(WHOA_FLAVOR_WOW)
    InitializeHashSource(0);    // ???
#else
    InitializeHashSource(0x100001);
#endif
}

static void BuildDefaultBasePath() {
    // NOTE: Pretty sure WoW's Blizzard::Process::GetProcessDirectory can replace this
#if defined(WHOA_SYSTEM_WIN)
    GetModuleFileNameA(GetModuleHandleA(nullptr), s_basepath, sizeof(s_basepath));
#else
    // TODO Mac version of this.
#endif
    char* separator = SStrChrR(s_basepath, '\\');
    if (separator) *separator = '\0';
    SStrPack(s_basepath, "\\", sizeof(s_basepath));
}

uint32_t BuildDefaultOpenFlags() {
    uint32_t result = 0;
    if (s_directaccess & SFILE_DIRECT_ENABLE_RELATIVE) result |= SFILE_OPENFLAG_CHECKDISK;
    if (s_directaccess & SFILE_DIRECT_ENABLE_NOPATH) result |= SFILE_OPENFLAG_CHECKDISK_NOPATH;
    if (!s_directaccess && s_archivelist.IsEmpty()) result |= SFILE_OPENFLAG_CHECKDISK;
    return result;
}

int32_t STORMAPI SFileCloseArchive(HSARCHIVE handle) {
    s_archivelock.Enter();

    ARCHIVEREC* archiveptr = reinterpret_cast<ARCHIVEREC*>(handle);
    s_archivelist.UnlinkNode(archiveptr);
    RemoveArchiveRef(archiveptr);

    s_archivelock.Leave();
    return 1;
}

int32_t STORMAPI SFileCloseFile(HSFILE handle) {
    s_archivelock.Enter();

    FILEREC* fileptr = reinterpret_cast<FILEREC*>(handle);
    s_filelist.UnlinkNode(fileptr);
    RemoveFileRef(fileptr);

    s_archivelock.Leave();
    return 1;
}

int32_t STORMAPI SFileDestroy() {
    //SFileDdaDestroy();    // DirectSound interface, excluded from squall
    //DestroyCdThread();    // Not supporting CD media in squall

    // More CD audio cleanup (not in squall)

    // Clean up audio streams (not in squall)

    while (FILEREC* node = s_filelist.Head()) {
        //SErrReportNamedResourceLeak("HSFILE", node->name);
        delete node;
    }

    while (ARCHIVEREC* node = s_archivelist.Head()) {
        //SErrReportNamedResourceLeak("HSARCHIVE", node->archivename);
        delete node;
    }

    if (s_hashsource) {
        STORM_FREE(s_hashsource);
        s_hashsource = nullptr;
    }

    //if (s_explodebuffer) { ... }      // handled by StormLib
    //if (s_soundreadbuffer) { ... }    // Part of SFileDda/CD audio
    return 1;
}

int32_t STORMAPI SFileEnableDirectAccess(uint32_t access) {
    s_directaccess = access;
    return 1;
}

#if defined(WHOA_SFILE_HAS_CDROM)
int32_t STORMAPI SFileGetArchiveInfo(HSARCHIVE archive, int32_t* priority, int32_t* cdrom) {
#else
int32_t STORMAPI SFileGetArchiveInfo(HSARCHIVE archive, int32_t* priority) {
    int32_t* cdrom = nullptr;
#endif
    if (cdrom) *cdrom = 0;
    if (priority) *priority = 0;

    ARCHIVEREC* archiveptr = reinterpret_cast<ARCHIVEREC*>(archive);

    s_archivelock.Enter();
    if (priority) *priority = archiveptr->priority;
    if (cdrom) *cdrom = archiveptr->cdrom == 2;
    s_archivelock.Leave();
    return 1;
}

int32_t STORMAPI SFileGetArchiveName(HSARCHIVE archive, char* buffer, uint32_t bufferchars) {
    STORM_VALIDATE_BEGIN;
    STORM_VALIDATE(buffer);
    STORM_VALIDATE_END;

    buffer[0] = '\0';

    s_archivelock.Enter();
    ARCHIVEREC* archiveptr = reinterpret_cast<ARCHIVEREC*>(archive);
    SStrCopy(buffer, archiveptr->archivename, bufferchars);
    s_archivelock.Leave();
    return 1;
}

int32_t STORMAPI SFileGetBasePath(char* buffer, uint32_t bufferchars) {
    s_archivelock.Enter();
    if (!s_basepath[0]) {
        BuildDefaultBasePath();
    }
    SStrCopy(buffer, s_basepath, bufferchars);
    s_archivelock.Leave();
    return 1;
}

int32_t STORMAPI SFileGetFileArchive(HSFILE file, HSARCHIVE* archive) {
    STORM_VALIDATE_BEGIN;
    STORM_VALIDATE(archive);
    STORM_VALIDATE_END;

    *archive = nullptr;

    s_archivelock.Enter();
    FILEREC* fileptr = reinterpret_cast<FILEREC*>(file);
    *archive = reinterpret_cast<HSARCHIVE>(fileptr->archive);
    s_archivelock.Leave();
    return 1;
}

int32_t STORMAPI SFileGetFileName(HSFILE file, char* buffer, uint32_t bufferchars) {
    STORM_VALIDATE_BEGIN;
    STORM_VALIDATE(buffer);
    STORM_VALIDATE_END;

    buffer[0] = '\0';

    s_archivelock.Enter();
    FILEREC* fileptr = reinterpret_cast<FILEREC*>(file);
    SStrCopy(buffer, fileptr->name, bufferchars);
    s_archivelock.Leave();
    return 1;
}

uint32_t STORMAPI SFileGetFileSize(HSFILE handle, uint32_t* filesizehigh) {
    if (filesizehigh) *filesizehigh = 0;

    uint32_t result = 0;
    s_archivelock.Enter();
    FILEREC* fileptr = reinterpret_cast<FILEREC*>(handle);

    if (fileptr->handle == INVALID_HANDLE_VALUE) {
        result = fileptr->block->sizefile;
        s_archivelock.Leave();
    }
    else {
        s_archivelock.Leave();
#if defined(WHOA_SYSTEM_WIN)
        result = GetFileSize(fileptr->handle, reinterpret_cast<LPDWORD>(filesizehigh));
#else
        // Stopgap
        FILE* hFile = static_cast<FILE*>(fileptr->handle);
        auto pos = std::ftell(hFile);
        std::fseek(hFile, 0, SEEK_END);
        result = std::ftell(hFile);
        std::fseek(hFile, pos, SEEK_SET);
#endif
    }
    return result;
}

void STORMAPI SFileGetInstallPath(char* buffer, uint32_t bufferchars, int32_t includeseparator) {
    char filename[MAX_PATH];
#if defined(WHOA_SYSTEM_WIN)
    if (!GetModuleFileNameA(nullptr, filename, sizeof(filename))) {
        filename[0] = '\0';
    }
    char* separator = SStrChrR(filename, '\\');
    if (separator) separator[includeseparator != 0] = '\0';
    SStrCopy(buffer, filename, bufferchars);
#else
    // ????
    buffer[0] = '\0';
#endif
}

uint16_t STORMAPI SFileGetLocale() {
    return s_languageId;
}

void STORMAPI SFileGetUserDataPath(char* buffer, uint32_t bufferchars, int32_t includeseparator) {
    size_t len = SStrPrintf(buffer, bufferchars, "%s", s_userDataPath);
    if (includeseparator) {
        if (buffer[len - 1] != '\\') SStrPack(buffer, "\\", bufferchars - static_cast<uint32_t>(len));
    }
    else {
        if (buffer[len - 1] == '\\') buffer[len - 1] = '\0';
    }
}

int32_t STORMAPI SFileLoadFile(const char* filename, void** buffer, uint32_t* bytes, uint32_t extrabytes, LPOVERLAPPED overlapped) {
    return SFileLoadFileEx(nullptr, filename, buffer, bytes, extrabytes, BuildDefaultOpenFlags(), overlapped);
}

int32_t STORMAPI SFileLoadFileEx(HSARCHIVE archive, const char* filename, void** buffer, uint32_t* bytes, uint32_t extrabytes, uint32_t flags, LPOVERLAPPED overlapped) {
    STORM_VALIDATE_BEGIN;
    STORM_VALIDATE(filename);
    STORM_VALIDATE(buffer);
    STORM_VALIDATE_END;

    *buffer = nullptr;
    if (bytes) *bytes = 0;

    HSFILE file = nullptr;
    if (SFileOpenFileEx(archive, filename, flags, &file)) {
        uint32_t filesize = SFileGetFileSize(file);

        uint8_t* data = static_cast<uint8_t*>(STORM_ALLOC(filesize + extrabytes));
        if (SFileReadFile(file, data, filesize, nullptr, overlapped)) {
            if (extrabytes != 0) {
                SMemZero(data + filesize, extrabytes);
            }

            *buffer = data;
            if (bytes) *bytes = filesize;
        }
        else {
            if (data) STORM_FREE(data);
        }
    }
    if (file) SFileCloseFile(file);
    return *buffer != nullptr;
}

void ConvertRelativePathName(const char* inputpath, char* outputpath, int32_t strippath) {
    if (!s_basepath) {
        BuildDefaultBasePath();
    }
    if (strippath) {
        const char* separator = SStrChrR(s_basepath, '\\');
        if (separator) {
            inputpath = separator + 1;
        }
    }
    char absolutepath[MAX_PATH];
    std::sprintf(absolutepath, "%s%s", s_basepath, inputpath);
#if defined(WHOA_SYSTEM_WIN)
    _fullpath(outputpath, absolutepath, MAX_PATH);
#else
    // ?????
#endif
}

int32_t CheckFileExists(const char* filename) {
#if defined(WHOA_SYSTEM_WIN)
    return !(GetFileAttributesA(filename) & FILE_ATTRIBUTE_DIRECTORY);
#else
    // Stopgap
    if (FILE* f = std::fopen(filename, "r")) {
        std::fclose(f);
        return 1;
    }
    return 0;
#endif
}

int32_t CheckForCdRom(const char* path) {
    // Not supporting CDRom in squall
    return 0;
}

void Decrypt(void* data, uint32_t bytes, uint32_t key) {
    uint32_t* data32 = static_cast<uint32_t*>(data);
    uint32_t adjust = 0xEEEEEEEE;
    for (uint32_t i = 0; i < bytes / 4; i++) {
        uint32_t v8 = (*s_hashsource)[HASH_ENCRYPTDATA][key & 0xFF];
        *data32 ^= v8 + key;
        adjust = *data32 + 33 * v8 + 3;
        key = ((~key << 0x15) + 0x11111111) | (key >> 0x0B);
        data32++;
    }
}

uint32_t Hash(const char* filename, int hashtype) {
    uint32_t result = 0x7FED7FED;
    uint32_t adjust = 0xEEEEEEEE;
    while (filename && *filename) {
        char origchar = std::toupper(*filename++);
        result = (*s_hashsource)[hashtype][origchar] ^ (adjust + result);
        adjust += origchar + 32 * adjust + result + 3;
    }
    return result;
}

int32_t ReadFileChecked(uint32_t position, uint32_t* currentposition, void* file, void* buffer, uint32_t bytestoread, uint32_t* bytesread, const char* filename) {
    uint32_t numcalls = 0;
    while (1) {
        int32_t result;
#if defined(WHOA_SYSTEM_WIN)
        if (position != UINT32_MAX && position != *currentposition) {
            SetFilePointer(file, position, nullptr, FILE_BEGIN);
        }
        result = ReadFile(file, buffer, bytestoread, bytesread, nullptr);
#else
        // Stopgap
        if (position != UINT32_MAX && position != *currentposition) {
            std::fseek(static_cast<FILE*>(file), position, SEEK_SET);
        }
        *bytesread = std::fread(buffer, 1, bytestoread, static_cast<FILE*>(file));
        result = !std::ferror(static_cast<FILE*>(file));
#endif

        if (result) break;

        uint32_t errorcode = 0;
#if defined(WHOA_SYSTEM_WIN)
        errorcode = GetLastError();
#else
        // ???
#endif
        switch(s_ioerrormode) {
        case 0:
            return 0;
        case 1:
            if (!s_ioerrorproc) return 0;
            if (!s_ioerrorproc(filename, errorcode, numcalls++)) return 0;
            break;
        case 2:
            SErrDisplayError(errorcode, filename, SERR_LINECODE_FILE, nullptr, 0, 1);
            break;
        }
    }
    return *bytesread == bytestoread;
}

int32_t ReadFileWin32(FILEREC* file, uint32_t offset, void* buffer, uint32_t bytestoread, uint32_t* bytesread) {
    uint32_t currentposition = UINT32_MAX;
    uint32_t localbytesread = 0;
    ReadFileChecked(offset, &currentposition, file->handle, buffer, bytestoread, &localbytesread, file->name);
    if (bytesread) *bytesread = localbytesread;
    return localbytesread == bytestoread;
}

int32_t STORMAPI SFileOpenArchive(const char* archivename, int32_t priority, uint32_t flags, HSARCHIVE* handle) {
    STORM_VALIDATE_BEGIN;
    STORM_VALIDATE(handle);
    *handle = nullptr;
    STORM_VALIDATE(archivename);
    STORM_VALIDATE(*archivename);
    STORM_VALIDATE_END;

    s_archivelock.Enter();
    Initialize();

    char localarchivename[MAX_PATH];
    SStrCopy(localarchivename, archivename, sizeof(localarchivename));
    localarchivename[sizeof(localarchivename) - 1] = '\0';

    if (!CheckFileExists(localarchivename)) {
        if (archivename[0] == '\\' || std::strstr(archivename, ":\\") || std::strstr(archivename, "\\\\")) {
            SStrCopy(localarchivename, archivename, sizeof(localarchivename));
        }
        else {
            ConvertRelativePathName(archivename, localarchivename, 0);
        }
    }

    int32_t cdrom = CheckForCdRom(localarchivename);
    int32_t cdonly = (flags & SFILE_ARCHIVE_READ_FROM_CD_ONLY) != 0;
    if (cdonly && !cdrom) {
        SErrSetLastError(ERROR_INVALID_DRIVE);
        s_archivelock.Leave();
        return 0;
    }

#if defined(WHOA_SYSTEM_WIN)
    HANDLE archivehandle = CreateFileA(localarchivename, GENERIC_READ, FILE_SHARE_READ, nullptr, CREATE_NEW | CREATE_ALWAYS, 0, nullptr);
#else
    // Stopgap
    void* archivehandle = std::fopen(localarchivename, "r");
    if (!archivehandle) archivehandle = INVALID_HANDLE_VALUE;
#endif

    if (archivehandle == INVALID_HANDLE_VALUE) {
        s_archivelock.Leave();
        return 0;
    }

    ARCHIVEREC* archiveptr = s_archivelist.NewNode(0, 0, 0);

    SStrCopy(archiveptr->archivename, localarchivename, sizeof(archiveptr->archivename));
    archiveptr->handle = archivehandle;
    archiveptr->refcount = 1;
    archiveptr->cdrom = cdrom ? 2 : 0;
    archiveptr->priority = priority;
    if (cdrom) {
        archiveptr->cdrom = 2;
    }
    else {
        archiveptr->cdrom = (flags & SFILE_ARCHIVE_ENABLE_OVERLAPPED) != 0 ? 1 : 0;
    }

    archiveptr->fileheader = STORM_NEW(ARCHIVEHEADER);
    uint32_t bytesread;
    for (archiveptr->startinglocation = 0; ; archiveptr->startinglocation += 512) {
#if defined(WHOA_SYSTEM_WIN)
        SetFilePointer(archivehandle, archiveptr->startinglocation, nullptr, FILE_BEGIN);
        ReadFile(archivehandle, archiveptr->fileheader, sizeof(ARCHIVEHEADER), &bytesread, nullptr);
#else
        // Stopgap
        std::fseek(static_cast<FILE*>(archivehandle), archiveptr->startinglocation, SEEK_SET);
        bytesread = std::fread(archiveptr->fileheader, 1, sizeof(ARCHIVEHEADER), static_cast<FILE*>(archivehandle));
#endif
        if (bytesread != sizeof(ARCHIVEHEADER)) {
            s_archivelist.DeleteNode(archiveptr);
            SErrSetLastError(STORM_ERROR_NOT_ARCHIVE);
            s_archivelock.Leave();
            return 0;
        }

        if (archiveptr->fileheader->signature == MPQ_SIGNATURE && archiveptr->fileheader->headersize >= sizeof(ARCHIVEHEADER)) {
            break;
        }
    }

    archiveptr->sectorsize = 512 << archiveptr->fileheader->sectorsizeid;
    archiveptr->sectorbuffer = static_cast<uint8_t*>(STORM_ALLOC(archiveptr->sectorsize));

    uint32_t hashtablesize = sizeof(HASHENTRY) * archiveptr->fileheader->hashcount;
    archiveptr->hashtable = static_cast<HASHENTRY*>(STORM_ALLOC(hashtablesize));

    ReadFileChecked(archiveptr->startinglocation + archiveptr->fileheader->hashoffset, &archiveptr->lastlocation, archivehandle, archiveptr->hashtable, hashtablesize, &bytesread, archiveptr->archivename);
    Decrypt(archiveptr->hashtable, hashtablesize, Hash("(hash table)", HASH_ENCRYPTKEY));

    uint32_t blocktablesize = sizeof(BLOCKENTRY) * archiveptr->fileheader->blockcount;
    archiveptr->blocktable = static_cast<BLOCKENTRY*>(STORM_ALLOC(blocktablesize));

    ReadFileChecked(archiveptr->startinglocation + archiveptr->fileheader->blockoffset, &archiveptr->lastlocation, archivehandle, archiveptr->blocktable, blocktablesize, &bytesread, archiveptr->archivename);
    Decrypt(archiveptr->blocktable, blocktablesize, Hash("(block table)", HASH_ENCRYPTKEY));

    if (archiveptr->startinglocation != 0) {
        for (uint32_t block = 0; block < archiveptr->fileheader->blockcount; block++) {
            if (archiveptr->blocktable[block].offset) {
                archiveptr->blocktable[block].offset += archiveptr->startinglocation;
            }
        }
    }

    ARCHIVEREC* curr;
    for (curr = s_archivelist.Head(); curr; curr = curr->Next()) {
        if (curr->priority <= priority) break;
    }
    s_archivelist.LinkNode(archiveptr, STORM_LIST_LINK_BEFORE, curr);

    if (archiveptr->cdrom) {
        //CreateCdThread(); // Not supporting CD in squall
    }
    else if (cdonly && !cdrom) {
        SFileCloseArchive(reinterpret_cast<HSARCHIVE>(archiveptr));
        SErrSetLastError(ERROR_INVALID_DRIVE);
        s_archivelock.Leave();
        return 0;
    }

    *handle = reinterpret_cast<HSARCHIVE>(archiveptr);
    s_archivelock.Leave();
    return 1;
}

int32_t STORMAPI SFileOpenFile(const char* filename, HSFILE* handle) {
    return SFileOpenFileEx(nullptr, filename, BuildDefaultOpenFlags(), handle);
}

int32_t STORMAPI SFileOpenFileEx(HSARCHIVE archivehandle, const char* filename, uint32_t flags, HSFILE* handle) {
    // TODO
    return 0;
}

int32_t STORMAPI SFileReadFile(HSFILE handle, void* buffer, uint32_t bytestoread, uint32_t* bytesread, LPOVERLAPPED overlapped) {
    // TODO
    return 0;
}

int32_t STORMAPI SFileSetBasePath(const char* path) {
    STORM_VALIDATE_BEGIN;
    STORM_VALIDATE(path);
    STORM_VALIDATE_END;

    if (!path[0]) {
        s_basepath[0] = '\0';
        return 1;
    }

    int32_t hasendingsep = path[SStrLen(path) - 1] == '\\';
    if (SStrLen(path) + (!hasendingsep ? 1 : 0) + 1 > MAX_PATH) {
        SErrSetLastError(ERROR_BAD_PATHNAME);
        return 0;
    }

    s_archivelock.Enter();
    SStrCopy(s_basepath, path, sizeof(s_basepath));
    if (!hasendingsep) {
        SStrPack(s_basepath, "\\", sizeof(s_basepath));
    }
    s_archivelock.Leave();
    return 1;
}

uint32_t STORMAPI SFileSetFilePointer(HSFILE handle, int32_t distancetomove, int32_t* distancetomovehigh, uint32_t movemethod) {
    if (distancetomovehigh && distancetomovehigh[0]) {
        SErrSetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    s_archivelock.Enter();
    FILEREC* fileptr = reinterpret_cast<FILEREC*>(handle);
    if (fileptr->handle != INVALID_HANDLE_VALUE) {
        s_archivelock.Leave();
#if defined(WHOA_SYSTEM_WIN)
        return SetFilePointer(fileptr->handle, distancetomove, nullptr, movemethod);
#else
        // Stopgap
        return std::fseek(static_cast<FILE*>(fileptr->handle), distancetomove, movemethod) == 0;
#endif
    }

    switch(movemethod) {
    case SFILE_BEGIN:
        fileptr->location = distancetomove;
        break;
    case SFILE_CURRENT:
        if (distancetomove >= 0 || fileptr->location >= static_cast<uint32_t>(-distancetomove)) {
            fileptr->location += distancetomove;
        }
        else {
            fileptr->location = 0;
        }
        break;
    case SFILE_END:
        if (distancetomove >= 0 || fileptr->block->sizefile >= static_cast<uint32_t>(-distancetomove)) {
            fileptr->location = fileptr->block->sizefile + distancetomove;
        }
        else {
            fileptr->location = 0;
        }
        break;
    }

    uint32_t result = fileptr->block->sizefile - 1;
    fileptr->readaheadoffset = 0;
    fileptr->readaheadbytes = 0;
    if (fileptr->location < result) {
        result = fileptr->location;
    }
    s_archivelock.Leave();
    return result;
}

int32_t STORMAPI SFileSetIoErrorMode(uint32_t errormode, SFILEERRORPROC errorproc) {
    s_ioerrormode = errormode;
    s_ioerrorproc = errorproc;
    return 1;
}

// TODO find out when this stopped returning 1
void STORMAPI SFileSetLocale(uint16_t lcid) {
    s_languageId = lcid;
}

void STORMAPI SFileSetPlatform(uint8_t platformId) {
    s_platformId = platformId;
}

void STORMAPI SFileSetUserDataPath(const char* datapath) {
    SStrCopy(s_userDataPath, datapath, sizeof(s_userDataPath));
}

int32_t SFileUnloadFile(void* buffer) {
    STORM_VALIDATE_BEGIN;
    STORM_VALIDATE(buffer);
    STORM_VALIDATE_END;

    STORM_FREE(buffer);
    return 1;
}
