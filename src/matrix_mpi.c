/*
 * Distributed matrix multiplication with MPI.
 *
 * Original CSC 462 coursework by Inioluwa Eboda and Suwilanji Mwanza.
 * Portfolio reconstruction based on the final packaged mmmpi.c.
 *
 * Functional differences from the archived source are marked PORTFOLIO REPAIR
 * and documented in docs/repairs.md.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <mpi.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

static int matrix_size = 0;
static int use_transpose = 0;

/* PORTFOLIO REPAIR: report returned MPI errors before terminating the job. */
static _Noreturn void abort_after_mpi_error(
    int rank,
    const char *operation,
    int error_code
) {
    char error_text[MPI_MAX_ERROR_STRING];
    int error_length = 0;
    int string_status = MPI_Error_string(error_code, error_text, &error_length);

    if (string_status == MPI_SUCCESS) {
        fprintf(
            stderr,
            "Rank %d: %s failed: %.*s\n",
            rank,
            operation,
            error_length,
            error_text
        );
    } else {
        fprintf(
            stderr,
            "Rank %d: %s failed with MPI error code %d.\n",
            rank,
            operation,
            error_code
        );
    }

    int abort_status = MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    if (abort_status != MPI_SUCCESS) {
        fprintf(stderr, "Rank %d: MPI_Abort also failed with code %d.\n", rank, abort_status);
    }
    exit(EXIT_FAILURE);
}

static _Noreturn void abort_with_message(int rank, const char *message) {
    fprintf(stderr, "Rank %d: %s\n", rank, message);
    int abort_status = MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    if (abort_status != MPI_SUCCESS) {
        fprintf(stderr, "Rank %d: MPI_Abort also failed with code %d.\n", rank, abort_status);
    }
    exit(EXIT_FAILURE);
}

static int finalize_mpi(int rank) {
    int finalize_status = MPI_Finalize();
    if (finalize_status != MPI_SUCCESS) {
        fprintf(
            stderr,
            "Rank %d: MPI_Finalize failed with error code %d.\n",
            rank,
            finalize_status
        );
        return 0;
    }
    return 1;
}

/* PORTFOLIO REPAIR: checked size arithmetic and allocation failure handling. */
static int allocate_matrix(
    int rows,
    int cols,
    double ***matrix_out,
    double **block_out
) {
    *matrix_out = NULL;
    *block_out = NULL;

    if (rows < 0 || cols < 0) {
        return 0;
    }
    if (rows == 0 || cols == 0) {
        return 1;
    }

    size_t row_count = (size_t)rows;
    size_t col_count = (size_t)cols;
    if (row_count > SIZE_MAX / col_count) {
        return 0;
    }

    size_t element_count = row_count * col_count;
    if (element_count > SIZE_MAX / sizeof(double)) {
        return 0;
    }
    if (row_count > SIZE_MAX / sizeof(double *)) {
        return 0;
    }

    double *block = (double *)malloc(element_count * sizeof(double));
    double **matrix = (double **)malloc(row_count * sizeof(double *));
    if (block == NULL || matrix == NULL) {
        free(block);
        free(matrix);
        return 0;
    }

    for (int row = 0; row < rows; row++) {
        matrix[row] = block + ((size_t)row * col_count);
    }

    *matrix_out = matrix;
    *block_out = block;
    return 1;
}

static void free_matrix(double **matrix, double *block) {
    free(matrix);
    free(block);
}

static double **transpose_matrix(
    double **matrix,
    int rows,
    int cols,
    double **block_out
) {
    if (matrix == NULL || block_out == NULL || rows <= 0 || cols <= 0) {
        return NULL;
    }

    double **transposed = NULL;
    double *block = NULL;
    if (!allocate_matrix(cols, rows, &transposed, &block)) {
        return NULL;
    }

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            transposed[col][row] = matrix[row][col];
        }
    }

    *block_out = block;
    return transposed;
}

static double dot_product(const double *a, const double *b, int length) {
    double sum = 0.0;
    for (int index = 0; index < length; index++) {
        sum += a[index] * b[index];
    }
    return sum;
}

static int multiply_local_rows(
    double **a,
    double **b,
    double **c,
    int local_row_count
) {
    /* Zero-row ranks intentionally have no local A or C allocation. */
    if (local_row_count < 0
        || b == NULL
        || (local_row_count > 0 && (a == NULL || c == NULL))) {
        return 0;
    }

    if (use_transpose) {
        double *transposed_block = NULL;
        double **transposed = transpose_matrix(
            b,
            matrix_size,
            matrix_size,
            &transposed_block
        );
        if (transposed == NULL) {
            return 0;
        }

        for (int row = 0; row < local_row_count; row++) {
            for (int col = 0; col < matrix_size; col++) {
                c[row][col] = dot_product(a[row], transposed[col], matrix_size);
            }
        }

        free_matrix(transposed, transposed_block);
        return 1;
    }

    for (int row = 0; row < local_row_count; row++) {
        for (int col = 0; col < matrix_size; col++) {
            double sum = 0.0;
            for (int shared = 0; shared < matrix_size; shared++) {
                sum += a[row][shared] * b[shared][col];
            }
            c[row][col] = sum;
        }
    }

    return 1;
}

static double milliseconds_between(struct timespec start, struct timespec finish) {
    long seconds = finish.tv_sec - start.tv_sec;
    long nanoseconds = finish.tv_nsec - start.tv_nsec;
    if (start.tv_nsec > finish.tv_nsec) {
        --seconds;
        nanoseconds += 1000000000L;
    }
    return 1000.0 * ((double)seconds + (double)nanoseconds / 1000000000.0);
}

/* PORTFOLIO REPAIR: MPI count parameters are int, so broadcast in safe chunks. */
static int broadcast_doubles(double *buffer, size_t element_count) {
    size_t offset = 0;
    while (offset < element_count) {
        size_t remaining = element_count - offset;
        int chunk = remaining > (size_t)INT_MAX ? INT_MAX : (int)remaining;
        int broadcast_status = MPI_Bcast(
            buffer + offset,
            chunk,
            MPI_DOUBLE,
            0,
            MPI_COMM_WORLD
        );
        if (broadcast_status != MPI_SUCCESS) {
            return broadcast_status;
        }
        offset += (size_t)chunk;
    }
    return MPI_SUCCESS;
}

/* PORTFOLIO REPAIR: treat strtol range errors as invalid input. */
static int parse_positive_int(const char *text, int *value_out) {
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno == ERANGE
        || text[0] == '\0'
        || end == NULL
        || *end != '\0'
        || value <= 0
        || value > INT_MAX) {
        return 0;
    }
    *value_out = (int)value;
    return 1;
}

static int parse_transpose_flag(const char *text, int *value_out) {
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno == ERANGE
        || text[0] == '\0'
        || end == NULL
        || *end != '\0'
        || (value != 0 && value != 1)) {
        return 0;
    }
    *value_out = (int)value;
    return 1;
}

int main(int argc, char **argv) {
    int rank = 0;
    int rank_count = 0;
    int mpi_status = MPI_Init(&argc, &argv);
    if (mpi_status != MPI_SUCCESS) {
        fprintf(stderr, "MPI_Init failed with error code %d.\n", mpi_status);
        return EXIT_FAILURE;
    }

    /* PORTFOLIO REPAIR: request returned errors so they can be reported. */
    mpi_status = MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
    if (mpi_status != MPI_SUCCESS) {
        abort_after_mpi_error(rank, "MPI_Comm_set_errhandler", mpi_status);
    }
    mpi_status = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (mpi_status != MPI_SUCCESS) {
        abort_after_mpi_error(rank, "MPI_Comm_rank", mpi_status);
    }
    mpi_status = MPI_Comm_size(MPI_COMM_WORLD, &rank_count);
    if (mpi_status != MPI_SUCCESS) {
        abort_after_mpi_error(rank, "MPI_Comm_size", mpi_status);
    }
    if (rank_count <= 0) {
        abort_with_message(rank, "MPI communicator reported no ranks.");
    }

    /* PORTFOLIO REPAIR: validate arguments and finalize MPI on errors. */
    int arguments_valid = argc == 3 && parse_positive_int(argv[1], &matrix_size);
    if (arguments_valid) {
        arguments_valid = parse_transpose_flag(argv[2], &use_transpose);
    }

    if (!arguments_valid) {
        if (rank == 0) {
            fprintf(
                stderr,
                "Usage: %s MATRIX_SIZE TRANSPOSE_FLAG\n"
                "  MATRIX_SIZE must be a positive integer.\n"
                "  TRANSPOSE_FLAG must be 0 or 1.\n",
                argv[0]
            );
        }
        finalize_mpi(rank);
        return EXIT_FAILURE;
    }

    if (rank == 0) {
        fprintf(stdout, "n = %d\nrank_count = %d\n", matrix_size, rank_count);
    }

    int small_row_count = matrix_size / rank_count;
    int remainder = matrix_size % rank_count;
    /* PORTFOLIO REPAIR: add an extra row only when a remainder exists. */
    int large_row_count = small_row_count;
    if (remainder > 0) {
        ++large_row_count;
    }
    int local_row_count = rank < remainder ? large_row_count : small_row_count;

    double **a = NULL;
    double **b = NULL;
    double **c = NULL;
    double *a_block = NULL;
    double *b_block = NULL;
    double *c_block = NULL;

    int local_allocation_ok = allocate_matrix(
        matrix_size,
        matrix_size,
        &b,
        &b_block
    );

    if (rank == 0) {
        local_allocation_ok = local_allocation_ok
            && allocate_matrix(matrix_size, matrix_size, &a, &a_block)
            && allocate_matrix(matrix_size, matrix_size, &c, &c_block);
    } else {
        local_allocation_ok = local_allocation_ok
            && allocate_matrix(local_row_count, matrix_size, &a, &a_block)
            && allocate_matrix(local_row_count, matrix_size, &c, &c_block);
    }

    /* PORTFOLIO REPAIR: verify rank-specific allocation invariants explicitly. */
    if (b == NULL || b_block == NULL) {
        local_allocation_ok = 0;
    } else if (rank == 0) {
        if (a == NULL || a_block == NULL || c == NULL || c_block == NULL) {
            local_allocation_ok = 0;
        }
    } else if (local_row_count > 0
        && (a == NULL || a_block == NULL || c == NULL || c_block == NULL)) {
        local_allocation_ok = 0;
    }

    /* PORTFOLIO REPAIR: fail the job consistently if any rank cannot allocate. */
    int all_allocations_ok = 0;
    mpi_status = MPI_Allreduce(
        &local_allocation_ok,
        &all_allocations_ok,
        1,
        MPI_INT,
        MPI_MIN,
        MPI_COMM_WORLD
    );
    if (mpi_status != MPI_SUCCESS) {
        abort_after_mpi_error(rank, "MPI_Allreduce for allocation status", mpi_status);
    }
    if (!local_allocation_ok || !all_allocations_ok) {
        if (rank == 0) {
            fprintf(stderr, "Matrix allocation failed on at least one MPI rank.\n");
        }
        free_matrix(a, a_block);
        free_matrix(b, b_block);
        free_matrix(c, c_block);
        finalize_mpi(rank);
        return EXIT_FAILURE;
    }

    /* PORTFOLIO REPAIR: enforce the checked invariants before dereferencing. */
    if (b == NULL || b_block == NULL) {
        abort_with_message(rank, "Matrix B allocation invariant failed.");
    }
    if (rank == 0
        && (a == NULL || a_block == NULL || c == NULL || c_block == NULL)) {
        abort_with_message(rank, "Rank 0 matrix allocation invariant failed.");
    }
    if (rank != 0 && local_row_count > 0
        && (a == NULL || a_block == NULL || c == NULL || c_block == NULL)) {
        abort_with_message(rank, "Worker matrix allocation invariant failed.");
    }

    if (rank == 0) {
        size_t element_count = (size_t)matrix_size * (size_t)matrix_size;
        for (size_t index = 0; index < element_count; index++) {
            a_block[index] = 4.0;
            c_block[index] = -1.0;
        }
        for (int row = 0; row < matrix_size; row++) {
            for (int col = 0; col < matrix_size; col++) {
                b[row][col] = row == col ? 1.0 : 0.0;
            }
        }
    }

    struct timespec start;
    struct timespec finish;
    if (rank == 0) {
        clock_gettime(CLOCK_MONOTONIC, &start);
    }

    size_t matrix_element_count = (size_t)matrix_size * (size_t)matrix_size;
    mpi_status = broadcast_doubles(b_block, matrix_element_count);
    if (mpi_status != MPI_SUCCESS) {
        abort_after_mpi_error(rank, "MPI_Bcast for matrix B", mpi_status);
    }

    if (rank == 0) {
        int current_row = local_row_count;
        for (int destination = 1; destination < rank_count; destination++) {
            int rows_to_send = destination < remainder
                ? large_row_count
                : small_row_count;
            size_t send_count = (size_t)rows_to_send * (size_t)matrix_size;

            /* PORTFOLIO REPAIR: do not narrow an oversized MPI count. */
            if (send_count > (size_t)INT_MAX) {
                abort_with_message(rank, "A row block exceeds the MPI integer count limit.");
            }
            if (send_count > 0) {
                mpi_status = MPI_Send(
                    a_block + ((size_t)current_row * (size_t)matrix_size),
                    (int)send_count,
                    MPI_DOUBLE,
                    destination,
                    123,
                    MPI_COMM_WORLD
                );
                if (mpi_status != MPI_SUCCESS) {
                    abort_after_mpi_error(rank, "MPI_Send for matrix A", mpi_status);
                }
            }
            current_row += rows_to_send;
        }
    } else {
        size_t receive_count = (size_t)local_row_count * (size_t)matrix_size;
        if (receive_count > (size_t)INT_MAX) {
            abort_with_message(rank, "A row block exceeds the MPI integer count limit.");
        }
        /* PORTFOLIO REPAIR: skip A[0] when this rank owns zero rows. */
        if (receive_count > 0) {
            mpi_status = MPI_Recv(
                a_block,
                (int)receive_count,
                MPI_DOUBLE,
                0,
                123,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            );
            if (mpi_status != MPI_SUCCESS) {
                abort_after_mpi_error(rank, "MPI_Recv for matrix A", mpi_status);
            }
        }
    }

    if (!multiply_local_rows(a, b, c, local_row_count)) {
        abort_with_message(rank, "Could not allocate the transposed matrix.");
    }

    if (local_row_count > 0 && c == NULL) {
        abort_with_message(rank, "Result matrix allocation invariant failed.");
    }

    int local_status = 1;
    for (int row = 0; row < local_row_count && local_status == 1; row++) {
        for (int col = 0; col < matrix_size; col++) {
            if (c[row][col] != 4.0) {
                local_status = 2;
                break;
            }
        }
    }

    int global_status = 0;
    /* PORTFOLIO REPAIR: pass the address of the local reduction value. */
    mpi_status = MPI_Reduce(
        &local_status,
        &global_status,
        1,
        MPI_INT,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );
    if (mpi_status != MPI_SUCCESS) {
        abort_after_mpi_error(rank, "MPI_Reduce for verification status", mpi_status);
    }

    if (rank == 0) {
        clock_gettime(CLOCK_MONOTONIC, &finish);
    }

    /* PORTFOLIO REPAIR: give every rank the job-wide verification result. */
    mpi_status = MPI_Bcast(&global_status, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (mpi_status != MPI_SUCCESS) {
        abort_after_mpi_error(rank, "MPI_Bcast for verification status", mpi_status);
    }

    if (rank == 0) {
        char hostname[HOST_NAME_MAX];
        gethostname(hostname, sizeof(hostname));
        hostname[sizeof(hostname) - 1] = '\0';

        fprintf(
            stdout,
            "Host %s with %d MPI ranks for matrix size %d, time: %.5f minutes\n",
            hostname,
            rank_count,
            matrix_size,
            milliseconds_between(start, finish) / 60000.0
        );

        if (global_status == 1) {
            fprintf(stdout, "Matrix multiplication verified: all local blocks matched.\n");
        } else {
            fprintf(stdout, "Matrix multiplication error: at least one rank found a mismatch.\n");
        }
    }

    free_matrix(a, a_block);
    free_matrix(b, b_block);
    free_matrix(c, c_block);

    int exit_status = global_status == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
    if (!finalize_mpi(rank)) {
        return EXIT_FAILURE;
    }
    return exit_status;
}
