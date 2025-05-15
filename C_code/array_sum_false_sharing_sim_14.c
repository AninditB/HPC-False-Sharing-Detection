#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

// Function to initialize the array
void load_array(unsigned long *array, unsigned long size) {
    for (unsigned long i = 0; i < size; i++) {
        array[i] = i + 1;
    }
}

int main(int argc, char *argv[]) {
    // Ensure correct number of arguments
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <mode> <size> <threads>\n", argv[0]);
        fprintf(stderr, "Modes:\n");
        fprintf(stderr, "  good    : no false sharing, no bad memory access\n");
        fprintf(stderr, "  bad-fs  : with false sharing\n");
        fprintf(stderr, "  bad-ma  : with inefficient memory access\n");
        return 1;
    }

    // Parse command-line arguments
    char *mode = argv[1];           // Mode: good, bad-fs, bad-ma
    unsigned long size = atol(argv[2]); // Array size
    int threads = atoi(argv[3]);    // Number of threads

    // Validate mode
    if (strcmp(mode, "good") != 0 && strcmp(mode, "bad-fs") != 0 && strcmp(mode, "bad-ma") != 0) {
        fprintf(stderr, "Invalid mode: %s\n", mode);
        fprintf(stderr, "Modes:\n");
        fprintf(stderr, "  good    : no false sharing, no bad memory access\n");
        fprintf(stderr, "  bad-fs  : with false sharing\n");
        fprintf(stderr, "  bad-ma  : with inefficient memory access\n");
        return 1;
    }

    // Allocate memory for the array
    unsigned long *array = (unsigned long*) malloc(size * sizeof(unsigned long));
    if (!array) {
        fprintf(stderr, "Memory allocation failed for size %lu\n", size);
        return 1;
    }

    // Load the array
    load_array(array, size);

    // Set the number of OpenMP threads
    omp_set_num_threads(threads);

    unsigned long sum = 0;
    double start_time = omp_get_wtime();

    // Perform the parallel reduction depending on the mode
    if (strcmp(mode, "good") == 0) {
        // Good mode: Linear access pattern, no false sharing or bad memory access
        printf("Mode: good (no false sharing, no bad memory access)\n");
        #pragma omp parallel for reduction(+:sum)
        for (unsigned long i = 0; i < size; i++) {
            sum += array[i];
        }
    } else if (strcmp(mode, "bad-fs") == 0) {
        // Bad-fs mode: Simulate false sharing
        printf("Mode: bad-fs (with false sharing)\n");
        int num_threads = threads;
        unsigned long *partial_sums = (unsigned long*) malloc(num_threads * sizeof(unsigned long));
        for (int i = 0; i < num_threads; i++) {
            partial_sums[i] = 0;
        }

        #pragma omp parallel shared(partial_sums)
        {
            int tid = omp_get_thread_num();
            #pragma omp for
            for (unsigned long i = 0; i < size; i++) {
                partial_sums[tid] += array[i];
            }
        }

        // Combine partial sums
        for (int i = 0; i < num_threads; i++) {
            sum += partial_sums[i];
        }

        free(partial_sums);
    } else if (strcmp(mode, "bad-ma") == 0) {
        // Bad-ma mode: Simulate inefficient memory access with stride
        printf("Mode: bad-ma (with inefficient memory access)\n");
        int stride = 64;
        #pragma omp parallel for reduction(+:sum)
        for (unsigned long i = 0; i < size; i++) {
            sum += array[i]; 
        }
    }

    double end_time = omp_get_wtime();

    // Print out the results
    printf("Size: %lu\n", size);
    printf("Threads: %d\n", threads);
    printf("Sum: %lu\n", sum);
    printf("Execution Time: %f seconds\n", (end_time - start_time));

    // Free allocated memory
    free(array);

    return 0;
}
