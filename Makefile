# Host build: simulator and unit tests. A board port adds its own hal_<board>.c
# and links the same src/ files except hal_sim.c and main_sim.c.

CC      ?= gcc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion -Werror
CPPFLAGS = -Iinclude
LDLIBS   = -lm
BUILD    = build

CORE = src/imu.c src/activity_model.c src/vitals.c src/heating.c src/voice.c \
       src/protocol.c src/app.c src/sj_types.c src/hal_sim.c
TESTS = tests/test_main.c tests/test_imu.c tests/test_model.c tests/test_vitals.c \
        tests/test_heating.c tests/test_voice.c tests/test_protocol.c tests/test_app.c

.PHONY: all sim test clean model

all: sim test

$(BUILD):
	mkdir -p $(BUILD)

sim: $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE) src/main_sim.c -o $(BUILD)/jacket_sim $(LDLIBS)

test: $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests $(CORE) $(TESTS) -o $(BUILD)/tests $(LDLIBS)
	./$(BUILD)/tests

model:
	python3 tools/train_activity.py

clean:
	rm -rf $(BUILD)
