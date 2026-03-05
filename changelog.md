# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.3] - 2026-03-05
### Added
- Native support for Apple Silicon (macOS) in `CMakeLists.txt` for IBM CPLEX.
- Support for specifying `CPLEX_ROOT` via CMake command-line argument (`-DCPLEX_ROOT="..."`).

### Changed
- Re-enabled `TSPopt` optimization engine in `main.c`.

## [0.2.2] - 2026-03-04
### Added
- Javadoc-style documentation for all functions in `main.c`, `tsp.c`, and `tsp.h`.
- Detailed verbosity level descriptions at the top of `main.c`.
- `//@COMPLEX@` tags to highlight and explain complex algorithmic sections.
- Logical segmentation of `main.c` using IDE-compatible `#region` tags.

### Changed
- Standardized function documentation with `@brief`, `@param`, and `@return` tags.
- Organized `main.c` declarations and implementations into distinct regions (PARSER, INPUT READER, etc.).

## [0.2.1] - 2026-03-01
### Fixed
- Fixed memory management in `free_instance` by commenting out unallocated memory `free` calls.

### Changed
- Updated `CPLEX_ROOT` in `CMakeLists.txt` for standard IBM CPLEX installation path.
- Adjusted `main.c` flow: temporarily disabled `TSPopt` and added tour plotting for verification.

## [0.2.0] - 2026-03-01
### Added
- Integrated **IBM ILOG CPLEX** for optimization.
- Implemented **Miller-Tucker-Zemlin (MTZ)** subtour elimination constraints.
- Added support for reading **TSPLIB** formatted files (`.tsp`).
- Added command-line argument parsing for optimization control (`-time_limit`, `-model_type`, `-seed`, etc.).
- Improved visualization with **GNUplot** via `plot_tour` function.
- Added high-precision timing utilities in `chrono.c`.
- Created basic `README.md` with usage instructions and project overview.

## [0.1.0] - 2026-02-28
### Added
- Initial project structure with `main.c`, `tsp.h`, and `tsp.c`.
- Basic data structures for TSP instances.
- Placeholder for distance calculation and solution initialization.
- Added sample instance `att48.tsp`.
- Legacy files preserved in `old_template/`.
