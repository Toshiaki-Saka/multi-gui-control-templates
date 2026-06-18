# C ABI Reference

`track_core` exposes a pure-C interface so the library can be consumed from any
language that supports FFI (C#, Python, Rust, …).  All symbols are decorated
with `__declspec(dllexport)` on Windows and `__attribute__((visibility("default")))` on Linux/macOS.

---

## Lifecycle

```
track_core_default_config(&cfg)        ← fill cfg with defaults
                                          (edit fields as desired)

ref  = track_core_make_reference(&cfg) ← build reference path
         … read ref arrays …
track_core_free_reference(ref)         ← release

sim  = track_core_simulate(&cfg)       ← run closed-loop simulation
         … read sim arrays & metrics …
track_core_free_simulation(sim)        ← release
```

All heap objects are allocated inside the library and **must** be freed with the
matching `_free_*` function.  Passing `NULL` to any accessor is safe (returns 0 or 0.0).

---

## Configuration — `TrackConfig`

```c
typedef struct TrackConfig {
    /* Plant */
    double m;                   /* mass [kg]                          default  0.1    */
    double izz;                 /* yaw inertia [kg·m²]                default  1.0    */
    double cornering_power;     /* fy = -C·β  [N/rad]                 default 20.0    */

    /* Timing */
    double h;                   /* integration step [s]               default  1e-4   */
    double tc;                  /* controller update interval [s]      default  1e-3   */
    double total_time;          /* total simulation time [s]           default  0.90   */

    /* Speed reference */
    double target_speed;        /* [m/s]                              default  1.0    */

    /* Lateral / yaw gains */
    double ky_p;                /* lateral P gain                     default 400.0   */
    double ky_i;                /* lateral I gain                     default   0.0   */
    double kpsi_p;              /* heading P gain                     default 200.0   */
    double kpsi_i;              /* heading I gain                     default   0.0   */
    double kr_damping;          /* yaw-rate damper gain               default  20.0   */

    /* Saturation limits */
    double n_moment_limit;      /* |N| limit [N·m]                    default 500.0   */
    double fx_limit;            /* |fx| limit [N]                     default   5.0   */
    double error_integral_limit;/* anti-windup clamp on σ_y, σ_ψ     default   0.2   */

    /* Path follower */
    int32_t lookahead_index;    /* look-ahead points                  default  60     */

    /* Initial conditions */
    double initial_y_offset;    /* y(0) [m]                           default -0.03   */
    double initial_heading_deg; /* ψ(0) [deg]                         default  3.0    */

    /* Reference path geometry */
    double straight1_len;       /* first straight [m]                 default  0.30   */
    double radius;              /* arc radius [m]                     default  0.20   */
    double straight2_len;       /* second straight [m]                default  0.30   */
    double ds;                  /* arc-length resampling step [m]     default  0.002  */
} TrackConfig;
```

All numeric fields are `double`; the only integer field is `lookahead_index` (`int32_t`).

---

## Functions

### Miscellaneous

```c
const char* track_core_version(void);
```
Returns a static string such as `"track_core 1.0.0"`.

---

### Configuration

```c
void track_core_default_config(TrackConfig* cfg);
```
Fills `*cfg` with the default values shown above.  Pass a `TrackConfig` on the stack,
call this, then modify individual fields before calling `make_reference` or `simulate`.

---

### Reference Path

```c
TrackReferencePath* track_core_make_reference(const TrackConfig* cfg);
void                track_core_free_reference(TrackReferencePath* ref);
```
`make_reference` allocates and returns the (straight → arc → straight) reference path.
Returns `NULL` if `cfg` is `NULL` or any field is invalid (non-finite, non-positive where required).

```c
int32_t track_core_ref_length(const TrackReferencePath* ref);
```
Number of reference points $N_{\rm ref}$.  With defaults: 458.

```c
int32_t track_core_ref_copy_x  (const TrackReferencePath*, double* buf, int32_t buf_len);
int32_t track_core_ref_copy_y  (const TrackReferencePath*, double* buf, int32_t buf_len);
int32_t track_core_ref_copy_psi(const TrackReferencePath*, double* buf, int32_t buf_len);
```
Copy the $x$, $y$, or $\psi$ array into a caller-allocated buffer of at least `ref_length` doubles.
Returns the number of elements copied, or 0 on error (`buf_len` too small, `NULL` pointer).

---

### Simulation

```c
TrackSimulation* track_core_simulate(const TrackConfig* cfg);
void             track_core_free_simulation(TrackSimulation* sim);
```
`simulate` runs the full closed-loop simulation and returns a handle to the results.
Returns `NULL` on invalid config or allocation failure.

```c
int32_t track_core_sim_length(const TrackSimulation* sim);
```
Number of recorded time steps.  Equal to `(int)(total_time / h)`.  With defaults: 9000.

#### Per-sample accessors

Each function copies one `double` per simulation step into `buf`.
Returns the element count, or 0 on error.

```c
int32_t track_core_sim_copy_time   (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_x      (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_y      (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_psi    (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_u      (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_v      (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_r      (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_beta   (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_ey     (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_epsi   (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_nmoment(const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_fx     (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_x_ref  (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_y_ref  (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_psi_ref(const TrackSimulation*, double* buf, int32_t buf_len);
```

| Accessor suffix | Quantity | Unit |
|-----------------|----------|------|
| `time` | Simulation time $t$ | s |
| `x` | Global X position | m |
| `y` | Global Y position | m |
| `psi` | Heading $\psi$ | rad |
| `u` | Longitudinal velocity | m/s |
| `v` | Lateral velocity | m/s |
| `r` | Yaw rate | rad/s |
| `beta` | Side-slip angle $\beta$ | rad |
| `ey` | Lateral error $e_y$ at look-ahead | m |
| `epsi` | Heading error $e_\psi$ at look-ahead | rad |
| `nmoment` | Applied yaw moment $N$ | N·m |
| `fx` | Applied longitudinal force $f_x$ | N |
| `x_ref` | X of nearest reference point | m |
| `y_ref` | Y of nearest reference point | m |
| `psi_ref` | $\psi$ of nearest reference point | rad |

> **Note.** `x_ref / y_ref / psi_ref` are logged at the *nearest* point $k^*$,
> not at the look-ahead point $k_{\rm la}$.

#### Aggregate metrics

```c
double track_core_sim_path_error_rms (const TrackSimulation*);
double track_core_sim_path_error_max (const TrackSimulation*);
double track_core_sim_ey_rms         (const TrackSimulation*);
double track_core_sim_ey_max_abs     (const TrackSimulation*);
double track_core_sim_epsi_rms       (const TrackSimulation*);
double track_core_sim_epsi_max_abs   (const TrackSimulation*);
double track_core_sim_nmoment_max_abs(const TrackSimulation*);
```

| Function | Definition |
|----------|-----------|
| `path_error_rms` | $\sqrt{\frac{1}{N}\sum\|\mathbf{p}_i - \mathbf{p}_{{\rm ref},i}\|^2}$ |
| `path_error_max` | $\max_i \|\mathbf{p}_i - \mathbf{p}_{{\rm ref},i}\|$ |
| `ey_rms` | $\sqrt{\frac{1}{N}\sum e_{y,i}^2}$ |
| `ey_max_abs` | $\max_i \|e_{y,i}\|$ |
| `epsi_rms` | $\sqrt{\frac{1}{N}\sum e_{\psi,i}^2}$ |
| `epsi_max_abs` | $\max_i \|e_{\psi,i}\|$ |
| `nmoment_max_abs` | $\max_i \|N_i\|$ |

`path_error` is the Euclidean distance from the vehicle position to the *nearest* reference point.
`ey` is the *look-ahead* lateral error; the two differ because of the look-ahead offset.

---

## Minimal C Usage Example

```c
#include "track_core.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    TrackConfig cfg;
    track_core_default_config(&cfg);

    /* Increase arc radius */
    cfg.radius = 0.30;

    TrackSimulation* sim = track_core_simulate(&cfg);
    if (!sim) { fputs("simulate failed\n", stderr); return 1; }

    int n = track_core_sim_length(sim);
    double* x = malloc(n * sizeof(double));
    double* y = malloc(n * sizeof(double));
    track_core_sim_copy_x(sim, x, n);
    track_core_sim_copy_y(sim, y, n);

    printf("path_error_rms = %.6f m\n", track_core_sim_path_error_rms(sim));

    track_core_free_simulation(sim);
    free(x); free(y);
    return 0;
}
```
