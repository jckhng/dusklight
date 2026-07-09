#!/usr/bin/env python3
import os
import time

import renderdoc as rd


WD = "/mnt/mmc/ports/sdl2shim-gles-renderdoc-probe"
EXE = f"{WD}/sdl2shim-gles-present-probe.aarch64"


def env_mod(name, value):
    env = rd.EnvironmentModification()
    env.name = name
    env.value = value
    env.mod = rd.EnvMod.Set
    env.sep = rd.EnvSep.NoSep
    return env


def show_processes(label):
    print(f"--- ps {label} ---", flush=True)
    os.system("ps | grep -E 'sdl2shim|renderdoc' | grep -v grep || true")


def main():
    os.system(f"rm -rf {WD}/captures && mkdir -p {WD}/captures")
    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    status, remote = rd.CreateRemoteServerConnection("localhost")
    print(f"connect status={status} remote={remote}", flush=True)
    if str(status) != "0":
        return 2

    envs = [
        env_mod("LD_LIBRARY_PATH", f"{WD}/renderdoc/lib:{WD}"),
        env_mod("SDL_VIDEODRIVER", "sdl2"),
        env_mod("SDL3SHIM_SDL2_LIB", "libSDL2-2.0.so.0"),
        env_mod("PROBE_RENDERDOC_CAPTURE_AFTER", "0"),
        env_mod("PROBE_IGNORE_INPUT", "1"),
    ]

    result = remote.ExecuteAndInject(EXE, WD, "3000", envs, rd.CaptureOptions())
    print(f"execute status={result.status} ident={result.ident}", flush=True)
    if str(result.status) != "0":
        remote.ShutdownServerAndConnection()
        rd.ShutdownReplay()
        return 3

    target = None
    for attempt in range(20):
        show_processes(attempt)
        try:
            targets = rd.EnumerateRemoteTargets("localhost", 0)
            print(f"enumerate targets={targets}", flush=True)
        except Exception as exc:
            print(f"enumerate failed {type(exc).__name__}: {exc}", flush=True)

        target = rd.CreateTargetControl("localhost", result.ident, "codex-remote-probe.py", True)
        print(f"target attempt={attempt} target={target}", flush=True)
        if target is not None:
            break
        time.sleep(0.5)

    if target is None:
        print("target control failed", flush=True)
        remote.ShutdownServerAndConnection()
        rd.ShutdownReplay()
        return 4

    print("target connected", flush=True)
    target.TriggerCapture(1)
    print("trigger sent", flush=True)
    try:
        target.QueueCapture(30, 1)
        print("queue capture sent", flush=True)
    except Exception as exc:
        print(f"queue capture failed {type(exc).__name__}: {exc}", flush=True)

    got = None
    start = time.time()
    while time.time() - start < 20:
        msg = target.ReceiveMessage(None)
        if msg is not None:
            msg_type = getattr(msg, "type", None)
            cap = getattr(msg, "newCapture", None)
            print(
                f"message type={msg_type} newCapturePath={getattr(cap, 'path', '')} "
                f"byteSize={getattr(cap, 'byteSize', 0)} api={getattr(cap, 'api', '')}",
                flush=True,
            )
            if msg_type == rd.TargetControlMessageType.NewCapture and cap is not None and cap.path:
                got = msg
                break
        time.sleep(0.25)

    if got is None:
        print("no capture message", flush=True)
    else:
        cap = got.newCapture
        attrs = [(name, getattr(cap, name)) for name in dir(cap) if not name.startswith("_")]
        print(f"capture attrs={attrs}", flush=True)

    target.Shutdown()
    remote.ShutdownServerAndConnection()
    rd.ShutdownReplay()
    os.system(f"find {WD}/captures -maxdepth 1 -type f -ls 2>/dev/null || true")
    return 0 if got is not None else 5


if __name__ == "__main__":
    raise SystemExit(main())
