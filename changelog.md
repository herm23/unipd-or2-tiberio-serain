# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
