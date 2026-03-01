# Traveling Salesman Problem (TSP) Solver

This repository contains a C-based solver for the Traveling Salesman Problem (TSP), developed as part of the Operations Research 2 (OR2) course at the University of Padua (unipd). The solver leverages **IBM ILOG CPLEX** to find optimal or near-optimal solutions and uses **GNUplot** for visualization.

## Overview

The project implements various mathematical models and heuristics to solve the TSP. It reads problem instances in the standard TSPLIB format, processes them using a CPLEX-based optimization engine, and outputs the results including the total cost and execution time.

## Requirements

### Software
- **C Compiler**: A C11-compliant compiler (e.g., GCC/MinGW, MSVC).
- **CMake**: Version 4.0 or higher.
- **IBM ILOG CPLEX Studio**: The project is configured for version 22.1.2. Ensure the `CPLEX_ROOT` path in `CMakeLists.txt` matches your installation directory.
- **GNUplot**: Optional, required for solution visualization (`plot_tour`).

### Environment Variables
- Ensure CPLEX binary directories are in your system's `PATH` (especially on Windows to find `.dll` files at runtime).

## Setup & Build

1. **Clone the repository**:
   ```powershell
   git clone <repository-url>
   cd unipd-or2-tiberio-serain
   ```

2. **Configure CPLEX path**:
   Open `CMakeLists.txt` and verify the `CPLEX_ROOT` variable:
   ```cmake
   set(CPLEX_ROOT "C:\\Program Files\\IBM\\ILOG\\CPLEX_Studio_Community2212")
   ```

3. **Build the project using CMake**:
   ```powershell
   mkdir build
   cd build
   cmake ..
   cmake --build . --target unipd_or2_tiberio_serain --config Debug
   ```

## Running the Solver

The executable accepts several command-line arguments to control the optimization process.

### Basic Usage
```powershell
./unipd_or2_tiberio_serain -file ../att48.tsp -model_type 0 -time_limit 3600
```

### Supported Arguments
| Argument | Description | Default |
|----------|-------------|---------|
| `-file` / `-f` / `-input` | Path to the `.tsp` input file | `NULL` |
| `-time_limit` | Maximum execution time in seconds | `CPX_INFBOUND` |
| `-model_type` / `-model` | Type of mathematical model to use (0, 1, ...) | `0` |
| `-seed` | Random seed for stochastic components | `0` |
| `-threads` | Number of threads for CPLEX | `0` (auto) |
| `-memory` | Available memory in MB for CPLEX | `12000` |
| `-max_nodes` | Maximum number of branching nodes | `-1` (unlimited) |
| `-cutoff` | Upper bound for the objective function | `CPX_INFBOUND` |
| `-int` | Use integer costs | `0` (false) |
| `-help` | Show available parameters | - |

## Project Structure

- `main.c`: Entry point, handles command-line parsing and high-level execution flow.
- `tsp.c` / `tsp.h`: Core TSP logic, data structures, and CPLEX model implementations.
- `chrono.c` / `chrono.h`: Utility functions for precise timing and performance measurement.
- `att48.tsp`: Sample TSP instance (48 cities in the US).
- `prompts/`: Contains documentation regarding prompt engineering used during development.
- `plot/`: Data and commands for GNUplot visualization.
- `old_template/`: Legacy files and alternative implementations.

## Scripts

- **Build**: Use `cmake --build .` as described in the Setup section.
- **Test**: Currently, no automated test suite is provided. Verification is done by running instances and checking optimality/validity. (See `todo.md` for test development plans).
