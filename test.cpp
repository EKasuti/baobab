#include "log.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

const char* TEST_FILE = "test-000001.log";

void check(bool ok, const std::string& what) {
    if (!ok) {
        printf("FAIL: %s\n", what.c_str());
        exit(EXIT_FAILURE);
    }
}

off_t file_size(const char* filename) {
    struct stat st;
    check(stat(filename, &st) == 0, "stat");
    return st.st_size;
}

void test_round_trip() {
    Record source = {RECORD_PUT, "hello", "world"};
    std::vector<uint8_t> buffer(record_size(&source));
    check(buffer.size() == HEADER_SIZE + 5 + 5, "record size");
    serialize_record(&source, buffer.data());

    Record destination;
    check(deserialize_record(buffer.data(), buffer.size(), &destination) == DESERIALIZE_SUCCESS,
          "round-trip deserializes");
    check(destination.type == RECORD_PUT && destination.key == "hello" &&
              destination.value == "world",
          "round-trip matches");

    check(deserialize_record(buffer.data(), buffer.size() - 1, &destination) ==
              DESERIALIZE_TRUNCATED,
          "short buffer rejected");
    buffer.back() ^= 0xFF;
    check(deserialize_record(buffer.data(), buffer.size(), &destination) == DESERIALIZE_BAD_CRC,
          "bad crc rejected");
}

void test_put_get_delete() {
    unlink(TEST_FILE);
    Log* log = log_open(TEST_FILE);

    // The file grows by exactly one record per put.
    for (int i = 0; i < 1000; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "val" + std::to_string(i);
        off_t before = file_size(TEST_FILE);
        check(log_put(log, key, value), "put " + key);
        check(file_size(TEST_FILE) - before == (off_t)(HEADER_SIZE + key.size() + value.size()),
              "file grew by record size for " + key);
    }

    std::string value;
    for (int i = 0; i < 1000; i++) {
        std::string key = "key" + std::to_string(i);
        check(log_get(log, key, &value) && value == "val" + std::to_string(i), "get " + key);
    }

    check(log_put(log, "key5", "new"), "overwrite");
    check(log_get(log, "key5", &value) && value == "new", "overwrite returns latest");

    off_t before = file_size(TEST_FILE);
    check(log_delete(log, "key7"), "delete");
    check(!log_get(log, "key7", &value), "deleted key is gone");
    check(file_size(TEST_FILE) - before == (off_t)(HEADER_SIZE + 4), "tombstone appended");
    check(!log_get(log, "missing", &value), "missing key");

    log_close(log);
    unlink(TEST_FILE);
}

int main() {
    test_round_trip();
    test_put_get_delete();
    printf("All tests passed.\n");
    return EXIT_SUCCESS;
}
