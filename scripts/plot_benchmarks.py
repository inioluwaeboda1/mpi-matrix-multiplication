#!/usr/bin/env python3
"""Regenerate portfolio benchmark charts from results/benchmarks.csv."""

from __future__ import annotations

import csv
import math
from dataclasses import dataclass
from html import escape
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
CSV_PATH = ROOT / "results" / "benchmarks.csv"
CHART_DIR = ROOT / "results" / "charts"

WIDTH = 1400
HEIGHT = 850
PLOT_LEFT = 150
PLOT_RIGHT = 1340
PLOT_TOP = 135
PLOT_BOTTOM = 705

BACKGROUND = "#f8fafc"
INK = "#172033"
MUTED = "#5d6879"
GRID = "#d8dee9"
COLORS = ["#1261a0", "#e07a1f", "#27896d", "#8d56b3", "#c33e4f"]


@dataclass(frozen=True)
class Series:
    label: str
    points: list[tuple[float, float]]


def load_font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    names = [
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf" if bold else "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ]
    for name in names:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def load_rows() -> list[dict[str, str]]:
    with CSV_PATH.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def select_series(
    rows: list[dict[str, str]],
    *,
    predicate,
    definitions: list[tuple[str, object]],
    x_field: str,
    y_field: str = "runtime",
) -> list[Series]:
    selected: list[Series] = []
    for label, series_predicate in definitions:
        points = [
            (float(row[x_field]), float(row[y_field]))
            for row in rows
            if predicate(row) and series_predicate(row)
        ]
        points.sort()
        if points:
            selected.append(Series(label, points))
    return selected


def nice_linear_ticks(low: float, high: float, count: int = 6) -> list[float]:
    if high <= low:
        return [low]
    raw_step = (high - low) / count
    magnitude = 10 ** math.floor(math.log10(raw_step))
    normalized = raw_step / magnitude
    step_multiplier = 1 if normalized <= 1 else 2 if normalized <= 2 else 5 if normalized <= 5 else 10
    step = step_multiplier * magnitude
    tick_low = math.floor(low / step) * step
    tick_high = math.ceil(high / step) * step
    ticks = []
    value = tick_low
    while value <= tick_high + step * 0.01:
        ticks.append(value)
        value += step
    return ticks


def log_ticks(low: float, high: float) -> list[float]:
    start = math.floor(math.log10(low))
    finish = math.ceil(math.log10(high))
    ticks = []
    for exponent in range(start, finish + 1):
        for multiplier in (1, 10):
            value = multiplier * (10 ** exponent)
            if low <= value <= high:
                ticks.append(float(value))
    return sorted(set(ticks))


def format_tick(value: float, log_scale: bool = False) -> str:
    if log_scale:
        if value >= 1_000_000:
            return f"{value / 1_000_000:g}M"
        if value >= 1_000:
            return f"{value / 1_000:g}k"
    if value >= 100 or value.is_integer():
        return f"{value:,.0f}"
    return f"{value:.2f}".rstrip("0").rstrip(".")


def render_chart(
    filename: str,
    title: str,
    subtitle: str,
    x_label: str,
    y_label: str,
    series: list[Series],
    *,
    log_y: bool = False,
    point_labels: bool = False,
) -> None:
    if not series:
        raise ValueError(f"No data selected for {filename}")

    all_x = [x for item in series for x, _ in item.points]
    all_y = [y for item in series for _, y in item.points]
    x_low, x_high = min(all_x), max(all_x)
    y_low, y_high = min(all_y), max(all_y)

    if log_y:
        y_display_low = 10 ** math.floor(math.log10(y_low))
        y_display_high = 10 ** math.ceil(math.log10(y_high))
        y_ticks = log_ticks(y_display_low, y_display_high)
        y_transform = math.log10
    else:
        padding = max((y_high - y_low) * 0.15, y_high * 0.04, 0.01)
        y_display_low = max(0.0, y_low - padding)
        y_display_high = y_high + padding
        y_ticks = nice_linear_ticks(y_display_low, y_display_high)
        if y_ticks:
            y_display_low, y_display_high = y_ticks[0], y_ticks[-1]
        y_transform = lambda value: value

    x_ticks = sorted(set(all_x))

    def map_x(value: float) -> float:
        if x_high == x_low:
            return (PLOT_LEFT + PLOT_RIGHT) / 2
        return PLOT_LEFT + (value - x_low) / (x_high - x_low) * (PLOT_RIGHT - PLOT_LEFT)

    transformed_low = y_transform(y_display_low)
    transformed_high = y_transform(y_display_high)

    def map_y(value: float) -> float:
        transformed = y_transform(value)
        return PLOT_BOTTOM - (transformed - transformed_low) / (transformed_high - transformed_low) * (PLOT_BOTTOM - PLOT_TOP)

    image = Image.new("RGB", (WIDTH, HEIGHT), BACKGROUND)
    draw = ImageDraw.Draw(image)
    title_font = load_font(34, bold=True)
    subtitle_font = load_font(19)
    axis_font = load_font(18, bold=True)
    tick_font = load_font(16)
    legend_font = load_font(17)
    label_font = load_font(15, bold=True)

    draw.text((PLOT_LEFT, 35), title, fill=INK, font=title_font)
    draw.text((PLOT_LEFT, 83), subtitle, fill=MUTED, font=subtitle_font)

    for tick in y_ticks:
        if not (y_display_low <= tick <= y_display_high):
            continue
        y = map_y(tick)
        draw.line((PLOT_LEFT, y, PLOT_RIGHT, y), fill=GRID, width=1)
        label = format_tick(float(tick), log_y)
        box = draw.textbbox((0, 0), label, font=tick_font)
        draw.text((PLOT_LEFT - 18 - (box[2] - box[0]), y - 9), label, fill=MUTED, font=tick_font)

    for tick in x_ticks:
        x = map_x(tick)
        draw.line((x, PLOT_BOTTOM, x, PLOT_BOTTOM + 8), fill=INK, width=2)
        label = format_tick(float(tick))
        box = draw.textbbox((0, 0), label, font=tick_font)
        draw.text((x - (box[2] - box[0]) / 2, PLOT_BOTTOM + 14), label, fill=MUTED, font=tick_font)

    draw.line((PLOT_LEFT, PLOT_TOP, PLOT_LEFT, PLOT_BOTTOM), fill=INK, width=2)
    draw.line((PLOT_LEFT, PLOT_BOTTOM, PLOT_RIGHT, PLOT_BOTTOM), fill=INK, width=2)

    for index, item in enumerate(series):
        color = COLORS[index % len(COLORS)]
        mapped = [(map_x(x), map_y(y)) for x, y in item.points]
        if len(mapped) > 1:
            draw.line(mapped, fill=color, width=4, joint="curve")
        for point_index, ((px, py), (_, value)) in enumerate(zip(mapped, item.points)):
            draw.ellipse((px - 7, py - 7, px + 7, py + 7), fill=BACKGROUND, outline=color, width=4)
            if point_labels:
                label = f"{value:.5f}".rstrip("0").rstrip(".")
                vertical = -29 if point_index % 2 == 0 else 13
                draw.text((px - 30, py + vertical), label, fill=color, font=label_font)

    legend_x = PLOT_LEFT
    legend_y = 780
    for index, item in enumerate(series):
        color = COLORS[index % len(COLORS)]
        draw.line((legend_x, legend_y, legend_x + 34, legend_y), fill=color, width=5)
        draw.ellipse((legend_x + 11, legend_y - 6, legend_x + 23, legend_y + 6), fill=BACKGROUND, outline=color, width=3)
        draw.text((legend_x + 44, legend_y - 11), item.label, fill=INK, font=legend_font)
        label_width = draw.textbbox((0, 0), item.label, font=legend_font)[2]
        legend_x += 70 + label_width

    x_box = draw.textbbox((0, 0), x_label, font=axis_font)
    draw.text(((PLOT_LEFT + PLOT_RIGHT - (x_box[2] - x_box[0])) / 2, 742), x_label, fill=INK, font=axis_font)

    y_layer = Image.new("RGBA", (300, 45), (0, 0, 0, 0))
    y_draw = ImageDraw.Draw(y_layer)
    y_draw.text((0, 5), y_label, fill=INK, font=axis_font)
    rotated = y_layer.rotate(90, expand=True)
    image.paste(rotated, (30, int((PLOT_TOP + PLOT_BOTTOM - rotated.height) / 2)), rotated)

    CHART_DIR.mkdir(parents=True, exist_ok=True)
    image.save(CHART_DIR / f"{filename}.png", optimize=True)

    render_svg(
        CHART_DIR / f"{filename}.svg",
        title,
        subtitle,
        x_label,
        y_label,
        series,
        x_ticks,
        y_ticks,
        map_x,
        map_y,
        log_y,
        point_labels,
    )


def render_svg(
    output: Path,
    title: str,
    subtitle: str,
    x_label: str,
    y_label: str,
    series: list[Series],
    x_ticks: list[float],
    y_ticks: list[float],
    map_x,
    map_y,
    log_y: bool,
    point_labels: bool,
) -> None:
    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{HEIGHT}" viewBox="0 0 {WIDTH} {HEIGHT}">',
        f'<rect width="{WIDTH}" height="{HEIGHT}" fill="{BACKGROUND}"/>',
        f'<text x="{PLOT_LEFT}" y="62" font-family="Arial, sans-serif" font-size="34" font-weight="700" fill="{INK}">{escape(title)}</text>',
        f'<text x="{PLOT_LEFT}" y="101" font-family="Arial, sans-serif" font-size="19" fill="{MUTED}">{escape(subtitle)}</text>',
    ]
    for tick in y_ticks:
        y = map_y(tick)
        lines.append(f'<line x1="{PLOT_LEFT}" y1="{y:.2f}" x2="{PLOT_RIGHT}" y2="{y:.2f}" stroke="{GRID}"/>')
        lines.append(f'<text x="{PLOT_LEFT - 18}" y="{y + 5:.2f}" text-anchor="end" font-family="Arial, sans-serif" font-size="16" fill="{MUTED}">{escape(format_tick(float(tick), log_y))}</text>')
    for tick in x_ticks:
        x = map_x(tick)
        lines.append(f'<line x1="{x:.2f}" y1="{PLOT_BOTTOM}" x2="{x:.2f}" y2="{PLOT_BOTTOM + 8}" stroke="{INK}" stroke-width="2"/>')
        lines.append(f'<text x="{x:.2f}" y="{PLOT_BOTTOM + 30}" text-anchor="middle" font-family="Arial, sans-serif" font-size="16" fill="{MUTED}">{escape(format_tick(float(tick)))}</text>')
    lines.extend([
        f'<line x1="{PLOT_LEFT}" y1="{PLOT_TOP}" x2="{PLOT_LEFT}" y2="{PLOT_BOTTOM}" stroke="{INK}" stroke-width="2"/>',
        f'<line x1="{PLOT_LEFT}" y1="{PLOT_BOTTOM}" x2="{PLOT_RIGHT}" y2="{PLOT_BOTTOM}" stroke="{INK}" stroke-width="2"/>',
        f'<text x="{(PLOT_LEFT + PLOT_RIGHT) / 2}" y="762" text-anchor="middle" font-family="Arial, sans-serif" font-size="18" font-weight="700" fill="{INK}">{escape(x_label)}</text>',
        f'<text x="42" y="{(PLOT_TOP + PLOT_BOTTOM) / 2}" text-anchor="middle" transform="rotate(-90 42 {(PLOT_TOP + PLOT_BOTTOM) / 2})" font-family="Arial, sans-serif" font-size="18" font-weight="700" fill="{INK}">{escape(y_label)}</text>',
    ])
    legend_x = PLOT_LEFT
    for index, item in enumerate(series):
        color = COLORS[index % len(COLORS)]
        points = " ".join(f"{map_x(x):.2f},{map_y(y):.2f}" for x, y in item.points)
        lines.append(f'<polyline points="{points}" fill="none" stroke="{color}" stroke-width="4" stroke-linejoin="round"/>')
        for point_index, (x_value, y_value) in enumerate(item.points):
            x, y = map_x(x_value), map_y(y_value)
            lines.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="7" fill="{BACKGROUND}" stroke="{color}" stroke-width="4"/>')
            if point_labels:
                label_y = y - 17 if point_index % 2 == 0 else y + 29
                label = f"{y_value:.5f}".rstrip("0").rstrip(".")
                lines.append(f'<text x="{x:.2f}" y="{label_y:.2f}" text-anchor="middle" font-family="Arial, sans-serif" font-size="15" font-weight="700" fill="{color}">{escape(label)}</text>')
        lines.append(f'<line x1="{legend_x}" y1="780" x2="{legend_x + 34}" y2="780" stroke="{color}" stroke-width="5"/>')
        lines.append(f'<circle cx="{legend_x + 17}" cy="780" r="6" fill="{BACKGROUND}" stroke="{color}" stroke-width="3"/>')
        lines.append(f'<text x="{legend_x + 44}" y="786" font-family="Arial, sans-serif" font-size="17" fill="{INK}">{escape(item.label)}</text>')
        legend_x += 70 + max(90, len(item.label) * 9)
    lines.append("</svg>")
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    rows = load_rows()

    local_definitions = [
        ("Baseline GCC", lambda row: row["implementation"] == "sequential_baseline" and row["compiler_flags"] == ""),
        ("Baseline GCC -O2", lambda row: row["implementation"] == "sequential_baseline" and row["compiler_flags"] == "-O2"),
        ("Transposed GCC -O2", lambda row: row["implementation"] == "sequential_transposed"),
    ]
    local = select_series(
        rows,
        predicate=lambda row: row["environment"] == "local_pc",
        definitions=local_definitions,
        x_field="matrix_size",
    )
    render_chart(
        "sequential-local",
        "Sequential matrix multiplication — local PC",
        "Preserved single-run measurements; lower is better",
        "Matrix dimension (N × N)",
        "Runtime (milliseconds, logarithmic scale)",
        local,
        log_y=True,
    )

    titan_definitions = [
        ("Baseline GCC", lambda row: row["implementation"] == "sequential_baseline" and row["compiler"] == "gcc" and row["compiler_flags"] == ""),
        ("Baseline GCC -O2", lambda row: row["implementation"] == "sequential_baseline" and row["compiler"] == "gcc" and row["compiler_flags"] == "-O2"),
        ("Baseline ICX", lambda row: row["implementation"] == "sequential_baseline" and row["compiler"] == "icx" and row["compiler_flags"] == ""),
        ("Baseline ICX -O2", lambda row: row["implementation"] == "sequential_baseline" and row["compiler"] == "icx" and row["compiler_flags"] == "-O2"),
        ("Transposed ICX -O2", lambda row: row["implementation"] == "sequential_transposed"),
    ]
    titan = select_series(
        rows,
        predicate=lambda row: row["environment"] == "titan_sandy_bridge" and row["runtime_unit"] == "milliseconds",
        definitions=titan_definitions,
        x_field="matrix_size",
    )
    render_chart(
        "sequential-titan",
        "Sequential matrix multiplication — Titan",
        "Compiler optimization and cache-friendly transposition",
        "Matrix dimension (N × N)",
        "Runtime (milliseconds, logarithmic scale)",
        titan,
        log_y=True,
    )

    mpi_definitions = [
        ("mpiicx -O2", lambda row: row["configuration"] == "mpi_v1"),
        ("mpiicx AVX/Ofast", lambda row: row["configuration"] == "mpi_v2"),
    ]
    mpi_one = select_series(
        rows,
        predicate=lambda row: row["implementation"] == "mpi_distributed" and row["matrix_size"] == "10000" and row["mpi_ranks_per_node"] == "1" and row["transpose"] == "true",
        definitions=mpi_definitions,
        x_field="node_count",
    )
    render_chart(
        "mpi-10000-one-rank-per-node",
        "MPI scaling at N = 10,000",
        "One MPI rank per node; transposed multiplication enabled",
        "Node count",
        "Runtime (minutes)",
        mpi_one,
    )

    mpi_sixteen = select_series(
        rows,
        predicate=lambda row: row["implementation"] == "mpi_distributed" and row["matrix_size"] == "10000" and row["mpi_ranks_per_node"] == "16" and row["transpose"] == "true",
        definitions=mpi_definitions,
        x_field="node_count",
    )
    render_chart(
        "mpi-10000-sixteen-ranks-per-node",
        "MPI scaling at N = 10,000",
        "Sixteen MPI ranks per node; transposed multiplication enabled",
        "Node count",
        "Runtime (minutes)",
        mpi_sixteen,
    )

    mpi_large = select_series(
        rows,
        predicate=lambda row: row["implementation"] == "mpi_distributed" and row["matrix_size"] == "20000" and row["mpi_ranks_per_node"] == "16",
        definitions=[("mpiicx AVX/Ofast", lambda row: row["configuration"] == "mpi_v2")],
        x_field="node_count",
    )
    render_chart(
        "mpi-20000-diminishing-returns",
        "Diminishing returns at N = 20,000",
        "Sixteen MPI ranks per node; the preserved minimum occurs at 48 nodes",
        "Node count",
        "Runtime (minutes)",
        mpi_large,
        point_labels=True,
    )

    print(f"Generated 5 PNG and 5 SVG charts in {CHART_DIR}")


if __name__ == "__main__":
    main()
