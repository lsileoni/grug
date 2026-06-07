"""WSGI entrypoint for production servers (gunicorn/uwsgi).

The Werkzeug dev server in app.main() is fine for local use but should not face
the public internet. Serve this module instead, e.g.:

    BENCH_CONFIG=/srv/grug-bench/config.yaml \
    gunicorn --chdir bench/server --workers 1 --threads 8 \
             --bind 127.0.0.1:8000 wsgi:app

IMPORTANT: keep it to a SINGLE worker process. The scheduler serialises batch
assignment with an in-process lock (server/scheduler.py), so multiple worker
processes could hand the same work out twice. Scale with threads, not workers.
Put a TLS reverse proxy in front (see deploy/ for nginx/Caddy examples).
"""
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_BENCH = os.path.dirname(_HERE)
sys.path.insert(0, _BENCH)
sys.path.insert(0, _HERE)

from app import create_app  # noqa: E402

_config = os.environ.get("BENCH_CONFIG") or os.path.join(_BENCH, "config.yaml")

application = create_app(_config)
app = application  # gunicorn's default callable name
