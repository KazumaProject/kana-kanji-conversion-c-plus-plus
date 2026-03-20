#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Run AJIMEE-Bench with astar_zenz_rerank_cli while selecting model path explicitly.

This is a thin wrapper over tools/run_ajimee_bench.py that builds the --cmd template.
"""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path
from typing import List


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Run AJIMEE-Bench with astar_zenz_rerank_cli")

    p.add_argument("--items", type=str, default="AJIMEE-Bench/JWTD_v2/v1/evaluation_items.json")
    p.add_argument("--subset", type=str, default="no_context", choices=["all", "no_context", "with_context"])

    p.add_argument("--n", type=int, default=10)
    p.add_argument("--beam", type=int, default=50)
    p.add_argument("--k", type=int, default=10)
    p.add_argument("--timeout", type=int, default=20)

    p.add_argument("--progress", action="store_true")
    p.add_argument("--every", type=int, default=20)
    p.add_argument("--verbose", action="store_true")

    p.add_argument("--out_jsonl", type=str, default="")
    p.add_argument("--fail_jsonl", type=str, default="")
    p.add_argument(
        "--fail_mode",
        type=str,
        default="either",
        choices=["acc1", "accn", "either", "both", "error"],
    )

    p.add_argument("--cli", type=str, default="./build/astar_zenz_rerank_cli")
    p.add_argument("--yomi_termid", type=str, default="./build/yomi_termid.louds")
    p.add_argument("--tango", type=str, default="./build/tango.louds")
    p.add_argument("--tokens", type=str, default="./build/token_array.bin")
    p.add_argument("--pos_table", type=str, default="./build/pos_table.bin")
    p.add_argument("--conn", type=str, default="./build/connection_single_column.bin")

    p.add_argument("--model", type=str, required=True, help="GGUF model path for --zenz_model")

    p.add_argument("--yomi_mode", type=str, default="cps", choices=["cps", "cps_pred", "cps_omit", "all"])
    p.add_argument("--pred_k", type=int, default=1)

    p.add_argument("--article_like", action="store_true", help="enable --zenz_article_like preset")
    p.add_argument("--zenz_rerank_mode", type=str, default="linear_fuse", choices=["zenz_only", "linear_fuse"])
    p.add_argument("--zenz_score_mode", type=str, default="diff", choices=["whole", "diff"])
    p.add_argument("--zenz_alpha", type=float, default=0.35)
    p.add_argument("--zenz_beta", type=float, default=1.0)
    p.add_argument("--zenz_use_raw", action="store_true")

    p.add_argument(
        "--extra_arg",
        action="append",
        default=[],
        help="extra raw arg string forwarded to astar_zenz_rerank_cli (repeatable)",
    )

    p.add_argument("--dry_run", action="store_true", help="print generated command and exit")

    return p.parse_args()


def quote_cmd(parts: List[str]) -> str:
    return " ".join(shlex.quote(x) for x in parts)


def build_rerank_cmd(args: argparse.Namespace) -> str:
    cmd: List[str] = [
        args.cli,
        "--yomi_termid",
        args.yomi_termid,
        "--tango",
        args.tango,
        "--tokens",
        args.tokens,
        "--pos_table",
        args.pos_table,
        "--conn",
        args.conn,
        "--zenz_model",
        args.model,
        "--q",
        "{q}",
        "--n",
        "{n}",
        "--beam",
        "{beam}",
        "--yomi_mode",
        args.yomi_mode,
        "--pred_k",
        str(args.pred_k),
    ]

    if args.article_like:
        cmd.append("--zenz_article_like")
    else:
        cmd.extend([
            "--zenz_rerank_mode",
            args.zenz_rerank_mode,
            "--zenz_score_mode",
            args.zenz_score_mode,
            "--zenz_alpha",
            str(args.zenz_alpha),
            "--zenz_beta",
            str(args.zenz_beta),
        ])
        if args.zenz_use_raw:
            cmd.append("--zenz_use_raw")

    for raw in args.extra_arg:
        cmd.extend(shlex.split(raw))

    return quote_cmd(cmd)


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parent.parent
    bench_script = repo_root / "tools" / "run_ajimee_bench.py"

    if not bench_script.exists():
        raise RuntimeError(f"bench script not found: {bench_script}")

    rerank_cmd = build_rerank_cmd(args)

    bench_cmd: List[str] = [
        sys.executable,
        str(bench_script),
        "--items",
        args.items,
        "--subset",
        args.subset,
        "--n",
        str(args.n),
        "--beam",
        str(args.beam),
        "--k",
        str(args.k),
        "--timeout",
        str(args.timeout),
        "--every",
        str(args.every),
        "--fail_mode",
        args.fail_mode,
        "--cmd",
        rerank_cmd,
    ]

    if args.progress:
        bench_cmd.append("--progress")
    if args.verbose:
        bench_cmd.append("--verbose")
    if args.out_jsonl:
        bench_cmd.extend(["--out_jsonl", args.out_jsonl])
    if args.fail_jsonl:
        bench_cmd.extend(["--fail_jsonl", args.fail_jsonl])

    print("[rerank_cmd]")
    print(rerank_cmd)
    print("[bench_cmd]")
    print(quote_cmd(bench_cmd))

    if args.dry_run:
        return 0

    cp = subprocess.run(bench_cmd, check=False)
    return cp.returncode


if __name__ == "__main__":
    raise SystemExit(main())
