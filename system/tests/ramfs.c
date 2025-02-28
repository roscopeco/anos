/* Unit tests for the ramfs subsystem.
 *
 * Copyright (c)2018-2025 Ross Bamford. See LICENSE for details.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "munit.h"
#include "ramfs.h"

static AnosRAMFSHeader *create_testmodel_no_files(int version) {
    AnosRAMFSHeader *model = calloc(1, sizeof(AnosRAMFSHeader));

    model->magic = ANOS_RAMFS_MAGIC;
    model->version = version;
    model->file_count = 0;
    model->fs_size = sizeof(AnosRAMFSHeader);

    return model;
}

static AnosRAMFSHeader *create_testmodel_one_file(int version) {
    char *content = "File content";
    AnosRAMFSHeader *model =
            calloc(1, sizeof(AnosRAMFSHeader) + sizeof(AnosRAMFSFileHeader) +
                              strlen(content) + 1);
    AnosRAMFSFileHeader *file = (AnosRAMFSFileHeader *)(model + 1);
    char *filedata = (char *)(file + 1);

    model->magic = ANOS_RAMFS_MAGIC;
    model->version = version;
    model->file_count = 1;
    model->fs_size = sizeof(AnosRAMFSHeader);

    file->file_length = strlen(content) + 1;
    strcpy(file->file_name, "file001");
    file->file_start = sizeof(AnosRAMFSFileHeader);

    strcpy(filedata, content);

    return model;
}

static AnosRAMFSHeader *create_testmodel_one_file_malformed(int version) {
    char *content1 = "File 1ontent";
    AnosRAMFSHeader *model =
            calloc(1, sizeof(AnosRAMFSHeader) + sizeof(AnosRAMFSFileHeader) +
                              strlen(content1) + 1);
    AnosRAMFSFileHeader *file = (AnosRAMFSFileHeader *)(model + 1);
    char *filedata = (char *)(file + 1);

    model->magic = ANOS_RAMFS_MAGIC;
    model->version = version;
    model->file_count = 1;
    model->fs_size = sizeof(AnosRAMFSHeader);

    file->file_length = strlen(content1) + 1;
    memcpy(file->file_name, "123456789^123456789^1234",
           24); // MALFORMED - NOT NULL TERMINATED
    file->file_start = sizeof(AnosRAMFSFileHeader);

    strcpy(filedata, content1);

    return model;
}

static AnosRAMFSHeader *create_testmodel_three_files(int version) {
    char *content1 = "File 1ontent"; // these must stay same length!
    char *content2 = "File 2ontent";

    AnosRAMFSHeader *model = calloc(
            1, sizeof(AnosRAMFSHeader) + (sizeof(AnosRAMFSFileHeader) * 3) +
                       (strlen(content1) * 2) + 2);

    model->magic = ANOS_RAMFS_MAGIC;
    model->version = version;
    model->file_count = 2;
    model->fs_size = sizeof(AnosRAMFSHeader);

    AnosRAMFSFileHeader *file = (AnosRAMFSFileHeader *)(model + 1);
    file->file_length = strlen(content1) + 1;
    strcpy(file->file_name, "file001");
    file->file_start = sizeof(AnosRAMFSFileHeader) * 3;

    file++;
    file->file_length = strlen(content2) + 1;
    strcpy(file->file_name, "file002");
    file->file_start = sizeof(AnosRAMFSFileHeader) * 2 + strlen(content1) + 1;

    file++;
    file->file_length = 0;
    strcpy(file->file_name, "file003_empty");
    file->file_start = 0;

    char *filedata = (char *)(file + 1);
    strcpy(filedata, content1);

    filedata = filedata + strlen(content1) + 1;
    strcpy(filedata, content2);

    return model;
}

/* MUnit test functions */

static MunitResult
test_is_valid_ramfs_with_null_ramfs(const MunitParameter params[], void *data) {
    void *null_ramfs = NULL;
    munit_assert_false(is_valid_ramfs(null_ramfs));
    return MUNIT_OK;
}

static MunitResult
test_is_valid_ramfs_with_invalid_ramfs(const MunitParameter params[],
                                       void *data) {
    void *invalid_ramfs = calloc(1, sizeof(AnosRAMFSHeader));

    munit_assert_false(is_valid_ramfs(invalid_ramfs));

    free(invalid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_is_valid_ramfs_with_incorrect_version(const MunitParameter params[],
                                           void *data) {
    AnosRAMFSHeader *invalid_ramfs =
            create_testmodel_no_files(ANOS_RAMFS_VERSION + 10);

    munit_assert_false(is_valid_ramfs(invalid_ramfs));

    free(invalid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_is_valid_ramfs_with_valid_ramfs(const MunitParameter params[],
                                     void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_no_files(ANOS_RAMFS_VERSION);

    munit_assert_true(is_valid_ramfs(valid_ramfs));

    free(valid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_size_with_null_ramfs(const MunitParameter params[], void *data) {
    void *null_ramfs = NULL;
    munit_assert_int(ramfs_size(null_ramfs), ==, -1);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_size_with_invalid_ramfs(const MunitParameter params[], void *data) {
    void *invalid_ramfs = calloc(1, sizeof(AnosRAMFSHeader));

    munit_assert_int(ramfs_size(invalid_ramfs), ==, -1);

    free(invalid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_size_with_incorrect_version(const MunitParameter params[],
                                       void *data) {
    AnosRAMFSHeader *invalid_ramfs =
            create_testmodel_no_files(ANOS_RAMFS_VERSION + 5);

    munit_assert_int(ramfs_size(invalid_ramfs), ==, -1);

    free(invalid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_size_with_valid_ramfs(const MunitParameter params[], void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_no_files(ANOS_RAMFS_VERSION);

    munit_assert_int(ramfs_size(valid_ramfs), ==, sizeof(AnosRAMFSHeader));

    free(valid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_file_count_with_null_ramfs(const MunitParameter params[],
                                      void *data) {
    void *null_ramfs = NULL;
    munit_assert_int(ramfs_file_count(null_ramfs), ==, -1);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_file_count_with_invalid_ramfs(const MunitParameter params[],
                                         void *data) {
    void *invalid_ramfs = calloc(1, sizeof(AnosRAMFSHeader));

    munit_assert_int(ramfs_file_count(invalid_ramfs), ==, -1);

    free(invalid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_file_count_with_incorrect_version(const MunitParameter params[],
                                             void *data) {
    AnosRAMFSHeader *invalid_ramfs =
            create_testmodel_no_files(ANOS_RAMFS_VERSION + 5);

    munit_assert_int(ramfs_file_count(invalid_ramfs), ==, -1);

    free(invalid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_file_count_with_valid_empty_ramfs(const MunitParameter params[],
                                             void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_no_files(ANOS_RAMFS_VERSION);

    munit_assert_int(ramfs_file_count(valid_ramfs), ==, 0);

    free(valid_ramfs);
    return MUNIT_OK;
}

static MunitResult test_ramfs_file_count_with_valid_ramfs_and_one_file(
        const MunitParameter params[], void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_one_file(ANOS_RAMFS_VERSION);

    munit_assert_int(ramfs_file_count(valid_ramfs), ==, 1);

    free(valid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_find_file_with_null_ramfs(const MunitParameter params[],
                                     void *data) {
    void *null_ramfs = NULL;
    munit_assert_null(ramfs_find_file(null_ramfs, "file001"));
    return MUNIT_OK;
}

static MunitResult
test_ramfs_find_file_with_invalid_ramfs(const MunitParameter params[],
                                        void *data) {
    void *invalid_ramfs = calloc(1, sizeof(AnosRAMFSHeader));

    munit_assert_null(ramfs_find_file(invalid_ramfs, "file001"));

    free(invalid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_find_file_with_incorrect_version(const MunitParameter params[],
                                            void *data) {
    AnosRAMFSHeader *invalid_ramfs =
            create_testmodel_no_files(ANOS_RAMFS_VERSION + 5);

    munit_assert_null(ramfs_find_file(invalid_ramfs, "file001"));

    free(invalid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_find_file_with_valid_empty_ramfs(const MunitParameter params[],
                                            void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_no_files(ANOS_RAMFS_VERSION);

    munit_assert_null(ramfs_find_file(valid_ramfs, "file001"));

    free(valid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_find_file_with_valid_empty_ramfs_but_null_filename(
        const MunitParameter params[], void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_no_files(ANOS_RAMFS_VERSION);

    munit_assert_null(ramfs_find_file(valid_ramfs, NULL));

    free(valid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_find_file_with_valid_ramfs_and_one_file_but_not_found(
        const MunitParameter params[], void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_one_file(ANOS_RAMFS_VERSION);

    munit_assert_null(ramfs_find_file(valid_ramfs, "filenotfound"));

    free(valid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_find_file_with_valid_ramfs_and_one_file_but_null_filename(
        const MunitParameter params[], void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_one_file(ANOS_RAMFS_VERSION);

    munit_assert_null(ramfs_find_file(valid_ramfs, NULL));

    free(valid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_find_file_with_valid_ramfs_and_one_file_can_handle_malformed_ramfs(
        const MunitParameter params[], void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_one_file_malformed(ANOS_RAMFS_VERSION);

    munit_assert_null(ramfs_find_file(valid_ramfs, "file001"));

    free(valid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_find_file_with_valid_ramfs_and_one_file_that_exists(
        const MunitParameter params[], void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_one_file(ANOS_RAMFS_VERSION);
    AnosRAMFSFileHeader *file = ramfs_find_file(valid_ramfs, "file001");

    munit_assert_not_null(file);
    munit_assert_string_equal(file->file_name, "file001");
    munit_assert_int(file->file_length, ==, strlen("File content") + 1);

    free(valid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_find_file_with_valid_ramfs_and_two_files_that_exists(
        const MunitParameter params[], void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_three_files(ANOS_RAMFS_VERSION);

    AnosRAMFSFileHeader *file = ramfs_find_file(valid_ramfs, "file001");
    munit_assert_not_null(file);
    munit_assert_string_equal(file->file_name, "file001");
    munit_assert_int(file->file_length, ==, strlen("File 1ontent") + 1);

    file = ramfs_find_file(valid_ramfs, "file002");
    munit_assert_not_null(file);
    munit_assert_string_equal(file->file_name, "file002");
    munit_assert_int(file->file_length, ==, strlen("File 2ontent") + 1);

    free(valid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_file_open_with_null_file(const MunitParameter params[], void *data) {
    AnosRAMFSFileHeader *null_file = NULL;
    munit_assert_null(ramfs_file_open(null_file));
    return MUNIT_OK;
}

static MunitResult
test_ramfs_file_open_with_empty_file(const MunitParameter params[],
                                     void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_three_files(ANOS_RAMFS_VERSION);
    AnosRAMFSFileHeader *file = ramfs_find_file(valid_ramfs, "file003_empty");

    munit_assert_null(ramfs_file_open(file));

    free(valid_ramfs);
    return MUNIT_OK;
}

static MunitResult
test_ramfs_file_open_with_normal_file(const MunitParameter params[],
                                      void *data) {
    AnosRAMFSHeader *valid_ramfs =
            create_testmodel_three_files(ANOS_RAMFS_VERSION);
    AnosRAMFSFileHeader *file1 = ramfs_find_file(valid_ramfs, "file001");
    AnosRAMFSFileHeader *file2 = ramfs_find_file(valid_ramfs, "file002");

    char *data1 = (char *)ramfs_file_open(file1);
    char *data2 = (char *)ramfs_file_open(file2);

    munit_assert_not_null(data1);
    munit_assert_string_equal(data1, "File 1ontent");

    munit_assert_not_null(data2);
    munit_assert_string_equal(data2, "File 2ontent");

    free(valid_ramfs);
    return MUNIT_OK;
}

/* Define the test suite */
static MunitTest ramfs_tests[] = {
        {"/is_valid_ramfs_with_null_ramfs", test_is_valid_ramfs_with_null_ramfs,
         NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/is_valid_ramfs_with_invalid_ramfs",
         test_is_valid_ramfs_with_invalid_ramfs, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/is_valid_ramfs_with_incorrect_version",
         test_is_valid_ramfs_with_incorrect_version, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/is_valid_ramfs_with_valid_ramfs",
         test_is_valid_ramfs_with_valid_ramfs, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        {"/ramfs_size_with_null_ramfs", test_ramfs_size_with_null_ramfs, NULL,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_size_with_invalid_ramfs", test_ramfs_size_with_invalid_ramfs,
         NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_size_with_incorrect_version",
         test_ramfs_size_with_incorrect_version, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_size_with_valid_ramfs", test_ramfs_size_with_valid_ramfs, NULL,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {"/ramfs_file_count_with_null_ramfs",
         test_ramfs_file_count_with_null_ramfs, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_file_count_with_invalid_ramfs",
         test_ramfs_file_count_with_invalid_ramfs, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_file_count_with_incorrect_version",
         test_ramfs_file_count_with_incorrect_version, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_file_count_with_valid_empty_ramfs",
         test_ramfs_file_count_with_valid_empty_ramfs, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_file_count_with_valid_ramfs_and_one_file",
         test_ramfs_file_count_with_valid_ramfs_and_one_file, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        {"/ramfs_find_file_with_null_ramfs",
         test_ramfs_find_file_with_null_ramfs, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_find_file_with_invalid_ramfs",
         test_ramfs_find_file_with_invalid_ramfs, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_find_file_with_incorrect_version",
         test_ramfs_find_file_with_incorrect_version, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_find_file_with_valid_empty_ramfs",
         test_ramfs_find_file_with_valid_empty_ramfs, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_find_file_with_valid_empty_ramfs_but_null_filename",
         test_ramfs_find_file_with_valid_empty_ramfs_but_null_filename, NULL,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_find_file_with_valid_ramfs_and_one_file_but_not_found",
         test_ramfs_find_file_with_valid_ramfs_and_one_file_but_not_found, NULL,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_find_file_with_valid_ramfs_and_one_file_but_null_filename",
         test_ramfs_find_file_with_valid_ramfs_and_one_file_but_null_filename,
         NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_find_file_with_valid_ramfs_and_one_file_can_handle_malformed_"
         "ramfs",
         test_ramfs_find_file_with_valid_ramfs_and_one_file_can_handle_malformed_ramfs,
         NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_find_file_with_valid_ramfs_and_one_file_that_exists",
         test_ramfs_find_file_with_valid_ramfs_and_one_file_that_exists, NULL,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_find_file_with_valid_ramfs_and_two_files_that_exists",
         test_ramfs_find_file_with_valid_ramfs_and_two_files_that_exists, NULL,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {"/ramfs_file_open_with_null_file", test_ramfs_file_open_with_null_file,
         NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_file_open_with_empty_file",
         test_ramfs_file_open_with_empty_file, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ramfs_file_open_with_normal_file",
         test_ramfs_file_open_with_normal_file, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

/* Define the test suite */
static const MunitSuite test_suite = {
        "/ramfs",               /* prefix to test names */
        ramfs_tests,            /* array of tests */
        NULL,                   /* array of suites */
        1,                      /* iterations */
        MUNIT_SUITE_OPTION_NONE /* options */
};

/* Main entry point */
int main(int argc, char *argv[]) {
    return munit_suite_main(&test_suite, NULL, argc, argv);
}