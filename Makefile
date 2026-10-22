CXX      ?= g++
.DEFAULT_GOAL := run
CXXFLAGS := -std=c++20 -Wall -Wextra -Werror -Wpedantic -pthread -Icommon -Itasks

SAN ?= none
ifeq ($(SAN),asan)
    CXXFLAGS += -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
else ifeq ($(SAN),tsan)
    CXXFLAGS += -O2 -g -fno-omit-frame-pointer -fsanitize=thread
else ifeq ($(SAN),bench)
    CXXFLAGS += -O3 -DNDEBUG
else
    CXXFLAGS += -O2 -g
endif

BUILD_DIR := build/make_$(SAN)
TASK_DIRS := $(sort $(wildcard tasks/*))

.PHONY: help run compile-all clean

help:
	@echo "Usage:"
	@echo "  make TASK=01              # Build & run Task 01"
	@echo "  make TASK=01 SAN=asan     # Build & run Task 01 with ASan + UBSan"
	@echo "  make TASK=02 SAN=tsan     # Build & run Task 02 with ThreadSanitizer"
	@echo "  make TASK=12 SAN=bench    # Build & run Task 12 with -O3 optimizations"
	@echo "  make compile-all          # Verify all 14 task targets compile cleanly"
	@echo "  make clean                # Remove build directory"

ifdef TASK
MATCHED_DIR := $(wildcard tasks/$(TASK)_*)
ifeq ($(strip $(MATCHED_DIR)),)
$(error No task matching 'tasks/$(TASK)_*'. Example: make TASK=01)
endif
BIN := $(BUILD_DIR)/$(notdir $(MATCHED_DIR))

run: $(BIN)
	@./$(BIN)

$(BIN): $(MATCHED_DIR)/test.cpp $(wildcard $(MATCHED_DIR)/*.hpp) $(wildcard common/*.hpp)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(MATCHED_DIR) $(MATCHED_DIR)/test.cpp -o $(BIN)
endif

compile-all:
	@mkdir -p $(BUILD_DIR)
	@for dir in $(TASK_DIRS); do \
		name=$$(basename $$dir); \
		echo "[CXX] $$name"; \
		$(CXX) $(CXXFLAGS) -I$$dir $$dir/test.cpp -o $(BUILD_DIR)/$$name || exit 1; \
	done
	@echo "All 14 targets compiled cleanly."

clean:
	rm -rf build
