#!/bin/bash
set -e

export MODE=test

./bin/drax ./tests/print.dx

./bin/drax ./tests/number.dx

./bin/drax ./tests/functions.dx

./bin/drax ./tests/string.dx

./bin/drax ./tests/frames.dx

./bin/drax ./tests/os.dx

./bin/drax ./tests/list.dx


