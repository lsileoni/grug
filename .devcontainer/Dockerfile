FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        clang-format \
        clang-tidy \
        cmake \
        curl \
        gcc-mingw-w64-x86-64 \
        git \
        libgomp1 \
        libstdc++6 \
        make \
        python3 \
        python3-flask \
        python3-pip \
        python3-pytest \
        python3-requests \
        python3-venv \
        python3-yaml \
        gunicorn \
        stockfish \
    && rm -rf /var/lib/apt/lists/*

# Ubuntu Noble does not package python-chess in the default repositories.
# Install it while building the image so runtime setup can stay offline.
RUN python3 -m pip install --break-system-packages --no-cache-dir "python-chess>=1.999"

# fastchess is a lightweight UCI match runner. The bench worker field is named
# `cutechess`, but worker/cutechess.py auto-detects fastchess by executable name.
# Pinned to a real release tag so the dev container and CI build the identical
# runner (CI clones the same ref in .github/workflows/benchmark.yml). Override
# with: docker build --build-arg FASTCHESS_REF=<ref> ...
ARG FASTCHESS_REF=v1.8.1-alpha
RUN git clone --depth 1 https://github.com/Disservin/fastchess /tmp/fastchess \
    && if [ -n "${FASTCHESS_REF}" ]; then \
        git -C /tmp/fastchess fetch --depth 1 origin "${FASTCHESS_REF}" \
        && git -C /tmp/fastchess checkout FETCH_HEAD; \
    fi \
    && make -C /tmp/fastchess -j"$(nproc)" \
    && install -m 0755 /tmp/fastchess/fastchess /usr/local/bin/fastchess \
    && rm -rf /tmp/fastchess
