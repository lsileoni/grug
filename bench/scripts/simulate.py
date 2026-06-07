#!/usr/bin/env python3
import argparse
import random
import sys

try:
    import requests
except ImportError:
    sys.exit("pip install requests (or run inside the project venv)")


def score(elo):
    return 1.0 / (1.0 + 10.0 ** (-elo / 400.0))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--server", default="http://127.0.0.1:8000")
    ap.add_argument("--token", default="local-dev-token")
    ap.add_argument("--elo", type=float, default=-35.0, help="true Elo of dev vs base")
    ap.add_argument("--draw-rate", type=float, default=0.30)
    ap.add_argument("--bounds", default="0,5", help="elo0,elo1")
    ap.add_argument("--dev", default="main")
    ap.add_argument("--base", default="main")
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()
    random.seed(args.seed)

    elo0, elo1 = (float(x) for x in args.bounds.split(","))
    H = {"Authorization": f"Bearer {args.token}"}
    S = args.server.rstrip("/")

    r = requests.post(f"{S}/api/tests", headers=H, json={
        "type": "sprt", "engine": "grug", "dev_ref": args.dev, "base_ref": args.base,
        "elo0": elo0, "elo1": elo1, "alpha": 0.05, "beta": 0.05,
        "tc": "st=0.05", "book": "starter.epd", "max_pairs": 0,
        "note": f"simulated patch ({args.elo:+.0f} Elo)"})
    if r.status_code != 200:
        sys.exit(f"failed to create test: {r.status_code}\n{r.text[:300]}")
    test_id = int(r.json()["id"])
    print(f"created test #{test_id}  true Elo={args.elo:+.0f}  bounds=[{elo0:g},{elo1:g}]")

    s = score(args.elo)
    pw = max(0.0, s - args.draw_rate / 2.0)
    pd = args.draw_rate

    def game():
        r = random.random()
        return 1.0 if r < pw else 0.5 if r < pw + pd else 0.0

    wid = requests.post(f"{S}/api/worker/heartbeat", json={
        "name": "sim/1", "hostname": "sim", "cores": 2,
        "has_cutechess": True, "has_stockfish": False}, headers=H).json()["worker_id"]

    n = 0
    while True:
        task = requests.post(f"{S}/api/worker/request",
                             json={"worker_id": wid}, headers=H).json()["task"]
        if not task:
            print("no more work")
            break
        penta = [0, 0, 0, 0, 0]
        w = d = l = 0
        for _ in range(task["pairs"]):
            a, b = game(), game()
            for g in (a, b):
                w += g == 1.0; d += g == 0.5; l += g == 0.0
            penta[int(round((a + b) * 2))] += 1
        t = requests.post(f"{S}/api/worker/submit", json={
            "batch_id": task["batch_id"], "worker_id": wid,
            "wdl": [w, d, l], "pentanomial": penta, "crashes": 0, "elapsed": 1},
            headers=H).json()["test"]
        n += 1
        if t and (n % 20 == 0 or t["status"] != "active"):
            print(f"  batch {n:3d}  games={t['games']:6d}  LLR={t['llr']:+6.2f} "
                  f"[{t['llr_lower']:.2f},{t['llr_upper']:.2f}]  "
                  f"elo={t['elo']:+5.1f} ±{(t['elo_hi']-t['elo_lo'])/2:4.1f}  "
                  f"LOS={t['los']*100:4.1f}%  {t['status']} {t['result']}")
        if t and t["status"] != "active":
            print(f"\nVERDICT: test #{test_id} -> {t['status'].upper()} "
                  f"({t['result'] or 'n/a'}) after {t['games']} games")
            break


if __name__ == "__main__":
    main()
