#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>
#include <string.h>

// This program demonstrates:
// - Reading data element-wise from an array
// - Writing data element-wise to an array
// - Reading, modifying, writing data element-wise to an array
// - Parameterization by array size
// - Both good (linear) and bad (random and strided) memory access patterns
// - Timing of operations to compare performance

// Function to initialize the array with numbers ranging from 1 to size of the array
void load_array(unsigned long *array, unsigned long size) {
    for (unsigned long i = 0; i < size; i++) {
        array[i] = i + 1;
    }
}

// Function to shuffle an array of indices for random access
void shuffle_array(unsigned long *indices, unsigned long size) {
    srand((unsigned)time(NULL));
    for (unsigned long i = size - 1; i > 0; i--) {
        unsigned long j = rand() % (i + 1);
        unsigned long temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
    }
}

// Linear access (Good memory performance)
void sum_linear(unsigned long *array, unsigned long size) {
    unsigned long sum = 0;
    double start_time = omp_get_wtime();
    for (unsigned long i = 0; i < size; i++) {
        sum += array[i];
    }
    double end_time = omp_get_wtime();
    printf("Linear Sum: %lu\n", sum);
    printf("Linear Execution Time: %f seconds\n", end_time - start_time);
}

// Random access (Bad memory performance)
void sum_random(unsigned long *array, unsigned long *indices, unsigned long size) {
    unsigned long sum = 0;
    double start_time = omp_get_wtime();
    for (unsigned long i = 0; i < size; i++) {
        sum += array[indices[i]];
    }
    double end_time = omp_get_wtime();
    printf("Random Sum: %lu\n", sum);
    printf("Random Execution Time: %f seconds\n", end_time - start_time);
}

// Strided access (Bad memory performance)
void sum_strided(unsigned long *array, unsigned long size, unsigned long stride) {
    unsigned long sum = 0;
    double start_time = omp_get_wtime();
    for (unsigned long i = 0; i < size; i += stride) {
        sum += array[i];
    }
    double end_time = omp_get_wtime();
    printf("Strided Sum (Stride %lu): %lu\n", stride, sum);
    printf("Strided Execution Time: %f seconds\n", end_time - start_time);
}

// Reading, modifying, and writing data element-wise
void modify_and_sum(unsigned long *array, unsigned long size) {
    unsigned long sum = 0;
    double start_time = omp_get_wtime();
    for (unsigned long i = 0; i < size; i++) {
        array[i] += 1; 
        sum += array[i]; 
    }
    double end_time = omp_get_wtime();
    printf("Modified Sum: %lu\n", sum);
    printf("Modification and Summing Execution Time: %f seconds\n", end_time - start_time);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s [good|bad] [size]\n", argv[0]);
        return 1;
    }

    // Parse command-line arguments
    char *mode = argv[1];
    unsigned long size = atol(argv[2]);
    if (size == 0) {
        fprintf(stderr, "Invalid size.\n");
        return 1;
    }

    // Allocate memory for the array
    unsigned long *array = (unsigned long *)malloc(size * sizeof(unsigned long));
    if (!array) {
        fprintf(stderr, "Memory allocation failed for the array.\n");
        return 1;
    }

    load_array(array, size);

    // Prepare indices for random access (only needed if mode == bad)
    unsigned long *indices = NULL;
    if (strcmp(mode, "bad") == 0) {
        indices = (unsigned long*)malloc(size * sizeof(unsigned long));
        if (!indices) {
            fprintf(stderr, "Memory allocation failed for indices.\n");
            free(array);
            return 1;
        }
        for (unsigned long i = 0; i < size; i++) {
            indices[i] = i;
        }
        shuffle_array(indices, size);
    }

    if (strcmp(mode, "good") == 0) {
        // Good memory access: linear and modify
        sum_linear(array, size);
        modify_and_sum(array, size);
    } else if (strcmp(mode, "bad") == 0) {
        // Bad memory access: random and strided
        sum_random(array, indices, size);
        sum_strided(array, size, 5);
    } else {
        printf("Invalid mode: %s\n", mode);
        printf("Usage: %s [good|bad] [size]\n", argv[0]);
        free(array);
        if (indices) free(indices);
        return 1;
    }

    free(array);
    if (indices) free(indices);

    return 0;
}
