typedef double* V3;

//3D math inline functions
static inline void v3_add(V3 a, V3 b, V3 c)
{
    c[0] = a[0] + b[0];
    c[1] = a[1] + b[1];
    c[2] = a[2] + b[2];
}

static inline void v3_subtract(V3 a, V3 b, V3 c)
{
    c[0] = a[0] - b[0];
    c[1] = a[1] - b[1];
    c[2] = a[2] - b[2];
}

static inline void v3_scale(V3 a, double s, V3 c)
{
    c[0] = s * a[0];
    c[1] = s * a[1];
    c[2] = s * a[2];
}

static inline double v3_dot(V3 a, V3 b)
{
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static inline void v3_cross(V3 a, V3 b, V3 c)
{
    c[0] = a[1]*b[2] - a[2]*b[1];
    c[1] = a[2]*b[0] - a[0]*b[2];
    c[2] = a[0]*b[1] - a[1]*b[0];
}

static inline void v3_reflect(V3 n, V3 b, V3 r)
{
    double temp[3] = {0, 0, 0};
    double scalar;
    v3_scale(n, 2.0, temp);
    scalar = v3_dot(temp, b);
    v3_scale(n, scalar, temp);
    v3_subtract(temp, b, r);
}

//This function returns the input value squared
static inline double sqr(double v)
{
    return v*v;
}

//this function normalizes the input vector
static inline void normalize(double* v)
{
    double len = sqrt(sqr(v[0]) + sqr(v[1]) + sqr(v[2]));
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
}

