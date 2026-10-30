#!/usr/bin/env python3
"""Generate MPI tables, charts and Markdown report in the OpenMP visual style."""

from __future__ import annotations

import argparse
import sys
from collections import defaultdict
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "openmp" / "code"))

from generate_benchmark_report import (  # noqa: E402
    Image,
    ImageDraw,
    font,
    line_chart,
    markdown_table,
    number,
    read_csv,
    save_image,
)


COLORS = {
    "static": "#2563eb",
    "cyclic": "#ea580c",
    "openmp": "#16a34a",
    "ideal": "#64748b",
}


def chart_strong(rows: list[dict[str, str]], output: Path) -> None:
    processes = sorted({int(row["processes"]) for row in rows})
    labels = [str(value) for value in processes]
    image = Image.new("RGB", (1800, 560), "#ffffff")
    draw = ImageDraw.Draw(image)
    draw.text((26, 18), "MPI: escalabilidade forte, 4096x4096, MAX_ITER = 1000",
              font=font(26, True), fill="#0f172a")
    panels = [(20, 65, 600, 540), (610, 65, 1190, 540), (1200, 65, 1780, 540)]

    time_series = []
    speedup_series = []
    efficiency_series = []
    for strategy in ("static", "cyclic"):
        strategy_rows = sorted(
            (row for row in rows if row["strategy"] == strategy),
            key=lambda row: int(row["processes"]),
        )
        time_series.append(
            (strategy, [number(row["total_median_s"]) for row in strategy_rows], COLORS[strategy])
        )
        speedup_series.append(
            (strategy, [number(row["speedup"]) for row in strategy_rows], COLORS[strategy])
        )
        efficiency_series.append(
            (strategy, [number(row["efficiency_percent"]) for row in strategy_rows], COLORS[strategy])
        )

    serial_time = number(rows[0]["serial_median_s"])
    time_series.append(("serial ref.", [serial_time] * len(labels), COLORS["ideal"]))
    speedup_series.append(("ideal", [float(value) for value in processes], COLORS["ideal"]))
    efficiency_series.append(("ideal", [100.0] * len(labels), COLORS["ideal"]))
    line_chart(draw, panels[0], "Tempo total", labels, time_series,
               "Computação + coleta + mestre (s)")
    line_chart(draw, panels[1], "Speedup", labels, speedup_series,
               "T serial / T MPI")
    line_chart(draw, panels[2], "Eficiência", labels, efficiency_series,
               "Speedup / processos (%)")
    save_image(image, output)


def chart_weak(rows: list[dict[str, str]], output: Path) -> None:
    processes = sorted({int(row["processes"]) for row in rows})
    labels = [str(value) for value in processes]
    image = Image.new("RGB", (1800, 560), "#ffffff")
    draw = ImageDraw.Draw(image)
    draw.text((26, 18), "MPI: escalabilidade fraca",
              font=font(26, True), fill="#0f172a")
    panels = [(20, 65, 600, 540), (610, 65, 1190, 540), (1200, 65, 1780, 540)]
    time_series = []
    speedup_series = []
    efficiency_series = []
    for strategy in ("static", "cyclic"):
        strategy_rows = sorted(
            (row for row in rows if row["strategy"] == strategy),
            key=lambda row: int(row["processes"]),
        )
        time_series.append(
            (strategy, [number(row["total_median_s"]) for row in strategy_rows], COLORS[strategy])
        )
        speedup_series.append(
            (strategy, [number(row["scaled_speedup"]) for row in strategy_rows], COLORS[strategy])
        )
        efficiency_series.append(
            (strategy, [number(row["weak_efficiency_percent"]) for row in strategy_rows], COLORS[strategy])
        )
    speedup_series.append(("ideal", [float(value) for value in processes], COLORS["ideal"]))
    efficiency_series.append(("ideal", [100.0] * len(labels), COLORS["ideal"]))
    line_chart(draw, panels[0], "Tempo total", labels, time_series,
               "Trabalho constante por processo (s)")
    line_chart(draw, panels[1], "Speedup escalado", labels, speedup_series,
               "p x T(1) / T(p)")
    line_chart(draw, panels[2], "Eficiência fraca", labels, efficiency_series,
               "T(1) / T(p) (%)")
    save_image(image, output)


def chart_communication(rows: list[dict[str, str]], output: Path) -> None:
    processes = sorted({int(row["processes"]) for row in rows})
    labels = [str(value) for value in processes]
    image = Image.new("RGB", (1800, 560), "#ffffff")
    draw = ImageDraw.Draw(image)
    draw.text((26, 18), "MPI: custo de comunicação e coleta",
              font=font(26, True), fill="#0f172a")
    panels = [(20, 65, 600, 540), (610, 65, 1190, 540), (1200, 65, 1780, 540)]
    gather_series = []
    master_series = []
    communication_series = []
    for strategy in ("static", "cyclic"):
        strategy_rows = sorted(
            (row for row in rows if row["strategy"] == strategy),
            key=lambda row: int(row["processes"]),
        )
        gather = [number(row["gather_median_s"]) for row in strategy_rows]
        master = [number(row["master_overhead_median_s"]) for row in strategy_rows]
        gather_series.append((strategy, gather, COLORS[strategy]))
        master_series.append((strategy, master, COLORS[strategy]))
        communication_series.append(
            (strategy, [left + right for left, right in zip(gather, master)], COLORS[strategy])
        )
    line_chart(draw, panels[0], "MPI_Gather", labels, gather_series,
               "Tempo de coleta (s)")
    line_chart(draw, panels[1], "Sobrecarga do mestre", labels, master_series,
               "Reordenação cíclica (s)")
    line_chart(draw, panels[2], "Comunicação + mestre", labels, communication_series,
               "Tempo agregado (s)")
    save_image(image, output)


def chart_balance(
    summaries: list[dict[str, str]], process_rows: list[dict[str, str]], output: Path
) -> None:
    max_processes = max(int(row["processes"]) for row in summaries)
    max_process_rows = [
        row for row in process_rows if int(row["processes"]) == max_processes
    ]
    ranks = sorted({int(row["rank"]) for row in max_process_rows})
    labels = [str(value) for value in ranks]
    image = Image.new("RGB", (1600, 590), "#ffffff")
    draw = ImageDraw.Draw(image)
    draw.text((26, 18), "MPI: vale dos cavalos-marinhos, MAX_ITER = 5000",
              font=font(25, True), fill="#0f172a")
    panels = [(20, 65, 790, 570), (800, 65, 1580, 570)]
    time_series = []
    for strategy in ("static", "cyclic"):
        strategy_rows = sorted(
            (row for row in max_process_rows if row["strategy"] == strategy),
            key=lambda row: int(row["rank"]),
        )
        time_series.append(
            (strategy, [number(row["compute_median_s"]) for row in strategy_rows], COLORS[strategy])
        )
    line_chart(draw, panels[0], f"Tempo por processo (p={max_processes})", labels, time_series,
               "Mediana da computação (s)")
    process_counts = sorted({int(row["processes"]) for row in summaries})
    process_labels = [str(value) for value in process_counts]
    values_by_strategy = {}
    fbc_series = []
    for strategy in ("static", "cyclic"):
        strategy_rows = sorted(
            (row for row in summaries if row["strategy"] == strategy),
            key=lambda row: int(row["processes"]),
        )
        values = [number(row["fbc_median"]) for row in strategy_rows]
        values_by_strategy[strategy] = values
        fbc_series.append((strategy, values, COLORS[strategy]))
    fbc_series.append(("ideal", [0.0] * len(process_counts), COLORS["ideal"]))
    line_chart(draw, panels[1], "FBC por número de processos", process_labels,
               fbc_series, "(T máximo - T mínimo) / T máximo", 0.0)

    # Repete a transformação de line_chart para rotular os valores medidos.
    left, top, right, bottom = panels[1]
    plot_left, plot_top = left + 62, top + 54
    plot_right, plot_bottom = right - 18, bottom - 56
    y_min = 0.0
    y_max = max(value for values in values_by_strategy.values() for value in values)
    y_max += max((y_max - y_min) * 0.12, 0.03)
    x_positions = [
        plot_left + index * (plot_right - plot_left) / max(len(process_counts) - 1, 1)
        for index in range(len(process_counts))
    ]
    value_font = font(11, True)
    for strategy_index, strategy in enumerate(("static", "cyclic")):
        for index, (x, value) in enumerate(zip(x_positions, values_by_strategy[strategy])):
            if index == 0 and strategy_index == 1:
                continue
            y = plot_bottom - (value - y_min) / (y_max - y_min) * (plot_bottom - plot_top)
            label = f"{value:.3f}".replace(".", ",")
            bounds = draw.textbbox((0, 0), label, font=value_font)
            width = bounds[2] - bounds[0]
            height = bounds[3] - bounds[1]
            text_x = x - width / 2
            text_y = y - height - 11
            draw.rectangle(
                (text_x - 3, text_y - 1, text_x + width + 3, text_y + height + 2),
                fill="#ffffff",
            )
            draw.text((text_x, text_y), label, font=value_font,
                      fill=COLORS[strategy])
    save_image(image, output)


def chart_comparison(rows: list[dict[str, str]], output: Path) -> None:
    resources = sorted({int(row["resources"]) for row in rows})
    labels = [str(value) for value in resources]
    by_strategy = defaultdict(dict)
    for row in rows:
        by_strategy[row["mpi_strategy"]][int(row["resources"])] = row
    openmp = {
        resource: by_strategy["static"][resource]
        for resource in resources
    }
    image = Image.new("RGB", (1800, 560), "#ffffff")
    draw = ImageDraw.Draw(image)
    draw.text((26, 18), "Comparação OpenMP x MPI no input padrão",
              font=font(26, True), fill="#0f172a")
    panels = [(20, 65, 600, 540), (610, 65, 1190, 540), (1200, 65, 1780, 540)]
    metrics = [
        ("Tempo", "mpi_time_s", "openmp_time_s", "Mediana (s)"),
        ("Speedup", "mpi_speedup", "openmp_speedup", "T serial / T paralelo"),
        ("Eficiência", "mpi_efficiency_percent", "openmp_efficiency_percent", "%"),
    ]
    for panel, (title, mpi_key, openmp_key, label) in zip(panels, metrics):
        series = [
            (
                "MPI static",
                [number(by_strategy["static"][value][mpi_key]) for value in resources],
                COLORS["static"],
            ),
            (
                "MPI cyclic",
                [number(by_strategy["cyclic"][value][mpi_key]) for value in resources],
                COLORS["cyclic"],
            ),
            (
                "OpenMP",
                [number(openmp[value][openmp_key]) for value in resources],
                COLORS["openmp"],
            ),
        ]
        line_chart(draw, panel, title, labels, series, label)
    save_image(image, output)


def strong_table(rows: list[dict[str, str]], strategy: str) -> str:
    selected = sorted(
        (row for row in rows if row["strategy"] == strategy),
        key=lambda row: int(row["processes"]),
    )
    return markdown_table(
        ["Processos", "T total (s)", "Computação (s)", "Gather (s)",
         "Mestre (s)", "Speedup", "Eficiência"],
        [
            [
                row["processes"], f"{number(row['total_median_s']):.3f}",
                f"{number(row['compute_median_s']):.3f}",
                f"{number(row['gather_median_s']):.3f}",
                f"{number(row['master_overhead_median_s']):.3f}",
                f"{number(row['speedup']):.3f}x",
                f"{number(row['efficiency_percent']):.1f}%",
            ]
            for row in selected
        ],
    )


def write_report(output_dir: Path) -> None:
    strong = read_csv(output_dir / "summary_strong_scaling.csv")
    weak = read_csv(output_dir / "summary_weak_scaling.csv")
    balance = read_csv(output_dir / "summary_load_balance.csv")
    process_rows = read_csv(output_dir / "seahorse_process_times.csv")
    correctness = read_csv(output_dir / "correctness.csv")
    comparison = read_csv(output_dir / "comparison_openmp_mpi.csv")
    environment = (output_dir / "AMBIENTE.txt").read_text(encoding="utf-8")

    graph_dir = output_dir / "graficos"
    graph_dir.mkdir(exist_ok=True)
    chart_strong(strong, graph_dir / "tempo_speedup_eficiencia_mpi.png")
    chart_weak(weak, graph_dir / "escalabilidade_fraca_mpi.png")
    chart_communication(strong, graph_dir / "comunicacao_mpi.png")
    chart_balance(balance, process_rows, graph_dir / "balanceamento_seahorse_mpi.png")
    chart_comparison(comparison, graph_dir / "comparacao_openmp_mpi.png")

    exact = sum(row["status"] == "APPROVED_EXACT" for row in correctness)
    approved = sum(row["status"].startswith("APPROVED") for row in correctness)
    max_diff = max(int(row["different_pixels"]) for row in correctness)
    max_abs = max(int(row["max_abs_difference"]) for row in correctness)
    max_processes = max(int(row["processes"]) for row in strong)
    max_rows = [row for row in strong if int(row["processes"]) == max_processes]
    best_max = min(max_rows, key=lambda row: number(row["total_median_s"]))
    max_balance_rows = [
        row for row in balance if int(row["processes"]) == max_processes
    ]
    best_balance = min(max_balance_rows, key=lambda row: number(row["fbc_median"]))
    max_by_strategy = {row["strategy"]: row for row in max_rows}
    cyclic_communication = (
        number(max_by_strategy["cyclic"]["gather_median_s"])
        + number(max_by_strategy["cyclic"]["master_overhead_median_s"])
    )
    static_communication = number(max_by_strategy["static"]["gather_median_s"])
    cyclic_communication_percent = (
        100.0 * cyclic_communication
        / number(max_by_strategy["cyclic"]["total_median_s"])
    )
    static_communication_percent = (
        100.0 * static_communication
        / number(max_by_strategy["static"]["total_median_s"])
    )

    correctness_table = markdown_table(
        ["Caso", "Estratégia", "Processos", "Pixels diferentes",
         "Diferença máxima", "Status"],
        [
            [row["case"], row["strategy"], row["processes"],
             row["different_pixels"], row["max_abs_difference"], row["status"]]
            for row in correctness
        ],
    )
    weak_table = markdown_table(
        ["Estratégia", "Processos", "Resolução", "Pixels/processo",
         "T total (s)", "Speedup escalado", "Eficiência fraca"],
        [
            [
                row["strategy"], row["processes"], f"{row['size']}²",
                f"{int(row['pixels_per_process']):,}",
                f"{number(row['total_median_s']):.3f}",
                f"{number(row['scaled_speedup']):.3f}x",
                f"{number(row['weak_efficiency_percent']):.1f}%",
            ]
            for row in weak
        ],
    )
    balance_table = markdown_table(
        ["Estratégia", "Processos", "T mín. (s)", "T máx. (s)", "FBC", "Gather (s)",
         "Mestre (s)", "T total (s)"],
        [
            [
                row["strategy"], row["processes"],
                f"{number(row['compute_min_median_s']):.6f}",
                f"{number(row['compute_max_median_s']):.6f}",
                f"{number(row['fbc_median']):.6f}",
                f"{number(row['gather_median_s']):.6f}",
                f"{number(row['master_overhead_median_s']):.6f}",
                f"{number(row['total_median_s']):.6f}",
            ]
            for row in balance
        ],
    )

    process_map = defaultdict(dict)
    for row in process_rows:
        if int(row["processes"]) == max_processes:
            process_map[int(row["rank"])][row["strategy"]] = row
    process_table = markdown_table(
        ["Processo", "Static (s)", "Cyclic (s)"],
        [
            [
                str(rank),
                f"{number(values['static']['compute_median_s']):.6f}",
                f"{number(values['cyclic']['compute_median_s']):.6f}",
            ]
            for rank, values in sorted(process_map.items())
        ],
    )

    comparison_map = defaultdict(dict)
    for row in comparison:
        comparison_map[int(row["resources"])][row["mpi_strategy"]] = row
    comparison_table = markdown_table(
        ["Recursos", "OpenMP T (s)", "MPI static T (s)", "MPI cyclic T (s)",
         "OpenMP Speedup", "MPI static Speedup", "MPI cyclic Speedup"],
        [
            [
                str(resource),
                f"{number(values['static']['openmp_time_s']):.3f}",
                f"{number(values['static']['mpi_time_s']):.3f}",
                f"{number(values['cyclic']['mpi_time_s']):.3f}",
                f"{number(values['static']['openmp_speedup']):.3f}x",
                f"{number(values['static']['mpi_speedup']):.3f}x",
                f"{number(values['cyclic']['mpi_speedup']):.3f}x",
            ]
            for resource, values in sorted(comparison_map.items())
        ],
    )
    comparison_at_max = comparison_map[max_processes]
    openmp_max_time = number(comparison_at_max["static"]["openmp_time_s"])
    mpi_static_max_time = number(comparison_at_max["static"]["mpi_time_s"])
    mpi_cyclic_max_time = number(comparison_at_max["cyclic"]["mpi_time_s"])
    cyclic_over_openmp = openmp_max_time / mpi_cyclic_max_time

    report = f"""# Resultados MPI - Mandelbrot

## Escopo e metodologia

Foram avaliadas duas estratégias: `static`, com faixas contíguas de linhas, e `cyclic`, com linhas distribuídas ciclicamente. Cada configuração foi executada três vezes e as tabelas usam a mediana. O tempo total é `T_max de computação + MPI_Gather + sobrecarga do mestre`; escrita em disco foi medida separadamente e excluída das métricas de desempenho.

Ambiente registrado:

```text
{environment.rstrip()}
```

## Corretude

A matriz `int32_t` de cada configuração oficial do input padrão foi comparada byte a byte com a referência serial. Também foram verificadas as duas estratégias no vale dos cavalos-marinhos. O critério admite até 0,01% de pixels divergentes, com diferença máxima de uma iteração.

{correctness_table}

Resultado: **{approved}/{len(correctness)} configurações aprovadas e {exact}/{len(correctness)} com igualdade exata**. O maior número de pixels divergentes foi {max_diff}, com diferença absoluta máxima {max_abs}.

## Escalabilidade forte - input padrão

Referência serial: mediana de {number(strong[0]['serial_median_s']):.3f} s para 4096×4096 e `MAX_ITER = 1000`.

### Distribuição estática

{strong_table(strong, 'static')}

### Distribuição cíclica

{strong_table(strong, 'cyclic')}

![Tempo, Speedup e Eficiência MPI](graficos/tempo_speedup_eficiencia_mpi.png)

Com {max_processes} processos, a melhor estratégia foi `{best_max['strategy']}`, com {number(best_max['total_median_s']):.3f} s, Speedup de {number(best_max['speedup']):.3f}x e Eficiência de {number(best_max['efficiency_percent']):.1f}%.

## Balanceamento de carga - vale dos cavalos-marinhos

Este experimento usa resolução 4096×4096, centro `(-0,743643887; 0,131825904)`, largura `3,0 × 10⁻³` e `MAX_ITER = 5000`, com {max_processes} processos.

Foi adotada a definição usada em sala: `FBC = (T_max - T_min) / T_max`, onde `T_max` e `T_min` são o maior e o menor tempo de **computação** entre os processos MPI. Cada processo mede `T` com `MPI_Wtime()` após uma barreira inicial e encerra a medição imediatamente após calcular suas linhas, antes da barreira e do `MPI_Gather`. Portanto, `T` exclui coleta, reordenação no mestre e escrita em disco. O FBC é calculado em cada execução para 1, 2, 4, 8 e 16 processos, e a tabela apresenta a mediana das três repetições; 0 indica equilíbrio perfeito e valores próximos de 1 indicam forte desbalanceamento.

{balance_table}

### Tempo de computação por processo com {max_processes} processos

{process_table}

![Balanceamento no vale dos cavalos-marinhos](graficos/balanceamento_seahorse_mpi.png)

Com {max_processes} processos, o menor FBC foi obtido por `{best_balance['strategy']}` ({number(best_balance['fbc_median']):.6f}). A distribuição cíclica tende a misturar linhas de custos diferentes, enquanto a estática agrupa regiões contíguas e pode concentrar pixels caros em poucos processos.

## Custo de comunicação

`MPI_Gather` foi medido isoladamente. Para `cyclic`, a reordenação das linhas no processo 0 aparece como sobrecarga do mestre; para `static`, essa etapa é nula. A escrita do binário ocorre depois das métricas e não compõe o tempo total.

![Custo de comunicação MPI](graficos/comunicacao_mpi.png)

O custo de coleta cresce com o volume e a quantidade de processos. A estratégia cíclica acrescenta uma passagem completa pela imagem no mestre para restaurar a ordem `row-major`. Com {max_processes} processos, `Gather + mestre` consumiu {cyclic_communication:.3f} s ({cyclic_communication_percent:.1f}% do total) em `cyclic`; em `static`, o `Gather` consumiu {static_communication:.3f} s ({static_communication_percent:.1f}% do total).

## Escalabilidade fraca

O trabalho foi mantido em aproximadamente 4096² pixels por processo: 4096²/1, 5792²/2, 8192²/4, 11584²/8 e 16384²/16. Como o tamanho total do problema cresce com o número de processos, o speedup apresentado é o escalado: `S_escalado(p) = p × T(1) / T(p)`.

{weak_table}

![Escalabilidade fraca MPI](graficos/escalabilidade_fraca_mpi.png)

A eficiência fraca é `S_escalado(p) / p = T(1) / T(p)`. Quedas refletem comunicação, contenção de memória e a parcela serial da coleta/reordenação.

## Comparação com OpenMP

Os resultados OpenMP usados são da rodada de 29/10/2026, no mesmo computador, com três repetições e medianas.

{comparison_table}

![Comparação OpenMP e MPI](graficos/comparacao_openmp_mpi.png)

OpenMP compartilha diretamente a matriz e evita a coleta de buffers privados, enquanto MPI paga por `Gather` e, no caso cíclico, pela reordenação. MPI, por outro lado, explicita custos relevantes para execução distribuída e pode escalar para múltiplos nós.

Com {max_processes} recursos, OpenMP levou {openmp_max_time:.3f} s, MPI `static` {mpi_static_max_time:.3f} s e MPI `cyclic` {mpi_cyclic_max_time:.3f} s. O MPI cíclico foi {cyclic_over_openmp:.2f}× mais rápido que o OpenMP medido, mas essa diferença não deve ser atribuída apenas a MPI: a curva forte do OpenMP usa distribuição estática contígua, cujo comportamento é muito próximo ao MPI `static`. O ganho principal vem da distribuição cíclica reduzir o desbalanceamento do Mandelbrot.

## Conclusão e relação com DGEMM

Ao contrário do DGEMM bloqueado, cujo custo por elemento é regular e previsível, Mandelbrot possui trabalho variável por pixel. Por isso, uma decomposição contígua pode ter FBC alto mesmo quando cada processo recebe o mesmo número de linhas. A distribuição cíclica melhora a mistura de carga, mas introduz reordenação no mestre. O experimento evidencia o compromisso entre balanceamento, comunicação e localidade que não aparece com a mesma intensidade no DGEMM regular.

## Arquivos

- `raw_timings.csv`: todas as execuções MPI e suas fases.
- `process_timings.csv`: tempos individuais de todos os processos.
- `correctness.csv`: comparação com a referência serial.
- `summary_strong_scaling.csv`: tempo, Speedup e Eficiência.
- `summary_weak_scaling.csv`: escalabilidade fraca.
- `summary_load_balance.csv` e `seahorse_process_times.csv`: FBC e tempos por processo.
- `comparison_openmp_mpi.csv`: comparação direta com OpenMP.
- `graficos/`: os cinco gráficos usados neste relatório.
"""
    (output_dir / "RELATORIO.md").write_text(report, encoding="utf-8")

    reproduce = """# Como reproduzir

1. Instale o runtime e o SDK MS-MPI compatíveis.
2. Compile em um terminal de desenvolvedor do Visual Studio x64:

```powershell
cmake -S mpi/code -B build/mpi -DMSMPI_ROOT="C:/caminho/MSMPI-SDK" -DCMAKE_BUILD_TYPE=Release
cmake --build build/mpi --config Release
```

3. Execute a suíte:

```powershell
python mpi/code/run_mpi_benchmarks.py <pasta-resultados> `
  --mpi-exe build/mpi/Release/mandelbrot_mpi.exe `
  --serial-exe build/mpi/Release/serial_reference.exe `
  --mpiexec "C:/Program Files/Microsoft MPI/Bin/mpiexec.exe" `
  --openmp-results openmp/resultados_openmp_2026-10-29
```

4. Gere o relatório e os gráficos com um Python que possua Pillow:

```powershell
python mpi/code/generate_mpi_report.py <pasta-resultados>
```

A suíte usa três repetições, processos 1/2/4/8/16, forte e fraca, duas estratégias, comparação binária de corretude e exclusão da escrita em disco dos tempos oficiais.
"""
    (output_dir / "REPRODUZIR.md").write_text(reproduce, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    write_report(args.output_dir.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
