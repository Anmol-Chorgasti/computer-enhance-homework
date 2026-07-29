set -euo pipefail

# change whenever necessary

# Force creation of the build directory if it doesn't exist
mkdir -p build

echo "Compiling with Clang..."
clang -g -Wall -Wextra -Werror -fsanitize=address,undefined haversine_parser.c -o build/havpar -lm

echo "Compiling with GCC..."
gcc -g -Wall -Wextra -Werror -fsanitize=address,undefined haversine_parser.c -o build/havpar -lm

echo "Build successful!"

echo "Executing Parser..."
build/havpar build/input.json build/results.bin