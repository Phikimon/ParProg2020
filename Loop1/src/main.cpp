#include <iostream>
#include <iomanip>
#include <fstream>
#include <mpi.h>
#include <cassert>
#include <unistd.h>
#include <cmath>

void calc(double* arr, uint32_t ySize, uint32_t xSize, int rank, int size)
{
  uint32_t cells_per_process = (int)((ySize * xSize) / (double)size);
  MPI_Bcast(&cells_per_process, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

  // I know that root process makes extra copy, but who cares
  double* mycells = (double*)calloc(cells_per_process, sizeof(double));
  assert(mycells);
#ifdef DEBUG_PRINTS
  if (rank == 0)
    printf("cells_per_process = %u\n", cells_per_process);
#endif

  MPI_Scatter(arr,     cells_per_process, MPI_DOUBLE,
              mycells, cells_per_process, MPI_DOUBLE,
              0, MPI_COMM_WORLD);

  for (uint32_t i = 0; i < cells_per_process; i++)
    mycells[i] = sin(0.00001*mycells[i]);

  MPI_Gather(mycells, cells_per_process, MPI_DOUBLE,
             arr,     cells_per_process, MPI_DOUBLE,
             0, MPI_COMM_WORLD);

  free(mycells);

  // Compute remainder
  if (rank == 0)
    for (uint32_t i = cells_per_process*size; i < ySize * xSize; i++)
      arr[i] = sin(0.00001*arr[i]);
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
