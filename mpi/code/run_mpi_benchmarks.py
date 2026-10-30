#!/usr/bin/env python3
"""Run the complete MPI Mandelbrot experiment suite and write canonical CSVs."""

from __future__ import annotations

import argparse
import csv
import os
import platform
import re
import subprocess
import tempfile
from array import array
from collections import defaultdict
from pathlib import Path
from statistics import median


REPETITIONS = 3
STRATEGIES = ("static", "cyclic")
PROCESS_COUNTS = (1, 2, 4, 8, 16)
WEAK_SCALING = (
    (1, 4096),
    (2, 5792),
    (4, 8192),
    (8, 11584),
    (16, 16384),
)

SERIAL_PATTERN = re.compile(
    r"serial_summary case=(?P<case>\S+) size=(?P<size>\d+) "
    r"compute=(?P<compute>[0-9.]+) write=(?P<write>[0-9.]+) "
    r"output_written=(?P<output_written>\d+)"
)
RANK_PATTERN = re.compile(
    r"rank_metric strategy=(?P<strategy>\S+) case=(?P<case>\S+) "
    r"size=(?P<size>\d+) procs=(?P<processes>\d+) rank=(?P<rank>\d+) "
    r"compute_time=(?P<compute>[0-9.]+)"
)
SUMMARY_PATTERN = re.compile(
    r"summary strategy=(?P<strategy>\S+) case=(?P<case>\S+) "
    r"size=(?P<size>\d+) procs=(?P<processes>\d+) "
    r"compute_max=(?P<compute_max>[0-9.]+) compute_min=(?P<compute_min>[0-9.]+) "
    r"gather=(?P<gather>[0-9.]+) master_overhead=(?P<master_overhead>[0-9.]+) "
    r"total=(?P<total>[0-9.]+) fbc=(?P<fbc>[0-9.]+) "
    r"write=(?P<write>[0-9.]+) output_written=(?P<output_written>\d+)"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--mpi-exe", type=Path, required=True)
    parser.add_argument("--serial-exe", type=Path, required=True)
    parser.add_argument("--mpiexec", type=Path, required=True)
    parser.add_argument("--openmp-results", type=Path, required=True)
    return parser.parse_args()


def write_csv(path: Path, rows: list[dict[str, object]], fields: list[str]) -> None:
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def run_command(command: list[str], work_dir: Path, log) -> str:
    printable = subprocess.list2cmdline(command)
    print(f"> {printable}", flush=True)
    log.write(f"> {printable}\n")
    log.flush()
    result = subprocess.run(
        command,
        cwd=work_dir,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    log.write(result.stdout)
    if result.stderr:
        log.write("\n[stderr]\n")
        log.write(result.stderr)
    log.write(f"\n[exit={result.returncode}]\n")
    log.flush()
    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed with exit code {result.returncode}: {printable}\n"
            f"{result.stdout}\n{result.stderr}"
        )
    return result.stdout


def parse_serial(output: str) -> dict[str, object]:
    match = SERIAL_PATTERN.search(output)
    if match is None:
        raise ValueError(f"Serial metrics not found in output:\n{output}")
    values = match.groupdict()
    return {
        "case": values["case"],
        "size": int(values["size"]),
        "compute_s": float(values["compute"]),
        "write_s": float(values["write"]),
        "output_written": int(values["output_written"]),
    }


def parse_mpi(output: str) -> tuple[dict[str, object], list[dict[str, object]]]:
    summary_match = SUMMARY_PATTERN.search(output)
    if summary_match is None:
        raise ValueError(f"MPI summary not found in output:\n{output}")
    values = summary_match.groupdict()
    summary: dict[str, object] = {
        "strategy": values["strategy"],
        "case": values["case"],
        "size": int(values["size"]),
        "processes": int(values["processes"]),
        "compute_max_s": float(values["compute_max"]),
        "compute_min_s": float(values["compute_min"]),
        "gather_s": float(values["gather"]),
        "master_overhead_s": float(values["master_overhead"]),
        "total_s": float(values["total"]),
        "fbc": float(values["fbc"]),
        "write_s": float(values["write"]),
        "output_written": int(values["output_written"]),
    }
    rank_rows = []
    for match in RANK_PATTERN.finditer(output):
        rank = match.groupdict()
        rank_rows.append(
            {
                "strategy": rank["strategy"],
                "case": rank["case"],
                "size": int(rank["size"]),
                "processes": int(rank["processes"]),
                "rank": int(rank["rank"]),
                "compute_s": float(rank["compute"]),
            }
        )
    if len(rank_rows) != summary["processes"]:
        raise ValueError(
            f"Expected {summary['processes']} rank metrics, found {len(rank_rows)}."
        )
    return summary, rank_rows


def compare_outputs(reference: Path, candidate: Path, pixel_count: int) -> tuple[int, int]:
    expected_bytes = pixel_count * 4
    if reference.stat().st_size != expected_bytes:
        raise ValueError(f"Unexpected reference size: {reference}")
    if candidate.stat().st_size != expected_bytes:
        raise ValueError(f"Unexpected candidate size: {candidate}")

    different = 0
    max_difference = 0
    chunk_bytes = 4 * 1024 * 1024
    with reference.open("rb") as reference_stream, candidate.open("rb") as candidate_stream:
        while True:
            reference_chunk = reference_stream.read(chunk_bytes)
            candidate_chunk = candidate_stream.read(chunk_bytes)
            if not reference_chunk:
                break
            if reference_chunk == candidate_chunk:
                continue
            reference_values = array("i")
            candidate_values = array("i")
            reference_values.frombytes(reference_chunk)
            candidate_values.frombytes(candidate_chunk)
            for expected, actual in zip(reference_values, candidate_values):
                difference = abs(expected - actual)
                if difference:
                    different += 1
                    if difference > max_difference:
                        max_difference = difference
    return different, max_difference


def summarize_strong(
    raw_rows: list[dict[str, object]], serial_rows: list[dict[str, object]]
) -> list[dict[str, object]]:
    serial_median = median(
        float(row["compute_s"])
        for row in serial_rows
        if row["case"] == "standard"
    )
    grouped: dict[tuple[str, int], list[dict[str, object]]] = defaultdict(list)
    for row in raw_rows:
        if row["suite"] == "strong_scaling":
            grouped[(str(row["strategy"]), int(row["processes"]))].append(row)

    summaries = []
    for (strategy, processes), rows in sorted(grouped.items()):
        total = median(float(row["total_s"]) for row in rows)
        speedup = serial_median / total
        summaries.append(
            {
                "strategy": strategy,
                "processes": processes,
                "serial_median_s": f"{serial_median:.9f}",
                "total_median_s": f"{total:.9f}",
                "compute_median_s": f"{median(float(row['compute_max_s']) for row in rows):.9f}",
                "gather_median_s": f"{median(float(row['gather_s']) for row in rows):.9f}",
                "master_overhead_median_s": f"{median(float(row['master_overhead_s']) for row in rows):.9f}",
                "speedup": f"{speedup:.6f}",
                "efficiency": f"{speedup / processes:.6f}",
                "efficiency_percent": f"{100.0 * speedup / processes:.3f}",
            }
        )
    return summaries


def summarize_weak(raw_rows: list[dict[str, object]]) -> list[dict[str, object]]:
    grouped: dict[tuple[str, int, int], list[float]] = defaultdict(list)
    for row in raw_rows:
        if row["suite"] == "weak_scaling":
            grouped[(str(row["strategy"]), int(row["processes"]), int(row["size"]))].append(
                float(row["total_s"])
            )
    baselines = {
        strategy: median(values)
        for (strategy, processes, _), values in grouped.items()
        if processes == 1
    }
    summaries = []
    for (strategy, processes, size), values in sorted(grouped.items()):
        elapsed = median(values)
        weak_efficiency = baselines[strategy] / elapsed
        summaries.append(
            {
                "strategy": strategy,
                "processes": processes,
                "size": size,
                "pixels": size * size,
                "pixels_per_process": size * size // processes,
                "total_median_s": f"{elapsed:.9f}",
                "scaled_speedup": f"{processes * weak_efficiency:.6f}",
                "weak_efficiency": f"{weak_efficiency:.6f}",
                "weak_efficiency_percent": f"{100.0 * weak_efficiency:.3f}",
            }
        )
    return summaries


def summarize_balance(
    raw_rows: list[dict[str, object]], rank_rows: list[dict[str, object]]
) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    run_grouped: dict[tuple[str, int, int], list[float]] = defaultdict(list)
    rank_grouped: dict[tuple[str, int, int], list[float]] = defaultdict(list)
    for row in rank_rows:
        if row["suite"] == "load_balance":
            strategy = str(row["strategy"])
            processes = int(row["processes"])
            run_grouped[(strategy, processes, int(row["rep"]))].append(
                float(row["compute_s"])
            )
            rank_grouped[(strategy, processes, int(row["rank"]))].append(
                float(row["compute_s"])
            )

    process_summaries = [
        {
            "strategy": strategy,
            "processes": processes,
            "rank": rank,
            "compute_median_s": f"{median(values):.9f}",
        }
        for (strategy, processes, rank), values in sorted(rank_grouped.items())
    ]

    grouped: dict[tuple[str, int], list[dict[str, object]]] = defaultdict(list)
    for row in raw_rows:
        if row["suite"] == "load_balance":
            grouped[(str(row["strategy"]), int(row["processes"]))].append(row)
    summaries = []
    for (strategy, processes), rows in sorted(grouped.items()):
        run_times = [
            values
            for (current_strategy, current_processes, _), values
            in run_grouped.items()
            if current_strategy == strategy and current_processes == processes
        ]
        minimums = [min(values) for values in run_times]
        maximums = [max(values) for values in run_times]
        factors = [
            (maximum - minimum) / maximum if maximum > 0.0 else 0.0
            for minimum, maximum in zip(minimums, maximums)
        ]
        summaries.append(
            {
                "strategy": strategy,
                "processes": processes,
                "size": int(rows[0]["size"]),
                "compute_min_median_s": f"{median(minimums):.9f}",
                "compute_max_median_s": f"{median(maximums):.9f}",
                "fbc_median": f"{median(factors):.9f}",
                "gather_median_s": f"{median(float(row['gather_s']) for row in rows):.9f}",
                "master_overhead_median_s": f"{median(float(row['master_overhead_s']) for row in rows):.9f}",
                "total_median_s": f"{median(float(row['total_s']) for row in rows):.9f}",
            }
        )

    return summaries, process_summaries


def make_openmp_comparison(
    strong_rows: list[dict[str, object]], openmp_results: Path
) -> list[dict[str, object]]:
    openmp_path = openmp_results / "summary_standard_scaling.csv"
    with openmp_path.open(newline="", encoding="utf-8-sig") as stream:
        openmp_rows = {int(row["threads"]): row for row in csv.DictReader(stream)}
    comparisons = []
    for row in strong_rows:
        processes = int(row["processes"])
        openmp = openmp_rows[processes]
        comparisons.append(
            {
                "resources": processes,
                "mpi_strategy": row["strategy"],
                "mpi_time_s": row["total_median_s"],
                "mpi_speedup": row["speedup"],
                "mpi_efficiency_percent": row["efficiency_percent"],
                "openmp_time_s": openmp["parallel_median_s"],
                "openmp_speedup": openmp["speedup"],
                "openmp_efficiency_percent": openmp["efficiency_percent"],
            }
        )
    return comparisons


def main() -> int:
    args = parse_args()
    output_dir = args.output_dir.resolve()
    if output_dir.exists() and any(output_dir.iterdir()):
        raise ValueError(f"Output directory must be empty: {output_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)

    mpi_exe = args.mpi_exe.resolve()
    serial_exe = args.serial_exe.resolve()
    mpiexec = args.mpiexec.resolve()
    openmp_results = args.openmp_results.resolve()
    for path in (mpi_exe, serial_exe, mpiexec):
        if not path.is_file():
            raise FileNotFoundError(path)

    available = os.cpu_count() or 1
    process_counts = tuple(count for count in PROCESS_COUNTS if count <= available)
    max_processes = max(process_counts)
    weak_scaling = tuple(item for item in WEAK_SCALING if item[0] <= available)

    serial_rows: list[dict[str, object]] = []
    raw_rows: list[dict[str, object]] = []
    rank_rows: list[dict[str, object]] = []
    correctness_rows: list[dict[str, object]] = []

    log_path = output_dir / "benchmark_run.log"
    with log_path.open("w", encoding="utf-8") as log, tempfile.TemporaryDirectory(
        prefix="mandelbrot_mpi_"
    ) as temp_name:
        work_dir = Path(temp_name)
        binary_dir = work_dir / "output_binaries"
        binary_dir.mkdir()
        references: dict[str, Path] = {}

        for case in ("standard", "seahorse"):
            for repetition in range(1, REPETITIONS + 1):
                output_name = f"serial_{case}.bin" if repetition == 1 else "-"
                output = run_command(
                    [str(serial_exe), case, "4096", output_name], work_dir, log
                )
                row = parse_serial(output)
                row["rep"] = repetition
                serial_rows.append(row)
                if repetition == 1:
                    references[case] = binary_dir / output_name

        def execute_mpi(
            suite: str,
            strategy: str,
            case: str,
            size: int,
            processes: int,
            repetition: int,
            correctness: bool,
        ) -> None:
            output_name = (
                f"{suite}_{strategy}_{case}_{size}_{processes}.bin"
                if correctness
                else "-"
            )
            output = run_command(
                [
                    str(mpiexec), "-n", str(processes), str(mpi_exe),
                    strategy, case, str(size), output_name,
                ],
                work_dir,
                log,
            )
            summary, ranks = parse_mpi(output)
            summary["suite"] = suite
            summary["rep"] = repetition
            raw_rows.append(summary)
            for rank in ranks:
                rank["suite"] = suite
                rank["rep"] = repetition
                rank_rows.append(rank)

            if correctness:
                candidate = binary_dir / output_name
                different, max_difference = compare_outputs(
                    references[case], candidate, size * size
                )
                allowed_different = size * size // 10000
                if different == 0:
                    status = "APPROVED_EXACT"
                elif different <= allowed_different and max_difference <= 1:
                    status = "APPROVED_TOLERANCE"
                else:
                    status = "REJECTED"
                correctness_rows.append(
                    {
                        "case": case,
                        "strategy": strategy,
                        "size": size,
                        "processes": processes,
                        "different_pixels": different,
                        "max_abs_difference": max_difference,
                        "allowed_different_pixels": allowed_different,
                        "allowed_max_abs_difference": 1,
                        "status": status,
                    }
                )
                print(
                    f"correctness: {case} {strategy} p={processes}: "
                    f"{status}, {different} differing pixels",
                    flush=True,
                )

        for strategy in STRATEGIES:
            for processes in process_counts:
                for repetition in range(1, REPETITIONS + 1):
                    execute_mpi(
                        "strong_scaling", strategy, "standard", 4096,
                        processes, repetition, repetition == 1,
                    )

        for strategy in STRATEGIES:
            for processes in process_counts:
                for repetition in range(1, REPETITIONS + 1):
                    execute_mpi(
                        "load_balance", strategy, "seahorse", 4096,
                        processes, repetition, repetition == 1,
                    )

        for strategy in STRATEGIES:
            for processes, size in weak_scaling:
                for repetition in range(1, REPETITIONS + 1):
                    execute_mpi(
                        "weak_scaling", strategy, "standard", size,
                        processes, repetition, False,
                    )

    if any(row["status"] == "REJECTED" for row in correctness_rows):
        raise RuntimeError("At least one correctness configuration was rejected.")

    serial_fields = ["case", "size", "rep", "compute_s", "write_s", "output_written"]
    raw_fields = [
        "case", "suite", "strategy", "size", "processes", "rep",
        "compute_max_s", "compute_min_s", "gather_s", "master_overhead_s",
        "total_s", "fbc", "write_s", "output_written",
    ]
    rank_fields = [
        "case", "suite", "strategy", "size", "processes", "rep", "rank",
        "compute_s",
    ]
    correctness_fields = [
        "case", "strategy", "size", "processes", "different_pixels",
        "max_abs_difference", "allowed_different_pixels",
        "allowed_max_abs_difference", "status",
    ]
    write_csv(output_dir / "serial_timings.csv", serial_rows, serial_fields)
    write_csv(output_dir / "raw_timings.csv", raw_rows, raw_fields)
    write_csv(output_dir / "process_timings.csv", rank_rows, rank_fields)
    write_csv(output_dir / "correctness.csv", correctness_rows, correctness_fields)

    strong_rows = summarize_strong(raw_rows, serial_rows)
    weak_rows = summarize_weak(raw_rows)
    balance_rows, process_summary_rows = summarize_balance(raw_rows, rank_rows)
    comparison_rows = make_openmp_comparison(strong_rows, openmp_results)

    write_csv(
        output_dir / "summary_strong_scaling.csv",
        strong_rows,
        [
            "strategy", "processes", "serial_median_s", "total_median_s",
            "compute_median_s", "gather_median_s",
            "master_overhead_median_s", "speedup", "efficiency",
            "efficiency_percent",
        ],
    )
    write_csv(
        output_dir / "summary_weak_scaling.csv",
        weak_rows,
        [
            "strategy", "processes", "size", "pixels", "pixels_per_process",
            "total_median_s", "scaled_speedup", "weak_efficiency",
            "weak_efficiency_percent",
        ],
    )
    write_csv(
        output_dir / "summary_load_balance.csv",
        balance_rows,
        [
            "strategy", "processes", "size", "compute_min_median_s",
            "compute_max_median_s", "fbc_median", "gather_median_s",
            "master_overhead_median_s", "total_median_s",
        ],
    )
    write_csv(
        output_dir / "seahorse_process_times.csv",
        process_summary_rows,
        ["strategy", "processes", "rank", "compute_median_s"],
    )
    write_csv(
        output_dir / "comparison_openmp_mpi.csv",
        comparison_rows,
        [
            "resources", "mpi_strategy", "mpi_time_s", "mpi_speedup",
            "mpi_efficiency_percent", "openmp_time_s", "openmp_speedup",
            "openmp_efficiency_percent",
        ],
    )

    with (output_dir / "AMBIENTE.txt").open("w", encoding="utf-8") as stream:
        stream.write(f"Sistema operacional: {platform.platform()}\n")
        stream.write(f"Processador: {platform.processor() or os.environ.get('PROCESSOR_IDENTIFIER', 'Nao informado')}\n")
        stream.write(f"Processadores logicos: {available}\n")
        stream.write("Compilador: MSVC 19.44.35228.0, /O2 /W4 /WX\n")
        stream.write("MPI: Microsoft MPI 10.1.12498.18 (runtime e SDK)\n")
        stream.write(f"Repeticoes por configuracao: {REPETITIONS}\n")
        stream.write(f"Processos: {', '.join(map(str, process_counts))}\n")
        stream.write("Estrategias: static (faixas contiguas), cyclic (linhas ciclicas)\n")
        stream.write("FBC: (T_max - T_min) / T_max; 0 representa equilibrio ideal\n")
        stream.write("Tempo total: T_max de computacao + Gather + sobrecarga do mestre; escrita excluida\n")

    print(
        f"Completed: {len(correctness_rows)} correctness checks, "
        f"{len(raw_rows)} MPI runs, {len(serial_rows)} serial runs.",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
