#!/usr/bin/env bash
# Fetch the latest version of the library
fetch() {
if [ -f "debug_draw.hpp" ]; then return; fi
URL="https://github.com/glampert/debug-draw/raw/refs/heads/master/debug_draw.hpp"

# Download the release
echo "Downloading debug_draw.hpp from $URL ..."
curl -L "$URL" -o "debug_draw.hpp"
echo ""
}


# Test the project
test() {
echo "Running 01-basic.cxx ..."
clang++ -std=c++17 -I. examples/01-basic.cxx        && ./a && echo -e "\n"
echo "Running 02-parse-string.cxx ..."
clang++ -std=c++17 -I. examples/02-parse-string.cxx && ./a && echo -e "\n"
echo "Running 03-advanced.cxx ..."
clang++ -std=c++17 -I. examples/03-advanced.cxx     && ./a && echo -e "\n"
}


# Main script
if [[ "$1" == "test" ]]; then test
elif [[ "$1" == "fetch" ]]; then fetch
else echo "Usage: $0 {fetch|test}"; fi
