# TDD approach: write tests first

CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O2
INCLUDES = -Isrc
BUILD_DIR = build

# Header-only: headers are in src/
# Source files
SRCS = src/main.cpp src/time_utils.cpp src/csv_reader.cpp \
       src/sleep_detector.cpp src/hr_aggregator.cpp \
       src/feature_extractor.cpp src/session_builder.cpp \
       src/synthetic_generator.cpp

OBJS = $(SRCS:src/%.cpp=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

TARGET = $(BUILD_DIR)/sleep-recovery-cli

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -c $< -o $@

-include $(DEPS)

test: $(TARGET)
	$(TARGET) \
		--input "../Fitabase Data 3.12.16-4.11.16/" \
		--output ../data/normalized/ \
		--features ../data/features/ \
		--labels ../data/labels/ \
		--golden ../data/samples/golden/

clean:
	rm -rf $(BUILD_DIR)
