#include "Test.hpp"
#include "storm/Error.hpp"
#include "storm/File.hpp"

HSARCHIVE OpenNullArchive() {
    return nullptr;
}

HSARCHIVE OpenTestArchive() {
    HSARCHIVE archive = nullptr;
    SFileOpenArchive("wowtest1.mpq", 0, 0, &archive);
    REQUIRE(archive != nullptr);
    return archive;
}

struct OpenFileTestCase {
    std::string info;
    HSARCHIVE (*OpenArchiveFn)();
    uint32_t flags;
};

HSFILE ReadTestFileFromDisk() {
    HSFILE file = nullptr;
    SFileOpenFileEx(nullptr, "test_diskonly.txt", SFILE_OPENFLAG_CHECKDISK, &file);
    return file;
}

HSFILE ReadTestFileFromMpq() {
    HSARCHIVE archive = nullptr;
    SFileOpenArchive("wowtest1.mpq", 0, 0, &archive);

    HSFILE file = nullptr;
    SFileOpenFileEx(archive, "test.txt", 0, &file);
    return file;
}

struct ReadFileTestCase {
    std::string info;
    HSFILE (*OpenFileFn)();
    uint32_t eofcode;
};

// most of these only pass against storm.dll for now
#if defined(WHOA_TEST_STORMDLL)
TEST_CASE("SFileCloseArchive", "[file]") {
    HSARCHIVE archive = nullptr;

    SECTION("closes an archive") {
        SFileOpenArchive("wowtest1.mpq", 0, 0, &archive);
        REQUIRE(archive != nullptr);

        HSFILE file;
        CHECK(SFileOpenFileEx(nullptr, "test.txt", 0, &file));
        CHECK(SFileCloseFile(file));

        CHECK(SFileCloseArchive(archive) == 1);

        CHECK_FALSE(SFileOpenFileEx(nullptr, "test.txt", 0, &file));
    }

    // idk how to test this yet
    SECTION("doesn't delete if there is more than one reference") {
        // TODO
    }
}

TEST_CASE("SFileCloseFile", "[file]") {
    HSFILE file = nullptr;

    SECTION("closes a MPQ file") {
        file = ReadTestFileFromMpq();
        CHECK(SFileCloseFile(file) == 1);
    }

    SECTION("closes a filesystem file") {
        file = ReadTestFileFromDisk();
        CHECK(SFileCloseFile(file) == 1);
    }

    // idk how to test this yet
    SECTION("doesn't delete if there is more than one reference") {
        // TODO
    }
}

TEST_CASE("SFileGetFileSize", "[file]") {
    HSARCHIVE archive;
    HSFILE file;

    SECTION("retrieves MPQ file size") {
        SFileOpenArchive("wowtest1.mpq", 0, 0, &archive);

        REQUIRE(SFileOpenFileEx(archive, "test2.txt", 0, &file));
        CHECK(SFileGetFileSize(file) == 13);

        REQUIRE(SFileOpenFileEx(archive, "empty.txt", 0, &file));
        CHECK(SFileGetFileSize(file) == 0);

        uint32_t filesizehigh = 1234;
        REQUIRE(SFileOpenFileEx(archive, "test.txt", 0, &file));
        CHECK(SFileGetFileSize(file, &filesizehigh) == 6);
        CHECK(filesizehigh == 0);
    }

    SECTION("retrieves filesystem file size") {
        REQUIRE(SFileOpenFileEx(nullptr, "empty_diskonly.txt", SFILE_OPENFLAG_CHECKDISK, &file));
        CHECK(SFileGetFileSize(file) == 0);

        uint32_t filesizehigh = 1234;
        REQUIRE(SFileOpenFileEx(nullptr, "test_diskonly.txt", SFILE_OPENFLAG_CHECKDISK, &file));
        CHECK(SFileGetFileSize(file, &filesizehigh) == 6);
        CHECK(filesizehigh == 0);
    }
}

TEST_CASE("SFileOpenArchive", "[file]") {
    HSARCHIVE mpq = nullptr;
    SErrSetLastError(ERROR_SUCCESS);

    SECTION("opens a MPQ archive") {
        CHECK(SFileOpenArchive("wowtest1.mpq", 0, 0, &mpq) == 1);
        CHECK(mpq != nullptr);
    }

    /* // something about CD ROM drives? not sure how to test this
    SECTION("fails if drive is inaccessible") {
        // TODO
    }
    */

    SECTION("fails if archive file is nonexistent") {
        mpq = reinterpret_cast<HSARCHIVE>(1234);
        CHECK_FALSE(SFileOpenArchive("nice_try.mpq", 0, 0, &mpq));

        CHECK(mpq == nullptr);
        CHECK(SErrGetLastError() == ERROR_SUCCESS);
    }

    SECTION("fails if archive file is too small") {
        mpq = reinterpret_cast<HSARCHIVE>(1234);
        CHECK_FALSE(SFileOpenArchive("bad_toosmall.mpq", 0, 0, &mpq));
        CHECK(mpq == nullptr);

        CHECK(SErrGetLastError() == STORM_ERROR_NOT_ARCHIVE);
    }

    SECTION("fails if using a directory") {
        mpq = reinterpret_cast<HSARCHIVE>(1234);
        CHECK_FALSE(SFileOpenArchive("directorytest", 0, 0, &mpq));
        CHECK(mpq == nullptr);

        CHECK(SErrGetLastError() == ERROR_SUCCESS);
    }

    SECTION("fails if archive header magic doesn't match") {
        mpq = reinterpret_cast<HSARCHIVE>(1234);
        CHECK_FALSE(SFileOpenArchive("bad_nomagic.mpq", 0, 0, &mpq));
        CHECK(mpq == nullptr);

        CHECK(SErrGetLastError() == STORM_ERROR_NOT_ARCHIVE);
    }

    SECTION("fails if archive header size doesn't match") {
        mpq = reinterpret_cast<HSARCHIVE>(1234);
        CHECK_FALSE(SFileOpenArchive("bad_headertoosmall.mpq", 0, 0, &mpq));
        CHECK(mpq == nullptr);

        CHECK(SErrGetLastError() == STORM_ERROR_NOT_ARCHIVE);
    }
}

TEST_CASE("SFileOpenFileEx", "[file]") {
    SECTION("shared testcases") {
        OpenFileTestCase testcase = GENERATE(
            OpenFileTestCase { "file from disk", OpenNullArchive, SFILE_OPENFLAG_CHECKDISK },
            OpenFileTestCase { "file from MPQ", OpenTestArchive, 0 }
        );

        INFO(testcase.info);
        HSARCHIVE archive = testcase.OpenArchiveFn();
        HSFILE file = nullptr;

        SECTION("opens a file") {
            CHECK(SFileOpenFileEx(archive, "test.txt", testcase.flags, &file) == 1);
            CHECK(file != nullptr);
            CHECK(file != reinterpret_cast<HSFILE>(1234));
        }

        SECTION("fails if file not found") {
            SErrSetLastError(ERROR_SUCCESS);
            file = reinterpret_cast<HSFILE>(1234);
            CHECK_FALSE(SFileOpenFileEx(archive, "nice try buddy but your file is in another castle", testcase.flags, &file));
            CHECK(file == nullptr);
            CHECK(SErrGetLastError() == ERROR_FILE_NOT_FOUND);
        }
    }

    SECTION("fails when trying to open a directory") {
        SErrSetLastError(ERROR_SUCCESS);
        HSFILE file = reinterpret_cast<HSFILE>(1234);
        CHECK_FALSE(SFileOpenFileEx(nullptr, "directorytest", SFILE_OPENFLAG_CHECKDISK, &file));
        CHECK(file == nullptr);
        CHECK(SErrGetLastError() == ERROR_FILE_NOT_FOUND);
    }

    SECTION("mpq testcases") {
        SECTION("opens the highest priority file from all MPQs") {
            // TODO
        }

        SECTION("opens the file for the currently selected locale") {
            // TODO
        }

        SECTION("fails if file not found in target MPQ") {
            // TODO
        }

        SECTION("fails if file not found in any MPQ") {
            // TODO
        }
    }
}

TEST_CASE("SFileReadFile", "[file]") {
    SECTION("shared testcases") {
        ReadFileTestCase testcase = GENERATE(
            ReadFileTestCase { "file from disk", ReadTestFileFromDisk, ERROR_SUCCESS },
            ReadFileTestCase { "file from MPQ", ReadTestFileFromMpq, ERROR_HANDLE_EOF }
        );

        INFO(testcase.info);
        HSFILE file = testcase.OpenFileFn();
        REQUIRE(file != nullptr);

        SECTION("reads a file") {
            char buffer[32] = {};
            CHECK(SFileReadFile(file, buffer, 6, nullptr, nullptr) == 1);
            CHECK(std::string(buffer) == "catdog");
        }

        SECTION("reads partial file if bytestoread is too small") {
            char buffer[4] = "";
            CHECK(SFileReadFile(file, &buffer, 3, nullptr, nullptr) == 1);
            CHECK(std::string(buffer) == "cat");
        }

        SECTION("continues reading from the last stored position") {
            char buffer[4] = "";
            CHECK(SFileReadFile(file, &buffer, 3, nullptr, nullptr) == 1);
            CHECK(SFileReadFile(file, &buffer, 3, nullptr, nullptr) == 1);
            CHECK(std::string(buffer) == "dog");
        }

        SECTION("continues reading from an explicitly set position") {
            char buffer[4] = "";
            SFileSetFilePointer(file, 3, nullptr, SFILE_BEGIN);
            CHECK(SFileReadFile(file, &buffer, 3, nullptr, nullptr) == 1);
            CHECK(std::string(buffer) == "dog");
        }

        SECTION("succeeds if bytestoread is 0") {
            char buffer;
            CHECK(SFileReadFile(file, &buffer, 0, nullptr, nullptr) == 1);

            uint32_t read = 42;
            CHECK(SFileReadFile(file, &buffer, 0, &read, nullptr) == 1);
            CHECK(read == 0);
        }

        SECTION("succeeds if bytestoread is 0 past eof") {
            char buffer[8];
            CHECK_FALSE(SFileReadFile(file, &buffer, 8, nullptr, nullptr));

            uint32_t read = 42;
            CHECK(SFileReadFile(file, &buffer, 0, &read, nullptr) == 1);
            CHECK(read == 0);
        }

        SECTION("fails when reading past end of file") {
            char buffer[8] = "";
            CHECK(SFileReadFile(file, &buffer, 6, nullptr, nullptr) == 1);
            CHECK(std::string(buffer) == "catdog");

            SErrSetLastError(0);
            CHECK_FALSE(SFileReadFile(file, &buffer, 1, nullptr, nullptr));
            CHECK(SErrGetLastError() == testcase.eofcode);

            CHECK(std::string(buffer) == "catdog");
        }

        SECTION("fails if bytestoread is larger than file size when reading from disk") {
            char buffer[32] = "";

            SErrSetLastError(0);
            CHECK_FALSE(SFileReadFile(file, buffer, sizeof(buffer), nullptr, nullptr));
            CHECK(SErrGetLastError() == testcase.eofcode);

            CHECK(std::string(buffer) == "catdog");
        }
    }
}

TEST_CASE("SFileSetFilePointer", "[file]") {
    ReadFileTestCase testcase = GENERATE(
        ReadFileTestCase { "file from disk", ReadTestFileFromDisk, ERROR_SUCCESS },
        ReadFileTestCase { "file from MPQ", ReadTestFileFromMpq, ERROR_HANDLE_EOF }
    );

    INFO(testcase.info);
    HSFILE file = testcase.OpenFileFn();
    REQUIRE(file != nullptr);

    SECTION("sets position from beginning") {
        // TODO
    }

    SECTION("sets position from current") {
        // TODO
    }

    SECTION("sets position from end") {
        // TODO
    }

    SECTION("resets pointer if value from current is invalid") {
        // TODO
    }

    SECTION("resets pointer if value from end is invalid") {
        // TODO
    }
}
#endif
