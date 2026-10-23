#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

/*
 * On-disk record (integers in native byte order):
 *
 *   crc (4) | type (1) | key_len (2) | val_len (4) | key | value
 *
 * crc covers every byte after the crc field.
 */
typedef struct {
    uint32_t crc;
    uint8_t type;
    uint16_t key_len;
    uint32_t val_len;
} RecordHeader;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

const uint32_t CRC_SIZE = size_of_attribute(RecordHeader, crc);
const uint32_t TYPE_SIZE = size_of_attribute(RecordHeader, type);
const uint32_t KEY_LEN_SIZE = size_of_attribute(RecordHeader, key_len);
const uint32_t VAL_LEN_SIZE = size_of_attribute(RecordHeader, val_len);
const uint32_t CRC_OFFSET = 0;
const uint32_t TYPE_OFFSET = CRC_OFFSET + CRC_SIZE;
const uint32_t KEY_LEN_OFFSET = TYPE_OFFSET + TYPE_SIZE;
const uint32_t VAL_LEN_OFFSET = KEY_LEN_OFFSET + KEY_LEN_SIZE;
const uint32_t HEADER_SIZE = VAL_LEN_OFFSET + VAL_LEN_SIZE;

const uint32_t MAX_KEY_SIZE = UINT16_MAX;
const uint32_t MAX_VAL_SIZE = UINT32_MAX - HEADER_SIZE - MAX_KEY_SIZE;

typedef enum : uint8_t { RECORD_DELETE = 0, RECORD_PUT = 1 } RecordType;

typedef struct {
    RecordType type;
    std::string key;
    std::string value;
} Record;

typedef enum {
    DESERIALIZE_SUCCESS,
    DESERIALIZE_TRUNCATED,
    DESERIALIZE_BAD_CRC
} DeserializeResult;

typedef struct {
    int file_descriptor;
    std::unordered_map<std::string, std::string> index;
} Log;

uint32_t record_size(Record* record);
void serialize_record(Record* source, void* destination);
DeserializeResult deserialize_record(void* source, uint32_t available, Record* destination);

Log* log_open(const char* filename);
void log_close(Log* log);
bool log_put(Log* log, const std::string& key, const std::string& value);
bool log_get(Log* log, const std::string& key, std::string* value);
bool log_delete(Log* log, const std::string& key);
