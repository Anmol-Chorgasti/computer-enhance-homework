
set -euo pipefail

# change whenever necessary

# Force creation of the build directory if it doesn't exist
mkdir -p build

echo "Compiling with Clang..."
clang -g -Wall -Wextra -Werror -fsanitize=address,undefined haversine_gen.c -o build/havgen -lm

echo "Compiling with GCC..."
gcc -g -Wall -Wextra -Werror -fsanitize=address,undefined haversine_gen.c -o build/havgen -lm

echo "Build successful!"


echo "Executing generator..."
build/havgen $RANDOM 10000000


echo "Compiling Base Parser with GCC..."
gcc -g parser_tool_baseprof.c -o build/basep -lm
build/basep build/input.json build/results.bin

echo "Compiling Medium profiled Parser with GCC..."
gcc -g parser_tool_bandwidth.c -o build/ptb -lm
build/ptb build/input.json build/results.bin

