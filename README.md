# Distributed Library Management System

## Overview
An MPI-implemented distributed system simulating library operations with:
- N×N grid of library processes
- Tree-structured borrower processes
- Coordinator for event management
- Leader election algorithms
- Book loaning/donation tracking

## System Components

### Process Hierarchy
| Process Type       | Quantity              | MPI Rank Range      |
|--------------------|-----------------------|---------------------|
| Coordinator        | 1                     | 0                   |
| Library Nodes      | N²                    | 1 to N²             |
| Borrower Nodes     | floor(N³/2)           | N²+1 to N²+floor(N³/2) |

### Library Grid Structure
- Each library at (i,j) has MPI rank = N×j + i
- Neighbors:
  - Row: (i±1,j) if exists
  - Column: (i,j±1) if exists

### Borrower Tree
- Formed via CONNECT events
- Each library manages N/2 borrowers

## Key Features

### 1. Leader Election
| Subsystem       | Algorithm                     | Message Types                     |
|----------------|-------------------------------|-----------------------------------|
| Libraries      | DFS Spanning Tree             | START_LE_LIBR, LE_LIBR_DONE       |
| Borrowers      | STtoLeader (Convergecast)     | START_LE_LOANERS, LE_LOANERS_DONE |

### 2. Book Operations
| Operation          | Message Flow                                  | Verification                     |
|--------------------|-----------------------------------------------|----------------------------------|
| takeBook           | Borrower → Library → Leader → Destination Lib | ACK_TB, DONE_FIND_BOOK           |
| donateBook         | Round-robin distribution                     | ACK_DB                           |
| getMostPopularBook | Broadcast + Convergecast                     | GET_POPULAR_BK_INFO, ACK_BK_INFO |
| checkNumBooksLoaned| Grid traversal + Tree convergecast           | NUM_BOOKS_LOANED, ACK_NBL        |

RUN 1ST TEST :
    mpirun --mca btl_vader_single_copy_mechanism none -np 23 --oversubscribe ./mpi_program 3 testfiles_hy486/testfile0/loaners_13_libs_9_np_23.txt output.txt

RUN 2ND TEST:
    mpirun --mca btl_vader_single_copy_mechanism none -np 49 --oversubscribe ./mpi_program 4 testfiles_hy486/testfile1/loaners_32_libs_16_np_49.txt output.txt

RUN 3RD TEST:
    mpirun --mca btl_vader_single_copy_mechanism none -np 88 --oversubscribe ./mpi_program 5 testfiles_hy486/testfile2/loaners_62_libs_25_np_88.txt output.txt
