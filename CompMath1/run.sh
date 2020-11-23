#! /bin/bash
# run.sh <number of repetitions>
compiler="mpic++"
flags="-std=c++11 -Wall -Werror"
src="./src/main.cpp"
build="./build"
exe="$build/task"
tests_dir="./tests"
delta=0.000001
procnum=4

function compare_curves {
  filein1=$1
  filein2=$2

  if ! command -v bc &> /dev/null; then
    echo "bc is required, run 'sudo apt --yes install bc'," \
         "'sudo yum -y install bc' or something similar"
    exit 1
  fi

  if [ ! -f "$filein1" ]; then
    echo "File '$filein1' does not exist"
    return 1
  fi
  if [ ! -f "$filein2" ]; then
    echo "File '$filein2' does not exist"
    return 1
  fi

  # Count and compare number of lines in input files
  # wc also prints file num, so cut only prints linenum
  linenum1=$(wc -l $filein1 | cut -d " " -f1)
  linenum2=$(wc -l $filein2 | cut -d " " -f1)
  if [ $linenum1 -ne $linenum2 ]; then
    echo "Different number of points($linenum1 vs. $linenum2)"
    echo "$filein1, $filein2"
    return 1
  fi
  if [ $linenum1 -eq 0 ]; then
    echo "Zero points found in both files"
    return 1
  fi

  # Create file that will have two values in each line to compare
  filedata=$(mktemp)
  paste $filein1 $filein2 > $filedata

  # Create file with actual bc code, which
  # itself is pretty much obvious
  filebccode=$(mktemp)
  cat  >$filebccode<<EOF
    define abs(i) {
      if (i < 0) return (-i);
      return (i);
    }

    off = read();
    len = read();
    for (p = off; p < off+len; p++) {
      v1 = read();
      v2 = read();
      if (abs(v1-v2) >= $delta) {
        print p, "\n";
        halt;
      }
    }
    print -1, "\n";
EOF

  # dry run of mktemp, get the name to create fifo
  tmppipe=$(mktemp -u)
  mkfifo $tmppipe
  # Attach pipe to 3rd fd
  exec 3<>$tmppipe

  # This will divide filedata in smaller files with prefix "$filedata."
  split -n l/$procnum $filedata "$filedata."

  TIME_START=$(date +%s)
  # Split created $procnum small files, launch bc for each of them
  chunkoffset=0
  for file in $(ls $filedata\.*); do
    chunksize=$(wc -l $file | cut -d " " -f1)
    echo $chunkoffset $chunksize $(cat $file) | bc $filebccode >&3 &
    chunkoffset=$(($chunkoffset+$chunksize))
  done

  # Collect execution results
  failed=0
  for ((i=0; i < $procnum; i++)); do
    read pointnum <&3
    if [ $pointnum -ne -1 ]; then
      echo "Failed point is $pointnum(numeration starts from 0)"
      failed=1
    fi
  done
  echo "bc took $(($(date +%s)-$TIME_START)) sec on $procnum CPUs"

  # cleanup, remove temporary files
  rm -f $filebccode $tmppipe $filedata $filedata.*
  # close pipe
  exec 3>&-

  return $failed
}

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
  for proc in {1..4}; do
    test="$(basename $test_dir)_p$proc"
    printf "\n[TEST $test]\n"
    echo "mpiexec -np $proc $exe $test_dir/input.txt $build/$test.txt"
    START=$(date +%s%N)
    mpiexec -np $proc $exe $test_dir/input.txt $build/$test.txt
    END=$(date +%s%N)
    DIFF=$((($END - $START)/1000000))
    if [ ! $? -eq 0 ]; then
      echo "[TEST $test] RUNTIME FAIL"
      continue;
    fi
    compare_curves $build/$test.txt $test_dir/output.txt
    if (($? == 0)); then
      echo "[TEST $test] OK ($DIFF ms)"
      SUCCESS_TESTS+=($test)
    else
      echo "[TEST $test] DIFF FAIL($DIFF ms): vimdiff $build/$test.txt $test_dir/output.txt"
      FAIL_TESTS+=($test)
    fi
  done
done
echo "==========="
echo "SUCCESSFUL: ${SUCCESS_TESTS[@]}"
echo "FAIL: ${FAIL_TESTS[@]}"
