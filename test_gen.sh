
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
build/havgen $RANDOM 1000