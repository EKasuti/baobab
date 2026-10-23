#include "log.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <zlib.h>

static uint32_t checksum(const uint8_t* data, uint32_t length) {
    return crc32(crc32(0L, Z_NULL, 0), data, length);
}

uint32_t record_size(Record* record) {
    return HEADER_SIZE + record->key.size() + record->value.size();
}

void serialize_record(Record* source, void* destination) {
    uint8_t* bytes = (uint8_t*)destination;
    uint8_t type = source->type;
    uint16_t key_len = source->key.size();
    uint32_t val_len = source->value.size();

    memcpy(bytes + TYPE_OFFSET, &type, TYPE_SIZE);
    memcpy(bytes + KEY_LEN_OFFSET, &key_len, KEY_LEN_SIZE);
    memcpy(bytes + VAL_LEN_OFFSET, &val_len, VAL_LEN_SIZE);
    memcpy(bytes + HEADER_SIZE, source->key.data(), key_len);
    memcpy(bytes + HEADER_SIZE + key_len, source->value.data(), val_len);

    // The crc goes in last: it covers everything after itself.
    uint32_t crc = checksum(bytes + TYPE_OFFSET, HEADER_SIZE - CRC_SIZE + key_len + val_len);
    memcpy(bytes + CRC_OFFSET, &crc, CRC_SIZE);
}

DeserializeResult deserialize_record(void* source, uint32_t available, Record* destination) {
    uint8_t* bytes = (uint8_t*)source;
    if (available < HEADER_SIZE) {
        return DESERIALIZE_TRUNCATED;
    }

    RecordHeader header;
    memcpy(&header.crc, bytes + CRC_OFFSET, CRC_SIZE);
    memcpy(&header.type, bytes + TYPE_OFFSET, TYPE_SIZE);
    memcpy(&header.key_len, bytes + KEY_LEN_OFFSET, KEY_LEN_SIZE);
    memcpy(&header.val_len, bytes + VAL_LEN_OFFSET, VAL_LEN_SIZE);

    uint64_t total = (uint64_t)HEADER_SIZE + header.key_len + header.val_len;
    if (available < total) {
        return DESERIALIZE_TRUNCATED;
    }
    if (header.crc != checksum(bytes + TYPE_OFFSET, total - CRC_SIZE)) {
        return DESERIALIZE_BAD_CRC;
    }
    if (header.type != RECORD_PUT && header.type != RECORD_DELETE) {
        return DESERIALIZE_BAD_CRC;
    }

    const char* key = (const char*)(bytes + HEADER_SIZE);
    destination->type = (RecordType)header.type;
    destination->key.assign(key, header.key_len);
    destination->value.assign(key + header.key_len, header.val_len);
    return DESERIALIZE_SUCCESS;
}

Log* log_open(const char* filename) {
    int fd = open(filename, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        printf("Unable to open file '%s': %s\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    Log* log = new Log;
    log->file_descriptor = fd;
    return log;
}

void log_close(Log* log) {
    if (close(log->file_descriptor) == -1) {
        printf("Error closing log file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    delete log;
}

static bool log_append(Log* log, Record* record) {
    std::vector<uint8_t> buffer(record_size(record));
    serialize_record(record, buffer.data());

    size_t written = 0;
    while (written < buffer.size()) {
        ssize_t bytes_written =
            write(log->file_descriptor, buffer.data() + written, buffer.size() - written);
        if (bytes_written == -1) {
            if (errno == EINTR) {
                continue;
            }
            printf("Error writing: %s\n", strerror(errno));
            return false;
        }
        written += bytes_written;
    }
    return true;
}

bool log_put(Log* log, const std::string& key, const std::string& value) {
    Record record = {RECORD_PUT, key, value};
    if (!log_append(log, &record)) {
        return false;
    }
    log->index[key] = value;
    return true;
}

bool log_get(Log* log, const std::string& key, std::string* value) {
    auto it = log->index.find(key);
    if (it == log->index.end()) {
        return false;
    }
    *value = it->second;
    return true;
}

bool log_delete(Log* log, const std::string& key) {
    // Always write the tombstone: the key may live in older data on disk
    // that this process hasn't loaded into the index.
    Record record = {RECORD_DELETE, key, ""};
    if (!log_append(log, &record)) {
        return false;
    }
    log->index.erase(key);
    return true;
}
