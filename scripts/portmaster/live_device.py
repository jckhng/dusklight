#!/usr/bin/env python3
"""
Small helper for the Dusklight PortMaster live-device loop.

This intentionally does not store a host or password. Pass --host each time,
and set PM_PASSWORD for password-based muOS test devices.
"""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
from pathlib import Path

try:
    import pexpect
except ImportError:  # pragma: no cover - only used on local developer machines
    pexpect = None


ROOT_DIR = Path(__file__).resolve().parents[2]
DEFAULT_STAGE_DIR = ROOT_DIR / "artifacts" / "portmaster-test" / "Dusklight"
REMOTE_GAME_DIR = "/mnt/mmc/ports/dusklight"
REMOTE_LAUNCHER = "/mnt/mmc/ROMS/Ports/dusklight.sh"
REMOTE_BINARY = f"{REMOTE_GAME_DIR}/dusklight.aarch64"
REMOTE_LOG = f"{REMOTE_GAME_DIR}/log.txt"


def run_interactive(cmd: list[str], password: str | None) -> None:
    print("$ " + shlex.join(cmd), flush=True)
    if not password:
        subprocess.run(cmd, check=True)
        return
    if pexpect is None:
        raise SystemExit("PM_PASSWORD was set, but python pexpect is not installed")

    child = pexpect.spawn(cmd[0], cmd[1:], encoding="utf-8", timeout=600)
    while True:
        idx = child.expect([r"[Pp]assword:", r"yes/no", pexpect.EOF, pexpect.TIMEOUT])
        sys.stdout.write(child.before)
        sys.stdout.flush()
        if idx == 0:
            child.sendline(password)
        elif idx == 1:
            child.sendline("yes")
        elif idx == 2:
            break
        else:
            child.close(force=True)
            raise SystemExit("Command timed out")
    child.close()
    if child.exitstatus not in (0, None):
        raise SystemExit(f"Command failed with status {child.exitstatus}: {shlex.join(cmd)}")


def ssh_cmd(host: str, remote_cmd: str) -> list[str]:
    return [
        "ssh",
        "-o",
        "StrictHostKeyChecking=no",
        "-o",
        "UserKnownHostsFile=/dev/null",
        f"root@{host}",
        remote_cmd,
    ]


def scp_cmd(source: Path, host: str, dest: str, recursive: bool = False) -> list[str]:
    cmd = [
        "scp",
        "-o",
        "StrictHostKeyChecking=no",
        "-o",
        "UserKnownHostsFile=/dev/null",
    ]
    if recursive:
        cmd.append("-r")
    cmd.extend([str(source), f"root@{host}:{dest}"])
    return cmd


def deploy(args: argparse.Namespace, password: str | None) -> None:
    stage_dir = args.stage_dir.resolve()
    binary = stage_dir / "dusklight" / "dusklight.aarch64"
    launcher = stage_dir / "dusklight.sh"
    lib_dir = stage_dir / "dusklight" / "lib.aarch64"
    res_dir = stage_dir / "dusklight" / "res"

    if not binary.is_file():
        raise SystemExit(f"Binary not found: {binary}")
    if not launcher.is_file():
        raise SystemExit(f"Launcher not found: {launcher}")

    if not args.no_kill:
        kill(args, password)

    run_interactive(
        ssh_cmd(args.host, f"mkdir -p {shlex.quote(REMOTE_GAME_DIR)} /mnt/mmc/ROMS/Ports"),
        password,
    )
    if not args.script_only:
        run_interactive(scp_cmd(binary, args.host, REMOTE_BINARY), password)
        if lib_dir.is_dir():
            run_interactive(scp_cmd(lib_dir, args.host, f"{REMOTE_GAME_DIR}/", recursive=True), password)
        if res_dir.is_dir():
            run_interactive(scp_cmd(res_dir, args.host, f"{REMOTE_GAME_DIR}/", recursive=True), password)
    run_interactive(scp_cmd(launcher, args.host, REMOTE_LAUNCHER), password)
    verify(args, password)


def kill(args: argparse.Namespace, password: str | None) -> None:
    run_interactive(ssh_cmd(args.host, "pkill -x dusklight.aarch64 || true"), password)


def verify(args: argparse.Namespace, password: str | None) -> None:
    remote_cmd = (
        f"chmod +x {shlex.quote(REMOTE_LAUNCHER)} {shlex.quote(REMOTE_BINARY)} 2>/dev/null || true; "
        f"ls -l {shlex.quote(REMOTE_LAUNCHER)} {shlex.quote(REMOTE_BINARY)}; "
        f"grep -nE 'STRIP_TOPOLOGY|BATCH_STRIPS|BATCH_QUADS|SAFE_PACING|GX_STATS|RENDER_WIDTH|RENDER_HEIGHT' "
        f"{shlex.quote(REMOTE_LAUNCHER)} || true"
    )
    run_interactive(ssh_cmd(args.host, remote_cmd), password)


def tail(args: argparse.Namespace, password: str | None) -> None:
    pattern = args.pattern or "PortMaster timing|gx_prim|FATAL|terminate|ERROR"
    remote_cmd = f"grep -E {shlex.quote(pattern)} {shlex.quote(REMOTE_LOG)} | tail -n {args.lines}"
    run_interactive(ssh_cmd(args.host, remote_cmd), password)


def remote_run(args: argparse.Namespace, password: str | None) -> None:
    run_interactive(ssh_cmd(args.host, args.remote_cmd), password)


def profile(args: argparse.Namespace, password: str | None) -> None:
    remote_cmd = f"""duration={int(args.duration)} interval={int(args.interval)}; end=$(( $(date +%s) + duration )); prev_time=$(date +%s); prev_ticks=; echo 'ts pid cpu_pct rss_kb threads mem_avail_kb gpu_freq_hz gpu_util_pct gpu_mem_kb load1'; while [ "$(date +%s)" -lt "$end" ]; do now=$(date +%s); pid=$(pidof dusklight.aarch64 2>/dev/null | awk '{{print $1}}'); cpu=''; rss=''; threads=''; if [ -n "$pid" ] && [ -r "/proc/$pid/stat" ]; then ticks=$(awk '{{print $14+$15}}' "/proc/$pid/stat"); if [ -n "$prev_ticks" ]; then dt=$(( now - prev_time )); [ "$dt" -le 0 ] && dt=1; hz=$(getconf CLK_TCK 2>/dev/null || echo 100); cpu=$(awk -v a="$ticks" -v b="$prev_ticks" -v hz="$hz" -v dt="$dt" 'BEGIN {{ printf "%.1f", ((a-b)/hz/dt)*100.0 }}'); fi; prev_ticks=$ticks; rss=$(awk '/VmRSS:/ {{print $2}}' "/proc/$pid/status" 2>/dev/null); threads=$(awk '/Threads:/ {{print $2}}' "/proc/$pid/status" 2>/dev/null); else prev_ticks=; fi; prev_time=$now; mem=$(awk '/MemAvailable:/ {{print $2}}' /proc/meminfo 2>/dev/null); freq=$(cat /sys/class/devfreq/gpu/cur_freq 2>/dev/null); gpu_dump=$(cat /sys/kernel/debug/sunxi_gpu/dump 2>/dev/null); util=$(printf '%s\\n' "$gpu_dump" | awk -F: '/Utilisation from last show/ {{gsub(/[^0-9.]/,\"\",$2); print $2}}'); gpu_mem=$(awk 'NR==1 {{print $2}}' /sys/kernel/debug/mali0/gpu_memory 2>/dev/null); load1=$(awk '{{print $1}}' /proc/loadavg 2>/dev/null); echo "$now ${{pid:-0}} ${{cpu:-na}} ${{rss:-0}} ${{threads:-0}} ${{mem:-0}} ${{freq:-0}} ${{util:-na}} ${{gpu_mem:-0}} ${{load1:-0}}"; sleep "$interval"; done"""
    run_interactive(ssh_cmd(args.host, remote_cmd), password)


def main() -> None:
    parser = argparse.ArgumentParser(description="Deploy and inspect Dusklight on a live PortMaster device")
    parser.add_argument("--host", required=True, help="Device IP or hostname")
    parser.add_argument(
        "--stage-dir",
        type=Path,
        default=DEFAULT_STAGE_DIR,
        help=f"Staged package directory, default: {DEFAULT_STAGE_DIR}",
    )
    parser.add_argument("--password", default=os.environ.get("PM_PASSWORD"), help="SSH password; prefer PM_PASSWORD")

    subparsers = parser.add_subparsers(dest="command", required=True)

    deploy_parser = subparsers.add_parser("deploy", help="Kill current process, copy binary and launcher, then verify")
    deploy_parser.add_argument("--script-only", action="store_true", help="Copy only dusklight.sh")
    deploy_parser.add_argument("--no-kill", action="store_true", help="Do not pkill before deploying")
    deploy_parser.set_defaults(func=deploy)

    kill_parser = subparsers.add_parser("kill", help="Kill the running Dusklight process")
    kill_parser.set_defaults(func=kill)

    verify_parser = subparsers.add_parser("verify", help="Show remote launcher and binary details")
    verify_parser.set_defaults(func=verify)

    tail_parser = subparsers.add_parser("tail", help="Show recent useful log lines")
    tail_parser.add_argument("--lines", type=int, default=30)
    tail_parser.add_argument("--pattern", default="")
    tail_parser.set_defaults(func=tail)

    run_parser = subparsers.add_parser("run", help="Run a diagnostic command on the device")
    run_parser.add_argument("remote_cmd")
    run_parser.set_defaults(func=remote_run)

    profile_parser = subparsers.add_parser("profile", help="Sample Dusklight CPU/RAM and available GPU counters")
    profile_parser.add_argument("--duration", type=int, default=60, help="Sampling duration in seconds")
    profile_parser.add_argument("--interval", type=int, default=2, help="Sampling interval in seconds")
    profile_parser.set_defaults(func=profile)

    args = parser.parse_args()
    args.func(args, args.password)


if __name__ == "__main__":
    main()
