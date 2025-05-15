#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <omp.h>

// Good mode: Efficient initialization without false sharing or inefficient access
void good_mode(int **a, int N, int threads) {
    #pragma omp parallel for num_threads(threads) schedule(static)
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            a[i][j] = 17;
        }
    }
}


// Modified Bad-fs mode: Initializes all rows with potential false sharing
void bad_fs_mode(int **a, int N, int threads) {
    #pragma omp parallel for num_threads(threads) schedule(static)
    for (int i = 0; i < N; i++) {
        int tid = omp_get_thread_num();
        for (int j = tid; j < N; j += threads) {
            a[j][i] = 17 + tid; // Introduce thread-specific writes to simulate false sharing
        }
    }
}


// Bad-ma mode: Simulates inefficient memory access by initializing in column-major order
void bad_ma_mode(int **a, int N, int threads) {
    #pragma omp parallel for num_threads(threads) schedule(static)
    for (int j = 0; j < N; j++) { // Column-major order
        for (int i = 0; i < N; i++) {
            a[i][j] = 17;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "       ./program <mode> <N> <threads>\n");
        fprintf(stderr, "Modes:\n");
        fprintf(stderr, "  good    : no false sharing, no bad memory access\n");
        fprintf(stderr, "  bad-fs  : with false sharing\n");
        fprintf(stderr, "  bad-ma  : with inefficient memory access\n");
        return 1;
    }

    char *mode = argv[1];
    int N = atoi(argv[2]);
    int threads = atoi(argv[3]);

    // Validate mode
    if (strcmp(mode, "good") != 0 && strcmp(mode, "bad-fs") != 0 && strcmp(mode, "bad-ma") != 0) {
        fprintf(stderr, "Invalid mode: %s\n", mode);
        fprintf(stderr, "Modes:\n");
        fprintf(stderr, "  good    : no false sharing, no bad memory access\n");
        fprintf(stderr, "  bad-fs  : with false sharing\n");
        fprintf(stderr, "  bad-ma  : with inefficient memory access\n");
        return 1;
    }

    // Allocate memory for the 2D array as a single contiguous block to enhance cache line sharing
    int **a = (int **)malloc(sizeof(int*) * N);
    if (!a){
        fprintf(stderr, "Memory allocation failed for row pointers.\n");
        return 1;
    }

    // Allocate a single contiguous block for all elements
    int *a_block = (int *)malloc(sizeof(int) * N * N);
    if (!a_block){
        fprintf(stderr, "Memory allocation failed for array data.\n");
        free(a);
        return 1;
    }

    // Assign row pointers to the contiguous block
    for (int i = 0; i < N; i++) {
        a[i] = a_block + i * N;
    }

    // Execute the selected mode
    double start_time = omp_get_wtime();
    if (strcmp(mode, "good") == 0) {
        good_mode(a, N, threads);
    } else if (strcmp(mode, "bad-fs") == 0) {
        bad_fs_mode(a, N, threads);
    } else if (strcmp(mode, "bad-ma") == 0) {
        bad_ma_mode(a, N, threads);
    }
    double end_time = omp_get_wtime();

    // Validate and print results
    if (N > 17){
        printf("a[17][17] = %d\n", a[17][17]);
    } else{
        printf("a[17][17] is out of bounds.\n");
    }
    printf("Execution Time: %f seconds\n", end_time - start_time);

    // Free allocated memory
    free(a_block);
    free(a);

    return 0;
}
