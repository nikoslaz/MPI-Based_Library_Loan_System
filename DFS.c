#include "DFS.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h> 

//---------------------------------------------------------------------------------------------------------------------- //
/* Struct Funtion */

void init_DFS(DFS_Node* node, int rank) {
    node->id = rank;
    node->parent = NULL_ID;
    node->leader = NULL_ID;
    node->children = (List*)malloc(sizeof(List));
    if (node->children == NULL)  MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); ;
    list_init(node->children, 64);
    node->unexplored = (List*)malloc(sizeof(List));
    if (node->unexplored == NULL)  MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); ;
    list_init(node->unexplored, 64);
    node->neighbors = (List*)malloc(sizeof(List));
    if (node->neighbors == NULL)  MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); ;
    list_init(node->neighbors, 64);
    node->children_acks = 0;
    node->children_acks_recv = 0;
    node->dfs_active = 0;
    node->is_terminated = 0;
    node->total_loans_reported_by_libraries = 0;
    node->libraries_reported_loan_count = 0;
    node->loaner_id = NULL_ID;
    node->book_id = NULL_ID;
}

void free_DFS(DFS_Node* node) {
    if (node == NULL) return;
    if (node->children) {
        list_free(node->children);
        free(node->children);
        node->children = NULL;
    }
    if (node->unexplored) {
        list_free(node->unexplored);
        free(node->unexplored);
        node->unexplored = NULL;
    }
    if (node->neighbors) {
        list_free(node->neighbors);
        free(node->neighbors);
        node->neighbors = NULL;
    }
}

//---------------------------------------------------------------------------------------------------------------------- //
/* MPI Handling */

void explore(int rank) {
    if (pi == NULL) return;
    printf("(DFS_EXPLORE): Unexplored size: %d. Parent: %d. Leader: %d\n",
           pi->unexplored->size, pi->parent, pi->leader);

    if (!list_is_empty(pi->unexplored)) {
        int pk = pi->unexplored->data[0];
        list_remove(pi->unexplored, pk);
        /* Instead of <leader, leader> sent, leader and rank */
        int message[2] = {pi->leader, rank}; 
        MPI_Send(message, 2, MPI_INT, pk, DFS_LEADER, MPI_COMM_WORLD);
        pi->children_acks++;
    } else if (pi->parent != rank && pi->parent != NULL_ID) {
        int message[2] = {pi->parent, pi->leader};
        MPI_Send(message, 2, MPI_INT, pi->parent, DFS_PARENT, MPI_COMM_WORLD);
    } else {
        /* Wait for all ACKS to terminate */
    } 
}


void handle_start_le_libr(int rank, int N_val) { // N_val is the grid dimension
    printf("%d (DF): Received START_LE_LIBR. Initializing for DFS \n", rank);

    if (pi == NULL) {
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); 
        return;
    }
    int max_lib = N_val * N_val;
    /* Reset and init for a new DFS phase */
    pi->parent = NULL_ID;
    pi->leader = NULL_ID; 
    list_free(pi->children); 
    list_init(pi->children, 64);
    list_free(pi->unexplored); 
    list_init(pi->unexplored, 64);
    for (int i = 0; i < pi->neighbors->size; i++) {
        int neigh = pi->neighbors->data[i];
        if (neigh >= 1 && neigh <= max_lib) {
            list_add(pi->unexplored, neigh);
        }
    }
    pi->dfs_active = 1;
    pi->children_acks = 0;
    pi->children_acks_recv = 0;
    pi->is_terminated = 0;
    pi->loaner_id = NULL_ID; 
    pi->book_id = NULL_ID;
    pi->leader = rank;
    pi->parent = rank; 
    explore(rank); 
}

void handle_dfs_leader(int rank, int recv_leader, int recv_id, MPI_Status status, int N_val) {
    printf("%d (DF): Received DFS_LEADER {L:%d, From:%d} from actual_sender %d. My current leader: %d\n",
        rank, recv_leader, recv_id, status.MPI_SOURCE, pi->leader);

    int max_lib = N_val * N_val;

    /* If it's over send the leader */
    if (pi->is_terminated) {
        if (pi->leader == recv_leader) {
            int message[2] = {pi->leader, rank}; 
            MPI_Send(message, 2, MPI_INT, status.MPI_SOURCE, DFS_ALREADY, MPI_COMM_WORLD);
        }
        return;
    }

    /* Elect new leader */
    if (pi->leader == NULL_ID || recv_leader > pi->leader) {
        pi->leader = recv_leader;
        pi->parent = status.MPI_SOURCE; 
        /* New leader so reset and init */
        list_free(pi->children);
        list_init(pi->children, 64);
        list_free(pi->unexplored);
        list_init(pi->unexplored, 64);
        for (int i = 0; i < pi->neighbors->size; i++) {
            int neigh = pi->neighbors->data[i];
            if (neigh != pi->parent) { 
                if (neigh >= 1 && neigh <= max_lib) {
                    list_add(pi->unexplored, neigh);
                }
            }
        }
        pi->children_acks = 0;
        pi->children_acks_recv = 0;
        pi->dfs_active = 1; 
        pi->is_terminated = 0; 
        explore(rank); 
    } else if (pi->leader == recv_leader) {
        /* Send ALREADY message */
        int message[2] = {pi->leader, rank}; 
        MPI_Send(message, 2, MPI_INT, status.MPI_SOURCE, DFS_ALREADY, MPI_COMM_WORLD);
    }
}

void handle_dfs_already(int rank, int recv_leader, int recv_id, MPI_Status status) {
    printf("%d (DF): Received DFS_ALREADY {L:%d, From:%d} from %d. My leader: %d\n", rank, recv_leader, recv_id, status.MPI_SOURCE, pi->leader);
    if (pi->is_terminated) return;
    if (pi->leader == recv_id) {
        if (pi->children_acks > 0) pi->children_acks--;
        explore(rank);
    }
}

void handle_dfs_parent(int rank, int recv_leader, int recv_id, MPI_Status status) {
    printf("%d (DF): Received DFS_PARENT {L:%d, FromChild:%d} from child %d. My leader: %d.\n",
           rank, recv_leader, recv_id, status.MPI_SOURCE, pi->leader);
    if (pi->is_terminated) return;

    if (pi->leader == recv_leader) { 
        if(!list_contains(pi->children, status.MPI_SOURCE)){
            list_add(pi->children, status.MPI_SOURCE);
        }
        pi->children_acks_recv++;
        explore(rank);
    }
}

void check_DFS_termination(int rank) {
    printf("%d (DF): ALREADY. Leader:%d, Parent:%d.\n", rank, pi->leader, pi->parent);

    if (pi == NULL || pi->is_terminated || !pi->dfs_active) {
        return;
    }

    if (list_is_empty(pi->unexplored)) {
        int children_reported = (pi->children_acks == pi->children_acks_recv);
        if (pi->parent == rank) {
            if (children_reported) {
                int report_leader = pi->leader;
                MPI_Send(&report_leader, 1, MPI_INT, 0, DFS_TERMINATE, MPI_COMM_WORLD);
                pi->is_terminated = 1;
                pi->dfs_active = 0; 
            } 
        }
    }
}