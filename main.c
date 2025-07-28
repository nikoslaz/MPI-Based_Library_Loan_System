#include "Tree.h"
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include "Book.h"
#include <math.h>   

#define TERMINATE_ALL 99

/* Struct heads */
Tree_Node* tree_head = NULL;
DFS_Node* pi = NULL;       
ST_Node* st_pi = NULL;    

int main(int argc, char** argv) {
    int num_procs;
    int rank;
    int N;
    char* file_name;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (argc < 3) {
        if (rank == 0) {
            fprintf(stderr, "[Rank %d] Usage: %s <N_dimension> <file_name> \n", rank, argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    N = atoi(argv[1]);
    file_name = argv[2];

    if (N <= 0) {
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    int dfs_leader = -1; /* Elected DFS leader */
    int st_leader  = -1; /* Elected ST leader */
    int libraries = N * N;
    int loaners = floor(pow(N, 3) / 2.0);
    int lib_flag = 0; /* Libary */
    int loa_flag = 0; /* Loaner */
    
    /* Rank ranges for libraries and loaners */
    int library_rank_start = 1;
    int library_rank_end = libraries;
    int loan_rank_start = libraries + 1; /* Loaners start after libraries (rank 1 to N*N) */
    int loan_rank_end = libraries + loaners;

    /* Initialize books for all */
    init_books(rank, N);

    /* Initialize DFS_Node for library processes (ranks 1 to N*N) */
    if (rank >= library_rank_start && rank <= library_rank_end) {
        lib_flag = 1;
        pi = (DFS_Node*)malloc(sizeof(DFS_Node));
        if (pi == NULL) {
            MPI_Finalize();
            exit(EXIT_FAILURE);
        }
        init_DFS(pi, rank);
    }

    /* Initialize ST_Node for loaner processes (ranks N*N + 1 to N*N + N^3/2) */
    if (rank >= loan_rank_start && rank <= loan_rank_end) {
        loa_flag = 1; 
        st_pi = (ST_Node*)malloc(sizeof(ST_Node));
        if (st_pi == NULL) {
            MPI_Finalize(); 
            exit(EXIT_FAILURE);
        }
        init_ST(st_pi, rank);
    }

    /* Coordinator */
    if (rank == 0) {
        FILE *file = fopen(file_name, "r");
        if (!file) {
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        MPI_Status status;
        char line[128];
        MPI_Status status_ack_coord;
        /* Read events from the test file line by line */
        while (fgets(line, sizeof(line), file)) {
            /* Trim newline character from the line */
            char* nl = strchr(line, '\n');
            if (nl) *nl = '\0';

            /* CONNECT Event */
            if (strstr(line, "CONNECT") != NULL) {
                int client1, client2;
                if (sscanf(line, "CONNECT %d %d", &client1, &client2) == 2) {
                    if (client1 < 0 || client1 >= num_procs ||
                        client2 < 0 || client2 >= num_procs) {
                        continue;
                    }

                    /* Send Connect message */
                    MPI_Send(&client2, 1, MPI_INT, client1, CONNECT, MPI_COMM_WORLD);
                    int ACK; 
                    /* Wait for 1st ACK */
                    MPI_Recv(&ACK, 1, MPI_INT, client1, 1, MPI_COMM_WORLD, &status_ack_coord); 
                    /* Wait for final ACK */
                    MPI_Recv(&ACK, 1, MPI_INT, client1, 3, MPI_COMM_WORLD, &status_ack_coord);    
                }
            }
            /* START_LE_LIBR Event */
            else if (strstr(line, "START_LE_LIBR") != NULL) {
                /* Send START_LE_LIBR message */
                for (int i = library_rank_start; i <= library_rank_end; i++) {
                    MPI_Send(NULL, 0, MPI_INT, i, START_LE_LIBR, MPI_COMM_WORLD);
                }

                int leader_id;
                MPI_Status dfs_status;
                MPI_Recv(&leader_id, 1, MPI_INT, MPI_ANY_SOURCE, DFS_TERMINATE, MPI_COMM_WORLD, &dfs_status);
                dfs_leader = leader_id; 

                /* Broadcast the DFS_LEADER */
                if (dfs_leader != -1) {
                    for (int i = library_rank_start; i <= library_rank_end; i++) {
                        MPI_Send(&dfs_leader, 1, MPI_INT, i, DFS_LEADER, MPI_COMM_WORLD);
                    }
                }
            }
            /* START_LE_LOANERS Event */
            else if (strstr(line, "START_LE_LOANERS") != NULL) {
                /* Send the START_LE_LIBR */
                for (int i = loan_rank_start; i <= loan_rank_end; i++) {
                    MPI_Send(NULL, 0, MPI_INT, i, START_LE_LOANERS, MPI_COMM_WORLD);  
                }
                
                int ACKS_recv = 0;
                int leader_id = -1;
                int expected_acks = loaners;
                MPI_Status st_status;  
                
                while (ACKS_recv < expected_acks) {
                    int reported_leader_id;
                    MPI_Recv(&reported_leader_id, 1, MPI_INT, MPI_ANY_SOURCE, ST_TERMINATION, MPI_COMM_WORLD, &st_status);
                    ACKS_recv++;
                    if (reported_leader_id > leader_id) {
                        leader_id = reported_leader_id;
                    }
                }
                st_leader = leader_id; 

                /* Broadcast the STLEADER */
                if (st_leader != -1) {
                    for (int i = loan_rank_start; i <= loan_rank_end; i++) {
                        MPI_Send(&st_leader, 1, MPI_INT, i, ST_LEADER_ANNOUNCEMENT, MPI_COMM_WORLD);
                    }
                }
            }
            /* TAKE_BOOK Event */
            else if (strstr(line, "TAKE_BOOK") != NULL) {
                int client_id, book_id;
                if (sscanf(line, "TAKE_BOOK %d %d", &client_id, &book_id) == 2) {
                    if (client_id < 0 || client_id >= num_procs) { 
                        continue;
                    }
                    MPI_Send(&book_id, 1, MPI_INT, client_id, TAKE_BOOK, MPI_COMM_WORLD);
                    int recv_buff_ack[1]; 
                    MPI_Recv(recv_buff_ack, 1, MPI_INT, client_id, DONE_FIND_BOOK, MPI_COMM_WORLD, &status);
                }
            }
            /* DONATE_BOOK Event */
            else if (strstr(line, "DONATE_BOOK") != NULL) {
                int client_id, book_id, num_copies;
                if (sscanf(line, "DONATE_BOOK %d %d %d", &client_id, &book_id, &num_copies) == 3) {
                    if (client_id < 0 || client_id >= num_procs) { 
                        continue;
                    }
                    int message[3] = {book_id, client_id, num_copies}; 
                    MPI_Send(message, 3, MPI_INT, client_id, DONATE_BOOK, MPI_COMM_WORLD);
                    int recv_buff_ack[1];
                    MPI_Recv(recv_buff_ack, 1, MPI_INT, client_id, DONE_DONATE_BOOK, MPI_COMM_WORLD, &status);
                }
            }
            /* GET_MOST_POPULAR_BOOK Event */
            else if (strstr(line, "GET_MOST_POPULAR_BOOK") != NULL) {
                if (st_leader != -1) { 
                    MPI_Send(NULL, 0, MPI_INT, st_leader, GET_MOST_POPULAR, MPI_COMM_WORLD);
                    int ACK; 
                    MPI_Recv(&ACK, 0, MPI_INT, st_leader, GET_MOST_POPULAR_DONE, MPI_COMM_WORLD, &status);
                }
            }
            /* CHECK_NUM_BOOKS_LOANED Event */
            else if (strstr(line, "CHECK_NUM_BOOKS_LOANED") != NULL) {
                /* Collect library loans */
                if (dfs_leader != -1) {
                    MPI_Send(NULL, 0, MPI_INT, dfs_leader, CHECK_NUM_BOOKS_LOAN, MPI_COMM_WORLD);
                }

                /* Collect loaner loans */
                if (st_leader != -1) {
                    MPI_Send(NULL, 0, MPI_INT, st_leader, CHECK_NUM_BOOKS_LOAN, MPI_COMM_WORLD);
                }

                /* Wait for CHECK_NUM_BOOKS_DONE from both leaders */
                int done_signals_received = 0;
                if (dfs_leader != -1 && st_leader != -1) {
                    while(done_signals_received < 2) { 
                        MPI_Status done_status;
                        int ACK[1]; 
                        MPI_Recv(ACK, 1, MPI_INT, MPI_ANY_SOURCE, CHECK_NUM_BOOKS_DONE, MPI_COMM_WORLD, &done_status);
                        done_signals_received++;
                    }
                }
            }
            /* End of events! */
        }
        fclose(file);
        print_tree();

        for (int i = 1; i < num_procs; i++) {
            MPI_Send(NULL, 0, MPI_CHAR, i, TERMINATE_ALL, MPI_COMM_WORLD);
        }
    }
    /* Non-Coordinators */
    else {
        MPI_Status status;
        int recv_buff[5]; 
        int count;        
        int processed_message; 

        while (1) {
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_INT, &count); 

            processed_message = 0; 

            /* TERMINATE_ALL */
            if (status.MPI_TAG == TERMINATE_ALL) {
                MPI_Recv(NULL, 0, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
                processed_message = 1;
                break; 
            }
            /* START_LE_LIBR */
            else if (lib_flag && status.MPI_TAG == START_LE_LIBR) {
                MPI_Recv(NULL, 0, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
                handle_start_le_libr(rank, N); 
                processed_message = 1;
            }
            /* START_LE_LOANERS */
            else if (loa_flag && status.MPI_TAG == START_LE_LOANERS) {
                MPI_Recv(NULL, 0, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
                handle_start_loan(rank, N); 
                processed_message = 1;
            }
            /* Messages with payload */
            else if (count >= 0 && count <= 5) { 
                if (count > 0) { 
                    MPI_Recv(recv_buff, count, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
                } else { 
                    MPI_Recv(NULL, 0, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
                }
                processed_message = 1; 

                // ---------------------------------------------------------------------------------------- //
                // CONNECT and NEIGHBOR handlers
                if (status.MPI_TAG == CONNECT) {
                    if (count == 1) {
                        int neighbor_to_connect = recv_buff[0];
                        if (lib_flag) { 
                            if (pi != NULL && pi->neighbors != NULL) {
                                if (!list_contains(pi->neighbors, neighbor_to_connect)) {
                                    list_add(pi->neighbors, neighbor_to_connect);
                                }
                            }
                            int ACK_to_coord = 1;
                            MPI_Send(&ACK_to_coord, 1, MPI_INT, 0, 1, MPI_COMM_WORLD); 
                            MPI_Send(&rank, 1, MPI_INT, neighbor_to_connect, NEIGHBOR, MPI_COMM_WORLD);
                            MPI_Recv(&ACK_to_coord, 1, MPI_INT, neighbor_to_connect, 2, MPI_COMM_WORLD, &status);
                            MPI_Send(&ACK_to_coord, 1, MPI_INT, 0, 3, MPI_COMM_WORLD); 

                        } else if (loa_flag) { 
                            handle_connect(rank, neighbor_to_connect, status); 
                        }
                    }
                }
                else if (status.MPI_TAG == NEIGHBOR) { 
                    if (count == 1) {
                        int neighbor_confirming = recv_buff[0];
                        if (lib_flag) { 
                            if (pi != NULL && pi->neighbors != NULL) {
                                if (!list_contains(pi->neighbors, status.MPI_SOURCE)) { 
                                    list_add(pi->neighbors, status.MPI_SOURCE);
                                }
                            }
                            int ACK_to_neighbor = 1;
                            MPI_Send(&ACK_to_neighbor, 1, MPI_INT, status.MPI_SOURCE, 2, MPI_COMM_WORLD);
                        } else if (loa_flag) { 
                            handle_neighbor(rank, neighbor_confirming); 
                        }
                    }
                }
                // ---------------------------------------------------------------------------------------- //
                // DFS Leader Election handlers (Libraries only)
                else if (status.MPI_TAG == DFS_LEADER && lib_flag == 1) {
                    if (count == 1) { 
                        pi->leader = recv_buff[0]; 
                        pi->dfs_active = 0; 
                        pi->is_terminated = 1; 
                    } else if (count == 2) { 
                        handle_dfs_leader(rank, recv_buff[0], recv_buff[1], status, N); 
                    }
                }
                else if (status.MPI_TAG == DFS_ALREADY && lib_flag == 1) {
                    if (count == 2) handle_dfs_already(rank, recv_buff[0], recv_buff[1], status);
                }
                else if (status.MPI_TAG == DFS_PARENT && lib_flag == 1) {
                    if (count == 2) handle_dfs_parent(rank, recv_buff[0], recv_buff[1], status);
                }
                // ---------------------------------------------------------------------------------------- //
                // ST Leader Election handlers (Loaners only)
                else if (status.MPI_TAG == ST_ELECT && loa_flag == 1) {
                    if (count == 1) handle_st_elect(rank, recv_buff[0], status, N); 
                }
                else if (status.MPI_TAG == ST_LEADER && loa_flag == 1) {
                    if (count == 1) handle_leader(rank, recv_buff[0], status, N); 
                }
                // ST_LEADER_ANNOUNCEMENT: Broadcast of overall ST Leader (Loaners only)
                else if (status.MPI_TAG == ST_LEADER_ANNOUNCEMENT && loa_flag) {
                    if (count == 1) { 
                        if (st_pi != NULL) {
                            st_pi->known_overall_st_leader_rank = recv_buff[0];
                        }
                    }
                }
                // ---------------------------------------------------------------------------------------- //
                /* TAKE_BOOK */
                else if (status.MPI_TAG == TAKE_BOOK) {
                    if (loa_flag) { 
                        if (count == 1) handle_take_book(rank, recv_buff[0], N, status); 
                        else {
                            if (status.MPI_SOURCE == 0) {
                                int failure_ack = -1; 
                                MPI_Send(&failure_ack, 1, MPI_INT, 0, DONE_FIND_BOOK, MPI_COMM_WORLD);
                            }
                        }
                    } else if (lib_flag) { 
                        if (status.MPI_SOURCE == 0) { 
                            int failure_response = -1; 
                            MPI_Send(&failure_response, 1, MPI_INT, 0, DONE_FIND_BOOK, MPI_COMM_WORLD);
                        }
                    }
                }
                /* LEND_BOOK */
                else if (lib_flag && status.MPI_TAG == LEND_BOOK) {
                    if (count == 1) handle_lend_book(rank, recv_buff[0], N, status); 
                }
                /* GET_BOOK */
                else if (loa_flag && status.MPI_TAG == GET_BOOK) {
                    if (count == 1) handle_get_book(rank, recv_buff[0], status);
                }
                /* FIND_BOOK */
                else if (lib_flag && status.MPI_TAG == FIND_BOOK) {
                    if (count == 1) handle_find_book(rank, recv_buff[0], N, status); 
                }
                /* FOUND_BOOK */
                else if (status.MPI_TAG == FOUND_BOOK && lib_flag == 1) { 
                    if (count == 2) { 
                        int book_id_from_found = recv_buff[0];
                        int lib_id_has_book_catalog_entry = recv_buff[1]; 
                        
                        int original_loaner_id = pi->loaner_id;
                        int original_book_id = pi->book_id;

                        if (original_loaner_id == NULL_ID || original_book_id != book_id_from_found) {
                            fprintf(stderr, "[Rank %d] (LIB_FOUND_BOOK): Error - Received FOUND_BOOK (%d) but no matching pending request or book ID mismatch. Pending L: %d, B: %d. Informing original loaner of failure if known.\n", 
                                   rank, book_id_from_found, original_loaner_id, original_book_id);
                            if (original_loaner_id != NULL_ID) { 
                                int failure_response = -1; 
                                MPI_Send(&failure_response, 1, MPI_INT, original_loaner_id, GET_BOOK, MPI_COMM_WORLD);
                            }
                            pi->loaner_id = NULL_ID; 
                            pi->book_id = NULL_ID;
                        }
                        else if (lib_id_has_book_catalog_entry == rank) { 
                            int found_now_for_lending = find_book_in_library(book_id_from_found);
                            if (found_now_for_lending) {
                                MPI_Send(&book_id_from_found, 1, MPI_INT, original_loaner_id, GET_BOOK, MPI_COMM_WORLD);
                            } else {
                                fprintf(stderr, "[Rank %d] (LIB_FOUND_BOOK): Book %d cataloged here, but became unavailable upon lending attempt. Sending failure GET_BOOK to loaner %d.\n", rank, book_id_from_found, original_loaner_id);
                                int failure_response = -1;
                                MPI_Send(&failure_response, 1, MPI_INT, original_loaner_id, GET_BOOK, MPI_COMM_WORLD);
                            }
                            pi->loaner_id = NULL_ID; 
                            pi->book_id = NULL_ID;
                        }
                        else { 
                            int message_to_lib_y[3] = {original_book_id, original_loaner_id, rank};
                            MPI_Send(message_to_lib_y, 3, MPI_INT, lib_id_has_book_catalog_entry, BOOK_REQUEST, MPI_COMM_WORLD);
                            
                            pi->loaner_id = NULL_ID; 
                            pi->book_id = NULL_ID;
                        }
                    }
                }
                /* BOOK_REQUEST */
                else if (status.MPI_TAG == BOOK_REQUEST) {
                    if (lib_flag && count == 3) { 
                        int book_to_lend = recv_buff[0];
                        int original_loaner_target = recv_buff[1]; 

                        int found_in_my_lib = find_book_in_library(book_to_lend); 
                        if (found_in_my_lib) {
                            MPI_Send(&book_to_lend, 1, MPI_INT, original_loaner_target, GET_BOOK, MPI_COMM_WORLD);
                        } else {
                            fprintf(stderr, "[Rank %d] (LIB_BOOK_REQUEST): Did NOT find book %d available. Sending failure GET_BOOK to original loaner %d.\n", rank, book_to_lend, original_loaner_target);
                            int failure_book_id = -1; 
                            MPI_Send(&failure_book_id, 1, MPI_INT, original_loaner_target, GET_BOOK, MPI_COMM_WORLD);
                        }
                    } else if (loa_flag) { 
                        fprintf(stderr, "[Rank %d] (LOANER): Warning - Received BOOK_REQUEST. This is unexpected for a loaner.\n", rank); 
                    }
                }
                /* ACK_TB */
                else if (status.MPI_TAG == ACK_TB && lib_flag == 1) { 
                    fprintf(stderr, "[Rank %d] (PROC_LOOP): Handling ACK_TB from %d. (This should not happen with current BOOK_REQUEST logic).\n", rank, status.MPI_SOURCE); 
                    if (count == 1) handle_ack_tb(rank, recv_buff[0], status);
                }
                // ---------------------------------------------------------------------------------------- //
                /* DONATE_BOOK */
                else if (status.MPI_TAG == DONATE_BOOK) {
                    if (loa_flag) { 
                        if (count == 3) {
                            int book_id    = recv_buff[0];
                            int client_id  = recv_buff[1];
                            int num_copies = recv_buff[2];
                            handle_donate_book(rank, book_id, client_id, num_copies, N, status, loa_flag); 
                        }
                    } else if (lib_flag) { 
                        if (status.MPI_SOURCE == 0 && count == 3) { 
                            fprintf(stderr, "[Rank %d] (LIB): Warning - Received initial DONATE_BOOK trigger from Coordinator. This role is for loaners. Sending failure ack.\n", rank); 
                            int failure_response = -1;
                            MPI_Send(&failure_response, 1, MPI_INT, 0, DONE_DONATE_BOOK, MPI_COMM_WORLD);
                        } else if (count == 2) { 
                            handle_donate_book(rank, recv_buff[0], rank, recv_buff[1], N, status, loa_flag); 
                        }
                    }
                }
                /* DONE_DONATE_BOOK */
                else if (status.MPI_TAG == DONE_DONATE_BOOK) {
                    if (loa_flag) { 
                        if (count == 1) {
                            MPI_Send(recv_buff, 1, MPI_INT, 0, DONE_DONATE_BOOK, MPI_COMM_WORLD);
                        }
                    }
                }
                /* ACK_DB */
                else if (status.MPI_TAG == ACK_DB) {
                    if (loa_flag && st_pi != NULL && st_pi->leader_id == rank) { 
                        if (count == 1) { 
                        }
                    }
                }
                // ---------------------------------------------------------------------------------------- //
                /* GET_MOST_POPULAR */
                else if (status.MPI_TAG == GET_MOST_POPULAR) {
                    if (loa_flag) { 
                        if (st_pi != NULL) {
                            if (rank == st_pi->known_overall_st_leader_rank && status.MPI_SOURCE == 0) {
                                int total_loaners_val = floor(pow(N, 3) / 2.0); 
                                handle_get_most_popular(rank, N, total_loaners_val); 
                            }
                            else if (rank != st_pi->known_overall_st_leader_rank && status.MPI_SOURCE == st_pi->known_overall_st_leader_rank) {
                                loaner_get_info(rank, N, st_pi->known_overall_st_leader_rank); 
                            }
                        }
                    }
                }
                /* GET_POPULAR_INFO */
                else if (status.MPI_TAG == GET_POPULAR_INFO && loa_flag) {
                    if (st_pi != NULL && rank == st_pi->known_overall_st_leader_rank) { 
                        if (count == 4) {
                            st_aggregate(rank, recv_buff[0], recv_buff[1], recv_buff[2], recv_buff[3]);
                            st_pi->reports_received_count++; 

                            int total_loaners_val = floor(pow(N, 3) / 2.0);
                            if (st_pi->reports_received_count == total_loaners_val) {
                                st_leader_popularity_contest(rank, total_loaners_val);
                            }
                        }
                    }
                }
                /* ACK_BK_INFO */
                else if (status.MPI_TAG == ACK_BK_INFO && loa_flag) {
                    printf("[Rank %d] ACK_BK_INFO\n", rank);
                }
                // ---------------------------------------------------------------------------------------- //
                /* CHECK_NUM_BOOKS_LOAN */
                else if (status.MPI_TAG == CHECK_NUM_BOOKS_LOAN) {
                    if (lib_flag) { 
                        if (rank == pi->leader && status.MPI_SOURCE == 0) { 
                            pi->total_loans_reported_by_libraries = 0;
                            pi->libraries_reported_loan_count = 0;
                            if (libraries > 0) {
                                MPI_Send(NULL, 0, MPI_INT, library_rank_start, CHECK_NUM_BOOKS_LOAN, MPI_COMM_WORLD);
                            } else { 
                                int zero_loans = 0;
                                MPI_Send(&zero_loans, 1, MPI_INT, 0, CHECK_NUM_BOOKS_DONE, MPI_COMM_WORLD);
                            }
                        } else { 
                            int my_total_loans_lib = 0;
                            Book* current_lib_book = book; 
                            while(current_lib_book != NULL) {
                                my_total_loans_lib += current_lib_book->times_loaned;
                                current_lib_book = current_lib_book->next;
                            }

                            MPI_Send(&my_total_loans_lib, 1, MPI_INT, pi->leader, NUM_BOOKS_LOANED, MPI_COMM_WORLD);
                            
                            int current_lib_linear_idx = rank - 1; 
                            int next_lib_linear_idx = -1;

                            if ((current_lib_linear_idx / N) % 2 == 0) { 
                                if ((current_lib_linear_idx % N) < N - 1) { 
                                    next_lib_linear_idx = current_lib_linear_idx + 1;
                                } else if ((current_lib_linear_idx / N) < N - 1) { 
                                    next_lib_linear_idx = current_lib_linear_idx + N;
                                }
                            } else { 
                                if ((current_lib_linear_idx % N) > 0) { 
                                    next_lib_linear_idx = current_lib_linear_idx - 1;
                                } else if ((current_lib_linear_idx / N) < N - 1) { 
                                    next_lib_linear_idx = current_lib_linear_idx + N;
                                }
                            }
                            
                            if (next_lib_linear_idx != -1 && next_lib_linear_idx < libraries) {
                                int next_lib_rank = next_lib_linear_idx + 1; 
                                MPI_Send(NULL, 0, MPI_INT, next_lib_rank, CHECK_NUM_BOOKS_LOAN, MPI_COMM_WORLD);
                            }
                        }
                    } else if (loa_flag) { 
                        if (rank == st_pi->known_overall_st_leader_rank && status.MPI_SOURCE == 0) { 
                            st_pi->total_loans_as_loaner_from_subtree = 0; 
                            st_pi->reports_received_count = 0; 

                            int total_loaners_val = floor(pow(N, 3) / 2.0);
                            
                            int my_total_loans_as_loaner = 0;
                            Book* current_loaner_book = book; 
                            while(current_loaner_book != NULL) {
                                my_total_loans_as_loaner += current_loaner_book->times_loaned;
                                current_loaner_book = current_loaner_book->next;
                            }
                            st_pi->total_loans_as_loaner_from_subtree += my_total_loans_as_loaner; 
                            st_pi->reports_received_count++; 

                            for (int i = 0; i < total_loaners_val; ++i) {
                                int loaner_to_trigger = loan_rank_start + i;
                                if (loaner_to_trigger != rank) { 
                                    MPI_Send(NULL, 0, MPI_INT, loaner_to_trigger, CHECK_NUM_BOOKS_LOAN, MPI_COMM_WORLD);
                                }
                            }
                            if (st_pi->reports_received_count == total_loaners_val) {
                                MPI_Send(&(st_pi->total_loans_as_loaner_from_subtree), 1, MPI_INT, 0, CHECK_NUM_BOOKS_DONE, MPI_COMM_WORLD);
                            }

                        } else if (status.MPI_SOURCE == st_pi->known_overall_st_leader_rank) { 
                            int my_total_loans_as_loaner = 0;
                            Book* current_loaner_book = book;
                            while(current_loaner_book != NULL) {
                                my_total_loans_as_loaner += current_loaner_book->times_loaned;
                                current_loaner_book = current_loaner_book->next;
                            }
                            MPI_Send(&my_total_loans_as_loaner, 1, MPI_INT, st_pi->known_overall_st_leader_rank, NUM_BOOKS_LOANED, MPI_COMM_WORLD);
                        }
                    }
                }
                /* NUM_BOOKS_LOANED */
                else if (status.MPI_TAG == NUM_BOOKS_LOANED) {
                    int num_reported_loans = recv_buff[0];
                    if (lib_flag && rank == pi->leader) { 
                        pi->total_loans_reported_by_libraries += num_reported_loans;
                        pi->libraries_reported_loan_count++;
                        
                        MPI_Send(NULL, 0, MPI_INT, status.MPI_SOURCE, ACK_NBL, MPI_COMM_WORLD);

                        if (pi->libraries_reported_loan_count == libraries) {
                            MPI_Send(&(pi->total_loans_reported_by_libraries), 1, MPI_INT, 0, CHECK_NUM_BOOKS_DONE, MPI_COMM_WORLD);
                        }
                    } else if (loa_flag && rank == st_pi->known_overall_st_leader_rank) { 
                        st_pi->total_loans_as_loaner_from_subtree += num_reported_loans;
                        st_pi->reports_received_count++; 

                        int total_loaners_val = floor(pow(N,3)/2.0);
                        if (st_pi->reports_received_count == total_loaners_val) {
                            MPI_Send(&(st_pi->total_loans_as_loaner_from_subtree), 1, MPI_INT, 0, CHECK_NUM_BOOKS_DONE, MPI_COMM_WORLD);
                        }
                    }
                }
                else if (status.MPI_TAG == ACK_NBL && lib_flag) {
                    printf("[Rank %d]: Received ACK_NBL\n", rank);
                }
            }
            else {
                processed_message = 1; 
            }

            if (processed_message && pi != NULL && pi->dfs_active &&
                (status.MPI_TAG == START_LE_LIBR || status.MPI_TAG == DFS_LEADER ||
                 status.MPI_TAG == DFS_ALREADY || status.MPI_TAG == DFS_PARENT)) {
                check_DFS_termination(rank);
            }
        } 
    } 

    /* Cleanup Memory */
    if (pi != NULL) {
        free_DFS(pi);
        free(pi);
        pi = NULL; 
    }
    if (st_pi != NULL) {
        free_ST(st_pi);
        free(st_pi);
        st_pi = NULL;
    }
    if (tree_head != NULL) {
        free_tree(tree_head);
        tree_head = NULL;
    }
    /* Free book list for libraries/loaners */
    if (book != NULL && (lib_flag || loa_flag)) {
        Book* current_book = book;
        while(current_book != NULL) {
            Book* temp = current_book;
            current_book = current_book->next;
            free(temp);
        }
        book = NULL;
    }
    /*  Free library_books for coordinator */
    if (rank == 0) {
        for (int i = 0; i < M; i++) { 
            Book* current_lib_book = library_books[i];
            while(current_lib_book != NULL) {
                Book* temp = current_lib_book;
                current_lib_book = current_lib_book->next;
                free(temp);
            }
            library_books[i] = NULL;
        }
    }


    MPI_Finalize();
    return 0;
}