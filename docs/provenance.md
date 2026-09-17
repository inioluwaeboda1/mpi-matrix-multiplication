# Provenance

## Project origin

This repository is a portfolio reconstruction of the CSC 462 matrix multiplication coursework originally completed collaboratively by **Inioluwa Eboda and Suwilanji Mwanza** during Spring 2025.

The original project studied performance and scalability through three implementations:

1. A baseline sequential matrix multiplication program.
2. A cache-friendly sequential version using matrix transposition and dot products.
3. An MPI implementation that distributed rows of matrix A and broadcast matrix B.

The collaborators also collected local measurements and measurements on Titan, Oral Roberts University's HPC cluster, analyzed the results, and prepared a report and presentation.

## Sources used in this reconstruction

The portfolio repository was derived from four preserved artifacts:

- The final packaged `mm.c`, used as the historical basis for `src/matrix_serial.c`.
- The earlier working `mm2I.c`, used as the basis for `src/matrix_transposed.c`. The packaged `mm2.c` was not used because it renamed its multiplication function without updating the call site.
- The final packaged `mmmpi.c`, used as the historical design basis for `src/matrix_mpi.c`.
- The final `mmResultsExcel.xlsx`, used to construct `results/benchmarks.csv`.

The archived MPI implementation was compared with earlier Lab 1 versions and later Lab 2 MPI/OpenMP descendants to identify conservative repairs. The final packaged MPI file is the version most clearly associated with the submission bundle, but it contains a broken `MPI_Reduce` call and cannot be the exact runnable source that produced the preserved results.

## Course scaffold influence

The original MPI implementation was developed within the structure of instructor/course-provided breadcrumb scaffolding. The original breadcrumb/scaffold file itself is not included in this repository. The scaffold influenced portions of the program structure and MPI workflow. The completed computational logic, data-distribution implementation, correctness work, and experiments were collaborative student work; the later portfolio hardening is subsequent portfolio engineering. This disclosure acknowledges provenance and does not claim ownership of instructor-provided material.

## Original coursework versus portfolio revisions

### Original collaborative coursework

- Algorithm progression and experimental design.
- Sequential, transposed, and MPI implementation concepts.
- Local and Titan HPC cluster benchmark measurements.
- Compiler and multi-node comparisons.
- Original analysis of scaling and diminishing returns.

### Portfolio curation and engineering revisions

- Descriptive filenames and repository organization.
- Identifier, formatting, and portability cleanup in the two sequential sources, plus portfolio-only allocation-failure handling. Their algorithms, matrix sizes, initialization, and successful-run timing boundaries remain unchanged.
- Contributor and provenance documentation.
- A normalized benchmark CSV with explicit units and MPI terminology.
- Reproducible charts generated from the CSV.
- Conservative repairs to the preserved MPI source.
- Portability, argument-validation, allocation, and MPI count checks.
- Documentation of uncertainties and limitations.

The reconstructed MPI implementation is a maintained portfolio artifact, not an untouched historical submission. It must not be described as the exact program used for the historical measurements.

## 2026 local portfolio validation

After reconstruction, `src/matrix_mpi.c` was compiled with strict warning flags and functionally tested on an Apple Silicon Mac using Open MPI 5.0.9. Passing tests covered single-rank and multi-rank execution, both multiplication kernels, uneven and evenly divided row distributions, zero-row ranks, invalid arguments, and representative UndefinedBehaviorSanitizer runs.

AddressSanitizer execution stalled in the local Open MPI runtime before program output and produced no sanitizer defect report, so ASan validation remains inconclusive. This testing belongs to the subsequent 2026 portfolio engineering work; it is not evidence that the reconstructed source produced, reproduces, or independently validates the original Titan HPC cluster measurements.

## Benchmark normalization decisions

The workbook uses a column labeled “Thread count,” but its meaning changes between experiment blocks:

- In the eight-node comparison, values 1 and 16 represent MPI ranks per node.
- In the one-rank-per-node scaling blocks, the value remains 1 while node count changes.
- In the 16-ranks-per-node blocks, the values are total MPI ranks: 16, 32, 64, 128, 256, and 320.
- In the 20,000-matrix block, values 512, 768, and 1,024 are total MPI ranks on 32, 48, and 64 nodes.

The CSV therefore records both `mpi_ranks_per_node` and `total_mpi_ranks`. Derived totals use only the experiment structure established by the workbook and the accompanying final analysis. Historical runtime values were not transformed.

The workbook labels the second MPI configuration as `mpiicx - mavx`. The full compiler flags are recorded as `-mavx -mtune=sandybridge -Ofast` because the preserved project documentation gives that exact command.

## Excluded source material

This repository intentionally excludes:

- The original instructor handouts, breadcrumb/scaffold file, and workbook templates. Their exclusion does not imply that the scaffold had no influence on the archived student MPI implementation.
- Textbooks, course PDFs, the syllabus, and course calendars.
- Submission ZIP archives and duplicate source copies.
- Titan HPC cluster account details, cluster paths, and MobaXterm material.
- The raw report, which contains embedded personal SharePoint relationships.
- Original presentation assets and template graphics.

Charts in this repository are newly generated from the normalized CSV.

## Public-release requirements

Before publication:

- Both original contributors should confirm the attribution and intended license.
- Multi-node execution should be validated separately before making any reproduction or portability claim beyond the completed local Open MPI tests.
- New benchmark results, if added, must be kept separate from the historical measurements.
- Any claim of reproduction should identify the cluster, compiler, MPI implementation, rank placement, and repeated-trial methodology.
