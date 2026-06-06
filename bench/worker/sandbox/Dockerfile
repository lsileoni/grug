# Sandbox image for Grug Bench workers.
#
# It does double duty: it builds engines from untrusted refs (needs a C/C++
# toolchain) and runs the matches (needs the runner + the engines' runtime
# libs). Containers are launched with --network none, --read-only, a non-root
# --user, dropped capabilities and CPU/memory/pid caps (see worker/sandbox.py),
# so this image only needs the tools, not any privileges.
#
# Build:  docker build -t grug-bench-sandbox:latest bench/worker/sandbox
#
# Keep the base distro/libc in step with the worker host: engine binaries are
# built and run inside this image, so its libc must be able to run them.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# build toolchain + engine/runner runtime libraries.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        make \
        git \
        ca-certificates \
        libstdc++6 \
        libgomp1 \
    && rm -rf /var/lib/apt/lists/*

# The match runner. fastchess is a single binary and the simplest to bundle.
# Leave FASTCHESS_REF empty to use the repository default branch, or pass a
# branch/tag/commit with: docker build --build-arg FASTCHESS_REF=<ref> ...
ARG FASTCHESS_REF=
RUN git clone --depth 1 https://github.com/Disservin/fastchess /tmp/fastchess \
    && if [ -n "${FASTCHESS_REF}" ]; then \
        git -C /tmp/fastchess fetch --depth 1 origin "${FASTCHESS_REF}" \
        && git -C /tmp/fastchess checkout FETCH_HEAD; \
    fi \
    && make -C /tmp/fastchess -j \
    && install -m 0755 /tmp/fastchess/fastchess /usr/local/bin/fastchess \
    && rm -rf /tmp/fastchess

# No ENTRYPOINT: the worker passes the exact build/match argv to run.
