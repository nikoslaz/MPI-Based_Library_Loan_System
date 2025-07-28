#ifndef DFS_H
#define DFS_H

#include <mpi.h>
#include "List.h"

/* MPI Messages */
#define NULL_ID       -1
#define START_LE_LIBR 30
#define DFS_LEADER    31
#define DFS_ALREADY   32
#define DFS_PARENT    33
#define DFS_TERMINATE 34

typedef struct DFS_Node {
    int id;
    int parent;
    int leader;
    List* children;
    List* unexplored;
    List* neighbors;
    int dfs_active;
    int is_terminated;
    int children_acks;
    int children_acks_recv;
    /* Needed for book.c */
    int total_loans_reported_by_libraries;
    int libraries_reported_loan_count;
    int loaner_id;  
    int book_id; 
} DFS_Node;

/* Struct head */
extern DFS_Node* pi;

/* Struct Funtion */
void init_DFS(DFS_Node* node, int rank);
void free_DFS(DFS_Node* node);

/* MPI Handling */
void explore(int rank);
void handle_start_le_libr(int rank, int N_val);
void handle_dfs_leader(int rank, int received_leader, int received_id, MPI_Status status, int N_val);
void handle_dfs_already(int rank, int received_leader, int received_id, MPI_Status status);
void handle_dfs_parent(int rank, int received_leader, int received_id, MPI_Status status);
void check_DFS_termination(int rank);

#endif // DFS_H