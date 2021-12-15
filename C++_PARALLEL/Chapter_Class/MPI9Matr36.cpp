#include "pt4.h"
#include "mpi.h"
#include <cmath>

int k;              // number of processes
int r;              // rank of the current process

int m, p, q;        // sizes of the given matrices
int m0, p0, q0;     // sizes of the matrix blocks
int k0;             // order of the Cartesian grid (equal to sqrt(k))

int *a_, *b_, *c_;  // arrays to store matrices in the master process
int *a, *b, *c, *t; // arrays to store matrix blocks in each process

MPI_Datatype MPI_BLOCK_A; // datatype for the block of the matrix A
MPI_Datatype MPI_BLOCK_B; // datatype for the block of the matrix B
MPI_Datatype MPI_BLOCK_C; // datatype for the block of the matrix C

MPI_Comm MPI_COMM_GRID = MPI_COMM_NULL;
             // communicator associated with a two-dimensional Cartesian grid
MPI_Comm MPI_COMM_ROW = MPI_COMM_NULL;
             // communicator associated with rows of a two-dimensional grid
MPI_Comm MPI_COMM_COL = MPI_COMM_NULL;
             // communicator associated with columns of a two-dimensional grid

void Matr4CreateCommGrid(int k0, MPI_Comm* comm)
{
    int dims[2] = { k0,k0 };
    int periods[2] = { true,false };
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, false, comm);
}
void Matr4CreateCommRow(MPI_Comm grid, MPI_Comm* row)
{
    int remain_dims[2] = { false, true };
    MPI_Cart_sub(grid, remain_dims, row);
}
void Matr4Calc1()
{
    int rank;
    Matr4CreateCommGrid(k0, &MPI_COMM_GRID);
    MPI_Comm_rank(MPI_COMM_GRID, &rank);
    int coords[2];
    MPI_Cart_coords(MPI_COMM_GRID, rank, 2, coords);
    Matr4CreateCommRow(MPI_COMM_GRID, &MPI_COMM_ROW);
    MPI_Comm_rank(MPI_COMM_ROW, &rank);
    for (int i = 0; i < m0 * p0; i++)
    {
        t[i] = a[i];
    }
    MPI_Bcast(t, m0 * p0, MPI_INT, coords[0], MPI_COMM_ROW);
    for (int i = 0; i < m0; i++)
    {
        for (int j = 0; j < q0; j++)
        {
            
            int sum = 0;
            for (int k = 0; k < p0; k++)
            {
                sum += t[k + i * p0] * b[j + k * q0];
            }
            c[j + i * q0] = sum;
        }
    }
}
void Solve()
{
    Task("MPI9Matr36");
    int flag;
    MPI_Initialized(&flag);
    if (flag == 0)
        return;
    int rank, size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    k = size;
    r = rank;
    k0 = (int)floor(sqrt((double)k) + 0.1);
    pt >> m0 >> p0 >> q0;
    a = new int[m0 * p0]; 
    b = new int[p0 * q0]; 
    c = new int[m0 * q0]; 
    t = new int[m0 * p0];
    for (int i = 0; i < m0 * p0; i++) {
        pt >> a[i];
    }
    for (int i = 0; i < p0 * q0; i++) {
        pt >> b[i];
    }
    for (int i = 0; i < m0 * q0; i++) {
        pt >> c[i];
    }
    for (int i = 0; i < m0 * p0; i++) {
        pt >> t[i];
    }
    Matr4Calc1();
    for (int i = 0; i < m0 * p0; i++)
    {
        pt << t[i];
    }
    for (int i = 0; i < m0 * q0; i++) {
        pt << c[i];
    }
}