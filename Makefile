CC      ?= cc
WIN_CC  ?= x86_64-w64-mingw32-gcc
BUILD_DIR ?= build
EXE     ?= $(BUILD_DIR)/grug
WIN_EXE ?= $(BUILD_DIR)/grug.exe
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wshadow -DNDEBUG
WIN_CFLAGS ?= $(CFLAGS)
LDFLAGS ?=
WIN_LDFLAGS ?=

ARCH    ?= -march=native
WIN_ARCH ?=

SRC := $(wildcard src/*.c src/algorithms/*.c)
OBJ := $(patsubst %.c,$(BUILD_DIR)/obj/native/%.o,$(SRC))
WIN_OBJ := $(patsubst %.c,$(BUILD_DIR)/obj/windows/%.o,$(SRC))
FORMAT_FILES := $(wildcard src/*.c src/*.h src/algorithms/*.c src/algorithms/*.h)
CLANG_FORMAT ?= clang-format
DOCKER ?= docker
ENV_IMAGE ?= grug-dev
ENV_CONTAINER ?= grug-dev
ENV_WORKDIR ?= /workspaces/grug
ENV_PORT ?= 8001
ENV_BUILD_ARGS ?=
ENV_RUN_ARGS ?=
GRUG_IN_CONTAINER ?= $(shell if [ -f /.dockerenv ]; then echo 1; else echo 0; fi)
ENV_SETUP_STAMP ?= .env-setup-stamp

ifeq ($(GRUG_IN_CONTAINER),1)
all: native windows

native: $(EXE)

windows: $(WIN_EXE)

$(EXE): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ARCH) -o $@ $(OBJ) $(LDFLAGS)

$(WIN_EXE): $(WIN_OBJ)
	@mkdir -p $(dir $@)
	$(WIN_CC) $(WIN_CFLAGS) $(WIN_ARCH) -o $@ $(WIN_OBJ) $(WIN_LDFLAGS)

$(BUILD_DIR)/obj/native/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ARCH) -c $< -o $@

$(BUILD_DIR)/obj/windows/%.o: %.c
	@mkdir -p $(dir $@)
	$(WIN_CC) $(WIN_CFLAGS) $(WIN_ARCH) -c $< -o $@

test: $(EXE)
	$(EXE) perft 5

bench: $(EXE)
	$(EXE) bench

format:
	$(CLANG_FORMAT) -i $(FORMAT_FILES)

style-check:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

tidy:
	cmake -S . -B $(BUILD_DIR)/tidy -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
	run-clang-tidy -p $(BUILD_DIR)/tidy -quiet '^.*/src/.*\.c$$'

clean:
	rm -rf $(BUILD_DIR)/obj $(BUILD_DIR)/tidy $(EXE) $(WIN_EXE)
	rm -f src/*.o src/algorithms/*.o grug grug.exe
else
HOST_SETUP_TARGETS := all native windows test bench format style-check tidy

$(HOST_SETUP_TARGETS): env-ready
	$(DOCKER) exec -e GRUG_IN_CONTAINER=1 $(ENV_CONTAINER) bash -lc 'cd $(ENV_WORKDIR) && make GRUG_IN_CONTAINER=1 $@'

clean: env-start
	$(DOCKER) exec -e GRUG_IN_CONTAINER=1 $(ENV_CONTAINER) bash -lc 'cd $(ENV_WORKDIR) && make GRUG_IN_CONTAINER=1 clean'
endif

env-build:
	$(DOCKER) build $(ENV_BUILD_ARGS) -t $(ENV_IMAGE) .devcontainer

env-image:
	@if ! $(DOCKER) image inspect $(ENV_IMAGE) >/dev/null 2>&1; then \
		$(DOCKER) build $(ENV_BUILD_ARGS) -t $(ENV_IMAGE) .devcontainer; \
	fi

env-start:
	@if $(DOCKER) ps -a --format '{{.Names}}' | grep -qx '$(ENV_CONTAINER)'; then \
		$(DOCKER) start $(ENV_CONTAINER) >/dev/null; \
	else \
		if ! $(DOCKER) image inspect $(ENV_IMAGE) >/dev/null 2>&1; then \
			$(DOCKER) build $(ENV_BUILD_ARGS) -t $(ENV_IMAGE) .devcontainer; \
		fi; \
		$(DOCKER) run -d --name $(ENV_CONTAINER) \
			--init \
			-p $(ENV_PORT):8000 \
			-v "$(CURDIR):$(ENV_WORKDIR)" \
			-w $(ENV_WORKDIR) \
			-u "$$(id -u):$$(id -g)" \
			-e HOME=/tmp \
			-e VIRTUAL_ENV=$(ENV_WORKDIR)/.venv \
			-e PATH=$(ENV_WORKDIR)/.venv/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
			$(ENV_RUN_ARGS) \
			$(ENV_IMAGE) sleep infinity >/dev/null; \
	fi

env-setup: env-start
	$(DOCKER) exec $(ENV_CONTAINER) bash -lc '.devcontainer/setup.sh'

env-ready: env-start
	@if ! $(DOCKER) exec $(ENV_CONTAINER) test -f $(ENV_SETUP_STAMP); then \
		$(DOCKER) exec $(ENV_CONTAINER) bash -lc '.devcontainer/setup.sh && touch $(ENV_SETUP_STAMP)'; \
	fi

env: env-setup
	@echo
	@echo "Environment container is running: $(ENV_CONTAINER)"
	@echo "Shell: make env-shell"
	@echo "Bench UI: http://127.0.0.1:$(ENV_PORT)"

env-shell: env-ready
	$(DOCKER) exec -it $(ENV_CONTAINER) bash

env-stop:
	$(DOCKER) stop $(ENV_CONTAINER)

env-rm:
	$(DOCKER) rm -f $(ENV_CONTAINER)

env-logs:
	$(DOCKER) logs $(ENV_CONTAINER)

.PHONY: all native windows test bench format style-check tidy clean env-build env-image env-start env-setup env-ready env env-shell env-stop env-rm env-logs
