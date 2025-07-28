#include "Tree.h"
#include <stdio.h>
#include <stdlib.h>

// ------------------------------------------------------------------------------------------------------------------------------------------------------------ //
/* Struct Functions */

Tree_Node* create_node(int rank) {
    Tree_Node* node = (Tree_Node*)malloc(sizeof(Tree_Node));
    if (node == NULL) {
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); 
    }
    node->rank = rank;
    node->neighbors = NULL;
    node->next = NULL;
    return node;
}

Tree_Node* search_node(int rank) {
    Tree_Node* current = tree_head;
    while (current) {
        if (current->rank == rank) return current;
        current = current->next;
    }
    return NULL;
}

void add_node(Tree_Node* node) {
    if (tree_head == NULL) {
        tree_head = node;
    } else {
        Tree_Node* current = tree_head;
        while (current->next != NULL) current = current->next;
        current->next = node;
    }
}

void add_neighbor(Tree_Node* node, int neigh) {
    Neighbors* current_neigh = node->neighbors;
    while(current_neigh != NULL) {
        if (current_neigh->rank == neigh) {
            return;
        }
        current_neigh = current_neigh->next;
    }

    Neighbors* new_neigh = (Neighbors*)malloc(sizeof(Neighbors));
    if (new_neigh == NULL) {
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    new_neigh->rank = neigh;
    new_neigh->next = node->neighbors;
    node->neighbors = new_neigh;
}

void free_tree() {
    Tree_Node* current_node = tree_head;
    while (current_node) {
        Neighbors* current_neighbor_ptr = current_node->neighbors;
        while (current_neighbor_ptr) {
            Neighbors* temp_neighbor = current_neighbor_ptr;
            current_neighbor_ptr = current_neighbor_ptr->next;
            free(temp_neighbor);
        }
        Tree_Node* temp_node = current_node;
        current_node = current_node->next;
        free(temp_node);
    }
    tree_head = NULL;
}

void print_tree() {
    Tree_Node* current = tree_head;
    while (current) {
        printf("Node %d (Tree Structure): Neighbors: ", current->rank);
        Neighbors* neighbor = current->neighbors;
        if (neighbor == NULL) {
            printf("None");
        }
        while (neighbor) {
            printf("%d ", neighbor->rank);
            neighbor = neighbor->next;
        }
        printf("\n");
        current = current->next;
    }
    fflush(stdout);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------ //
/* MPI Handling */

void handle_connect(int rank, int neigh, MPI_Status status) {
    printf("%d (CO): Received CONNECT from %d, to connect with neighbor %d.\n", rank, status.MPI_SOURCE, neigh);

    /* Search if we have the node, else create it*/
    Tree_Node* node = search_node(rank);
    if (!node) {
        node = create_node(rank);
        add_node(node);
    }

    /* Add the neihbor */
    add_neighbor(node, neigh);
    /* Add also to loaner list */
    if (st_pi != NULL) { 
        if (st_pi->neighbors == NULL) {
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        if (!list_contains(st_pi->neighbors, neigh)) { 
            list_add(st_pi->neighbors, neigh);
        }
    }
    /* Add also to library list */
    if (pi != NULL) { 
         if (pi->neighbors == NULL) { 
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        if (!list_contains(pi->neighbors, neigh)) { 
            list_add(pi->neighbors, neigh);
        }
    }

    /* Send ACK to 0 */
    int ACK = 1;
    MPI_Send(&ACK, 1, MPI_INT, 0, 1, MPI_COMM_WORLD); 
    int num_procs; 
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    if (rank != neigh) { 
        if (neigh < 0 || neigh >= num_procs) {
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); 
        }

        /* Send to neigh */
        MPI_Send(&rank, 1, MPI_INT, neigh, NEIGHBOR, MPI_COMM_WORLD);
        
        /* Receive Ack */
        int recv_ACK;
        MPI_Recv(&recv_ACK, 1, MPI_INT, neigh, 2, MPI_COMM_WORLD, &status); 
    } 
    /* Send final ACK to 0*/
    MPI_Send(&ACK, 1, MPI_INT, 0, 3, MPI_COMM_WORLD);
}

void handle_neighbor(int rank, int neigh) {
    printf("%d (CO): Received NEIGHBOR from %d.\n", rank, neigh);

    /* Search if we have the node, else create it*/
    Tree_Node* node = search_node(rank);
    if (!node) {
        node = create_node(rank);
        add_node(node);
    }

    /* Add the neihbor */
    add_neighbor(node, neigh);

    /* Add also to loaner list */
    if (st_pi != NULL) {
        if (st_pi->neighbors == NULL) {
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        if (!list_contains(st_pi->neighbors, neigh)) { 
            list_add(st_pi->neighbors, neigh);
        }
    }
    /* Add also to library list */
    if (pi != NULL) {
        if (pi->neighbors == NULL) {
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        if (!list_contains(pi->neighbors, neigh)) { 
            list_add(pi->neighbors, neigh);
        }
    }

    /* Send ACK to neighbor */
    int ACK = 1;
    MPI_Send(&ACK, 1, MPI_INT, neigh, 2, MPI_COMM_WORLD); 
}

/* End of Tree.c */