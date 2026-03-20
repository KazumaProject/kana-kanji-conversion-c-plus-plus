#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Download a Hugging Face model and convert it to GGUF via vendored llama.cpp.

Default target model:
  ku-nlp/gpt2-small-japanese-char

Example:
  python3 tools/build_hf_gguf.py \
    --repo-id ku-nlp/gpt2-small-japanese-char \
        --outtype f16 \
        --quantize-type Q4_K_M
"""

from __future__ import annotations

import argparse
import importlib
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Optional


DEFAULT_REPO_ID = "ku-nlp/gpt2-small-japanese-char"


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Download HF model and convert to GGUF with local llama.cpp")
    p.add_argument("--repo-id", type=str, default=DEFAULT_REPO_ID, help="Hugging Face repo id")
    p.add_argument("--revision", type=str, default="", help="optional HF revision")
    p.add_argument("--download-dir", type=str, default="", help="local download directory")
    p.add_argument("--outfile", type=str, default="", help="output gguf file path")
    p.add_argument(
        "--outtype",
        type=str,
        default="f16",
        choices=["f32", "f16", "bf16", "q8_0", "tq1_0", "tq2_0", "auto"],
        help="GGUF output type (same as convert_hf_to_gguf.py)",
    )
    p.add_argument("--hf-token", type=str, default="", help="HF token (or use HF_TOKEN env)")
    p.add_argument("--force-download", action="store_true", help="force re-download from HF")
    p.add_argument("--convert-only", action="store_true", help="skip download and convert existing local files")
    p.add_argument(
        "--quantize-type",
        type=str,
        default="",
        help="optional llama.cpp quantization type (e.g. Q4_K_M, Q5_K_M, Q8_0)",
    )
    p.add_argument(
        "--quantized-outfile",
        type=str,
        default="",
        help="output gguf file path for quantized model (default: <outfile>.<quant>.gguf)",
    )
    p.add_argument(
        "--quantize-bin",
        type=str,
        default="",
        help="path to llama-quantize binary (auto-discovered when omitted)",
    )
    p.add_argument(
        "--keep-base",
        action="store_true",
        help="keep non-quantized output file when --quantize-type is used",
    )
    p.add_argument("--verbose", action="store_true")
    return p.parse_args()


def default_download_dir(repo_id: str) -> Path:
    # Keep one local directory per model repository.
    safe = repo_id.replace("/", "--")
    return Path("models") / "hf-rerank" / safe


def default_outfile(repo_id: str, outtype: str) -> Path:
    name = repo_id.split("/")[-1]
    return default_download_dir(repo_id) / f"{name}.{outtype}.gguf"


def get_repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def ensure_hf_download(repo_id: str, revision: Optional[str], target_dir: Path, token: Optional[str], force: bool) -> None:
    try:
        snapshot_download = importlib.import_module("huggingface_hub").snapshot_download
    except ImportError as e:
        raise RuntimeError(
            "huggingface_hub is required for download. Install with: pip install huggingface_hub"
        ) from e

    target_dir.mkdir(parents=True, exist_ok=True)

    kwargs = {
        "repo_id": repo_id,
        "local_dir": str(target_dir),
        "revision": revision if revision else None,
        "token": token if token else None,
        "force_download": force,
    }

    # Remove None values to avoid passing unsupported defaults to older versions.
    kwargs = {k: v for k, v in kwargs.items() if v is not None}
    snapshot_download(**kwargs)


def ensure_convert_compat_config(model_dir: Path) -> None:
    config_path = model_dir / "config.json"
    if not config_path.exists():
        return

    with config_path.open("r", encoding="utf-8") as f:
        cfg = json.load(f)

    # llama.cpp GPT2 converter expects `n_ctx` for GPT2LMHeadModel.
    if cfg.get("model_type") == "gpt2" and "n_ctx" not in cfg:
        n_ctx = cfg.get("n_positions") or cfg.get("max_position_embeddings")
        if isinstance(n_ctx, int) and n_ctx > 0:
            cfg["n_ctx"] = n_ctx
            with config_path.open("w", encoding="utf-8") as f:
                json.dump(cfg, f, ensure_ascii=False, indent=2)
                f.write("\n")
            print(f"[patch] added n_ctx={n_ctx} to {config_path}")


def run_convert(convert_script: Path, model_dir: Path, outtype: str, outfile: Path, verbose: bool) -> None:
    cmd = [
        sys.executable,
        str(convert_script),
        "--outtype",
        outtype,
        "--outfile",
        str(outfile),
        str(model_dir),
    ]
    if verbose:
        cmd.insert(2, "--verbose")

    cp = subprocess.run(cmd, check=False)
    if cp.returncode != 0:
        raise RuntimeError(f"GGUF conversion failed (exit={cp.returncode})")


def find_quantize_bin(repo_root: Path, user_path: str) -> Path:
    if user_path:
        p = Path(user_path)
        if p.exists() and p.is_file():
            return p
        raise RuntimeError(f"llama-quantize not found: {p}")

    candidates = [
        repo_root / "build" / "bin" / "llama-quantize",
        repo_root / "build" / "third_party" / "llama.cpp" / "bin" / "llama-quantize",
        repo_root / "third_party" / "llama.cpp" / "build" / "bin" / "llama-quantize",
    ]
    for p in candidates:
        if p.exists() and p.is_file():
            return p

    raise RuntimeError(
        "llama-quantize binary not found. Build it first (example: cmake --build build --target llama-quantize) "
        "or pass --quantize-bin <path>."
    )


def default_quantized_outfile(base_outfile: Path, quant_type: str) -> Path:
    suffix = quant_type.lower()
    return base_outfile.with_name(f"{base_outfile.stem}.{suffix}.gguf")


def run_quantize(quantize_bin: Path, infile: Path, outfile: Path, quant_type: str) -> None:
    cmd = [str(quantize_bin), str(infile), str(outfile), quant_type]
    cp = subprocess.run(cmd, check=False)
    if cp.returncode != 0:
        raise RuntimeError(f"llama-quantize failed (exit={cp.returncode})")


def main() -> int:
    args = parse_args()
    repo_root = get_repo_root()

    convert_script = repo_root / "third_party" / "llama.cpp" / "convert_hf_to_gguf.py"
    if not convert_script.exists():
        raise RuntimeError(f"convert script not found: {convert_script}")

    model_dir = Path(args.download_dir) if args.download_dir else default_download_dir(args.repo_id)
    out_path = Path(args.outfile) if args.outfile else default_outfile(args.repo_id, args.outtype)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    token = args.hf_token or os.environ.get("HF_TOKEN", "")

    if not args.convert_only:
        print(f"[download] repo={args.repo_id} -> {model_dir}")
        ensure_hf_download(
            repo_id=args.repo_id,
            revision=args.revision,
            target_dir=model_dir,
            token=token,
            force=args.force_download,
        )

    ensure_convert_compat_config(model_dir)

    print(f"[convert] model_dir={model_dir}")
    print(f"[convert] outfile={out_path}")
    run_convert(convert_script=convert_script, model_dir=model_dir, outtype=args.outtype, outfile=out_path, verbose=args.verbose)

    final_out = out_path
    if args.quantize_type:
        quantize_bin = find_quantize_bin(repo_root, args.quantize_bin)
        quant_out = Path(args.quantized_outfile) if args.quantized_outfile else default_quantized_outfile(out_path, args.quantize_type)
        quant_out.parent.mkdir(parents=True, exist_ok=True)

        print(f"[quantize] bin={quantize_bin}")
        print(f"[quantize] type={args.quantize_type}")
        print(f"[quantize] infile={out_path}")
        print(f"[quantize] outfile={quant_out}")
        run_quantize(quantize_bin=quantize_bin, infile=out_path, outfile=quant_out, quant_type=args.quantize_type)
        final_out = quant_out

        if not args.keep_base and out_path != quant_out and out_path.exists():
            out_path.unlink()
            print(f"[cleanup] removed base gguf: {out_path}")

    print(f"[done] gguf={final_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
