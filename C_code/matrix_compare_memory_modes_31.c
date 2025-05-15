#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <time.h>

// Define cache line size for padding (typically 64 bytes)
#define CACHE_LINE_SIZE 64

// Structure to prevent false sharing by padding
typedef struct {
    unsigned long diff_count;
    char padding[CACHE_LINE_SIZE - sizeof(unsigned long)];
} PaddedDiff;

// Function to initialize the matrices with sequential values and introduce differences
void initialize_matrices(unsigned long *A, unsigned long *B, unsigned long size) {
    // Initialize both matrices with the same values
    #pragma omp parallel for schedule(static)
    for (unsigned long i = 0; i < size; i++) {
        A[i] = i % 100;
        B[i] = A[i];
    }

    // Introduce differences in B: every 1000th element differs
    #pragma omp parallel for schedule(static)
    for (unsigned long i = 0; i < size; i += 1000) {
        B[i] = A[i] + 1;
    }
}

// Function to shuffle an array of indices for random access
void shuffle_indices(unsigned long *indices, unsigned long size) {
    srand((unsigned)time(NULL));
    for (unsigned long i = size - 1; i > 0; i--) {
        unsigned long j = rand() % (i + 1);
        unsigned long temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
    }
}

// Function to perform the matrix comparison in 'good' mode (no false sharing, linear access)
unsigned long compare_good(unsigned long *A, unsigned long *B, unsigned long size, int num_threads) {
    unsigned long total_diffs = 0;

    // Allocate per-thread difference counts with padding to prevent false sharing
    PaddedDiff *partial_diffs = (PaddedDiff *) malloc(num_threads * sizeof(PaddedDiff));
    if (!partial_diffs) {
        fprintf(stderr, "Memory allocation failed for partial_diffs in good mode.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize partial difference counts
    for (int i = 0; i < num_threads; i++) {
        partial_diffs[i].diff_count = 0;
    }

    double start_time = omp_get_wtime();

    // Perform the matrix comparison
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        unsigned long chunk_size = size / num_threads;
        unsigned long start = tid * chunk_size;
        unsigned long end = (tid == num_threads - 1) ? size : start + chunk_size;

        for (unsigned long i = start; i < end; i++) {
            if (A[i] != B[i]) {
                partial_diffs[tid].diff_count++;
            }
        }
    }

    // Aggregate the partial difference counts
    for (int i = 0; i < num_threads; i++) {
        total_diffs += partial_diffs[i].diff_count;
    }

    double end_time = omp_get_wtime();
    printf("Good Mode - Total Differences: %lu\n", total_diffs);
    printf("Good Mode - Execution Time: %f seconds\n", end_time - start_time);

    free(partial_diffs);
    return total_diffs;
}

// Function to perform the matrix comparison in 'bad-fs' mode (with false sharing, linear access)
unsigned long compare_bad_fs(unsigned long *A, unsigned long *B, unsigned long size, int num_threads) {
    unsigned long total_diffs = 0;

    // Allocate per-thread difference counts without padding to introduce false sharing
    unsigned long *partial_diffs = (unsigned long *) malloc(num_threads * sizeof(unsigned long));
    if (!partial_diffs) {
        fprintf(stderr, "Memory allocation failed for partial_diffs in bad-fs mode.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize partial difference counts
    for (int i = 0; i < num_threads; i++) {
        partial_diffs[i] = 0;
    }

    double start_time = omp_get_wtime();

    // Perform the matrix comparison
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        unsigned long chunk_size = size / num_threads;
        unsigned long start = tid * chunk_size;
        unsigned long end = (tid == num_threads - 1) ? size : start + chunk_size;

        for (unsigned long i = start; i < end; i++) {
            if (A[i] != B[i]) {
                partial_diffs[tid]++;
            }
        }
    }

    // Aggregate the partial difference counts
    for (int i = 0; i < num_threads; i++) {
        total_diffs += partial_diffs[i];
    }

    double end_time = omp_get_wtime();
    printf("Bad-FS Mode - Total Differences: %lu\n", total_diffs);
    printf("Bad-FS Mode - Execution Time: %f seconds\n", end_time - start_time);

    free(partial_diffs);
    return total_diffs;
}

// Function to perform the matrix comparison in 'bad-ma' mode (inefficient memory access, random access)
unsigned long compare_bad_ma(unsigned long *A, unsigned long *B, unsigned long size, int num_threads, unsigned long *shuffled_indices){
    unsigned long total_diffs = 0;

    // Allocate per-thread difference counts with padding to prevent false sharing
    PaddedDiff *partial_diffs = (PaddedDiff *) malloc(num_threads * sizeof(PaddedDiff));
    if (!partial_diffs) {
        fprintf(stderr, "Memory allocation failed for partial_diffs in bad-ma mode.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize partial difference counts
    for(int i=0; i<num_threads; i++) {
        partial_diffs[i].diff_count = 0;
    }

    double start_time = omp_get_wtime();

    // Perform the matrix comparison with random access
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        unsigned long chunk_size = size / num_threads;
        unsigned long start = tid * chunk_size;
        unsigned long end = (tid == num_threads - 1) ? size : start + chunk_size;

        for(unsigned long i = start; i < end; i++) {
            unsigned long idx = shuffled_indices[i];
            if (A[idx] != B[idx]) {
                partial_diffs[tid].diff_count++;
            }
        }
    }

    // Aggregate the partial difference counts
    for(int i=0; i<num_threads; i++) {
        total_diffs += partial_diffs[i].diff_count;
    }

    double end_time = omp_get_wtime();
    printf("Bad-MA Mode (Random Access) - Total Differences: %lu\n", total_diffs);
    printf("Bad-MA Mode (Random Access) - Execution Time: %f seconds\n", end_time - start_time);

    free(partial_diffs);
    return total_diffs;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s [good|bad-fs|bad-ma] [size] [threads]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Parse command-line arguments
    char *mode = argv[1];
    unsigned long N = atol(argv[2]);
    int num_threads = atoi(argv[3]);

    // Validate mode
    if (strcmp(mode, "good") != 0 && strcmp(mode, "bad-fs") != 0 && strcmp(mode, "bad-ma") != 0) {
        fprintf(stderr, "Invalid mode: %s\n", mode);
        fprintf(stderr, "Valid modes are: good, bad-fs, bad-ma\n");
        return EXIT_FAILURE;
    }

    // Validate size and threads
    if (N == 0) {
        fprintf(stderr, "Error: Size must be a positive integer.\n");
        return EXIT_FAILURE;
    }
    if (num_threads <= 0) {
        fprintf(stderr, "Error: Number of threads must be a positive integer.\n");
        return EXIT_FAILURE;
    }

    // Set the number of threads for OpenMP
    omp_set_num_threads(num_threads);

    // Calculate total number of elements
    unsigned long total_elements = N * N;

    // Allocate memory for the matrices
    unsigned long *A = (unsigned long *) malloc(total_elements * sizeof(unsigned long));
    unsigned long *B = (unsigned long *) malloc(total_elements * sizeof(unsigned long));
    if (!A || !B) {
        fprintf(stderr, "Memory allocation failed for matrices.\n");
        free(A);
        free(B);
        return EXIT_FAILURE;
    }

    // Initialize the matrices and introduce differences
    initialize_matrices(A, B, total_elements);

    // Prepare shuffled indices for 'bad-ma' mode
    unsigned long *shuffled_indices = NULL;
    if (strcmp(mode, "bad-ma") == 0) {
        shuffled_indices = (unsigned long*) malloc(total_elements * sizeof(unsigned long));
        if (!shuffled_indices) {
            fprintf(stderr, "Memory allocation failed for shuffled_indices in bad-ma mode.\n");
            free(A);
            free(B);
            return EXIT_FAILURE;
        }
        for (unsigned long i = 0; i < total_elements; i++) {
            shuffled_indices[i] = i;
        }
        shuffle_indices(shuffled_indices, total_elements);
    }

    // Perform the matrix comparison based on the mode
    if (strcmp(mode, "good") == 0) {
        compare_good(A, B, total_elements, num_threads);
    }
    else if (strcmp(mode, "bad-fs") == 0) {
        compare_bad_fs(A, B, total_elements, num_threads);
    }
    else if (strcmp(mode, "bad-ma") == 0) {
        compare_bad_ma(A, B, total_elements, num_threads, shuffled_indices);
    }

    // Free allocated memory
    free(A);
    free(B);
    if (shuffled_indices) free(shuffled_indices);

    return EXIT_SUCCESS;
}
