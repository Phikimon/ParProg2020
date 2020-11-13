#include <iostream>
#include <iomanip>
#include <fstream>
#include <mpi.h>
#include <unistd.h>
#include <cmath>
#include <cassert>

void st_calc(double* arr, uint32_t y_size, uint32_t x_size, int rank, int size)
{
  for (uint32_t y = 4; y < y_size; y++)
    for (uint32_t x = 0; x < x_size; x++)
      arr[y*x_size + x] = sin(arr[(y - 4)*x_size + x]);
}

void calc(double* arr, uint32_t y_size, uint32_t x_size, int rank, int size)
{
  // it is only interesting if there are 1 or 4 threads, so fit other cases to these
  if (size == 0)
    return;
  if (size < 4) {
    if (rank == 0)
      st_calc(arr, y_size, x_size, rank, size);
    return;
  }

  MPI_Comm my_comm;
  int color = (rank > 3) ? MPI_UNDEFINED : 0;
  MPI_Comm_split(MPI_COMM_WORLD, color, rank, &my_comm);
  if (rank > 3)
    return;
  assert(size >= 4);
  size = 4;

  uint32_t r_y_size = y_size; // r for rounded
  if (y_size % 4)
    r_y_size += 4 - y_size % 4;

  MPI_Bcast(&  x_size, 1, MPI_UNSIGNED, 0, my_comm);
  MPI_Bcast(&r_y_size, 1, MPI_UNSIGNED, 0, my_comm);

  double* mempool = NULL;
  double *arrs[4] = {NULL};
  if (rank == 0) {
    mempool = (double*)calloc(x_size * r_y_size, sizeof(double));
    assert(mempool);
    for (int i = 0; i < 4; i++)
      arrs[i] = &mempool[(x_size * r_y_size / 4) * i];

    int j = 0;
    for (uint32_t i = 0; i < y_size; i++) {
      memcpy(&arrs[i % 4][x_size * j], &arr[x_size * i], x_size * sizeof(double));
      j += (i % 4 == 3);
    }
  }

  double* localarr = NULL;
  if (rank == 0)
    localarr = arrs[0];
  else {
  localarr = (double*)calloc(x_size * r_y_size / 4, x_size);
  assert(localarr);
  }

  MPI_Scatter(mempool,  (x_size * r_y_size / 4), MPI_DOUBLE,
              localarr, (x_size * r_y_size / 4), MPI_DOUBLE,
              0, my_comm);

  // Cache friendly?
  for (uint32_t y = 1; y < r_y_size/4; y++)
    for (uint32_t x = 0; x < x_size; x++)
      localarr[y*x_size + x] = sin(localarr[(y - 1)*x_size + x]);

  MPI_Gather(localarr, (x_size * r_y_size / 4), MPI_DOUBLE,
             mempool,  (x_size * r_y_size / 4), MPI_DOUBLE,
             0, my_comm);

  if (rank == 0) {
    int j = 0;
    for (uint32_t i = 0; i < y_size; i++) {
      memcpy(&arr[x_size * i], &arrs[i % 4][x_size * j], x_size * sizeof(double));
      j += (i % 4 == 3);
    }
  }

  if (rank)
    free(localarr);
  if (!rank)
    free(mempool);

  MPI_Comm_free(&my_comm);
}

int main(int argc, char** argv)
{
  int rank = 0, size = 0, buf = 0;
  uint32_t y_size = 0, x_size = 0;
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
    input >> y_size >> x_size;
    MPI_Bcast(&buf, 1, MPI_INT, 0, MPI_COMM_WORLD);

    arr = new double[y_size * x_size];

    for (uint32_t y = 0; y < y_size; y++)
    {
     for (uint32_t x = 0; x < x_size; x++)
      {
        input >> arr[y*x_size + x];
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

  calc(arr, y_size, x_size, rank, size);

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
    for (uint32_t y = 0; y < y_size; y++)
    {
      for (uint32_t x = 0; x < x_size; x++)
      {
        output << " " << arr[y*x_size + x];
      }
      output << std::endl;
    }
    output.close();
    delete arr;
  }

  MPI_Finalize();
  return 0;
}
