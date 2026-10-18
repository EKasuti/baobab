Author: Kasuti Makau
Date: Oct 18, 2026

A database is a:
- persistent, ordered key-value store
    - every write is appeded to a file. Old data is never overwritten so a crash can't corrupt it
    - forces the OS's buffers to stable storage
    - re-read the log front to back to rebuild the in-memory map
    - rotate a new log file when it gets big, periodically compact old files keeping only the lates record per key
- with a query language and execution engine on top
- wrapped in transactional guarantees. 
Each layer only talks to the one below it