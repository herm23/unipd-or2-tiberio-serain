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
| `-out` / `--out` | Path to the output file for results | `NULL` |
| `-nodes_count` | Number of nodes for random instances | - |
| `-time_limit` | Maximum execution time in seconds | `1.0E+20` |
| `-model_type` / `-model` | Type of heuristic/strategy to use (see table below) | `0` |
| `-seed` | Random seed for stochastic components | `-1` |
| `-no-plot` / `--no-plot` | Disable GNUplot visualization | (False) |
| `-help` / `--help` | Show available parameters | - |

### Solving Strategies (`-model_type`)
The solver implements several strategies based on construction heuristics and refinement techniques.

| Model Type | Strategy | Description |
|------------|----------|-------------|
| `0` | `NN` | Nearest Neighbor construction only. |
| `1` | `NN_TWO_OPT_MULTISTART` | NN with 2-opt refinement at each iteration. |
| `2` | `NN_AND_TWO_OPT` | NN construction followed by a single 2-opt refinement. |
| `3` | `NN_VNS_MULTISTART` | NN with VNS (2-opt + Kick) at each iteration. |
| `4` | `NN_AND_VNS` | NN construction followed by VNS refinement. |
| `5` | `GRASP` | GRASP (Greedy Randomized Adaptive Search Procedure). |
| `6` | `GRASP_TWO_OPT_MULTISTART` | GRASP with 2-opt refinement at each iteration. |
| `7` | `GRASP_AND_TWO_OPT` | GRASP followed by a single 2-opt refinement. |
| `8` | `GRASP_VNS_MULTISTART` | GRASP with VNS refinement at each iteration. |
| `9` | `GRASP_AND_VNS` | GRASP followed by VNS refinement. |

## Project Structure

- `main.c`: Entry point, handles command-line parsing and high-level execution flow.
- `src/tsp.c` / `headers/tsp.h`: Core TSP logic, construction heuristics (NN, GRASP), and refinement strategies (2-opt, VNS kick).
- `src/utils.c` / `headers/utils.h`: Shared utility functions, distance matrix initialization, and TSPLIB parsing logic.
- `src/chrono.c` / `headers/chrono.h`: Utility functions for precise timing and performance measurement.
- `src/greedyNN.c`: Dedicated implementation for Nearest Neighbor search.
- `headers/consts.h`: Global constants, verbosity levels, and shared definitions.
- `res/`: Contains TSP instances (e.g., `att48.tsp`).
- `python/`: Scripts for parsing results and plotting performance profiles.

## Scripts

- **Build**: Use `cmake --build .` as described in the Setup section.
- **Python**: Utility scripts are located in the `python/` directory. Ensure requirements are installed:
  ```powershell
  pip install -r python/requirements.txt
  ```
- **Test**: Verification is currently done by running instances and checking optimality/validity. Automated test suite plans are in `todo.md`.
