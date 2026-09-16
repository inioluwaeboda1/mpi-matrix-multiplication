# Benchmark Data

`benchmarks.csv` is a normalized transcription of the preserved `mmResultsExcel.xlsx` workbook. Runtime values and units are copied without conversion or interpolation.

## Columns

| Column | Meaning |
| --- | --- |
| `benchmark_id` | Stable identifier assigned during portfolio curation |
| `environment` | `local_pc` or `titan_sandy_bridge`, the preserved Titan HPC cluster environment label |
| `implementation` | Sequential, transposed dot product, or MPI |
| `configuration` | Human-readable historical configuration label |
| `compiler` | Compiler recorded in the source materials |
| `compiler_flags` | Flags explicitly established by the workbook or accompanying project documentation |
| `matrix_size` | Dimension of the square matrices |
| `node_count` | Number of cluster nodes when established |
| `mpi_ranks_per_node` | MPI processes assigned to each node |
| `total_mpi_ranks` | Total MPI processes in the run |
| `transpose` | Whether the transposed multiplication path was used |
| `runtime` | Preserved measured runtime |
| `runtime_unit` | Milliseconds or minutes, as recorded |
| `source_sheet` | Workbook sheet containing the value |
| `source_row` | Workbook row containing the value |
| `notes` | Normalization or historical context |

## MPI terminology normalization

The original workbook's “Thread count” field is inconsistent across blocks. The CSV resolves it as follows:

- Rows 9–10: ranks per node on eight nodes.
- Rows 12–17 and 26–31: one rank per node.
- Rows 19–24 and 33–38: total ranks for 16 ranks per node.
- Rows 40–42: total ranks for 16 ranks per node.

This interpretation is supported by the experiment descriptions and by the exact products of node count and 16. Blank cells mean the source does not establish the value.

## Chart generation

All images in `results/charts/` are generated from `benchmarks.csv` by:

```bash
python3 scripts/plot_benchmarks.py
```

PNG files are used in the README. Matching SVG files are also generated for scalable reuse.
