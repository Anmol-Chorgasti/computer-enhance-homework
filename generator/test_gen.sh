
set -euo pipefail

echo "Compiling with Clang..."
clang -g -Wall -Wextra -Werror -fsanitize=address,undefined haversine_gen.c -o havgen -lm

echo "Compiling with GCC..."
gcc -g -Wall -Wextra -Werror -fsanitize=address,undefined haversine_gen.c -o havgen -lm

echo "Build successful!"