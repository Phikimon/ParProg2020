#include <iostream>
#include <iomanip>
#include <fstream>
#include <mpi.h>
#include <unistd.h>
#include <cmath>

double acceleration(double t)
{
  return sin(t);
}

void calc(double* trace, uint32_t traceSize, double t0, double dt, double y0, double y1, int rank, int size)
{
  // Sighting shot
  double v0 = 0;
  if (rank == 0 && size > 0)
  {
    trace[0] = y0;
    trace[1] = y0 + dt*v0;
    for (uint32_t i = 2; i < traceSize; i++)
    {
      trace[i] = dt*dt*acceleration(t0 + (i - 1)*dt) + 2*trace[i - 1] - trace[i - 2];
    }
  }

  // The final shot
  if (rank == 0 && size > 0)
  {
    v0 = (y1 - trace[traceSize - 1])/(dt*traceSize);
    trace[0] = y0;
    trace[1] = y0 + dt*v0;
    for (uint32_t i = 2; i < traceSize; i++)
    {
      trace[i] = dt*dt*acceleration(t0 + (i - 1)*dt) + 2*trace[i - 1] - trace[i - 2];
    }
  }

}

int main(int argc, char** argv)
{
  int rank = 0, size = 0, status = 0;
  uint32_t traceSize = 0;
  double t0 = 0, t1 = 0, dt = 0, y0 = 0, y1 = 0;
  double* trace = 0;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (rank == 0)
  {
    // Check arguments
    if (argc != 3)
    {
      std::cout << "[Error] Usage <inputfile> <output file>\n";
      status = 1;
      MPI_Bcast(&status, 1, MPI_INT, 0, MPI_COMM_WORLD);
      return 1;
    }

    // Prepare input file
    std::ifstream input(argv[1]);
    if (!input.is_open())
    {
      std::cout << "[Error] Can't open " << argv[1] << " for write\n";
      status = 1;
      MPI_Bcast(&status, 1, MPI_INT, 0, MPI_COMM_WORLD);
      return 1;
    }

    // Read arguments from input
    input >> t0 >> t1 >> dt >> y0 >> y1;
    MPI_Bcast(&status, 1, MPI_INT, 0, MPI_COMM_WORLD);
    traceSize = (t1 - t0)/dt;
    trace = new double[traceSize];

    input.close();
  } else {
    MPI_Bcast(&status, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (status != 0)
    {
      return 1;
    }
  }

  calc(trace, traceSize, t0, dt, y0, y1, rank, size);

  if (rank == 0)
  {
    // Prepare output file
    std::ofstream output(argv[2]);
    if (!output.is_open())
    {
      std::cout << "[Error] Can't open " << argv[2] << " for read\n";
      delete trace;
      return 1;
    }

    for (uint32_t i = 0; i < traceSize; i++)
    {
      output << std::fixed << std::setprecision(13) << trace[i] << std::endl;
    }
    output.close();
    delete trace;
  }

  MPI_Finalize();
  return 0;
}
