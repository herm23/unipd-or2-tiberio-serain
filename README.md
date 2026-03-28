# Traveling Salesman Problem (TSP) Solver

This repository contains a C-based solver for the Traveling Salesman Problem (TSP), developed as part of the Operations Research 2 (OR2) course at the University of Padua (unipd). The solver implements various heuristics and meta-heuristics, and is designed to integrate with **IBM ILOG CPLEX** for exact methods. It uses **GNUplot** for solution visualization.

## Overview

The project implements various mathematical models and heuristics to solve the TSP. It reads problem instances in the standard TSPLIB format, processes them using a set of optimization algorithms (Nearest Neighbor, 2-opt refinement, etc.), and outputs the results including the total cost and execution time.

## Requirements

### Software
- **C Compiler**: A C11-compliant compiler (e.g., GCC/MinGW, MSVC, Clang).
- **CMake**: Version 3.10 or higher.
- **IBM ILOG CPLEX Studio** (Optional): Required for exact optimization models. Ensure the `CPLEX_ROOT` path is correctly configured in `CMakeLists.txt` or passed as a CMake variable.
- **GNUplot**: Optional, required for solution visualization.

## Setup & Build

1. **Clone the repository**:
   ```powershell
   git clone <repository-url>
   cd unipd-or2-tiberio-serain
   ```

2. **Configure and build the project**:
   ```powershell
   mkdir build
   cd build
   cmake ..
   cmake --build . --target unipd_or2_tiberio_serain --config Release
   ```

   *Note: If you want to use CPLEX, specify the path during configuration:*
   ```powershell
   cmake -DCPLEX_ROOT="C:\Program Files\IBM\ILOG\CPLEX_Studio2212" ..
   ```

## Running the Solver

The executable accepts several command-line arguments to control the optimization process.

### Basic Usage
```powershell
./unipd_or2_tiberio_serain -file ../res/att48.tsp -model_type 0 -time_limit 3600
```

### Supported Arguments
| Argument | Description | Default |
|----------|-------------|---------|
| `-file` / `-f` / `-input` | Path to the `.tsp` input file | `NULL` |
| `-nodes_count` | Number of nodes for random instances | - |
| `-time_limit` | Maximum execution time in seconds | `1.0E+20` |
| `-model_type` / `-model` | Type of heuristic/strategy to use | `0` |
| `-seed` | Random seed for stochastic components | `-1` |
| `-help` | Show available parameters | - |

## Project Structure

- `main.c`: Entry point, handles command-line parsing and high-level execution flow.
- `src/tsp.c` / `headers/tsp.h`: Core TSP logic, construction heuristics (NN, GRASP), and refinement strategies (2-opt).
- `src/utils.c` / `headers/utils.h`: Shared utility functions, distance calculations, and TSPLIB parsing logic.
- `src/chrono.c` / `headers/chrono.h`: Utility functions for precise timing and performance measurement.
- `headers/consts.h`: Global constants and verbosity level definitions.
- `res/`: Contains TSP instances (e.g., `att48.tsp`).

## Scripts

- **Build**: Use `cmake --build .` as described in the Setup section.
- **Test**: Currently, no automated test suite is provided. Verification is done by running instances and checking optimality/validity. (See `todo.md` for test development plans).
