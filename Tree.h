#ifndef TREE_H
#define TREE_H

#include "List.h"
#include "DFS.h"
#include "ST.h"
#include <mpi.h>

/* MPI Messages */
#define CONNECT 10
#define NEIGHBOR 11

typedef struct Neighbors {
    int rank;
    struct Neighbors* next;
} Neighbors;

typedef struct Tree_Node {
    int rank;
    Neighbors* neighbors;
    struct Tree_Node* next;
} Tree_Node;

/* Struct head */
extern Tree_Node* tree_head;

/* Struct Functions */
Tree_Node* create_node(int rank);
Tree_Node* search_node(int rank);
void add_node(Tree_Node* node);
void add_neighbor(Tree_Node* node, int neigh_rank);
void free_tree();
void print_tree();

/* MPI Handling */
void handle_connect(int rank, int neigh, MPI_Status status);
void handle_neighbor(int rank, int neigh);

#endif /* End of Tree.h */