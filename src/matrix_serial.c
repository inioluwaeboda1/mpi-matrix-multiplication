/*
 * Baseline sequential matrix multiplication.
 *
 * Original CSC 462 coursework by Inioluwa Eboda and Suwilanji Mwanza.
 * Portfolio reconstruction based on the final packaged mm.c.
 * Portfolio hardening adds allocation checks; successful-run multiplication
 * and benchmark behavior are unchanged.
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

void multiply_matrices(
    double **a,
    double **b,
    double **c,
    int rows_a,
    int shared_dimension,
    int cols_b
) {
    for (int i = 0; i < rows_a; i++) {
        for (int j = 0; j < cols_b; j++) {
            c[i][j] = 0.0;
            for (int k = 0; k < shared_dimension; k++) {
                c[i][j] += a[i][k] * b[k][j];
            }
        }
    }
}

double milliseconds_between(struct timespec start, struct timespec finish) {
    long seconds = finish.tv_sec - start.tv_sec;
    long nanoseconds = finish.tv_nsec - start.tv_nsec;

    if (start.tv_nsec > finish.tv_nsec) {
        --seconds;
        nanoseconds += 1000000000L;
    }

    return (seconds * 1000.0) + (nanoseconds / 1000000.0);
}

int main(void) {
    int sizes[] = {100, 1000, 4000, 6000};
    int size_count = (int)(sizeof(sizes) / sizeof(sizes[0]));

    srand((unsigned int)time(NULL));

    for (int index = 0; index < size_count; index++) {
        int size = sizes[index];
        printf("\nSequential matrix multiplication for size: %d x %d\n", size, size);

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
        multiply_matrices(a, b, c, size, size, size);
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
