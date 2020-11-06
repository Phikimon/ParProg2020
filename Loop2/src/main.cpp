#include <iostream>
#include <iomanip>
#include <fstream>
#include <mpi.h>
#include <unistd.h>
#include <cassert>
#include <cmath>

void calc(double* arr, uint32_t ySize, uint32_t xSize, int rank, int size)
{
  uint32_t rows_per_process = (int)((ySize-1) / (double)size);
  MPI_Bcast(&rows_per_process, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
  MPI_Bcast(&xSize,            1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
  int cells_per_process = rows_per_process * xSize;

  // I know that root process makes extra copy, but who cares
  double* myinputcells = (double*)calloc(cells_per_process, sizeof(double));
  assert(myinputcells);
  double* myoutputcells = (double*)calloc(cells_per_process, sizeof(double));
  assert(myoutputcells);

#ifdef DEBUG_PRINTS
  if (rank == 0) {
    printf("cells_per_process = %u\n", cells_per_process);
    printf("rows_per_process = %u\n", rows_per_process);
    printf("ySize - 1 = %u\n", ySize - 1);
  }
#endif

  MPI_Scatter(arr + xSize,  cells_per_process, MPI_DOUBLE,
              myinputcells, cells_per_process, MPI_DOUBLE,
              0, MPI_COMM_WORLD);

  for (uint32_t y = 0; y < rows_per_process; y++)
    for (uint32_t x = 3; x < xSize; x++)
      myoutputcells[y*xSize + x] = sin(0.00001*myinputcells[y*xSize + x - 3]);

  if (rank != 0) {
    for (uint32_t y = 0; y < rows_per_process; y++) {
      MPI_Send(&myoutputcells[y*xSize + 3], xSize - 3, MPI_DOUBLE,
               0, 0, MPI_COMM_WORLD);
    }
  } else {
    for (uint32_t y = 0; y < rows_per_process; y++)
      memcpy(&arr[y*xSize + 3], &myoutputcells[y*xSize + 3], (xSize - 3) * sizeof(double));
    MPI_Status status = {0};
    for (uint32_t y = rows_per_process; y < rows_per_process * size; y++) {
      int process = y / rows_per_process;
#ifdef DEBUG_PRINTS
      printf("process = %d/%d = %d\n", y, rows_per_process, process);
#endif
      MPI_Recv(&arr[y*xSize + 3], xSize - 3, MPI_DOUBLE,
               process, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    }
  }

  free(myoutputcells);
  free(myinputcells);

  // compute remainder
  if (rank == 0)
    for (uint32_t y = rows_per_process * size; y < ySize - 1; y++)
      for (uint32_t x = 3; x < xSize; x++)
        arr[y*xSize + x] = sin(0.00001*arr[(y + 1)*xSize + x - 3]);

}

int main(int argc, char** argv)
{
  int rank = 0, size = 0, buf = 0;
  uint32_t ySize = 0, xSize = 0;
  double* arr = 0;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (rank == 0)
  {
    // Check arguments
    if (argc != 3)
    {
      std::cout << "[Error] Usage <inputfile> <output file>\n";
      buf = 1;
      MPI_Bcast(&buf, 1, MPI_INT, 0, MPI_COMM_WORLD);
      return 1;
    }

    // Prepare input file
    std::ifstream input(argv[1]);
    if (!input.is_open())
    {
      std::cout << "[Error] Can't open " << argv[1] << " for write\n";
      buf = 1;
      MPI_Bcast(&buf, 1, MPI_INT, 0, MPI_COMM_WORLD);
      return 1;
    }

    // Read arguments from input
    input >> ySize >> xSize;
    MPI_Bcast(&buf, 1, MPI_INT, 0, MPI_COMM_WORLD);

    arr = new double[ySize * xSize];

    for (uint32_t y = 0; y < ySize; y++)
    {
     for (uint32_t x = 0; x < xSize; x++)
      {
        input >> arr[y*xSize + x];
      }
    }
    input.close();
  } else {
    MPI_Bcast(&buf, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (buf != 0)
    {
      return 1;
    }
  }

  calc(arr, ySize, xSize, rank, size);

  if (rank == 0)
  {
    // Prepare output file
    std::ofstream output(argv[2]);
    if (!output.is_open())
    {
      std::cout << "[Error] Can't open " << argv[2] << " for read\n";
      delete arr;
      return 1;
    }
    for (uint32_t y = 0; y < ySize; y++)
    {
      for (uint32_t x = 0; x < xSize; x++)
      {
        output << " " << arr[y*xSize + x];
      }
      output << std::endl;
    }
    output.close();
    delete arr;
  }

  MPI_Finalize();
  return 0;
}
