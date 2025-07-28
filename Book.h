#ifndef BOOK_H
#define BOOK_H

#include <mpi.h>
#include "DFS.h"
#include "ST.h"

/* MPI Messages */
#define TAKE_BOOK              50
#define LEND_BOOK              51
#define GET_BOOK               52
#define FIND_BOOK              53
#define BOOK_REQUEST           54
#define ACK_TB                 55
#define DONE_FIND_BOOK         56
#define FOUND_BOOK             57
#define DONATE_BOOK            60   
#define ACK_DB                 61  
#define DONE_DONATE_BOOK       62
#define GET_MOST_POPULAR       70
#define GET_POPULAR_INFO       71
#define ACK_BK_INFO            72
#define GET_MOST_POPULAR_DONE  73
#define CHECK_NUM_BOOKS_LOAN   80
#define NUM_BOOKS_LOANED       81
#define ACK_NBL                82
#define CHECK_NUM_BOOKS_DONE   83

/* N^4 */
#define M 625

typedef struct Book {
    int book_id;
    int cost;
    int num_copies_available;
    int times_loaned;
    struct Book* next;
} Book;

/* Struct head */
extern Book* book;
extern Book* library_books[M];

/* Struct functions */
void init_books(int rank, int N);
void update_book_stats(int book_id);
int find_book_in_library(int book_id);
int calculate_library_id(int book_id, int N);

/* MPI Handling TakeBook */
void handle_take_book(int rank, int book_id, int N, MPI_Status status);
void handle_lend_book(int rank, int book_id, int N_param, MPI_Status status); 
void handle_get_book(int rank, int book_id, MPI_Status status);
void handle_find_book(int rank, int book_id, int N, MPI_Status status);
void handle_book_request(int rank, int book_id, int c_id, int l_id, MPI_Status status);
void handle_ack_tb(int rank, int book_id, MPI_Status status);

/* MPI Handling DonateBook */
void handle_donate_book(int rank, int book_id, int client_id, int num_copies, int N, MPI_Status status, int loa_flag);

/* MPI Handling GetMostPopuralBook */
void handle_get_most_popular(int st_leader_rank, int N_param, int total_num_loaners);
void loaner_get_info(int rank, int N_param, int parent_rank_in_st);
void st_aggregate(int rank, int b_id, int times_loaned, int cost, int l_id_group);
void st_leader_popularity_contest(int rank, int total_loaners_expected_to_report);

#endif /* End of Book.h */