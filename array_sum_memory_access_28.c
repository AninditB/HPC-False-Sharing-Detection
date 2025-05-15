#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <time.h>

// Define cache line size for padding (typically 64 bytes)
#define CACHE_LINE_SIZE 64

// Structure to prevent false sharing by padding
typedef struct {
    unsigned long sum;
    char padding[CACHE_LINE_SIZE - sizeof(unsigned long)];
} PaddedSum;

// Function to initialize the array with sequential values
void load_array(unsigned long *array, unsigned long size) {
    #pragma omp parallel for schedule(static)
    for (unsigned long i = 0; i < size; i++) {
        array[i] = i + 1;
    }
}

// Function to shuffle an array of indices for random access (if needed in future extensions)
void shuffle_array(unsigned long *indices, unsigned long size) {
    srand((unsigned)time(NULL));
    for (unsigned long i = size - 1; i > 0; i--) {
        unsigned long j = rand() % (i + 1);
        unsigned long temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
    }
}

// Function to perform the sum operation in 'good' mode (no false sharing, linear access)
unsigned long sum_good(unsigned long *array, unsigned long size, int num_threads) {
    unsigned long total_sum = 0;

    // Allocate per-thread sums with padding to prevent false sharing
    PaddedSum *partial_sums = (PaddedSum *) malloc(num_threads * sizeof(PaddedSum));
    if (!partial_sums) {
        fprintf(stderr, "Memory allocation failed for partial_sums in good mode.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize partial sums
    for (int i = 0; i < num_threads; i++) {
        partial_sums[i].sum = 0;
    }

    double start_time = omp_get_wtime();

    // Perform the sum operation
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        unsigned long chunk_size = size / num_threads;
        unsigned long start = tid * chunk_size;
        unsigned long end = (tid == num_threads - 1) ? size : start + chunk_size;

        for (unsigned long i = start; i < end; i++) {
            partial_sums[tid].sum += array[i];
        }
    }

    // Aggregate the partial sums
    for (int i = 0; i < num_threads; i++) {
        total_sum += partial_sums[i].sum;
    }

    double end_time = omp_get_wtime();
    printf("Good Mode - Total Sum: %lu\n", total_sum);
    printf("Good Mode - Execution Time: %f seconds\n", end_time - start_time);

    free(partial_sums);
    return total_sum;
}

// Function to perform the sum operation in 'bad-fs' mode (with false sharing, linear access)
unsigned long sum_bad_fs(unsigned long *array, unsigned long size, int num_threads) {
    unsigned long total_sum = 0;

    // Allocate per-thread sums without padding to introduce false sharing
    unsigned long *partial_sums = (unsigned long *) malloc(num_threads * sizeof(unsigned long));
    if (!partial_sums) {
        fprintf(stderr, "Memory allocation failed for partial_sums in bad-fs mode.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize partial sums
    for (int i = 0; i < num_threads; i++) {
        partial_sums[i] = 0;
    }

    double start_time = omp_get_wtime();

    // Perform the sum operation
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        unsigned long chunk_size = size / num_threads;
        unsigned long start = tid * chunk_size;
        unsigned long end = (tid == num_threads - 1) ? size : start + chunk_size;

        for (unsigned long i = start; i < end; i++) {
            partial_sums[tid] += array[i];
        }
    }

    // Aggregate the partial sums
    for (int i = 0; i < num_threads; i++) {
        total_sum += partial_sums[i];
    }

    double end_time = omp_get_wtime();
    printf("Bad-FS Mode - Total Sum: %lu\n", total_sum);
    printf("Bad-FS Mode - Execution Time: %f seconds\n", end_time - start_time);

    free(partial_sums);
    return total_sum;
}

// Function to perform the sum operation in 'bad-ma' mode (inefficient memory access, strided access)
unsigned long sum_bad_ma(unsigned long *array, unsigned long size, int num_threads, unsigned long stride){
    unsigned long total_sum = 0;

    // Ensure stride is co-prime with size to cover all elements
    unsigned long a = stride;
    unsigned long b = size;
    while(b != 0){
        unsigned long temp = b;
        b = a % b;
        a = temp;
    }
    if(a !=1){
        fprintf(stderr, "Warning: stride=%lu and size=%lu are not co-prime. Sum will not include all elements.\n", stride, size);
    }

    // Allocate per-thread sums with padding to prevent false sharing
    PaddedSum *partial_sums = (PaddedSum *) malloc(num_threads * sizeof(PaddedSum));
    if (!partial_sums) {
        fprintf(stderr, "Memory allocation failed for partial_sums in bad-ma mode.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize partial sums
    for(int i=0;i<num_threads;i++) partial_sums[i].sum=0;

    double start_time = omp_get_wtime();

    // Perform the sum operation with strided access
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();

        for(unsigned long i = tid; i < size; i += num_threads){
            unsigned long idx = (i * stride) % size;
            partial_sums[tid].sum += array[idx];
        }
    }

    // Aggregate the partial sums
    for(int i=0;i<num_threads;i++) total_sum += partial_sums[i].sum;

    double end_time = omp_get_wtime();
    printf("Bad-MA Mode (Stride %lu) - Total Sum: %lu\n", stride, total_sum);
    printf("Bad-MA Mode (Stride %lu) - Execution Time: %f seconds\n", stride, end_time - start_time);

    free(partial_sums);
    return total_sum;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s [good|bad-fs|bad-ma] [size] [threads]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Parse command-line arguments
    char *mode = argv[1];
    unsigned long size = atol(argv[2]);
    int num_threads = atoi(argv[3]);

    // Validate mode
    if (strcmp(mode, "good") != 0 && strcmp(mode, "bad-fs") != 0 && strcmp(mode, "bad-ma") != 0) {
        fprintf(stderr, "Invalid mode: %s\n", mode);
        fprintf(stderr, "Valid modes are: good, bad-fs, bad-ma\n");
        return EXIT_FAILURE;
    }

    // Validate size and threads
    if (size == 0) {
        fprintf(stderr, "Error: Size must be a positive integer.\n");
        return EXIT_FAILURE;
    }
    if (num_threads <= 0) {
        fprintf(stderr, "Error: Number of threads must be a positive integer.\n");
        return EXIT_FAILURE;
    }

    // Set the number of threads for OpenMP
    omp_set_num_threads(num_threads);

    // Allocate memory for the array
    unsigned long *array = (unsigned long *) malloc(size * sizeof(unsigned long));
    if (!array) {
        fprintf(stderr, "Memory allocation failed for the array.\n");
        return EXIT_FAILURE;
    }

    // Initialize the array
    load_array(array, size);

    // Perform the sum operation based on the mode
    if (strcmp(mode, "good") == 0) {
        sum_good(array, size, num_threads);
    }
    else if (strcmp(mode, "bad-fs") == 0) {
        sum_bad_fs(array, size, num_threads);
    }
    else if (strcmp(mode, "bad-ma") == 0) {
        unsigned long stride = 7;
        sum_bad_ma(array, size, num_threads, stride);
    }

    // Free allocated memory
    free(array);

    return EXIT_SUCCESS;
}
