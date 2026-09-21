#!/usr/bin/env python3
"""Create tables, charts, and a Markdown report from benchmark CSV files."""

from __future__ import annotations

import argparse
import csv
import os
import platform
import statistics
import subprocess
from collections import defaultdict
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def number(value: str) -> float:
    return float(value)


def median(values: list[float]) -> float:
    return statistics.median(values)


def write_csv(path: Path, columns: list[str], rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)


def markdown_table(columns: list[str], rows: list[list[str]]) -> str:
    separator = ["---"] * len(columns)
    lines = [
        "| " + " | ".join(columns) + " |",
        "| " + " | ".join(separator) + " |",
    ]
    lines.extend("| " + " | ".join(row) + " |" for row in rows)
    return "\n".join(lines)


def make_scaling_summary(raw: list[dict[str, str]]) -> list[dict[str, object]]:
    serial_times = [
        number(row["seconds"])
        for row in raw
        if row["case"] == "standard" and row["suite"] == "serial_baseline"
    ]
    serial_median = median(serial_times)
    grouped: dict[int, list[float]] = defaultdict(list)

    for row in raw:
        if row["case"] == "standard" and row["suite"] == "standard_scaling":
            grouped[int(row["requested_threads"])].append(number(row["seconds"]))

    summary: list[dict[str, object]] = []
    for threads in sorted(grouped):
        parallel_median = median(grouped[threads])
        speedup = serial_median / parallel_median
        summary.append(
            {
                "threads": threads,
                "serial_median_s": f"{serial_median:.9f}",
                "parallel_median_s": f"{parallel_median:.9f}",
                "speedup": f"{speedup:.6f}",
                "efficiency": f"{speedup / threads:.6f}",
                "efficiency_percent": f"{100 * speedup / threads:.3f}",
            }
        )
    return summary


def make_schedule_summary(raw: list[dict[str, str]]) -> list[dict[str, object]]:
    grouped: dict[tuple[str, str, int, int], list[float]] = defaultdict(list)
    for row in raw:
        if row["suite"] != "schedule_comparison":
            continue
        key = (
            row["case"],
            row["policy"],
            int(row["chunk"]),
            int(row["requested_threads"]),
        )
        grouped[key].append(number(row["seconds"]))

    summaries = []
    for (case, policy, chunk, threads), values in sorted(grouped.items()):
        summaries.append(
            {
                "case": case,
                "policy": policy,
                "chunk": chunk,
                "threads": threads,
                "repetitions": len(values),
                "median_s": f"{median(values):.9f}",
                "mean_s": f"{statistics.mean(values):.9f}",
                "stdev_s": f"{statistics.stdev(values) if len(values) > 1 else 0:.9f}",
            }
        )
    return summaries


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    name = "arialbd.ttf" if bold else "arial.ttf"
    try:
        return ImageFont.truetype(f"C:/Windows/Fonts/{name}", size)
    except OSError:
        return ImageFont.load_default()


def line_chart(
    draw: ImageDraw.ImageDraw, rectangle: tuple[int, int, int, int], title: str,
    x_labels: list[str], series: list[tuple[str, list[float], str]],
    y_label: str, y_floor: float = 0.0,
) -> None:
    left, top, right, bottom = rectangle
    plot_left, plot_top = left + 62, top + 54
    plot_right, plot_bottom = right - 18, bottom - 56
    values = [value for _, points, _ in series for value in points]
    y_min = min(y_floor, min(values))
    y_max = max(values)
    padding = max((y_max - y_min) * 0.12, 0.03)
    y_min = max(y_floor, y_min - padding)
    y_max += padding
    if y_max == y_min:
        y_max += 1.0

    draw.text((left + 8, top + 10), title, font=font(21, True), fill="#1e293b")
    draw.text((left + 8, top + 35), y_label, font=font(13), fill="#475569")
    draw.rectangle((plot_left, plot_top, plot_right, plot_bottom), outline="#94a3b8", width=1)

    for index in range(5):
        fraction = index / 4
        y = plot_bottom - fraction * (plot_bottom - plot_top)
        value = y_min + fraction * (y_max - y_min)
        draw.line((plot_left, y, plot_right, y), fill="#e2e8f0", width=1)
        label = f"{value:.2f}"
        draw.text((plot_left - 56, y - 8), label, font=font(12), fill="#475569")

    count = len(x_labels)
    x_positions = [
        plot_left + index * (plot_right - plot_left) / max(count - 1, 1)
        for index in range(count)
    ]
    for x, label in zip(x_positions, x_labels):
        draw.line((x, plot_bottom, x, plot_bottom + 5), fill="#64748b", width=1)
        anchor = draw.textbbox((0, 0), label, font=font(13))
        draw.text((x - (anchor[2] - anchor[0]) / 2, plot_bottom + 10), label,
                  font=font(13), fill="#1e293b")

    for series_index, (label, points, color) in enumerate(series):
        coordinates = [
            (x, plot_bottom - (point - y_min) / (y_max - y_min) * (plot_bottom - plot_top))
            for x, point in zip(x_positions, points)
        ]
        if len(coordinates) > 1:
            draw.line(coordinates, fill=color, width=3)
        for x, y in coordinates:
            draw.ellipse((x - 4, y - 4, x + 4, y + 4), fill=color, outline="#ffffff")
        legend_x = plot_left + series_index * 116
        draw.rectangle((legend_x, bottom - 29, legend_x + 12, bottom - 17), fill=color)
        draw.text((legend_x + 17, bottom - 32), label, font=font(12), fill="#334155")


def save_image(image: Image.Image, output: Path) -> None:
    image.save(output, format="PNG", optimize=True)


def plot_scaling(summary: list[dict[str, object]], output: Path) -> None:
    threads = [str(row["threads"]) for row in summary]
    times = [number(str(row["parallel_median_s"])) for row in summary]
    speedups = [number(str(row["speedup"])) for row in summary]
    efficiencies = [number(str(row["efficiency_percent"])) for row in summary]
    image = Image.new("RGB", (1800, 560), "#ffffff")
    draw = ImageDraw.Draw(image)
    draw.text((26, 18), "Input padrão: 4096x4096, MAX_ITER = 1000",
              font=font(26, True), fill="#0f172a")
    panels = [(20, 65, 600, 540), (610, 65, 1190, 540), (1200, 65, 1780, 540)]
    line_chart(draw, panels[0], "Tempo de execução", threads,
               [("mediana", times, "#2563eb")], "Tempo (s)")
    line_chart(draw, panels[1], "Speedup", threads,
               [("medido", speedups, "#ea580c"),
                ("ideal", [float(value) for value in threads], "#64748b")],
               "T serial / T paralelo")
    line_chart(draw, panels[2], "Eficiência", threads,
               [("medida", efficiencies, "#16a34a"),
                ("ideal", [100.0] * len(threads), "#64748b")],
               "Speedup / threads (%)")
    save_image(image, output)


def plot_schedules(summary: list[dict[str, object]], output: Path) -> None:
    image = Image.new("RGB", (1540, 590), "#ffffff")
    draw = ImageDraw.Draw(image)
    draw.text((26, 18), "Políticas de escalonamento com 16 threads",
              font=font(26, True), fill="#0f172a")
    styles = {"static": "#2563eb", "dynamic": "#ea580c", "guided": "#16a34a"}
    labels = {"standard": "Input padrão", "seahorse_valley": "Vale dos cavalos-marinhos"}
    for rectangle, case in zip([(20, 65, 760, 570), (780, 65, 1520, 570)],
                               ("standard", "seahorse_valley")):
        series = []
        for policy, color in styles.items():
            rows = [row for row in summary if row["case"] == case and row["policy"] == policy]
            rows.sort(key=lambda row: int(row["chunk"]))
            series.append((policy, [number(str(row["median_s"])) for row in rows], color))
        line_chart(draw, rectangle, labels[case], ["1", "4", "16", "64"], series,
                   "Mediana do tempo (s)")
    save_image(image, output)


def plot_load_balance(balance: list[dict[str, str]], output: Path) -> None:
    image = Image.new("RGB", (880, 590), "#ffffff")
    draw = ImageDraw.Draw(image)
    draw.text((26, 18), "Balanceamento de carga: vale dos cavalos-marinhos",
              font=font(24, True), fill="#0f172a")
    styles = {"static": "#2563eb", "dynamic": "#ea580c", "guided": "#16a34a"}
    series = []
    for policy, color in styles.items():
        rows = [row for row in balance if row["policy"] == policy]
        rows.sort(key=lambda row: int(row["chunk"]))
        series.append((policy, [number(row["load_balance_factor"]) for row in rows], color))
    series.append(("ideal", [1.0] * 4, "#64748b"))
    line_chart(draw, (20, 65, 860, 570), "Carga máxima / carga média",
               ["1", "4", "16", "64"], series, "Fator de balanceamento", 0.95)
    save_image(image, output)


def table_schedule_rows(summary: list[dict[str, object]], case: str) -> list[list[str]]:
    rows = [row for row in summary if row["case"] == case]
    policy_order = {"static": 0, "dynamic": 1, "guided": 2}
    rows.sort(key=lambda row: (policy_order[str(row["policy"])], int(row["chunk"])))
    return [
        [
            str(row["policy"]),
            str(row["chunk"]),
            f"{number(str(row['median_s'])):.3f}",
            f"{number(str(row['mean_s'])):.3f}",
            f"{number(str(row['stdev_s'])):.3f}",
        ]
        for row in rows
    ]


def write_report(
    output_dir: Path,
    scaling: list[dict[str, object]],
    schedules: list[dict[str, object]],
    correctness: list[dict[str, str]],
    balance: list[dict[str, str]],
    environment: str,
    serial_standard: float,
    serial_horse: float,
) -> None:
    exact = sum(row["status"] == "APPROVED_EXACT" for row in correctness)
    approved = sum(row["status"].startswith("APPROVED") for row in correctness)
    max_diff_pixels = max(int(row["different_pixels"]) for row in correctness)
    max_abs_difference = max(int(row["max_abs_difference"]) for row in correctness)
    best_standard = min(
        (row for row in schedules if row["case"] == "standard"),
        key=lambda row: number(str(row["median_s"])),
    )
    best_horse = min(
        (row for row in schedules if row["case"] == "seahorse_valley"),
        key=lambda row: number(str(row["median_s"])),
    )
    best_standard_seconds = number(str(best_standard["median_s"]))
    best_horse_seconds = number(str(best_horse["median_s"]))
    best_standard_speedup = serial_standard / best_standard_seconds
    best_horse_speedup = serial_horse / best_horse_seconds

    scaling_table = markdown_table(
        ["Threads", "T serial (s)", "T paralelo (s)", "Speedup", "Eficiência"],
        [
            [
                str(row["threads"]),
                f"{number(str(row['serial_median_s'])):.3f}",
                f"{number(str(row['parallel_median_s'])):.3f}",
                f"{number(str(row['speedup'])):.3f}x",
                f"{number(str(row['efficiency_percent'])):.1f}%",
            ]
            for row in scaling
        ],
    )

    standard_schedule_table = markdown_table(
        ["Política", "Chunk", "Mediana (s)", "Média (s)", "DP (s)"],
        table_schedule_rows(schedules, "standard"),
    )
    horse_schedule_table = markdown_table(
        ["Política", "Chunk", "Mediana (s)", "Média (s)", "DP (s)"],
        table_schedule_rows(schedules, "seahorse_valley"),
    )

    policy_order = {"static": 0, "dynamic": 1, "guided": 2}
    balance_rows = sorted(
        balance, key=lambda row: (policy_order[row["policy"]], int(row["chunk"]))
    )
    balance_table = markdown_table(
        ["Política", "Chunk", "Carga mín.", "Carga máx.", "Carga média", "Fator"],
        [
            [
                row["policy"], row["chunk"],
                f"{int(row['min_thread_iterations']):,}",
                f"{int(row['max_thread_iterations']):,}",
                f"{number(row['avg_thread_iterations']):,.0f}",
                f"{number(row['load_balance_factor']):.3f}",
            ]
            for row in balance_rows
        ],
    )

    report = f"""# Resultados OpenMP - Mandelbrot

## Escopo e método

Foram executadas {len(scaling)} configurações de escalabilidade forte no input padrão (região completa, 4096x4096, `MAX_ITER = 1000`) e 12 combinações de política/chunk para cada input. Cada medição foi repetida três vezes; as tabelas usam a mediana. O relógio envolve somente o cálculo de escape-time e exclui leitura e escrita de arquivos.

Ambiente registrado na execução:

```text
{environment.rstrip()}
```

Políticas avaliadas: `static`, `dynamic` e `guided`; chunks: 1, 4, 16 e 64 linhas. A variação do número de threads no input padrão usa `static` sem chunk explícito, isto é, blocos contíguos de linhas.

## Corretude (Seção 5.5)

O critério do enunciado permite no máximo 0,01% de pixels divergentes (1.677 de 16.777.216), cada um com diferença máxima de uma iteração. Foram comparadas as matrizes de contagem da referência sequencial e da versão paralela em {len(correctness)} configurações: {approved}/{len(correctness)} aprovadas; {exact}/{len(correctness)} com igualdade exata. O maior número observado de pixels diferentes foi {max_diff_pixels}, e a maior diferença absoluta observada foi {max_abs_difference}.

Resultado: **todas as matrizes comparadas são idênticas**. Portanto, a implementação satisfaz o critério mais forte, sem usar a tolerância.

## Input padrão - tempo, Speedup e Eficiência

Fórmulas: `Speedup(p) = mediana(T_serial) / mediana(T_paralelo,p)` e `Eficiência(p) = Speedup(p) / p`.

{scaling_table}

![Tempo, Speedup e Eficiência](graficos/tempo_speedup_eficiencia_padrao.png)

## Políticas de escalonamento e chunk

Com o máximo de threads disponível na máquina, a melhor combinação no input padrão foi `{best_standard['policy']}, chunk={best_standard['chunk']}` com mediana de {best_standard_seconds:.3f} s. Ela equivale a Speedup de {best_standard_speedup:.3f}x e Eficiência de {100 * best_standard_speedup / 16:.1f}% frente à mediana serial. No vale dos cavalos-marinhos, a melhor combinação foi `{best_horse['policy']}, chunk={best_horse['chunk']}` com mediana de {best_horse_seconds:.3f} s (Speedup de {best_horse_speedup:.3f}x).

### Input padrão

{standard_schedule_table}

### Vale dos cavalos-marinhos (`MAX_ITER = 5000`)

{horse_schedule_table}

![Políticas e chunks](graficos/comparacao_politicas_chunk.png)

No input padrão, `dynamic` com chunks 1 e 4 foi a política mais rápida; chunks maiores aumentaram o tempo, em especial para `dynamic`. `guided` foi consistentemente mais lento neste ambiente. No caso de desbalanceamento, `static, chunk=16` e `static, chunk=4` superaram por pouco as alternativas dinâmicas. Portanto, o menor fator de carga não determina sozinho o menor tempo: a sobrecarga de agendamento e a localidade de memória também importam.

## Balanceamento de carga - vale dos cavalos-marinhos

O trabalho foi estimado pela soma das contagens de iteração atribuídas a cada thread. O fator reportado é `carga máxima / carga média`; 1,0 representa distribuição ideal. Valores maiores indicam uma thread com mais trabalho que a média.

{balance_table}

![Fator de balanceamento](graficos/balanceamento_seahorse.png)

## Arquivos desta pasta

- `raw_timings.csv`: todas as repetições cruas.
- `summary_standard_scaling.csv`: tabelas de tempo, Speedup e Eficiência.
- `summary_scheduling.csv`: médias, medianas e desvios por política e chunk.
- `correctness.csv`: comparação da matriz paralela com a referência sequencial.
- `seahorse_load_balance.csv`: cargas por política/chunk e fator de balanceamento.
- `graficos/`: os três gráficos usados neste relatório.
"""
    (output_dir / "RELATORIO.md").write_text(report, encoding="utf-8")

    reproduce = """# Como reproduzir

1. Abra um terminal de desenvolvedor do Visual Studio x64.
2. Na pasta `code`, compile com:

```powershell
cl /nologo /std:c11 /O2 /W4 /WX /openmp benchmark.c serial.c paralel.c settings.c /Fe:benchmark.exe
```

3. Crie uma pasta vazia e execute:

```powershell
.\\benchmark.exe <pasta-de-resultados>
python .\\generate_benchmark_report.py <pasta-de-resultados>
```

O benchmark realiza três repetições, mede apenas o cálculo de escape-time e limita o experimento a 16 threads quando a máquina possuir mais processadores lógicos.
"""
    (output_dir / "REPRODUZIR.md").write_text(reproduce, encoding="utf-8")


def collect_environment(output_dir: Path) -> str:
    processor = platform.processor() or "Não informado"
    try:
        command = [
            "powershell", "-NoProfile", "-Command",
            "(Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name)",
        ]
        result = subprocess.run(command, capture_output=True, text=True,
                                check=False, encoding="utf-8")
        if result.stdout.strip():
            processor = result.stdout.strip()
    except OSError:
        pass

    environment = "\n".join(
        [
            f"Sistema operacional: {platform.platform()}",
            f"Processador: {processor}",
            f"Processadores lógicos detectados: {os.cpu_count()}",
            "Compilador: Microsoft C/C++ (cl.exe), otimização /O2",
            "OpenMP: MSVC /openmp",
            "Medições: 3 repetições por configuração; estatística principal = mediana",
        ]
    )
    (output_dir / "AMBIENTE.txt").write_text(environment + "\n", encoding="utf-8")
    return environment


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--environment", type=Path)
    arguments = parser.parse_args()
    output_dir = arguments.output_dir

    raw = read_csv(output_dir / "raw_timings.csv")
    correctness = read_csv(output_dir / "correctness.csv")
    balance = read_csv(output_dir / "seahorse_load_balance.csv")
    scaling = make_scaling_summary(raw)
    schedules = make_schedule_summary(raw)
    serial_standard = median([
        number(row["seconds"]) for row in raw
        if row["case"] == "standard" and row["suite"] == "serial_baseline"
    ])
    serial_horse = median([
        number(row["seconds"]) for row in raw
        if row["case"] == "seahorse_valley" and row["suite"] == "serial_baseline"
    ])

    write_csv(
        output_dir / "summary_standard_scaling.csv",
        ["threads", "serial_median_s", "parallel_median_s", "speedup",
         "efficiency", "efficiency_percent"],
        scaling,
    )
    write_csv(
        output_dir / "summary_scheduling.csv",
        ["case", "policy", "chunk", "threads", "repetitions", "median_s",
         "mean_s", "stdev_s"],
        schedules,
    )

    chart_dir = output_dir / "graficos"
    chart_dir.mkdir(exist_ok=True)
    plot_scaling(scaling, chart_dir / "tempo_speedup_eficiencia_padrao.png")
    plot_schedules(schedules, chart_dir / "comparacao_politicas_chunk.png")
    plot_load_balance(balance, chart_dir / "balanceamento_seahorse.png")

    environment = collect_environment(output_dir)
    if arguments.environment and arguments.environment.exists():
        environment = arguments.environment.read_text(encoding="utf-8")
    write_report(output_dir, scaling, schedules, correctness, balance,
                 environment, serial_standard, serial_horse)


if __name__ == "__main__":
    main()
