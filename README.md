RUN 1ST TEST :
    mpirun --mca btl_vader_single_copy_mechanism none -np 23 --oversubscribe ./mpi_program 3 testfiles_hy486/testfile0/loaners_13_libs_9_np_23.txt output.txt

RUN 2ND TEST:
    mpirun --mca btl_vader_single_copy_mechanism none -np 49 --oversubscribe ./mpi_program 4 testfiles_hy486/testfile1/loaners_32_libs_16_np_49.txt output.txt

RUN 3RD TEST:
    mpirun --mca btl_vader_single_copy_mechanism none -np 88 --oversubscribe ./mpi_program 5 testfiles_hy486/testfile2/loaners_62_libs_25_np_88.txt output.txt