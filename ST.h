#ifndef ST_H
#define ST_H

#include "List.h"
#include <mpi.h>

/* MPI Messages */
#define NULL_ID                -1
#define START_LE_LOANERS       40
#define ST_ELECT               41
#define ST_LEADER              42
#define ST_LEADER_ANNOUNCEMENT 43
#define ST_TERMINATION         49

typedef struct ST_Node {
    int id;
    List* neighbors;
    List* elect_received;
    int elect_sent;
    int is_leader;
    int st_active;
    int elect_count;
    int neighbor_count;
    int leader_id;
    int max_id;
    int known_overall_st_leader_rank;

    /* For GetMostPopularBook */
    int parent_in_st_tree;
    List* children_in_st_tree;
    int reports_received_count;
    int popular_book_id;
    int popular_times_loaned;
    int popular_book_cost;
    int popular_origin_l_id;  
    int overall_best_book_id;
    int overall_best_times_loaned;
    int overall_best_book_cost;
    int overall_best_l_id_group;
    int total_loans_as_loaner_from_subtree; 
    int expected_loan_reports_from_children;
    int received_loan_reports_from_children; 
} ST_Node;

/* Struct head */
extern ST_Node* st_pi;

/* Struct Funtion */
void init_ST(ST_Node* node, int rank);
void free_ST(ST_Node* node);

/* MPI Handling */
void handle_start_loan(int rank, int N);
void handle_st_elect(int rank, int sender_id_of_elect, MPI_Status status, int N_val);
void handle_leader(int rank, int leader_id, MPI_Status status, int N_val);

#endif /* End of ST.h */