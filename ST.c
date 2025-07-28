#include "ST.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

//---------------------------------------------------------------------------------------------------------------------------------//
/* Struct Funtion */

void init_ST(ST_Node* node, int rank) {
    if (node == NULL) {
        fprintf(stderr, "%d: init_ST failed, node is NULL\n", rank);
        exit(EXIT_FAILURE);
    }
    node->id = rank;
    node->is_leader = 0;
    node->st_active = 0;
    node->elect_sent = -1;
    node->elect_count = 0;
    node->neighbor_count = 0;
    node->leader_id = -1;
    node->max_id = rank;
    node->neighbors = (List*)malloc(sizeof(List));
    if (node->neighbors == NULL)  MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); 
    list_init(node->neighbors, 64);
    node->elect_received = (List*)malloc(sizeof(List));
    if (node->elect_received == NULL)  MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); 
    list_init(node->elect_received, 64);

    /* GetMostPopularBook members */
    node->parent_in_st_tree = -1; 
    node->children_in_st_tree = (List*)malloc(sizeof(List));
    if (node->children_in_st_tree == NULL) MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); 
    list_init(node->children_in_st_tree, 64); 
    node->popular_book_id = -1;
    node->popular_times_loaned = 0;
    node->popular_book_cost = 0;
    node->popular_origin_l_id = -1;
    node->overall_best_book_id = -1;
    node->overall_best_times_loaned = -1;
    node->overall_best_book_cost = -1;
    node->overall_best_l_id_group = -1;
    node->reports_received_count = 0;
    node->known_overall_st_leader_rank = -1; 
    node->total_loans_as_loaner_from_subtree = 0;
    node->expected_loan_reports_from_children = 0;
    node->received_loan_reports_from_children = 0;
}


void free_ST(ST_Node* node) {
    if (node == NULL) return;
    if (node->neighbors) {
        list_free(node->neighbors);
        free(node->neighbors);
        node->neighbors = NULL;
    }
    if (node->elect_received) {
        list_free(node->elect_received);
        free(node->elect_received);
        node->elect_received = NULL;
    }
}

//---------------------------------------------------------------------------------------------------------------------------------//
/* MPI Handling */

void handle_start_loan(int rank, int N) {
    printf("%d (ST): Entering handle_start_loan\n", rank);
    if (st_pi == NULL) return;

    st_pi->st_active = 1;
    st_pi->elect_count = 0;
    st_pi->elect_sent = -1;
    st_pi->max_id = st_pi->id;
    st_pi->leader_id = -1;
    st_pi->is_leader = 0;
    list_free(st_pi->elect_received);
    list_init(st_pi->elect_received, 64);
    list_free(st_pi->children_in_st_tree);
    list_init(st_pi->children_in_st_tree, 64);
    st_pi->reports_received_count = 0;

    int loan_star = N * N + 1;
    int loan_end  = N * N + floor(pow(N, 3) / 2.0);

    /* Count only the loans neighbors */
    List* loan_neighbors = (List*)malloc(sizeof(List));
    if (loan_neighbors == NULL){
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); 
    }
    list_init(loan_neighbors, 64);

    if (st_pi->neighbors) {
        for (int i = 0; i < st_pi->neighbors->size; i++) {
            int neigh = st_pi->neighbors->data[i];
            /* Skip itself */
            if (neigh == rank) {
                continue;
            }
            if (neigh >= loan_star && neigh <= loan_end) {
                if (!list_contains(loan_neighbors, neigh)) {
                    list_add(loan_neighbors, neigh);
                }
            }
        }
    }
    int size_neigh = loan_neighbors->size;

    /* Check if we have any neighbors or if we are isolated */
    if (size_neigh == 0) {
        st_pi->leader_id = rank;
        st_pi->is_leader = 1;
        MPI_Send(&st_pi->leader_id, 1, MPI_INT, 0, ST_TERMINATION, MPI_COMM_WORLD);
        st_pi->st_active = 0;
        list_free(loan_neighbors); free(loan_neighbors);
        return;
    } else if (size_neigh == 1) {
        int neigh = loan_neighbors->data[0];
        MPI_Send(&st_pi->max_id, 1, MPI_INT, neigh, ST_ELECT, MPI_COMM_WORLD);
        st_pi->elect_sent = neigh;
    }

    list_free(loan_neighbors);
    free(loan_neighbors);
}


void handle_st_elect(int rank, int recv_id, MPI_Status status, int N) {
    printf("%d (ST): Entering handle_st_elect from rank %d (original sender of this ELECT content: %d)\n", rank, status.MPI_SOURCE, recv_id);
    if (st_pi == NULL || !st_pi->st_active) {
        return;
    }
 
    /* The process that sent the message */
    int sender = status.MPI_SOURCE;
    int loan_star = N * N + 1;
    int loan_end  = N * N + floor(pow(N, 3) / 2.0);
    if (!(sender >= loan_star && sender <= loan_end)) {
        return;
    }
    
    /* See if it has already came here */
    if (!list_contains(st_pi->elect_received, sender)) {
        list_add(st_pi->elect_received, sender);
    }
    st_pi->elect_count = st_pi->elect_received->size; 

    /* Keep track of the max id */
    if (recv_id > st_pi->max_id) st_pi->max_id = recv_id;

    /* Count only the loans neighbors */
    int size_neigh = 0;
    List* loan_neigh = list_create();
    if(st_pi->neighbors){
        for (int i = 0; i < st_pi->neighbors->size; i++) {
            int neigh = st_pi->neighbors->data[i];
            if (neigh == rank) continue; 
            if (neigh >= loan_star && neigh <= loan_end) {
                if(!list_contains(loan_neigh, neigh)){
                    list_add(loan_neigh, neigh);
                }
            }
        }
    }
    size_neigh = loan_neigh->size;
    list_free(loan_neigh); free(loan_neigh);

    /* Sent ST_ELECT to loan neighbors */
    if (st_pi->elect_count == size_neigh - 1 && st_pi->elect_sent == -1) {
        int remaining_neighbor = -1;
        if(st_pi->neighbors){
            for (int i = 0; i < st_pi->neighbors->size; i++) {
                int neigh = st_pi->neighbors->data[i];
                if (neigh == rank) continue;
                if (neigh >= loan_star && neigh <= loan_end && !list_contains(st_pi->elect_received, neigh)) {
                    remaining_neighbor = neigh;
                    break;
                }
            }
        }
        if (remaining_neighbor != -1) {
            MPI_Send(&st_pi->max_id, 1, MPI_INT, remaining_neighbor, ST_ELECT, MPI_COMM_WORLD);
            st_pi->elect_sent = remaining_neighbor;
        }
    }

    /* Check if we checked all neighbors to find the leader */
    if (st_pi->elect_count == size_neigh) {
        if(size_neigh == 0 && rank != st_pi->max_id) {
            st_pi->max_id = rank;
        }
        st_pi->leader_id = st_pi->max_id;
        st_pi->is_leader = (st_pi->max_id == st_pi->id);
        /* Leader elected, sending ST_LEADER */
        if(st_pi->neighbors){
            for (int i = 0; i < st_pi->neighbors->size; i++) {
                int neigh = st_pi->neighbors->data[i];
                if (neigh == rank) continue;
                if (neigh >= loan_star && neigh <= loan_end) {
                    MPI_Send(&st_pi->leader_id, 1, MPI_INT, neigh, ST_LEADER, MPI_COMM_WORLD);
                }
            }
        }
        /* Found leader, send to 0 */
        MPI_Send(&st_pi->leader_id, 1, MPI_INT, 0, ST_TERMINATION, MPI_COMM_WORLD);
        st_pi->st_active = 0;
    }
}

void handle_leader(int rank, int leader_id, MPI_Status status, int N) {
    printf("%d (ST): Entering handle_leader, leader_id=%d, sender=%d\n", rank, leader_id, status.MPI_SOURCE);
    if (st_pi == NULL || !st_pi->st_active) return;

    int loan_star = N * N + 1;
    int loan_end  = N * N + floor(pow(N, 3) / 2.0);

    if (!(status.MPI_SOURCE >= loan_star && status.MPI_SOURCE <= loan_end)) {
        return;
    }

    /* Elect new leader */
    st_pi->leader_id = leader_id;
    st_pi->is_leader = (leader_id == rank);
    st_pi->st_active = 0; 

    /* Send ST_LEADER to all other loaner neighbors */
    for (int i = 0; i < st_pi->neighbors->size; i++) {
        int neighbor = st_pi->neighbors->data[i];
        if (neighbor != status.MPI_SOURCE &&
            neighbor >= loan_star && neighbor <= loan_end) {
            MPI_Send(&st_pi->leader_id, 1, MPI_INT, neighbor, ST_LEADER, MPI_COMM_WORLD);
        }
    }
    /* Termination */
    MPI_Send(&st_pi->leader_id, 1, MPI_INT, 0, ST_TERMINATION, MPI_COMM_WORLD);
}

/* End of ST.c */