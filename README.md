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

- A query goes through a chain of components in order to retrieve or modify data. 
- The front-end consists of
    - Tokenizer
    - Parser
    - Code generator
    * - the frontend is a SQL query. the output is sqlite virtual machine bytecode
- The backend consists of
    - virtual machine
        -  takes bytecode as instructions and performs operations to one or more tables or idx each of which is stored in a data structure called B-tree
    - B-tree
        - consists of many nodes. Each node is one page in length
    - pager
        -  receives commands to read or write pages of data
    - os interface
        -  it's the layer that differs depending on which operating system sqlite was compiled for


## Resources
1. Bitcask paper [link](https://riak.com/assets/bitcask-intro.pdf)
2. Cstask tutorial [link](https://cstack.github.io/db_tutorial)