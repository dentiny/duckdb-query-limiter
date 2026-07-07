PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=query_limiter
EXT_CONFIG=${PROJ_DIR}extension_config.cmake

# Include the Makefile from extension-ci-tools
include extension-ci-tools/makefiles/duckdb_extension.Makefile

format-all: format
	find src \( -iname '*.hpp' -o -iname '*.cpp' \) -print | xargs clang-format --sort-includes=0 -style=file -i
	cmake-format -i CMakeLists.txt
	cmake-format -i extension_config.cmake

.PHONY: format-all
