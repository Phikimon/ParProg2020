#include <iostream>
#include <iomanip>
#include <fstream>
#include <mpi.h>
#include <unistd.h>
#include <cmath>
#include <cassert>

void calc_one_plane(double* arr, uint32_t y_size, uint32_t x_size, int rank, int size)
{
  uint32_t rows_per_process = (int)((y_size-1) / (double)size);
  MPI_Bcast(&rows_per_process, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
  MPI_Bcast(&x_size,           1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
  int cells_per_process = rows_per_process * x_size;

  // I know that root process makes extra copy, but who cares
  double* myinputcells = (double*)calloc(cells_per_process, sizeof(double));
  assert(myinputcells);
  double* myoutputcells = (double*)calloc(cells_per_process, sizeof(double));
  assert(myoutputcells);

#ifdef DEBUG_PRINTS
  if (rank == 0) {
    printf("cells_per_process = %u\n", cells_per_process);
    printf("rows_per_process = %u\n", rows_per_process);
    printf("y_size - 1 = %u\n", y_size - 1);
  }
#endif

  MPI_Scatter(arr + x_size,  cells_per_process, MPI_DOUBLE,
              myinputcells, cells_per_process, MPI_DOUBLE,
              0, MPI_COMM_WORLD);

  // are output cells actually needed?
  for (uint32_t y = 0; y < rows_per_process; y++)
    for (uint32_t x = 0; x < x_size - 1; x++)
      myoutputcells[y*x_size + x] = sin(myinputcells[y*x_size + x + 1]);

  if (rank != 0) {
    for (uint32_t y = 0; y < rows_per_process; y++) {
      MPI_Send(&myoutputcells[y*x_size], x_size - 1, MPI_DOUBLE,
               0, 0, MPI_COMM_WORLD);
    }
  } else {
    for (uint32_t y = 0; y < rows_per_process; y++)
      memcpy(&arr[y*x_size], &myoutputcells[y*x_size], (x_size - 1) * sizeof(double));
    MPI_Status status = {0};
    for (uint32_t y = rows_per_process; y < rows_per_process * size; y++) {
      int process = y / rows_per_process;
#ifdef DEBUG_PRINTS
      printf("process = %d/%d = %d\n", y, rows_per_process, process);
#endif
      MPI_Recv(&arr[y*x_size], x_size - 1, MPI_DOUBLE,
               process, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    }
  }

  free(myoutputcells);
  free(myinputcells);

  // compute remainder
  if (rank == 0)
    for (uint32_t y = rows_per_process * size; y < y_size - 1; y++)
      for (uint32_t x = 0; x < x_size - 1; x++)
        arr[y*x_size + x] = sin(arr[(y + 1)*x_size + x + 1]);
}

void calc(double* arr, uint32_t z_size, uint32_t y_size, uint32_t x_size, int rank, int size)
{
  MPI_Bcast(&z_size, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

  double* temparr = NULL;
  if (rank == 0) {
    temparr = (double*)calloc(y_size*x_size, sizeof(double));
    assert(temparr);
  }
  for (uint32_t z = 1; z < z_size; z++) {
    if (rank == 0)
      memcpy(temparr, &arr[(z-1)*y_size*x_size], y_size * x_size * sizeof(double));

    // runs in parallel line
    calc_one_plane(temparr, y_size, x_size, rank, size);
    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
      for (uint32_t y = 0; y < y_size - 1; y++)
        temparr[y_size*(y+1) - 1] = arr[z*y_size*x_size + y_size*(y+1) - 1];
      memcpy(&arr[z*y_size*x_size], temparr, (y_size - 1) * x_size * sizeof(double));
    }
  }
  if (rank == 0)
    free(temparr);
}

int main(int argc, char** argv)
{
  int rank = 0, size = 0, buf = 0;
  uint32_t zSize = 0, ySize = 0, xSize = 0;
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
    input >> zSize >> ySize >> xSize;
    MPI_Bcast(&buf, 1, MPI_INT, 0, MPI_COMM_WORLD);

    arr = new double[zSize * ySize * xSize];
    for (uint32_t z = 0; z < zSize; z++) {
      for (uint32_t y = 0; y < ySize; y++) {
        for (uint32_t x = 0; x < xSize; x++) {
          input >> arr[z*ySize*xSize + y*xSize + x];
        }
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

  calc(arr, zSize, ySize, xSize, rank, size);

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

    for (uint32_t z = 0; z < zSize; z++) {
      for (uint32_t y = 0; y < ySize; y++) {
        for (uint32_t x = 0; x < xSize; x++) {
          output << " " << arr[z*ySize*xSize + y*xSize + x];
        }
        output << std::endl;
      }
      output << std::endl;
    }
    output.close();
    delete arr;
  }

  MPI_Finalize();
  return 0;
}
