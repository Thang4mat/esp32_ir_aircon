#!/bin/sh
# Chạy từ thư mục gốc của dự án: ./test/run.sh
set -e
cd "$(dirname "$0")/.."
g++ -std=c++17 -Wall -Wextra -I test/stub -I controller/src test/test_controller.cpp -o test/.test_bin
./test/.test_bin
