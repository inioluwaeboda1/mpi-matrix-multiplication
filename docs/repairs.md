# MPI Repair Record

## Historical design retained

`src/matrix_mpi.c` retains the final archived implementation's central design:

- Contiguous storage for each matrix with a row-pointer view.
- Full allocation and initialization of A, B, and C on rank 0.
- Full allocation of B on every rank.
- Uneven row distribution when the matrix dimension is not divisible by the number of ranks.
- Explicit point-to-point distribution of A from rank 0.
- Broadcast of B to all ranks.
- Optional transposition of B before multiplication.
- Per-rank validation followed by an `MPI_MAX` reduction.
- Timing of the measured parallel phase, including communication, computation, and validation. Allocation and matrix initialization remain outside the timer.

## Functional repairs

Every functional difference from the final archived `mmmpi.c` is listed below.

### 1. Corrected the MPI reduction send buffer

The archived code calls:

```c
MPI_Reduce(mismatch, &global_result, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
```

`MPI_Reduce` requires an address for its send buffer. The reconstructed version passes `&local_status`. This matches the earlier Lab 1 implementation and all three later Lab 2 MPI/OpenMP descendants.

### 2. Added chunked broadcast for large matrices

The archived implementation passes `n * n` directly to the integer `count` parameter of `MPI_Bcast`. The course specification explicitly warned that this count can exceed the signed integer limit, and later preserved descendants broadcast large matrices in blocks.

The reconstructed implementation walks the contiguous B allocation in chunks of at most `INT_MAX` elements. For the historical 10,000 and 20,000 matrix runs, this does not change the communicated data.

### 3. Added checked contiguous allocation

The archived allocator did not check negative dimensions, multiplication overflow, or allocation failure. The reconstructed allocator:

- Calculates element counts with `size_t`.
- Rejects size overflows before calling `malloc`.
- Returns failure when either allocation fails.
- Handles a zero-row allocation without dereferencing row zero.
- Verifies the rank-specific pointer invariants before any allocated storage is used; zero-row worker ranks are still permitted to have no local A or C storage.

All ranks collectively check whether their initial allocations succeeded. A failure terminates the MPI job rather than allowing only some ranks to continue.

### 4. Added safe MPI count checks for row distribution

`MPI_Send` and `MPI_Recv` also use integer element counts. The reconstructed implementation checks each local row block before converting its size to `int`. It aborts with an error if a block would exceed `INT_MAX` elements.

### 5. Added coordinated argument validation

The archived program calls `exit` after `MPI_Init` when arguments are missing. The reconstructed version validates the matrix size and transpose flag on every rank, reports usage from rank 0, calls `MPI_Finalize`, and exits cleanly.

The accepted transpose values are explicitly limited to `0` and `1`.

### 6. Handled zero-row ranks

The reconstructed code skips point-to-point communication and multiplication for ranks assigned zero rows. This prevents dereferencing `A[0]` when the number of MPI ranks exceeds the matrix dimension.

### 7. Removed the unused OpenMP dependency

The archived Lab 1 MPI source includes `<omp.h>` but contains no OpenMP directives or calls. The portfolio version removes that include so the MPI-only program does not require an unrelated OpenMP header.

### 8. Added POSIX feature selection and clearer errors

The source defines `_POSIX_C_SOURCE` before including headers so `clock_gettime` and `gethostname` are declared consistently by conforming C toolchains. Allocation and MPI-count failures now identify the failing operation.

### 9. Propagated the verification result to every rank

`MPI_Reduce` only returns its result to rank 0. The portfolio program returns a process status derived from the job-wide verification result, so it broadcasts that result after the reduction. This ensures every rank exits successfully only when every local matrix block passed validation.

The elapsed-time clock is stopped before this added status broadcast so the historical timing boundary still ends with the correctness reduction.

### 10. Prevented overflow in the row-count calculation

The archived distribution logic always calculates `small_row_count + 1`, even when the matrix divides evenly among ranks. At the largest accepted integer input with one rank, that unused addition can overflow a signed `int` before allocation validation occurs.

The portfolio version increments the large-row count only when a remainder exists. This preserves the original uneven row distribution for all valid experiment configurations while removing the overflow.

### 11. Added range-aware integer parsing

The portfolio argument parser now clears `errno` before each `strtol` call and rejects `ERANGE`. Both the positive matrix dimension and the `0`/`1` transpose flag are validated before use.

### 12. Added conservative MPI return-value handling

The portfolio version requests `MPI_ERRORS_RETURN` for `MPI_COMM_WORLD`, checks the return values of communicator setup, collectives, point-to-point operations, and finalization, and reports returned MPI errors before aborting the job. Program-detected fatal conditions also call `MPI_Abort` and then terminate if the abort call unexpectedly returns.

These checks do not change the historical communication pattern: A is still distributed with explicit sends and receives, B is still broadcast, and correctness is still reduced to rank 0.

## Sequential portfolio hardening

The final packaged `mm.c` and the working `mm2I.c` did not check allocation results. The portfolio sequential implementations now:

- Reject invalid or overflowing allocation dimensions.
- Check the row-pointer and every row allocation.
- Release any rows already allocated when a later row allocation fails.
- Report the affected matrix size, release all completed matrices, and exit with failure.
- Propagate failure when the cache-friendly implementation cannot allocate its transposed matrix.

This is portfolio hardening, not original historical behavior. Successful-run algorithms, matrix sizes, initialization, multiplication order, and timing boundaries remain unchanged.

## Changes intentionally not made

- The explicit send/receive design was not replaced with `MPI_Scatterv`.
- Rank 0 still allocates full A and C matrices, matching the archived design.
- C is not gathered because the original program validates each local block and reduces only the validation status.
- The constant A matrix and identity B matrix are retained for historical comparability.
- Timing still includes B broadcast, A distribution, multiplication, and correctness reduction.
- The multiplication order and arithmetic are unchanged.
- The sequential programs still do not add a checksum or correctness pass after timing. That remains a potential future testing enhancement so the historical benchmark behavior is not silently changed.

## Verification status

In 2026, the repaired MPI implementation compiled cleanly with strict warning flags on an Apple Silicon Mac using Open MPI 5.0.9. Local functional tests passed for:

- Single-rank and multi-rank execution.
- Transpose disabled and enabled.
- Uneven, evenly divided, and zero-row rank distributions.
- Invalid matrix-size and transpose-flag inputs.
- Representative UndefinedBehaviorSanitizer runs.

AddressSanitizer execution stalled in the local Open MPI runtime before program output and produced no sanitizer defect report, so ASan validation remains inconclusive. Multi-node validation under an HPC scheduler and historical benchmark reproduction remain unverified. The completed tests establish local functional validation of the 2026 portfolio reconstruction only; they do not validate the original Titan HPC cluster measurements.
