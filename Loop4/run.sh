#! /bin/bash
# run.sh <number of repetitions>
compiler="mpic++"
flags="-std=c++11 -Wall -Werror"
src="./src/main.cpp"
build="./build"
exe="$build/task"
tests_dir="./tests"

echo "[CLEAR]"
echo "  rm $build -r"
rm $build -rf

echo "[BUILD]"
echo "  mkdir $build"
mkdir $build
echo "  $compiler $src -o $exe $flags"
$compiler $src -o $exe $flags

if [ ! $? -eq 0 ]; then
  echo "[ERROR] Can't compile $src"
  exit 1
fi

SUCCESS_TESTS=()
FAIL_TESTS=()

for test_dir in $tests_dir/*; do
  for proc in {1..8}; do
    test="$(basename $test_dir)_p$proc"
    printf "\n[TEST $test]\n"
    echo "mpiexec -np $proc $exe $test_dir/input.txt $build/$test.txt"
    START=$(date +%s%N)
    mpiexec --oversubscribe --allow-run-as-root -np $proc $exe $test_dir/input.txt $build/$test.txt
    END=$(date +%s%N)
    DIFF=$((($END - $START)/1000000))
    if [ ! $? -eq 0 ]; then
      echo "[TEST $test] RUNTIME FAIL"
      continue;
    fi
    if cmp -s $build/$test.txt $test_dir/output.txt; then
      echo "[TEST $test] OK ($DIFF ms)"
      SUCCESS_TESTS+=($test)
    else
      echo "[TEST $test] DIFF FAIL($DIFF ms): vimdiff $build/$test.txt $test_dir/output.txt"
      FAIL_TESTS+=($test)
      exit 1
    fi
  done
done
echo "==========="
echo "SUCCESSFUL: ${SUCCESS_TESTS[@]}"
echo "FAIL: ${FAIL_TESTS[@]}"
