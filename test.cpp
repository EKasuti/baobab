#include "log.h"

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

const char* TEST_FILE = "test-000001.log";

int checks_passed = 0;

// Stops the run on failure; prints nothing on success (for checks in loops).
void require(bool ok, const std::string& what) {
    if (!ok) {
        printf("  FAIL: %s\n", what.c_str());
        exit(EXIT_FAILURE);
    }
    checks_passed++;
}

void pass(const std::string& what) { printf("  PASS: %s\n", what.c_str()); }

void check(bool ok, const std::string& what) {
    require(ok, what);
    pass(what);
}

off_t file_size(const char* filename) {
    struct stat st;
    require(stat(filename, &st) == 0, "stat");
    return st.st_size;
}

void test_round_trip() {
    printf("test_round_trip\n");
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
    printf("test_put_get_delete\n");
    unlink(TEST_FILE);
    Log* log = log_open(TEST_FILE);

    // The file grows by exactly one record per put.
    for (int i = 0; i < 1000; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "val" + std::to_string(i);
        off_t before = file_size(TEST_FILE);
        require(log_put(log, key, value), "put " + key);
        require(file_size(TEST_FILE) - before == (off_t)(HEADER_SIZE + key.size() + value.size()),
                "file grew by record size for " + key);
    }
    pass("1000 puts, file grew by exactly one record each (" +
         std::to_string(file_size(TEST_FILE)) + " bytes)");

    std::string value;
    for (int i = 0; i < 1000; i++) {
        std::string key = "key" + std::to_string(i);
        require(log_get(log, key, &value) && value == "val" + std::to_string(i), "get " + key);
    }
    pass("1000 gets returned the right value");

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

void test_replay() {
    printf("test_replay\n");
    unlink(TEST_FILE);
    Log* log = log_open(TEST_FILE);
    for (int i = 0; i < 1000; i++) {
        require(log_put(log, "key" + std::to_string(i), "val" + std::to_string(i)), "put");
    }
    require(log_put(log, "key5", "new"), "overwrite");
    require(log_delete(log, "key7"), "delete");
    log_close(log);

    log = log_open(TEST_FILE);
    check(log->index.size() == 999, "reopen loads 999 keys (1000 minus one delete)");

    std::string value;
    bool all_match = true;
    for (int i = 0; i < 1000; i++) {
        if (i == 5 || i == 7) {
            continue;
        }
        all_match &= log_get(log, "key" + std::to_string(i), &value) &&
                     value == "val" + std::to_string(i);
    }
    check(all_match, "every key returns its value after reopen");
    check(log_get(log, "key5", &value) && value == "new", "overwrite survives reopen");
    check(!log_get(log, "key7", &value), "delete survives reopen");

    log_close(log);
    unlink(TEST_FILE);
}

// Writes three records, then lets `damage` mangle the last one.
Log* reopen_after_damage(void (*damage)()) {
    unlink(TEST_FILE);
    Log* log = log_open(TEST_FILE);
    require(log_put(log, "a", "apple"), "put a");
    require(log_put(log, "b", "banana"), "put b");
    require(log_put(log, "c", "cherry"), "put c");
    log_close(log);

    damage();
    return log_open(TEST_FILE);
}

void cut_last_five_bytes() {
    require(truncate(TEST_FILE, file_size(TEST_FILE) - 5) == 0, "truncate");
}

void flip_last_byte() {
    int fd = open(TEST_FILE, O_RDWR);
    off_t last = file_size(TEST_FILE) - 1;
    uint8_t byte;
    require(pread(fd, &byte, 1, last) == 1, "read last byte");
    byte ^= 0xFF;
    require(pwrite(fd, &byte, 1, last) == 1, "write last byte");
    close(fd);
}

void check_torn_tail_recovery(Log* log, const std::string& how) {
    std::string value;
    check(log_get(log, "a", &value) && value == "apple" && log_get(log, "b", &value) &&
              value == "banana",
          how + ": records before the tear are intact");
    check(!log_get(log, "c", &value) && log->index.size() == 2,
          how + ": torn record ignored, no garbage key");

    off_t intact = 2 * HEADER_SIZE + 1 + 5 + 1 + 6;
    check(file_size(TEST_FILE) == intact, how + ": file cut back to last good record");

    require(log_put(log, "d", "date"), "put after recovery");
    log_close(log);
    log = log_open(TEST_FILE);
    check(log_get(log, "d", &value) && value == "date" && log->index.size() == 3,
          how + ": appends after recovery survive the next reopen");
    log_close(log);
    unlink(TEST_FILE);
}

void test_torn_tail() {
    printf("test_torn_tail\n");
    check_torn_tail_recovery(reopen_after_damage(cut_last_five_bytes), "short tail");
    check_torn_tail_recovery(reopen_after_damage(flip_last_byte), "bad crc tail");
}

int main() {
    test_round_trip();
    test_put_get_delete();
    test_replay();
    test_torn_tail();
    printf("All tests passed (%d checks).\n", checks_passed);
    return EXIT_SUCCESS;
}
