# Getting Started

This repo contains:

- `grug`: a C11 UCI chess engine.
- `bench/`: a Flask + SQLite benchmark server and worker system for testing git refs with a UCI match runner.

The fastest contribution path is the project container. It installs the compiler toolchain, Python dependencies, `fastchess`, Stockfish, and a local benchmark config.

## Docker Environment

Prerequisites on your host:

- Docker.
- `make`.

Start the development container:

```sh
make env
```

That builds `.devcontainer/Dockerfile`, starts a long-running container named `grug-dev`, mounts this repo at `/workspaces/grug`, runs `.devcontainer/setup.sh`, and forwards host port `8001` to container port `8000`. The image includes the Python packages needed by the helper scripts, so setup does not need pip network access once the image is built.

After that, normal project targets run inside the container even when typed on the host:

```sh
make
make test
make bench
make style-check
```

This keeps compiler and Python behavior consistent while still writing build output into the shared repo directory.

Open a shell in it:

```sh
make env-shell
```

You can also use regular Docker directly:

```sh
docker exec -it grug-dev bash
```

Stop it:

```sh
make env-stop
```

Remove it when you want a fresh container:

```sh
make env-rm
make env
```

Useful overrides:

```sh
make env ENV_CONTAINER=my-grug ENV_PORT=8010
make env ENV_BUILD_ARGS='--build-arg FASTCHESS_REF=main'
make env ENV_RUN_ARGS='-v /var/run/docker.sock:/var/run/docker.sock'
```

`FASTCHESS_REF` is optional; by default the image builds Fastchess from its repository default branch. The Docker socket mount is only needed if you want to build/run the optional worker sandbox image from inside the environment container.

## Dev Container Editors

The same image is also a Dev Container. If your editor supports Dev Containers, open the repo in it and use the provided `.devcontainer/devcontainer.json`.

On first create, `.devcontainer/setup.sh` will:

- Install Python dependencies into `.venv`.
- Create `bench/config.yaml` if it does not exist.
- Build `build/grug`.
- Configure the local bench stack with:
  - admin token: `local-admin-token`
  - worker token: `local-worker-token`
  - runner: `fastchess`
  - Stockfish reference engine from the container package.

## Common Commands

Run these inside the Docker environment, Dev Container, or on the host. Host runs delegate into the container:

```sh
make native
make test
make bench
make style-check
```

Play against the engine:

```sh
python tools/play.py --engine ./build/grug --side white
```

Run the benchmark stack locally:

```sh
cd bench
./benchctl start both --config config.yaml
./benchctl status --config config.yaml
./benchctl logs server --config config.yaml
./benchctl logs worker --config config.yaml
./benchctl stop --config config.yaml
```

Open the UI:

```text
http://127.0.0.1:8001
```

Creating or stopping runs requires login:

```text
http://127.0.0.1:8001/login
```

Use `local-admin-token`.

If you are using a Dev Container editor or a manual host setup instead of `make env`, the bench server listens directly on `http://127.0.0.1:8000`.

## Manual Setup

If you are not using the project container, install the local Linux prerequisites:

```sh
sudo apt-get update
sudo apt-get install -y build-essential gcc-mingw-w64-x86-64 git python3 python3-venv python3-pip cmake clang-format stockfish
```

Install Python dependencies:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
```

Build the engine:

```sh
make native
```

For benchmark workers, install a match runner. Either build the helper copy of `cutechess-cli`:

```sh
bench/scripts/get_cutechess.sh
```

or install/use `fastchess` and set `worker.cutechess: "fastchess"` in `bench/config.yaml`.

Create a local bench config:

```sh
cp bench/config.example.yaml bench/config.yaml
```

For trusted local development, set:

```yaml
auth:
  admin_token: "local-admin-token"
  worker_token: "local-worker-token"
  session_secret: "local-session-secret"
  allow_insecure: true

engines:
  grug:
    source: "/absolute/path/to/this/repo"
    build: "make -j ARCH= EXE=grug"
    binary: "grug"

server:
  host: "127.0.0.1"
  port: 8000
  database: "data/bench.db"
  batch_pairs: 25
  secure_cookies: false

worker:
  server_url: "http://127.0.0.1:8000"
  concurrency: 2
  cutechess: "fastchess"
  cache: "data/engines"
  references:
    stockfish:
      path: "stockfish"
```

Start server and worker:

```sh
cd bench
./benchctl start both --config config.yaml
```

The `source` path must point at a git repository containing the refs being tested. If the server cannot resolve a branch or SHA, fetch it in that source repo first.

## Creating A Benchmark Test

Browser flow:

```text
http://127.0.0.1:8000/login
http://127.0.0.1:8000/new
```

API flow:

```sh
curl -sS -X POST http://127.0.0.1:8000/api/tests \
  -H "Authorization: Bearer local-admin-token" \
  -H "Content-Type: application/json" \
  -d '{
    "engine": "grug",
    "type": "sprt",
    "dev_ref": "your-branch-or-sha",
    "base_ref": "main",
    "elo0": -5,
    "elo1": 0,
    "tc": "nodes=200000",
    "max_pairs": 4000
  }'
```

## Production Bench Deployment

The short version:

- Use split secrets: `auth.admin_token` for UI/CI, `auth.worker_token` for workers.
- Serve `bench/server/wsgi.py` with `bench/scripts/run_gunicorn.sh`.
- Keep Gunicorn to one worker process and scale with `BENCH_THREADS`.
- Bind the app to `127.0.0.1` and put Caddy or Nginx in front for HTTPS.
- Set `server.secure_cookies: true` behind HTTPS.
- Run workers on disposable hosts.
- Enable the worker sandbox for untrusted refs.

Production references:

- [bench/SECURITY.md](bench/SECURITY.md)
- [bench/deploy/Caddyfile.example](bench/deploy/Caddyfile.example)
- [bench/deploy/nginx.conf.example](bench/deploy/nginx.conf.example)
- [bench/worker/sandbox/Dockerfile](bench/worker/sandbox/Dockerfile)

Build the optional worker sandbox image:

```sh
docker build -t grug-bench-sandbox:latest bench/worker/sandbox
```

You can pin Fastchess there the same way:

```sh
docker build --build-arg FASTCHESS_REF=main -t grug-bench-sandbox:latest bench/worker/sandbox
```

## GitHub Actions

`.github/workflows/benchmark.yml` submits an SPRT benchmark for same-repository pull requests targeting `main`.

Required repository secrets:

- `BENCH_URL`: public benchmark server URL, for example `https://bench.example.com`.
- `BENCH_TOKEN`: benchmark admin token, not the worker token.

The benchmark server must be able to resolve both the PR head SHA and `main` from `engines.grug.source`.

## Troubleshooting

`ref not found`: fetch the missing branch/SHA in the configured engine source repo.

`refusing to start: token is unset or a known placeholder`: set real `auth.admin_token` and `auth.worker_token`, or use `auth.allow_insecure: true` for local-only development.

`has_cutechess: no`: install the configured match runner and make `worker.cutechess` point to it. The field can point to `cutechess-cli` or `fastchess`.

`opening book not found`: keep books in `bench/books/`; the default is `starter.epd`.

`build did not produce 'grug'`: make sure `engines.grug.build` and `engines.grug.binary` agree. The local config uses `make -j ARCH= EXE=grug` and `binary: "grug"`.
