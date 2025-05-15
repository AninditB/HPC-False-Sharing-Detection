#!/bin/bash

# Array of source files and corresponding executable names
files=(
  "array_sum_false_sharing_sim_14.c vec_14"
  "array_sum_memory_access_28.c sc_28"
  "array_sum_performance_variation_10.c seq_10"
  "matrix_compare_memory_modes_31.c mc_31"
  "matrix_init_access_modes_23.c vec_23"
  "matrix_init_access_variation_29.c sc_29"
)

# Loop through each file and compile
for file in "${files[@]}"; do
  # Extract the source file and executable name
  src_file=$(echo $file | awk '{print $1}')
  exe_file=$(echo $file | awk '{print $2}')

  # Compile the source file with OpenMP flag
  gcc -fopenmp -o "$exe_file" "$src_file"

  # Check if the compilation was successful
  if [ $? -eq 0 ]; then
    echo "Compiled $src_file to $exe_file successfully."
  else
    echo "Failed to compile $src_file."
  fi

done