# Vehicle Model and Controller

## 1. Coordinate Frame and State Variables

The simulation uses a **right-hand planar frame**.
Global axes $X$–$`Y`$ are fixed to the world; the body frame rotates with the vehicle heading $\psi$.

| Symbol | Unit | Description |
|--------|------|-------------|
| $u$ | m/s | Longitudinal velocity (body frame, forward) |
| $v$ | m/s | Lateral velocity (body frame, leftward) |
| $r$ | rad/s | Yaw rate (positive = counter-clockwise) |
| $x$ | m | Global X position |
| $y$ | m | Global Y position |
| $\psi$ | rad | Heading (angle from +X axis) |

---

## 2. Plant — 3-DOF Planar Vehicle

### Equations of Motion

$$
\dot{u} = \frac{f_x}{m} + r v
$$

$$
\dot{v} = \frac{f_y}{m} - r u
$$

$$
\dot{r} = \frac{N}{I_{zz}}
$$

$$
\dot{x} = u \cos\psi - v \sin\psi
$$

$$
\dot{y} = u \sin\psi + v \cos\psi
$$

$$
\dot{\psi} = r
$$

where $m$ is the vehicle mass, $I_{zz}$ is the yaw moment of inertia, $f_x$ is the longitudinal force input, $f_y$ is the lateral (tyre) force, and $N$ is the applied yaw moment.

The $r v$ and $r u$ terms are the Coriolis accelerations that appear when writing Newton's second law in the rotating body frame.

### Lateral Tyre Force — Linear Model

The body side-slip angle is

$$
\beta = \arctan\!\left(\frac{v}{u}\right)
$$

and the lateral force follows a linear (small-angle brush) model:

$$
f_y = -C \,\beta
$$

where $C$ is the **cornering power** (N/rad).  Positive $\beta$ (nose pointing left of velocity vector) generates a restoring force to the right ($f_y < 0$), which acts to reduce the slip angle.

### Default Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| $m$ | 0.1 kg | Vehicle mass |
| $I_{zz}$ | 1.0 kg·m² | Yaw inertia |
| $C$ | 20.0 N/rad | Cornering power |

---

## 3. Reference Path

The reference consists of three segments concatenated and resampled at constant arc-length step $\Delta s = 0.002$ m.

### Segment 1 — Straight along +X

$$
x_{\rm ref}(s) = s,\quad
y_{\rm ref}(s) = 0,\quad
\psi_{\rm ref}(s) = 0
\qquad s \in [0,\, L_1)
$$

### Segment 2 — 90° Left-hand Arc

The arc has radius $R$ and is centred at $(L_1,\, R)$.
Parameterised by the traversal angle $\theta \in [0, \pi/2)$:

$$
x_{\rm ref}(\theta) = L_1 + R\sin\theta, \quad
y_{\rm ref}(\theta) = R\,(1 - \cos\theta), \quad
\psi_{\rm ref}(\theta) = \theta
$$

At $\theta = 0$ the point coincides with the end of Segment 1; at $\theta = \pi/2$ the tangent direction is $\psi = \pi/2$ (+Y).

### Segment 3 — Straight along +Y

Continues from the arc endpoint $\bigl(L_1 + R,\; R\bigr)$ in direction $\psi = \pi/2$:

$$
x_{\rm ref} = L_1 + R,\quad
y_{\rm ref}(s) = R + s,\quad
\psi_{\rm ref} = \frac{\pi}{2}
\qquad s \in [0,\, L_2)
$$

### Point Count per Segment

To match NumPy's `np.arange(0, L, ds)` semantics (exclusive upper bound), the number of points per segment is

$$
N = \left\lceil \frac{L}{\Delta s} - 10^{-12} \right\rceil
$$

For the arc the angular step is $\Delta\theta = \Delta s / R$ and $L = \pi/2$.

### Default Geometry

| Parameter | Default | Description |
|-----------|---------|-------------|
| $L_1$ | 0.30 m | First straight length |
| $R$ | 0.20 m | Arc radius |
| $L_2$ | 0.30 m | Second straight length |
| $\Delta s$ | 0.002 m | Arc-length resampling step |

With these defaults, 458 reference points are generated.

---

## 4. Tracking Errors

### Nearest-Point Search

At each step a **windowed linear search** finds the index of the closest reference point:

$$
k^* = \underset{k \;\in\; [\max(0,\; k_{\rm prev}-5),\; \min(N_{\rm ref},\; k_{\rm prev}+120))}
      {\arg\min}
      \left\|(x - x_{\rm ref}[k],\; y - y_{\rm ref}[k])\right\|_2
$$

The window $`[k^*-5, k^*+120)`$ prevents backward jumps while allowing the vehicle to advance quickly through the path.

### Look-ahead Target

$$
k_{\rm la} = \min\!\bigl(k^* + L_{\rm la},\; N_{\rm ref} - 1\bigr)
$$

where $L_{\rm la} = 60$ points by default ($= 60 \times 0.002 = 0.12$ m ahead).

### Error Coordinates

Let $\delta x = x - x_{\rm ref}[k_{\rm la}]$, $\delta y = y - y_{\rm ref}[k_{\rm la}]$, and $\psi_r = \psi_{\rm ref}[k_{\rm la}]$.

**Lateral error** — signed distance from the reference tangent line, positive when the vehicle is to the **left**:

$$
e_y = -\sin\psi_r \cdot \delta x + \cos\psi_r \cdot \delta y
$$

This is the projection of the position error onto the reference normal direction $(-\sin\psi_r, \cos\psi_r)$.

**Heading error** — wrapped to $(-\pi, \pi]$:

$$
e_\psi = \mathrm{wrap}_{(-\pi,\pi]}\!\bigl(\psi_r - \psi\bigr)
$$

Positive $e_\psi$ means the reference heading points more counter-clockwise than the vehicle heading.

---

## 5. Controller

The controller is updated every $t_c$ seconds (every $\lfloor t_c / h \rfloor$ integration steps, default 10 steps).

### 5.1 Speed Controller (Longitudinal PI)

$$
v_{\rm spd} = \sqrt{u^2 + v^2}
$$

$$
e_{\rm spd}[k] = v_{\rm target} - v_{\rm spd}[k]
$$

$$
\sigma_{\rm spd}[k] = \sigma_{\rm spd}[k-1] + e_{\rm spd}[k] \cdot t_c
$$

$$
f_x = \mathrm{sat}\!\Bigl(100\; e_{\rm spd} + 0.1\; \sigma_{\rm spd},\; \pm f_{x,\rm lim}\Bigr)
$$

The P gain of 100 and I gain of 0.1 are hard-coded to match the Python reference.

### 5.2 Lateral + Yaw Controller (Coupled PI)

Integrators with anti-windup clamping:

$$
\sigma_y[k] = \mathrm{sat}\!\Bigl(\sigma_y[k-1] + e_y[k] \cdot t_c,\; \pm \sigma_{\rm lim}\Bigr)
$$

$$
\sigma_\psi[k] = \mathrm{sat}\!\Bigl(\sigma_\psi[k-1] + e_\psi[k] \cdot t_c,\; \pm \sigma_{\rm lim}\Bigr)
$$

Moment command:

$$
N_{\rm raw} =
  -k_{y,p}\; e_y
  - k_{y,i}\; \sigma_y
  + k_{\psi,p}\; e_\psi
  + k_{\psi,i}\; \sigma_\psi
  - k_r\; r
$$

$$
N = \mathrm{sat}(N_{\rm raw},\; \pm N_{\rm lim})
$$

The $-k_{y,p} e_y$ term steers the vehicle toward the path; $+k_{\psi,p} e_\psi$ aligns the heading with the reference tangent; $-k_r r$ is a **yaw-rate damper** that suppresses oscillation on the arc.

### 5.3 Default Gains

| Gain | Default | Role |
|------|---------|------|
| $k_{y,p}$ | 400.0 | Lateral proportional |
| $k_{y,i}$ | 0.0 | Lateral integral (disabled) |
| $k_{\psi,p}$ | 200.0 | Heading proportional |
| $k_{\psi,i}$ | 0.0 | Heading integral (disabled) |
| $k_r$ | 20.0 | Yaw-rate damping |
| $N_{\rm lim}$ | 500.0 N·m | Moment saturation |
| $f_{x,\rm lim}$ | 5.0 N | Force saturation |
| $\sigma_{\rm lim}$ | 0.2 | Integrator anti-windup |
| $L_{\rm la}$ | 60 pts | Look-ahead index |

### 5.4 Timing

| Parameter | Default | Description |
|-----------|---------|-------------|
| $h$ | $10^{-4}$ s | Integrator step |
| $t_c$ | $10^{-3}$ s | Controller update period |
| $T$ | 0.90 s | Total simulation time |

Steps per run: $T/h = 9000$.  Controller updates per run: $T/t_c = 900$.
