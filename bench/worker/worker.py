from __future__ import annotations

import argparse
import os
import shutil
import socket
import sys
import time
import traceback

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(_HERE))
sys.path.insert(0, _HERE)

import requests                              # noqa: E402

import builder                               # noqa: E402
import cutechess                             # noqa: E402
import references                            # noqa: E402
import sandbox as sandboxmod                 # noqa: E402
from bench import config as configmod        # noqa: E402

IDLE_SLEEP = 5
ERROR_SLEEP = 10


class Client:
    def __init__(self, cfg: dict):
        self.cfg = cfg
        self.w = configmod.worker(cfg)
        self.base = self.w["server_url"].rstrip("/")
        self.session = requests.Session()
        self.session.headers["Authorization"] = f"Bearer {configmod.auth(cfg)['worker']}"
        self.name = f"{socket.gethostname()}/{os.getpid()}"
        self.cache = configmod.resolve(cfg, self.w["cache"])
        self.books = os.path.join(cfg.get("_dir", "."), "books")
        self.workdir = configmod.resolve(cfg, "data/work")
        self.worker_id = None
        self.sandbox = sandboxmod.from_config(self.w)
        cc = self.w["cutechess"]
        if os.sep in cc and not os.path.isabs(cc):
            cc = configmod.resolve(cfg, cc)
        self.cutechess = shutil.which(cc) or cc

    def _post(self, path: str, payload: dict) -> dict:
        r = self.session.post(self.base + path, json=payload, timeout=60)
        r.raise_for_status()
        return r.json()

    def heartbeat(self):
        body = {
            "name": self.name, "hostname": socket.gethostname(),
            "cores": os.cpu_count() or 1,
            "has_cutechess": bool(shutil.which(self.cutechess) or os.path.exists(self.cutechess)),
            "has_stockfish": references.available(self.w, "stockfish"),
        }
        self.worker_id = self._post("/api/worker/heartbeat", body)["worker_id"]

    def request(self) -> dict:
        return self._post("/api/worker/request", {"worker_id": self.worker_id})["task"]

    def submit(self, batch_id: int, results: dict):
        body = {"batch_id": batch_id, "worker_id": self.worker_id, **results}
        self._post("/api/worker/submit", body)

    def submit_error(self, batch_id: int, error: str):
        self._post("/api/worker/submit", {"batch_id": batch_id,
                                          "worker_id": self.worker_id, "error": error})

    def _build_side(self, task: dict, side: dict) -> tuple:
        if side["kind"] == "reference":
            return references.resolve(self.w, side["name"]), dict(side.get("options", {}))
        name = task["engine"]["name"]
        source = self.w.get("sources", {}).get(name) or task["engine"]["source"]
        path = builder.ensure_build(self.cache, source, side["sha"],
                                     task["engine"]["build"], task["engine"]["binary"],
                                     sandbox=self.sandbox)
        return path, dict(side.get("options", {}))

    def _validate_options(self, cmd: str, options: dict, label: str):
        if not options:
            return
        proc = self.sandbox.exec(
            [cmd],
            ro=[self.cache],
            input="uci\nquit\n",
            timeout=15,
        )
        supported = set()
        for line in proc.stdout.splitlines():
            if not line.startswith("option name "):
                continue
            rest = line[len("option name "):]
            name = rest.split(" type ", 1)[0].strip()
            supported.add(name)
        missing = [name for name in options if name not in supported]
        if missing:
            available = ", ".join(sorted(supported)) if supported else "none"
            raise RuntimeError(
                f"{label} does not support UCI option(s): {', '.join(missing)}. "
                f"Available options: {available}. The cached build is probably from a ref "
                "that does not contain the requested algorithm.")

    def _resolve_book(self, name: str) -> str:
        if name in ("", ".", "..") or os.path.basename(name) != name:
            raise ValueError(f"invalid opening book name: {name!r}")
        real_books = os.path.realpath(self.books)
        real_path = os.path.realpath(os.path.join(real_books, name))
        if os.path.commonpath([real_books, real_path]) != real_books:
            raise ValueError(f"opening book escapes books directory: {name!r}")
        if not os.path.exists(real_path):
            raise FileNotFoundError(f"opening book not found: {real_path}")
        return real_path

    def do_task(self, task: dict):
        batch_id = task["batch_id"]
        cmd_a, opt_a = self._build_side(task, task["first"])
        cmd_b, opt_b = self._build_side(task, task["second"])
        self._validate_options(cmd_a, opt_a, "engine A")
        self._validate_options(cmd_b, opt_b, "engine B")
        book = self._resolve_book(task["book"])

        wd = os.path.join(self.workdir, f"batch-{batch_id}")
        ro_mounts = [self.cutechess, self.cache, self.books,
                     os.path.dirname(cmd_a), os.path.dirname(cmd_b)]
        print(f"  batch {batch_id}: {task['pairs']} pairs  "
              f"A={os.path.basename(cmd_a)}  B={os.path.basename(cmd_b)}  tc={task['tc']}")
        results = cutechess.run_match(
            self.cutechess, cmd_a, cmd_b, options_a=opt_a, options_b=opt_b,
            tc=task["tc"], book=book, pairs=task["pairs"],
            concurrency=self.w["concurrency"], workdir=wd,
            runner=self.w.get("runner"),
            sandbox=self.sandbox, ro_mounts=ro_mounts)
        print(f"  batch {batch_id}: W/D/L={results['wdl']}  penta={results['pentanomial']}  "
              f"{results['elapsed']}s")
        self.submit(batch_id, results)

    def run(self):
        print(f"worker {self.name} -> {self.base}")
        print(f"  cutechess: {self.cutechess}  cache: {self.cache}")
        if self.sandbox.enabled:
            print(f"  sandbox: {self.sandbox.engine} image={self.sandbox.image}")
        else:
            print("  sandbox: DISABLED - builds and engines run directly on this host. "
                  "Set worker.sandbox.enabled for untrusted refs.")
        last_beat = 0.0
        while True:
            try:
                if time.time() - last_beat > 30:
                    self.heartbeat()
                    last_beat = time.time()
                task = self.request()
                if not task:
                    time.sleep(IDLE_SLEEP)
                    continue
                try:
                    self.do_task(task)
                except Exception as e:
                    print(f"  batch {task['batch_id']} FAILED: {e}")
                    traceback.print_exc()
                    self.submit_error(task["batch_id"], str(e))
            except requests.RequestException as e:
                print(f"transport error: {e}; retrying in {ERROR_SLEEP}s")
                time.sleep(ERROR_SLEEP)
            except KeyboardInterrupt:
                print("\nstopping.")
                return


def main():
    ap = argparse.ArgumentParser(description="Grug Bench worker")
    ap.add_argument("--config", default=os.environ.get("BENCH_CONFIG", "config.yaml"))
    args = ap.parse_args()
    cfg = configmod.load(args.config)
    Client(cfg).run()


if __name__ == "__main__":
    main()
