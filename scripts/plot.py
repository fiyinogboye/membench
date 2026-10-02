#!/usr/bin/env python3
"""Plot membench CSV output.

Usage: python3 scripts/plot.py results [--title "CPU name"] [--out docs]
Requires: pip install matplotlib
"""
import argparse
import csv
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter


def read_csv(path):
    if not os.path.exists(path):
        return None
    with open(path) as f:
        rows = list(csv.DictReader(f))
    return {k: [float(r[k]) for r in rows] for k in rows[0]} if rows else None


def fmt_bytes(x, _pos=None):
    if x >= 1 << 30:
        return f"{x / (1 << 30):g} GB"
    if x >= 1 << 20:
        return f"{x / (1 << 20):g} MB"
    return f"{x / (1 << 10):g} KB"


def size_plot(data, ycol, ylabel, title, path, logy):
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.plot(data["bytes"], data[ycol], marker="o", markersize=4, linewidth=1.8, color="#d6342b")
    ax.set_xscale("log", base=2)
    if logy:
        ax.set_yscale("log")
    ticks = [b for b in data["bytes"] if (int(b) & (int(b) - 1)) == 0]
    ax.set_xticks(ticks[::2])
    ax.xaxis.set_major_formatter(FuncFormatter(fmt_bytes))
    ax.minorticks_off()
    plt.setp(ax.get_xticklabels(), rotation=45, ha="right")
    ax.set_xlabel("Working set size")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print("wrote", path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir", help="directory containing the CSV files")
    ap.add_argument("--title", default="", help="appended to each plot title (e.g. CPU model)")
    ap.add_argument("--out", default=None, help="output directory (default: same as input)")
    args = ap.parse_args()
    out = args.out or args.dir
    os.makedirs(out, exist_ok=True)
    suffix = f" - {args.title}" if args.title else ""

    lat = read_csv(os.path.join(args.dir, "latency.csv"))
    if lat:
        size_plot(lat, "ns", "Latency per load (ns)", "Memory latency" + suffix,
                  os.path.join(out, "latency.png"), logy=True)

    bw = read_csv(os.path.join(args.dir, "bandwidth.csv"))
    if bw:
        size_plot(bw, "gbs", "Read bandwidth (GB/s)", "Single-thread read bandwidth" + suffix,
                  os.path.join(out, "bandwidth.png"), logy=False)

    st = read_csv(os.path.join(args.dir, "stream.csv"))
    if st:
        fig, ax = plt.subplots(figsize=(9, 5))
        ax.plot(st["threads"], st["gbs"], marker="o", linewidth=1.8, color="#d6342b")
        ax.set_xlabel("Threads")
        ax.set_ylabel("Triad bandwidth (GB/s)")
        ax.set_title("Memory bandwidth scaling" + suffix)
        ax.set_xticks(st["threads"])
        ax.grid(True, alpha=0.3)
        fig.tight_layout()
        path = os.path.join(out, "stream.png")
        fig.savefig(path, dpi=150)
        plt.close(fig)
        print("wrote", path)


if __name__ == "__main__":
    main()
