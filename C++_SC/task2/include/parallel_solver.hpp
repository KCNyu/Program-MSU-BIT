#pragma once

#include <mpi.h>

#include "solver.hpp"
#include "cube.hpp"
#include "printer.hpp"

struct Process
{
    int rank;
    int size;

    Process()
    {
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);
    }
};

class ParallelSolver : public Solver
{
private:
    Cube *u;

    Process p;

    std::vector<Scale> rank_scales;
    std::vector<Cube> send_cubes;
    std::vector<Cube> recv_cubes;

    std::vector<std::pair<int, Scale>> wait_send;
    std::vector<std::pair<int, Scale>> wait_recv;

    void split(Scale origin)
    {
        int px, py, pz;
        std::tie(pz, py, px) = Scale::split_triplet(p.size);

        int dx = std::max(1, origin.x_len / px);
        int dy = std::max(1, origin.y_len / py);
        int dz = std::max(1, origin.z_len / pz);

        for (int i = 0, x_l = origin.x_l; i < px; ++i)
        {
            int x_r = (i == px - 1) ? origin.x_r : x_l + dx - 1;

            for (int j = 0, y_l = origin.y_l; j < py; ++j)
            {
                int y_r = (j == py - 1) ? origin.y_r : y_l + dy - 1;

                for (int k = 0, z_l = origin.z_l; k < pz; ++k)
                {
                    int z_r = (k == pz - 1) ? origin.z_r : z_l + dz - 1;

                    rank_scales.emplace_back(Scale(x_l, x_r, y_l, y_r, z_l, z_r));

                    z_l = z_r + 1;
                }

                y_l = y_r + 1;
            }

            x_l = x_r + 1;
        }
    }
    bool is_inside(const Scale &local, const Scale &remote, const Scale::Axis axis) const
    {
        switch (axis)
        {
        case Scale::Axis::X:
            return local.y_l >= remote.y_l && remote.y_r >= local.y_r && local.z_l >= remote.z_l && remote.z_r >= local.z_r;
        case Scale::Axis::Y:
            return local.x_l >= remote.x_l && remote.x_r >= local.x_r && local.z_l >= remote.z_l && remote.z_r >= local.z_r;
        case Scale::Axis::Z:
            return local.x_l >= remote.x_l && remote.x_r >= local.x_r && local.y_l >= remote.y_l && remote.y_r >= local.y_r;
        }
    }
    void handle_scales(int i, const Scale &local, const Scale &remote, int send, int recv, Scale::Axis axis)
    {
        int x_l, x_r, y_l, y_r, z_l, z_r;

        if (is_inside(local, remote, axis))
        {
            std::tie(x_l, x_r, y_l, y_r, z_l, z_r) = local.unpack();
        }
        else if (is_inside(remote, local, axis))
        {
            std::tie(x_l, x_r, y_l, y_r, z_l, z_r) = remote.unpack();
        }
        else
        {
            return;
        }

        Scale send_scale, recv_scale;
        switch (axis)
        {
        case Scale::Axis::X:
            send_scale = Scale(send, send, y_l, y_r, z_l, z_r);
            recv_scale = Scale(recv, recv, y_l, y_r, z_l, z_r);
            break;
        case Scale::Axis::Y:
            send_scale = Scale(x_l, x_r, send, send, z_l, z_r);
            recv_scale = Scale(x_l, x_r, recv, recv, z_l, z_r);
            break;
        case Scale::Axis::Z:
            send_scale = Scale(x_l, x_r, y_l, y_r, send, send);
            recv_scale = Scale(x_l, x_r, y_l, y_r, recv, recv);
            break;
        }

        wait_send.emplace_back(i, send_scale);
        wait_recv.emplace_back(i, recv_scale);
    }
    void init_send_recv()
    {
        Scale local_scale = rank_scales[p.rank];

        for (int i = 0; i < p.size; i++)
        {
            if (i != p.rank)
            {
                Scale remote_scale = rank_scales[i];
                int send = -1, recv = -1;
                Scale::Axis axis = Scale::Axis::X;

                if (local_scale.x_l == remote_scale.x_r + 1 || remote_scale.x_l == local_scale.x_r + 1)
                {
                    send = local_scale.x_l == remote_scale.x_r + 1 ? local_scale.x_l : local_scale.x_r;
                    recv = remote_scale.x_l == local_scale.x_r + 1 ? remote_scale.x_l : remote_scale.x_r;
                    axis = Scale::Axis::X;

                    handle_scales(i, local_scale, remote_scale, send, recv, axis);
                }
                if (local_scale.y_l == remote_scale.y_r + 1 || remote_scale.y_l == local_scale.y_r + 1)
                {
                    send = local_scale.y_l == remote_scale.y_r + 1 ? local_scale.y_l : local_scale.y_r;
                    recv = remote_scale.y_l == local_scale.y_r + 1 ? remote_scale.y_l : remote_scale.y_r;
                    axis = Scale::Axis::Y;

                    handle_scales(i, local_scale, remote_scale, send, recv, axis);
                }
                if (local_scale.z_l == remote_scale.z_r + 1 || remote_scale.z_l == local_scale.z_r + 1)
                {
                    send = local_scale.z_l == remote_scale.z_r + 1 ? local_scale.z_l : local_scale.z_r;
                    recv = remote_scale.z_l == local_scale.z_r + 1 ? remote_scale.z_l : remote_scale.z_r;
                    axis = Scale::Axis::Z;

                    handle_scales(i, local_scale, remote_scale, send, recv, axis);
                }
                // periodic boundary conditions for Axis::Z
                if (local_scale.z_l == 0 && remote_scale.z_r == task.N)
                {
                    send = 0 + 1;
                    recv = task.N - 1;
                    axis = Scale::Axis::Z;

                    handle_scales(i, local_scale, remote_scale, send, recv, axis);
                }
                if (local_scale.z_r == task.N && remote_scale.z_l == 0)
                {
                    send = task.N - 1;
                    recv = 0 + 1;
                    axis = Scale::Axis::Z;

                    handle_scales(i, local_scale, remote_scale, send, recv, axis);
                }
            }
        }
    }
    void get_send_cube(Cube &send_cube, Cube &c, Scale remote_scale)
    {
        for (int i = remote_scale.x_l; i <= remote_scale.x_r; i++)
        {
            for (int j = remote_scale.y_l; j <= remote_scale.y_r; j++)
            {
                for (int k = remote_scale.z_l; k <= remote_scale.z_r; k++)
                {
                    send_cube(i, j, k) = c(i, j, k);
                }
            }
        }
    }
    void send_recv_cube(int n)
    {
        int sz = wait_send.size();

        std::vector<MPI_Request> requests(2 * sz);
        std::vector<MPI_Status> statuses(2 * sz);

        for (int i = 0; i < sz; i++)
        {
            get_send_cube(send_cubes[i], u[loop(n)], wait_send[i].second);
            MPI_Isend(send_cubes[i].data, wait_send[i].second.size, MPI_DOUBLE, wait_send[i].first, 0, MPI_COMM_WORLD, &requests[i]);
            MPI_Irecv(recv_cubes[i].data, wait_recv[i].second.size, MPI_DOUBLE, wait_recv[i].first, 0, MPI_COMM_WORLD, &requests[i + sz]);
        }

        MPI_Waitall(2 * sz, requests.data(), statuses.data());
    }

    void init_boundary(Cube &c, double t)
    {
        if (c.scale.z_l == 0)
        {
#pragma omp parallel for
            for (int i = c.scale.x_l; i <= c.scale.x_r; i++)
            {
                for (int j = c.scale.y_l; j <= c.scale.y_r; j++)
                {
                    c(i, j, 0) = task.f(i * task.g.hx, j * task.g.hy, 0, t);
                }
            }
        }
        if (c.scale.z_r == task.N)
        {
#pragma omp parallel for
            for (int i = c.scale.x_l; i <= c.scale.x_r; i++)
            {
                for (int j = c.scale.y_l; j <= c.scale.y_r; j++)
                {
                    c(i, j, task.N) = task.f(i * task.g.hx, j * task.g.hy, task.d.Lz, t);
                }
            }
        }
    }
    int loop(int n) const
    {
        return n % 3;
    }
    double actual_u(Cube &c, int i, int j, int k) const
    {
        if (c.scale.contains(i, j, k))
        {
            return c(i, j, k);
        }

        for (int r_i = 0; r_i < recv_cubes.size(); r_i++)
        {
            if (recv_cubes[r_i].scale.contains(i, j, k))
            {
                return recv_cubes[r_i](i, j, k);
            }
        }

        throw std::runtime_error("actual_u: no cube found");
    }
    inline double laplacian(Cube &c, int i, int j, int k) const
    {
        double dx = (actual_u(c, i - 1, j, k) - 2 * c(i, j, k) + actual_u(c, i + 1, j, k)) / (task.g.hx * task.g.hx);
        double dy = (actual_u(c, i, j - 1, k) - 2 * c(i, j, k) + actual_u(c, i, j + 1, k)) / (task.g.hy * task.g.hy);
        double dz = (actual_u(c, i, j, k - 1) - 2 * c(i, j, k) + actual_u(c, i, j, k + 1)) / (task.g.hz * task.g.hz);
        return dx + dy + dz;
    }
    void reserve()
    {
        split(Scale(task.N));

        for (int i = 0; i < 3; i++)
        {
            u[i] = Cube(rank_scales[p.rank]);
        }

        init_send_recv();

        for (auto s : wait_send)
        {
            send_cubes.emplace_back(s.second);
        }
        for (auto r : wait_recv)
        {
            recv_cubes.emplace_back(r.second);
        }
    }
    void init()
    {
        reserve();

        init_boundary(u[0], 0);
        init_boundary(u[1], task.g.tau);

        int x_l = std::max(1, u[0].scale.x_l);
        int x_r = std::min(task.N - 1, u[0].scale.x_r);

        int y_l = std::max(1, u[0].scale.y_l);
        int y_r = std::min(task.N - 1, u[0].scale.y_r);

        int z_l = std::max(1, u[0].scale.z_l);
        int z_r = std::min(task.N - 1, u[0].scale.z_r);

#pragma omp parallel for
        for (int i = x_l; i <= x_r; i++)
        {
            for (int j = y_l; j <= y_r; j++)
            {
                for (int k = z_l; k <= z_r; k++)
                {
                    u[0](i, j, k) = task.f(i * task.g.hx, j * task.g.hy, k * task.g.hz, 0);
                }
            }
        }

        send_recv_cube(0);

#pragma omp parallel for
        for (int i = x_l; i <= x_r; i++)
        {
            for (int j = y_l; j <= y_r; j++)
            {
                for (int k = z_l; k <= z_r; k++)
                {
                    u[1](i, j, k) = u[0](i, j, k) + task.f.a_2 * task.g.tau * task.g.tau / 2.0 * laplacian(u[0], i, j, k);
                }
            }
        }
    }
    void iterate_boundary(int n)
    {
        int x_l = std::max(1, u[loop(n)].scale.x_l);
        int x_r = std::min(task.N - 1, u[loop(n)].scale.x_r);

        int y_l = std::max(1, u[loop(n)].scale.y_l);
        int y_r = std::min(task.N - 1, u[loop(n)].scale.y_r);

        int z_l = std::max(1, u[loop(n)].scale.z_l);
        int z_r = std::min(task.N - 1, u[loop(n)].scale.z_r);

        if (u[loop(n)].scale.z_l == 0)
        {
#pragma omp parallel for
            for (int i = x_l; i <= x_r; i++)
            {
                for (int j = y_l; j <= y_r; j++)
                {
                    double l = (actual_u(u[loop(n - 1)], i + 1, j, 0) - 2 * actual_u(u[loop(n - 1)], i, j, 0) + actual_u(u[loop(n - 1)], i - 1, j, 0)) / (task.g.hx * task.g.hx) +
                               (actual_u(u[loop(n - 1)], i, j + 1, 0) - 2 * actual_u(u[loop(n - 1)], i, j, 0) + actual_u(u[loop(n - 1)], i, j - 1, 0)) / (task.g.hy * task.g.hy) +
                               (actual_u(u[loop(n - 1)], i, j, 0 + 1) - 2 * actual_u(u[loop(n - 1)], i, j, 0) + actual_u(u[loop(n - 1)], i, j, 0 + task.N - 1)) / (task.g.hz * task.g.hz);
                    u[loop(n)](i, j, 0) = 2 * u[loop(n - 1)](i, j, 0) -
                                          u[loop(n - 2)](i, j, 0) +
                                          task.f.a_2 * task.g.tau * task.g.tau * l;
                }
            }
        }
        if (u[loop(n)].scale.z_r == task.N)
        {
#pragma omp parallel for
            for (int i = x_l; i <= x_r; i++)
            {
                for (int j = y_l; j <= y_r; j++)
                {
                    double l = (actual_u(u[loop(n - 1)], i + 1, j, task.N) - 2 * actual_u(u[loop(n - 1)], i, j, task.N) + actual_u(u[loop(n - 1)], i - 1, j, task.N)) / (task.g.hx * task.g.hx) +
                               (actual_u(u[loop(n - 1)], i, j + 1, task.N) - 2 * actual_u(u[loop(n - 1)], i, j, task.N) + actual_u(u[loop(n - 1)], i, j - 1, task.N)) / (task.g.hy * task.g.hy) +
                               (actual_u(u[loop(n - 1)], i, j, task.N - task.N + 1) - 2 * actual_u(u[loop(n - 1)], i, j, task.N) + actual_u(u[loop(n - 1)], i, j, task.N - 1)) / (task.g.hz * task.g.hz);
                    (u[loop(n)])(i, j, task.N) = 2 * u[loop(n - 1)](i, j, task.N) -
                                                 u[loop(n - 2)](i, j, task.N) +
                                                 task.f.a_2 * task.g.tau * task.g.tau * l;
                }
            }
        }
    }
    void iterate(int n)
    {
        int x_l = std::max(1, u[loop(n)].scale.x_l);
        int x_r = std::min(task.N - 1, u[loop(n)].scale.x_r);

        int y_l = std::max(1, u[loop(n)].scale.y_l);
        int y_r = std::min(task.N - 1, u[loop(n)].scale.y_r);

        int z_l = std::max(1, u[loop(n)].scale.z_l);
        int z_r = std::min(task.N - 1, u[loop(n)].scale.z_r);

        send_recv_cube(n);

#pragma omp parallel for
        for (int i = x_l; i <= x_r; i++)
        {
            for (int j = y_l; j <= y_r; j++)
            {
                for (int k = z_l; k <= z_r; k++)
                {
                    (u[loop(n + 1)])(i, j, k) = 2 * u[loop(n)](i, j, k) -
                                                u[loop(n - 1)](i, j, k) +
                                                task.f.a_2 * task.g.tau * task.g.tau * laplacian(u[loop(n)], i, j, k);
                }
            }
        }

        iterate_boundary(n + 1);
    }
    double getError(int n)
    {
        double e = 0;

#pragma omp parallel for reduction(max : e)
        for (int i = u[loop(n)].scale.x_l; i <= u[loop(n)].scale.x_r; i++)
        {
#pragma omp parallel for reduction(max : e)
            for (int j = u[loop(n)].scale.y_l; j <= u[loop(n)].scale.y_r; j++)
            {
#pragma omp parallel for reduction(max : e)
                for (int k = u[loop(n)].scale.z_l; k <= u[loop(n)].scale.z_r; k++)
                {
                    double numerical = u[loop(n)](i, j, k);
                    double analytical = task.f(i * task.g.hx, j * task.g.hy, k * task.g.hz, task.g.tau * n);
                    double local_error = abs(numerical - analytical);

                    e = std::max(e, local_error);
                }
            }
        }

        double global_error = 0;
        MPI_Reduce(&e, &global_error, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

        return global_error;
    }

public:
    ParallelSolver(Task task) : Solver(task)
    {
        u = new Cube[3];
    }

    void run() override
    {
        init();

        double error = 0;

        for (int n = 1; n < task.steps; n++)
        {
            iterate(n);
            error = max(error, getError(n + 1));
        }

        if (p.rank == 0)
        {
            Printer::print("error", error);
        }
    }

    ~ParallelSolver() override
    {
        delete[] u;
    }
};