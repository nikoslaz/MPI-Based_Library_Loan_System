#include "Book.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define MAX_BOOKS 1000
#define M 625

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------- //
/* Global */

Book library[MAX_BOOKS];
Book* book = NULL;
Book* library_books[M] = {NULL};
int num_books = 0;

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------- //
/* Struct functions */

void init_books(int rank, int N) {
    srand((unsigned int)time(NULL) + rank * 1000);
    int libraries = N * N;
    int total_books = N * N * N * N;

    /* Libraries */
    if (rank >= 1 && rank <= libraries) {
        int lib_id = rank - 1;
        int start_book_id = lib_id * N * N + 1;
        int end_book_id = (lib_id + 1) * N * N;
        for (int b_id = start_book_id; b_id <= end_book_id; b_id++) {
            for (int copy = 0; copy < N; copy++) {
                Book* new_book = (Book*)malloc(sizeof(Book));
                if (new_book == NULL) {
                    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); 
                }
                new_book->book_id = b_id;
                new_book->cost = (rand() % 96) + 5;
                new_book->num_copies_available = 1;
                new_book->times_loaned = 0;
                new_book->next = book;
                book = new_book;
            }
        }
    }
    /* Cordinator */
    if (rank == 0) {
        for (int b_id = 1; b_id <= total_books; b_id++) {
            int lib_id = calculate_library_id(b_id, N);
            Book* b = (Book*)malloc(sizeof(Book));
            if (b == NULL) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); 
            }
            b->book_id = b_id;
            b->cost = (rand() % 96) + 5;
            b->num_copies_available = N;
            b->times_loaned = 0;
            b->next = library_books[lib_id - 1];
            library_books[lib_id - 1] = b;
        }
    }
}

int find_book_in_library(int book_id) {
    Book* current = book;
    while (current != NULL) {
        if (current->book_id == book_id && current->num_copies_available > 0) {
            current->num_copies_available--;
            current->times_loaned++;
            return 1;
        }
        current = current->next;
    }
    return 0;
}

void update_book_stats(int book_id) {
    Book* current = book; 
    while (current != NULL) {
        if (current->book_id == book_id) {
            current->times_loaned++;
            break;
        }
        current = current->next;
    }
}

int calculate_library_id(int book_id, int N) {
    /* To map the lib id */
    int lib_id = ((book_id - 1) / (N * N)) % (N * N) + 1;
    return lib_id;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------- //
/* MPI Handling TakeBook */

void handle_take_book(int rank, int book_id, int N, MPI_Status status) {
    int library_id = calculate_library_id(book_id, N);
    MPI_Send(&book_id, 1, MPI_INT, library_id, LEND_BOOK, MPI_COMM_WORLD);
}

void handle_lend_book(int rank, int book_id, int N, MPI_Status status) { 
    printf("%d (BO): Received LEND_BOOK for book ID %d from %d\n", rank, book_id, status.MPI_SOURCE);
    int found = find_book_in_library(book_id);
    if (found) {
        MPI_Send(&book_id, 1, MPI_INT, status.MPI_SOURCE, GET_BOOK, MPI_COMM_WORLD);
    } else { 
        if (pi && pi->leader != NULL_ID) { 
            pi->loaner_id = status.MPI_SOURCE;
            pi->book_id   = book_id;
            /* I am NOT the DFS leader */
            if (pi->leader != rank) { 
                MPI_Send(&book_id, 1, MPI_INT, pi->leader, FIND_BOOK, MPI_COMM_WORLD);
            } else { /* I AM the DFS leader */
                int lib_id = calculate_library_id(book_id, N);

                if (lib_id == rank) {
                    int fail_ACK = -1;
                    MPI_Send(&fail_ACK, 1, MPI_INT, pi->loaner_id, GET_BOOK, MPI_COMM_WORLD);
                } else {
                    int message[3] = {pi->book_id, pi->loaner_id, rank};
                    MPI_Send(message, 3, MPI_INT, lib_id, BOOK_REQUEST, MPI_COMM_WORLD);
                }
                pi->loaner_id = NULL_ID;
                pi->book_id = NULL_ID;
            }
        } else {
            int fail_ACK = -1;
            MPI_Send(&fail_ACK, 1, MPI_INT, status.MPI_SOURCE, GET_BOOK, MPI_COMM_WORLD);
        }
    }
}

void update_record(int rank, int book_id_loaned) {
    Book* current = book;
    Book* found   = NULL;

    /* Check if book already in loaner's list */
    while (current != NULL) {
        if (current->book_id == book_id_loaned) {
            found = current;
            break;
        }
        current = current->next;
    }

    if (found != NULL) {
        found->times_loaned++;
    } else {
        /* New book */
        Book* new_book = (Book*)malloc(sizeof(Book));
        if (new_book == NULL) {
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            return;
        }
        new_book->book_id = book_id_loaned;
        new_book->cost = (rand() % 96) + 5;
        new_book->num_copies_available = 0;
        new_book->times_loaned = 1;
        new_book->next = book;     
        book = new_book;
    }
}


void handle_get_book(int rank, int book_id, MPI_Status status) {
    printf("%d (BO): Received GET_BOOK for book ID %d from library %d.\n", rank, book_id, status.MPI_SOURCE);
    update_record(rank, book_id);
    /*  Send DONE_FIND_BOOK to 0 */
    MPI_Send(&book_id, 1, MPI_INT, 0, DONE_FIND_BOOK, MPI_COMM_WORLD);
}

void handle_find_book(int rank, int book_id, int N, MPI_Status status) {
    int lib_id = calculate_library_id(book_id, N);
    int message[2] = {book_id, lib_id};
    printf("%d (BO): Received FIND_BOOK for book ID %d, replying with library ID %d\n", rank, book_id, lib_id);
    MPI_Send(message, 2, MPI_INT, status.MPI_SOURCE, FOUND_BOOK, MPI_COMM_WORLD);
}

void handle_book_request(int rank, int book_id, int c_id, int l_id, MPI_Status status) {
    printf("%d (BO): Received BOOK_REQUEST: book ID %d, client %d, from library %d\n", rank, book_id, c_id, l_id);
    int found = find_book_in_library(book_id);
    if (found) {
        MPI_Send(&book_id, 1, MPI_INT, l_id, ACK_TB, MPI_COMM_WORLD);
    }
}

void handle_ack_tb(int rank, int book_id, MPI_Status status) {
    printf("%d (BO): Received ACK_TB for book ID %d, updating statistics\n", rank, book_id);
    update_book_stats(book_id);
    MPI_Send(&book_id, 1, MPI_INT, 0, DONE_FIND_BOOK, MPI_COMM_WORLD);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- //
/* MPI Handling DonateBook */

void handle_donate_book(int rank, int book_id, int client_id, int num_copies, int N, MPI_Status status, int loa_flag) {
    /* Loaner */
    if (loa_flag && st_pi != NULL) {
        if (st_pi->leader_id != -1 && st_pi->leader_id == rank) {
            printf("%d (DO): ST Leader (rank %d) received DONATE_BOOK for book %d, client %d, %d copies\n",
                   rank, rank, book_id, client_id, num_copies);
            
            if (num_copies <= 0) {
                int fail_ACK = -1;
                int num_procs;
                MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
                if (client_id >= 0 && client_id < num_procs) {
                    MPI_Send(&fail_ACK, 1, MPI_INT, client_id, DONE_DONATE_BOOK, MPI_COMM_WORLD);
                }
                return;
            }

            int N2 = N * N;
            int num_procs;
            MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
            /* Checks */
            if (N2 <= 0 || (N2 +1 > num_procs && N2 > 0) || client_id < 0 || client_id >= num_procs) {
                int fail_ACK = -1;
                if (client_id >= 0 && client_id < num_procs) {
                    MPI_Send(&fail_ACK, 1, MPI_INT, client_id, DONE_DONATE_BOOK, MPI_COMM_WORLD);
                }
                return;
            }
            if (N2 == 0 && num_copies > 0) {
                int fail_ACK = -1;
                if (client_id >= 0 && client_id < num_procs) {
                    MPI_Send(&fail_ACK, 1, MPI_INT, client_id, DONE_DONATE_BOOK, MPI_COMM_WORLD);
                }
                return;
            }

            static int donate_next_val = 0;
            int i;

            /* Distribute copies */
            for (i = 0; i < num_copies; i++) {
                int library_id_target = (N2 > 0) ? ((donate_next_val + i) % N2) + 1 : -1;
                if (library_id_target == -1 || library_id_target >= num_procs || library_id_target <= 0) {
                    continue;
                }
                int message[2] = {book_id, 1};
                MPI_Send(message, 2, MPI_INT, library_id_target, DONATE_BOOK, MPI_COMM_WORLD);
            }

            for (i = 0; i < num_copies; i++) {
                int ack_book_id_recv;
                MPI_Status ack_status_recv;
                MPI_Recv(&ack_book_id_recv, 1, MPI_INT, MPI_ANY_SOURCE, ACK_DB, MPI_COMM_WORLD, &ack_status_recv);
            }
            if (N2 > 0) {
                donate_next_val = (donate_next_val + num_copies) % N2;
            }
           
            MPI_Send(&book_id, 1, MPI_INT, client_id, DONE_DONATE_BOOK, MPI_COMM_WORLD);

        } else if (st_pi->leader_id != -1 && st_pi->leader_id != rank) {
            printf("%d (DO): Non-leader loaner (rank %d), forwarding DONATE_BOOK for book %d (client %d, %d copies) to my component leader %d (loa_flag=1)\n",
                   rank, rank, book_id, client_id, num_copies, st_pi->leader_id);
            int message_to_fwd[3] = {book_id, client_id, num_copies};
            MPI_Send(message_to_fwd, 3, MPI_INT, st_pi->leader_id, DONATE_BOOK, MPI_COMM_WORLD);
        } else {
            int fail_ACK = -1;
            int num_procs;
            MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
            if (client_id >= 0 && client_id < num_procs) {
                MPI_Send(&fail_ACK, 1, MPI_INT, client_id, DONE_DONATE_BOOK, MPI_COMM_WORLD);
            }
        }
    } else { /* Library */
        printf("%d (DO): Library received DONATE_BOOK for book %d, (num_copies in msg: %d), adding 1 copy\n", rank, book_id, num_copies);
        Book* curr_book = book;
        int found = 0;
        /* Check for book*/
        while (curr_book != NULL) {
            if (curr_book->book_id == book_id) {
                curr_book->num_copies_available += 1;
                found = 1;
                break;
            }
            curr_book = curr_book->next;
        }
        /* Book was not found */
        if (!found) {
            Book* new_book = (Book*)malloc(sizeof(Book));
            if (new_book == NULL) {
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            } else {
                new_book->book_id = book_id;
                new_book->cost = (rand() % 96) + 5;
                new_book->num_copies_available = 1;
                new_book->times_loaned = 0;
                new_book->next = book; 
                book = new_book;      
            }
        }
        MPI_Send(&book_id, 1, MPI_INT, status.MPI_SOURCE, ACK_DB, MPI_COMM_WORLD);
    }
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- //
/* MPI Handling GetMostPopuralBook */

int calculate_loaner_l_id(int loaner_rank, int N) {
    if (N <= 0) return -1;
    int num_libs = N * N;
    if (loaner_rank <= num_libs) return -1;

    int loaners_offset = loaner_rank - (num_libs + 1);
    int loaners_per_group = N / 2;
    if (loaners_per_group == 0 && N == 1) loaners_per_group = 1;
    if (loaners_per_group == 0) return -1; 

    int library_index = loaners_offset / loaners_per_group; 
    return library_index + 1;
}

void find_popular_book(int rank, int N) {
    if (st_pi == NULL || book == NULL) {
        st_pi->popular_book_id = -1;
        st_pi->popular_times_loaned = 0;
        st_pi->popular_book_cost = 0;
        st_pi->popular_origin_l_id = -1;
        return;
    }

    Book* current = book;
    int max_book_id = -1;
    int max_loans = -1;
    int max_cost = -1;

    while (current != NULL) {
        /* Only consider books that were loaned  */
        if (current->times_loaned > 0) {
            if (current->times_loaned > max_loans) {
                max_loans = current->times_loaned;
                max_book_id = current->book_id;
                max_cost = current->cost;
            } else if (current->times_loaned == max_loans) {
                if (current->cost > max_cost) {
                    max_book_id = current->book_id;
                    max_cost = current->cost;
                }
            }
        }
        current = current->next;
    }

    st_pi->popular_book_id = max_book_id;
    st_pi->popular_times_loaned = (max_book_id != -1) ? max_loans : 0;
    st_pi->popular_book_cost = (max_book_id != -1) ? max_cost : 0;
    st_pi->popular_origin_l_id = calculate_loaner_l_id(rank, N);
}


void handle_get_most_popular(int st_leader_rank, int N, int total_num_loaners) {
    printf("%d (PO): Entering handle_get_most_popular\n", st_leader_rank);

    if (st_pi == NULL) {
        return;
    }

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); 

    /* Reset vars */ 
    st_pi->overall_best_book_id = -1;
    st_pi->overall_best_times_loaned = -1;
    st_pi->overall_best_book_cost = -1;
    st_pi->overall_best_l_id_group = -1;
    st_pi->reports_received_count = 0; 

    int num_libraries = N * N;
    int loan_start = num_libraries + 1;

    /* Send to all other loaners */
    for (int i = 0; i < total_num_loaners; ++i) {
        int loaner = loan_start + i;
        if (loaner != st_leader_rank) {
            MPI_Send(NULL, 0, MPI_INT, loaner, GET_MOST_POPULAR, MPI_COMM_WORLD);
        }
    }

    /* Find ST's book */
    find_popular_book(st_leader_rank, N);

    /* Aggregate */
    if (st_pi->popular_book_id != -1) { 
        st_aggregate(st_leader_rank, st_pi->popular_book_id, st_pi->popular_times_loaned, st_pi->popular_book_cost, st_pi->popular_origin_l_id);
    }
    st_pi->reports_received_count++; 


    /* Check if all reports are in */
    if (st_pi->reports_received_count == total_num_loaners) {
        st_leader_popularity_contest(st_leader_rank, total_num_loaners);
    }
}

void loaner_get_info(int rank, int N, int st_leader) {
    if (st_pi == NULL || st_leader == -1) {
        return;
    }

    find_popular_book(rank, N);

    /* Books loaned by this loaner */
    if (st_pi->popular_book_id != -1) {
        int message[4] = {
            st_pi->popular_book_id,
            st_pi->popular_times_loaned,
            st_pi->popular_book_cost,
            st_pi->popular_origin_l_id
        };
        MPI_Send(message, 4, MPI_INT, st_leader, GET_POPULAR_INFO, MPI_COMM_WORLD);
    } else { 
        int message[4] = {-1, 0, 0, calculate_loaner_l_id(rank, N)};
        MPI_Send(message, 4, MPI_INT, st_leader, GET_POPULAR_INFO, MPI_COMM_WORLD);
    }
}

void st_aggregate(int st_leader_rank, int b_id, int times_loaned, int cost, int l_id_group) {
    if (st_pi == NULL || st_pi->known_overall_st_leader_rank != st_leader_rank ||
        (b_id == -1 && times_loaned == 0)) {
        return;
    }

    /* Find the overall best book */
    if (times_loaned > st_pi->overall_best_times_loaned) {
        st_pi->overall_best_times_loaned = times_loaned;
        st_pi->overall_best_book_id = b_id;
        st_pi->overall_best_book_cost = cost;
        st_pi->overall_best_l_id_group = l_id_group;
    } else if (times_loaned == st_pi->overall_best_times_loaned) {
        if (st_pi->overall_best_book_id == -1 || cost > st_pi->overall_best_book_cost) { 
            st_pi->overall_best_book_id = b_id;
            st_pi->overall_best_book_cost = cost;
            st_pi->overall_best_l_id_group = l_id_group;
        }
    }
}

void st_leader_popularity_contest(int rank, int total_loaners_expected_to_report) {
    if (st_pi == NULL || !st_pi->is_leader) return;

    /* Send ACK_BK_INFO */
    for (int i = 0; i < st_pi->children_in_st_tree->size; ++i) {
       MPI_Send(NULL, 0, MPI_INT, st_pi->children_in_st_tree->data[i], ACK_BK_INFO, MPI_COMM_WORLD);
    }
    /* Send GET_MOST_POPULAR_DONE to coordinator */
    MPI_Send(NULL, 0, MPI_INT, 0, GET_MOST_POPULAR_DONE, MPI_COMM_WORLD);
}