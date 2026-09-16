/*
 * Cache-friendly sequential matrix multiplication.
 *
 * Original CSC 462 coursework by Inioluwa Eboda and Suwilanji Mwanza.
 * Portfolio reconstruction based on the working mm2I.c source.
 * The right-hand matrix is transposed so each dot product reads contiguous rows.
 * Portfolio hardening adds allocation checks without changing successful-run
 * multiplication or timing boundaries.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/* PORTFOLIO HARDENING: reject invalid sizes and clean up partial allocations. */
int allocate_matrix(int rows, int cols, double ***matrix_out) {
    *matrix_out = NULL;

    if (rows <= 0 || cols <= 0) {
        return 0;
    }

    size_t row_count = (size_t)rows;
    size_t col_count = (size_t)cols;
    if (row_count > SIZE_MAX / sizeof(double *)
        || col_count > SIZE_MAX / sizeof(double)) {
        return 0;
    }

    double **matrix = (double **)malloc(row_count * sizeof(double *));
    if (matrix == NULL) {
        return 0;
    }

    for (size_t row = 0; row < row_count; row++) {
        matrix[row] = (double *)malloc(col_count * sizeof(double));
        if (matrix[row] == NULL) {
            while (row > 0) {
                free(matrix[--row]);
            }
            free(matrix);
            return 0;
        }
    }

    *matrix_out = matrix;
    return 1;
}

void fill_matrix(double **matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix[i][j] = rand() % 10;
        }
    }
}

void free_matrix(double **matrix, int rows) {
    if (matrix == NULL) {
        return;
    }
    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

double **transpose_matrix(double **matrix, int rows, int cols) {
    double **transposed = NULL;
    if (!allocate_matrix(cols, rows, &transposed)) {
        return NULL;
    }
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            transposed[j][i] = matrix[i][j];
        }
    }
    return transposed;
}

double dot_product(const double *a, const double *b, int length) {
    double sum = 0.0;
    for (int i = 0; i < length; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

int multiply_matrices_transposed(
    double **a,
    double **b,
    double **c,
    int rows_a,
    int shared_dimension,
    int cols_b
) {
    double **b_transposed = transpose_matrix(b, shared_dimension, cols_b);
    if (b_transposed == NULL) {
        return 0;
    }

    for (int i = 0; i < rows_a; i++) {
        for (int j = 0; j < cols_b; j++) {
            c[i][j] = dot_product(a[i], b_transposed[j], shared_dimension);
        }
    }

    free_matrix(b_transposed, cols_b);
    return 1;
}

double milliseconds_between(struct timespec start, struct timespec finish) {
    long seconds = finish.tv_sec - start.tv_sec;
    long nanoseconds = finish.tv_nsec - start.tv_nsec;

    if (start.tv_nsec > finish.tv_nsec) {
        --seconds;
        nanoseconds += 1000000000L;
    }

    return 1000.0 * ((double)seconds + (double)nanoseconds / 1000000000.0);
}

int main(void) {
    int sizes[] = {100, 1000, 4000, 6000};
    int size_count = (int)(sizeof(sizes) / sizeof(sizes[0]));

    srand((unsigned int)time(NULL));

    for (int index = 0; index < size_count; index++) {
        int size = sizes[index];
        printf(
            "\nTransposed matrix multiplication for size: %d x %d\n",
            size,
            size
        );

        double **a = NULL;
        double **b = NULL;
        double **c = NULL;

        /* PORTFOLIO HARDENING: fail cleanly if any historical matrix allocation fails. */
        int allocations_ok = allocate_matrix(size, size, &a)
            && allocate_matrix(size, size, &b)
            && allocate_matrix(size, size, &c);
        if (!allocations_ok) {
            fprintf(stderr, "Matrix allocation failed for size %d x %d.\n", size, size);
            free_matrix(a, size);
            free_matrix(b, size);
            free_matrix(c, size);
            return EXIT_FAILURE;
        }

        fill_matrix(a, size, size);
        fill_matrix(b, size, size);

        struct timespec start;
        struct timespec finish;
        clock_gettime(CLOCK_MONOTONIC, &start);
        if (!multiply_matrices_transposed(a, b, c, size, size, size)) {
            fprintf(stderr, "Transposed-matrix allocation failed for size %d x %d.\n", size, size);
            free_matrix(a, size);
            free_matrix(b, size);
            free_matrix(c, size);
            return EXIT_FAILURE;
        }
        clock_gettime(CLOCK_MONOTONIC, &finish);

        printf(
            "Time taken for %d x %d: %.3f milliseconds\n",
            size,
            size,
            milliseconds_between(start, finish)
        );

        free_matrix(a, size);
        free_matrix(b, size);
        free_matrix(c, size);
    }

    return 0;
}
