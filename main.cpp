#include "log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>

typedef struct {
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
} InputBuffer;

typedef enum { META_COMMAND_SUCCESS, META_COMMAND_UNRECOGNIZED_COMMAND } MetaCommandResult;

typedef enum {
    PREPARE_SUCCESS,
    PREPARE_KEY_TOO_LONG,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

typedef enum { STATEMENT_PUT, STATEMENT_GET, STATEMENT_DELETE } StatementType;

typedef enum { EXECUTE_SUCCESS, EXECUTE_KEY_NOT_FOUND, EXECUTE_WRITE_ERROR } ExecuteResult;

typedef struct {
    StatementType type;
    std::string key;
    std::string value;
} Statement;

const char* DATA_FILE = "data-000001.log";

InputBuffer* new_input_buffer() {
    InputBuffer* input_buffer = (InputBuffer*)malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;
    return input_buffer;
}

void print_prompt() { printf("db > "); }

void read_input(InputBuffer* input_buffer) {
    ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);
    if (bytes_read <= 0) {
        printf("Error reading input\n");
        exit(EXIT_FAILURE);
    }

    // Ignore trailing newline
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = 0;
}

void close_input_buffer(InputBuffer* input_buffer) {
    free(input_buffer->buffer);
    free(input_buffer);
}

MetaCommandResult do_meta_command(InputBuffer* input_buffer, Log* log) {
    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        close_input_buffer(input_buffer);
        log_close(log);
        exit(EXIT_SUCCESS);
    }
    return META_COMMAND_UNRECOGNIZED_COMMAND;
}

PrepareResult prepare_put(InputBuffer* input_buffer, Statement* statement) {
    statement->type = STATEMENT_PUT;

    strtok(input_buffer->buffer, " ");
    char* key = strtok(NULL, " ");
    char* value = strtok(NULL, "");  // rest of the line, spaces allowed
    if (key == NULL || value == NULL) {
        return PREPARE_SYNTAX_ERROR;
    }
    if (strlen(key) > MAX_KEY_SIZE) {
        return PREPARE_KEY_TOO_LONG;
    }

    statement->key = key;
    statement->value = value;
    return PREPARE_SUCCESS;
}

PrepareResult prepare_key_only(InputBuffer* input_buffer, Statement* statement,
                               StatementType type) {
    statement->type = type;

    strtok(input_buffer->buffer, " ");
    char* key = strtok(NULL, " ");
    if (key == NULL || strtok(NULL, " ") != NULL) {
        return PREPARE_SYNTAX_ERROR;
    }
    if (strlen(key) > MAX_KEY_SIZE) {
        return PREPARE_KEY_TOO_LONG;
    }

    statement->key = key;
    return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement) {
    if (strncmp(input_buffer->buffer, "put ", 4) == 0) {
        return prepare_put(input_buffer, statement);
    }
    if (strncmp(input_buffer->buffer, "get ", 4) == 0) {
        return prepare_key_only(input_buffer, statement, STATEMENT_GET);
    }
    if (strncmp(input_buffer->buffer, "del ", 4) == 0) {
        return prepare_key_only(input_buffer, statement, STATEMENT_DELETE);
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_put(Statement* statement, Log* log) {
    if (!log_put(log, statement->key, statement->value)) {
        return EXECUTE_WRITE_ERROR;
    }
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_get(Statement* statement, Log* log) {
    std::string value;
    if (!log_get(log, statement->key, &value)) {
        return EXECUTE_KEY_NOT_FOUND;
    }
    printf("%s\n", value.c_str());
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_delete(Statement* statement, Log* log) {
    if (!log_delete(log, statement->key)) {
        return EXECUTE_WRITE_ERROR;
    }
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement* statement, Log* log) {
    switch (statement->type) {
        case (STATEMENT_PUT):
            return execute_put(statement, log);
        case (STATEMENT_GET):
            return execute_get(statement, log);
        case (STATEMENT_DELETE):
            return execute_delete(statement, log);
    }
    return EXECUTE_SUCCESS;
}

int main() {
    Log* log = log_open(DATA_FILE);
    InputBuffer* input_buffer = new_input_buffer();
    while (true) {
        print_prompt();
        read_input(input_buffer);

        if (input_buffer->buffer[0] == '.') {
            switch (do_meta_command(input_buffer, log)) {
                case (META_COMMAND_SUCCESS):
                    continue;
                case (META_COMMAND_UNRECOGNIZED_COMMAND):
                    printf("Unrecognized command '%s'\n", input_buffer->buffer);
                    continue;
            }
        }

        Statement statement;
        switch (prepare_statement(input_buffer, &statement)) {
            case (PREPARE_SUCCESS):
                break;
            case (PREPARE_KEY_TOO_LONG):
                printf("Key is too long.\n");
                continue;
            case (PREPARE_SYNTAX_ERROR):
                printf("Syntax error. Could not parse statement.\n");
                continue;
            case (PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
                continue;
        }

        switch (execute_statement(&statement, log)) {
            case (EXECUTE_SUCCESS):
                printf("Executed.\n");
                break;
            case (EXECUTE_KEY_NOT_FOUND):
                printf("Error: Key not found.\n");
                break;
            case (EXECUTE_WRITE_ERROR):
                printf("Error: Write failed.\n");
                break;
        }
    }
}
