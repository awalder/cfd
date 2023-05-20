
## Density solver step
```c
void density_step(
        int grid_size,
        float* density,
        float* previous_density,
        float* velocity_u,
        float* velocity_v,
        float diffusion_rate,
        float time_step)
{
    add_source(grid_size, density, previous_density, time_step);
    SWAP(previous_density, density);
    diffuse(grid_size, 0, density, previous_density, diffusion_rate, time_step);
    SWAP(previous_density, density);
    advect(grid_size, 0, density, previous_density, velocity_u, velocity_v, time_step);
}
```

### add_source()
This function adds the source term to a field (like density or velocity) over the entire grid. The source term represents any external addition to the field, such as new fluid being added to the system. The field is updated in place.
```c
void add_source(int grid_size, float* field, float* source, float time_step)
{
    int i, size = (grid_size + 2) * (grid_size + 2);
    for(i = 0; i < size; i++)
    {
        field[i] += time_step * source[i];
    }
}

```

### diffuse()
This function applies the diffusion step to a field (like density or velocity) over the entire grid. The diffusion step spreads out the field over time to simulate the effect of viscosity. The diffusion_rate parameter controls how quickly the field spreads out. The field is updated in place.
```c
void diffuse(
        int grid_size,
        int boundary,
        float* field,
        float* previous_field,
        float diffusion_rate,
        float time_step)
{
    int i, j, iteration;
    float a = time_step * diffusion_rate * grid_size * grid_size;
    for(iteration = 0; iteration < 20; iteration++)
    {
        for(i = 1; i <= grid_size; i++)
        {
            for(j = 1; j <= grid_size; j++)
            {
                field[IX(i, j)] = (previous_field[IX(i, j)]
                                   + a
                                             * (field[IX(i - 1, j)] + field[IX(i + 1, j)]
                                                + field[IX(i, j - 1)] + field[IX(i, j + 1)]))
                                  / (1 + 4 * a);
            }
        }
        set_boundary(grid_size, boundary, field);
    }
}
```

### advect()
This function applies the advection step to a field (like density or velocity) over the entire grid. The advection step moves the field around based on the velocity field, simulating the flow of the fluid. The field is updated in place. The velocity_u and velocity_v parameters are the horizontal and vertical components of the velocity field, respectively. The previous_field parameter is the state of the field at the previous time step, and the field parameter is the state of the field at the current time step. The boundary parameter is used to handle the boundary conditions of the field.
```c
void advect(
        int grid_size,
        int boundary,
        float* field,
        float* previous_field,
        float* velocity_u,
        float* velocity_v,
        float time_step)
{
    int i, j, i0, j0, i1, j1;
    float x, y, s0, t0, s1, t1, dt0;
    dt0 = time_step * grid_size;
    for(i = 1; i <= grid_size; i++)
    {
        for(j = 1; j <= grid_size; j++)
        {
            x = i - dt0 * velocity_u[IX(i, j)];
            y = j - dt0 * velocity_v[IX(i, j)];
            if(x < 0.5)
                x = 0.5;
            if(x > grid_size + 0.5)
                x = grid_size + 0.5;
            i0 = (int)x;
            i1 = i0 + 1;
            if(y < 0.5)
                y = 0.5;
            if(y > grid_size + 0.5)
                y = grid_size + 0.5;
            j0 = (int)y;
            j1 = j0 + 1;
            s1 = x - i0;
            s0 = 1 - s1;
            t1 = y - j0;
            t0 = 1 - t1;
            field[IX(i, j)] =
                    s0 * (t0 * previous_field[IX(i0, j0)] + t1 * previous_field[IX(i0, j1)])
                    + s1 * (t0 * previous_field[IX(i1, j0)] + t1 * previous_field[IX(i1, j1)]);
        }
    }
    set_boundary(grid_size, boundary, field);
}
```

```c
void advect(int N, int b, float* d, float* d0, float* u, float* v, float dt)
{
    int i, j, i0, j0, i1, j1;
    float x, y, s0, t0, s1, t1, dt0;
    dt0 = dt * N;
    for(i = 1; i <= N; i++)
    {
        for(j = 1; j <= N; j++)
        {
            x = i - dt0 * u[IX(i, j)];
            y = j - dt0 * v[IX(i, j)];

            if(x < 0.5)
                x = 0.5;
            if(x > N + 0.5)
                x = N + 0.5;

            i0 = (int)x;
            i1 = i0 + 1;

            if(y < 0.5)
                y = 0.5;
            if(y > N + 0.5)
                y = N + 0.5;

            j0 = (int)y;
            j1 = j0 + 1;

            s1 = x - i0;
            s0 = 1 - s1;

            t1 = y - j0;
            t0 = 1 - t1;

            d[IX(i, j)] = s0 * (t0 * d0[IX(i0, j0)] + t1 * d0 [IX(i0, j1)])
                          + s1 * (t0 * d0[IX(i1, j0)] + t1 * d0 [IX(i1, j1)]);
        }
    }
    set_bnd(N, b, d);
}
```


##Velocity solver step

## #Velocity step
This function updates the velocity field over the entire grid for a single time step. The steps are as follows:

    - Add the source term to the velocity field. This represents any external forces acting on the fluid, such as wind or a fan.

    - Apply the diffusion step to the velocity field. This spreads out the velocity field over time, simulating the effect of viscosity.

    - Apply the projection step to the velocity field. This makes the velocity field mass-conserving, meaning that the amount of fluid entering a cell is equal to the amount of fluid leaving the cell.

    - Apply the advection step to the velocity field. This moves the velocity field around based on its own values, simulating the flow of the fluid.

    - Apply the projection step again to the velocity field to ensure it remains mass-conserving after the advection step.

The velocity_u and velocity_v parameters are the horizontal and vertical components of the velocity field, respectively. The previous_velocity_u and previous_velocity_v parameters are the state of the velocity field at the previous time step. The viscosity parameter controls how quickly the velocity field spreads out during the diffusion step. The time_step parameter is the amount of time that has passed since the previous time step.
```c
void velocity_step(
        int grid_size,
        float* velocity_u,
        float* velocity_v,
        float* previous_velocity_u,
        float* previous_velocity_v,
        float viscosity,
        float time_step)
{
    add_source(grid_size, velocity_u, previous_velocity_u, time_step);
    add_source(grid_size, velocity_v, previous_velocity_v, time_step);

    SWAP(previous_velocity_u, velocity_u);
    diffuse(grid_size, 1, velocity_u, previous_velocity_u, viscosity, time_step);

    SWAP(previous_velocity_v, velocity_v);
    diffuse(grid_size, 2, velocity_v, previous_velocity_v, viscosity, time_step);

    project(grid_size, velocity_u, velocity_v, previous_velocity_u, previous_velocity_v);

    SWAP(previous_velocity_u, velocity_u);
    SWAP(previous_velocity_v, velocity_v);

    advect(grid_size,
           1,
           velocity_u,
           previous_velocity_u,
           previous_velocity_u,
           previous_velocity_v,
           time_step);
    advect(grid_size,
           2,
           velocity_v,
           previous_velocity_v,
           previous_velocity_u,
           previous_velocity_v,
           time_step);

    project(grid_size, velocity_u, velocity_v, previous_velocity_u, previous_velocity_v);
}

```

### Project
This function applies the projection step to the velocity field. The projection step makes the velocity field mass-conserving, meaning that the amount of fluid entering a cell is equal to the amount of fluid leaving the cell. This is achieved by first calculating the divergence of the velocity field, which measures how much fluid is entering or leaving each cell. Then, a pressure field is calculated that counteracts the divergence of the velocity field. Finally, the pressure field is subtracted from the velocity field to make it mass-conserving. The velocity_u and velocity_v parameters are the horizontal and vertical components of the velocity field, respectively. The pressure parameter is the pressure field that is calculated during the projection step. The divergence parameter is the divergence of the velocity field. The grid_size parameter is the size of the grid.
```c
void project(int grid_size,
        float* velocity_u,
        float* velocity_v,
        float* pressure,
        float* divergence)
{
    int i, j, iteration;
    float h = 1.0 / grid_size;
    for(i = 1; i <= grid_size; i++)
    {
        for(j = 1; j <= grid_size; j++)
        {
            divergence[IX(i, j)] = -0.5 * h
                                   * (velocity_u[IX(i + 1, j)] - velocity_u[IX(i - 1, j)]
                                      + velocity_v[IX(i, j + 1)] - velocity_v[IX(i, j - 1)]);
            pressure[IX(i, j)] = 0;
        }
    }
    set_boundary(grid_size, 0, divergence);
    set_boundary(grid_size, 0, pressure);
    for(iteration = 0; iteration < 20; iteration++)
    {
        for(i = 1; i <= grid_size; i++)
        {
            for(j = 1; j <= grid_size; j++)
            {
                pressure[IX(i, j)] =
                        (divergence[IX(i, j)] + pressure[IX(i - 1, j)] + pressure[IX(i + 1, j)]
                         + pressure[IX(i, j - 1)] + pressure[IX(i, j + 1)])
                        / 4;
            }
        }
        set_boundary(grid_size, 0, pressure);
    }
    for(i = 1; i <= grid_size; i++)
    {
        for(j = 1; j <= grid_size; j++)
        {
            velocity_u[IX(i, j)] -= 0.5 * (pressure[IX(i + 1, j)] - pressure[IX(i - 1, j)]) / h;
            velocity_v[IX(i, j)] -= 0.5 * (pressure[IX(i, j + 1)] - pressure[IX(i, j - 1)]) / h;
        }
    }
    set_boundary(grid_size, 1, velocity_u);
    set_boundary(grid_size, 2, velocity_v);
}
```

### set_boundary
This function sets the boundary conditions for a field (like density or velocity) over the entire grid. The boundary conditions determine how the field behaves at the edges of the grid. The field parameter is the field to set the boundary conditions for. The boundary parameter determines the type of boundary conditions to set. If boundary is 1, then the field is reflected at the boundaries (like a mirror). If boundary is 2, then the field is inverted at the boundaries (like a photograph negative). The grid_size parameter is the size of the grid.
```c
void set_boundary(int grid_size, int boundary, float* field)
{
    int i;
    for(i = 1; i <= grid_size; i++)
    {
        field[IX(0, i)] = boundary == 1 ? -field[IX(1, i)] : field[IX(1, i)];
        field[IX(grid_size + 1, i)] =
                boundary == 1 ? -field[IX(grid_size, i)] : field[IX(grid_size, i)];
        field[IX(i, 0)] = boundary == 2 ? -field[IX(i, 1)] : field[IX(i, 1)];
        field[IX(i, grid_size + 1)] =
                boundary == 2 ? -field[IX(i, grid_size)] : field[IX(i, grid_size)];
    }
    field[IX(0, 0)] = 0.5 * (field[IX(1, 0)] + field[IX(0, 1)]);
    field[IX(0, grid_size + 1)] = 0.5 * (field[IX(1, grid_size + 1)] + field[IX(0, grid_size)]);
    field[IX(grid_size + 1, 0)] = 0.5 * (field[IX(grid_size, 0)] + field[IX(grid_size + 1, 1)]);
    field[IX(grid_size + 1, grid_size + 1)] =
            0.5 * (field[IX(grid_size, grid_size + 1)] + field[IX(grid_size + 1, grid_size)]);
}
```


