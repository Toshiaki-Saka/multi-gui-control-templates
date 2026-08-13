# pi_path_tracking — PI path following

Back to the [algorithms overview](../algorithms.md) · [C ABI](api.md) · [日本語](../../ja/pi_path_tracking/theory.md)


Source: `examples/pi_path_tracking/core/src/track_core.cpp`.
Port of `planar_path_tracking_pi_tuned.py`.
C ABI details: [api.md](api.md).

## 1. States

| Symbol | Unit | Meaning |
|--------|------|---------|
| $u$ | m/s | body-frame longitudinal velocity |
| $v$ | m/s | body-frame lateral velocity (positive left) |
| $r$ | rad/s | yaw rate (positive counter-clockwise) |
| $x, y$ | m | global position |
| $\psi$ | rad | heading measured from $+X$ |

## 2. Plant — 3-DOF planar vehicle

$$\dot u = \frac{f_x}{m} + r\,v, \qquad \dot v = \frac{f_y}{m} - r\,u, \qquad \dot r = \frac{N}{I_{zz}}$$

$$\dot x = u\cos\psi - v\sin\psi, \qquad \dot y = u\sin\psi + v\cos\psi, \qquad \dot\psi = r$$

The $r v$ and $r u$ terms are the Coriolis accelerations that appear because the
momentum equations are written in the rotating body frame.

Lateral force from a linear slip model:

$$\beta = \arctan\frac{v}{u}, \qquad f_y = -C\,\beta$$

with $C$ the cornering power [N/rad]. Note the one-step lag in the
implementation: $\beta$ is recomputed at the **end** of each step, so the
$`f_y`$ used within step $n$ is built from the slip angle of step $n-1$
($`\beta_0 = 0`$). This is deliberate — it mirrors the reference script.

Defaults: $m = 0.1$ kg, $I_{zz} = 1.0$ kg·m², $C = 20$ N/rad.

## 3. Reference path

Three segments resampled at constant arc length $\Delta s$:

1. **Straight along $+X$**, $s \in [0, L_1)$: $(s,\ 0)$, $`\psi_{\rm ref} = 0`$.
2. **90° left arc** of radius $R$ centred at $(L_1, R)$, $\theta \in [0, \pi/2)$
   with $\Delta\theta = \Delta s / R$:

   $$x = L_1 + R\sin\theta, \qquad y = R(1-\cos\theta), \qquad \psi_{\rm ref} = \theta$$

3. **Straight along $+Y$** from the arc endpoint, $s \in [0, L_2)$:
   $(L_1+R,\ R + s)$, $`\psi_{\rm ref} = \pi/2`$.

Point counts follow `np.arange(0, L, ds)` semantics (upper bound excluded):

$$N_{\rm seg} = \left\lceil \frac{L}{\Delta s} - 10^{-12} \right\rceil$$

Defaults $L_1 = 0.30$ m, $R = 0.20$ m, $L_2 = 0.30$ m, $\Delta s = 0.002$ m give
458 points. The path is exposed separately through
`track_core_make_reference()` so a frontend can draw it without running a
simulation.

## 4. Nearest point and look-ahead

A **windowed** linear search, seeded with the previous index, keeps the cost
$O(1)$ per step and prevents the vehicle from latching onto a point behind it:

$$k^\* = \arg\min_{k\,\in\,[\max(0,\,k_{\rm prev}-5),\ \min(N_{\rm ref},\,k_{\rm prev}+120))} \bigl\lVert (x,y) - (x_{\rm ref}[k],\,y_{\rm ref}[k]) \bigr\rVert_2$$

The backward slack of 5 points tolerates small retreats; the forward window of
120 points ($0.24$ m at the default $\Delta s$) bounds how far the vehicle may
advance in one step.

The control target is a **look-ahead** point:

$$k_{\rm la} = \min\bigl(k^\* + L_{\rm la},\ N_{\rm ref}-1\bigr), \qquad L_{\rm la} = 60\ \text{points} = 0.12\ \text{m}$$

Look-ahead is what turns a pure position error into an anticipatory steering
command; without it, the $-k_{y,p}e_y$ term alone oscillates on the arc.

## 5. Error coordinates

With $`\delta x = x - x_{\rm ref}[k_{\rm la}]`$,
$`\delta y = y - y_{\rm ref}[k_{\rm la}]`$ and
$`\psi_r = \psi_{\rm ref}[k_{\rm la}]`$:

$$e_y = -\sin\psi_r\ \delta x + \cos\psi_r\ \delta y$$

$$e_\psi = \mathrm{wrap}_{(-\pi,\pi]}\bigl(\psi_r - \psi\bigr)$$

$`e_y`$ is the projection of the position error onto the reference normal
$(-\sin\psi_r, \cos\psi_r)$ — positive when the vehicle is to the left of the
path. The wrap is

$$\mathrm{wrap}_{(-\pi,\pi]}(\alpha) = \bigl((\alpha + \pi) \bmod 2\pi\bigr) - \pi$$

implemented with `std::fmod` plus a correction for the negative-remainder case,
so a heading error never jumps by $2\pi$ when the reference crosses $\pm\pi$.

## 6. Controllers

The controller runs on its own period $t_c$, i.e. every
$`\lfloor t_c/h \rfloor = 10`$ integration steps; between updates $f_x$ and $N$
are held constant (zero-order hold on the actuators). The counter starts at
zero and is tested with `>=`, so the **first** update happens at step 10, not
step 0: for the first millisecond the vehicle coasts with $f_x = N = 0$.

**Longitudinal PI** (gains hard-coded to match the reference):

$$e_{\rm spd} = v_{\rm target} - \sqrt{u^2+v^2}, \qquad \sigma_{\rm spd} \mathrel{+}= e_{\rm spd}\,t_c$$

$$f_x = \mathrm{sat}\bigl(100\,e_{\rm spd} + 0.1\,\sigma_{\rm spd},\ \pm f_{x,\rm lim}\bigr)$$

**Lateral + yaw PI with rate damping**, integrators clamped for anti-windup:

$$\sigma_y = \mathrm{sat}\bigl(\sigma_y + e_y t_c,\ \pm\sigma_{\rm lim}\bigr), \qquad \sigma_\psi = \mathrm{sat}\bigl(\sigma_\psi + e_\psi t_c,\ \pm\sigma_{\rm lim}\bigr)$$

$$N_{\rm raw} = -k_{y,p}e_y - k_{y,i}\sigma_y + k_{\psi,p}e_\psi + k_{\psi,i}\sigma_\psi - k_r\,r$$

$$N = \mathrm{sat}(N_{\rm raw},\ \pm N_{\rm lim})$$

Sign reading: $`-k_{y,p}e_y`$ yaws the vehicle back toward the path,
$`+k_{\psi,p}e_\psi`$ aligns the heading with the reference tangent, and
$`-k_r r`$ is a yaw-rate damper that suppresses the oscillation the first two
terms would otherwise excite on the arc.

| Gain | Default | Role |
|------|---------|------|
| $`k_{y,p}`$ | 400.0 | lateral proportional |
| $`k_{y,i}`$ | 0.0 | lateral integral (off by default) |
| $`k_{\psi,p}`$ | 200.0 | heading proportional |
| $`k_{\psi,i}`$ | 0.0 | heading integral (off by default) |
| $`k_r`$ | 20.0 | yaw-rate damping |
| $`N_{\rm lim}`$ | 500.0 N·m | moment saturation |
| $`f_{x,\rm lim}`$ | 5.0 N | force saturation |
| $`\sigma_{\rm lim}`$ | 0.2 | integrator clamp |
| $`L_{\rm la}`$ | 60 pts | look-ahead |
| $h$ / $t_c$ / $T$ | 10⁻⁴ / 10⁻³ / 0.90 s | step / control period / horizon |

Initial condition: on-path speed $u = v_{\rm target}$, offset
$y_0 = -0.03$ m and heading error $\psi_0 = 3°$, so the run starts with a
non-trivial transient to reject.

## 7. Integration — per-state scalar RK4 (and why it is Euler)

Each of the six states is advanced by its own scalar RK4 call, with every
*other* state frozen at its start-of-step value:

$$k_1 = h f(q_n),\quad k_2 = h f(q_n + \tfrac{k_1}{2}),\quad k_3 = h f(q_n + \tfrac{k_2}{2}),\quad k_4 = h f(q_n + k_3)$$

$$q_{n+1} = q_n + \frac{k_1 + 2k_2 + 2k_3 + k_4}{6}$$

But **no derivative in this model reads its own integrated variable**:

| State $q$ | Derivative $`\dot q`$ | Self-dependent? |
|-----------|----------------------|-----------------|
| $u$ | $`f_x/m + r_{\rm old}\,v_{\rm old}`$ | no |
| $v$ | $`f_y/m - r_{\rm old}\,u_{\rm old}`$ | no |
| $r$ | $`N/I_{zz}`$ | no |
| $x$ | $`u_{\rm old}\cos\psi_{\rm old} - v_{\rm old}\sin\psi_{\rm old}`$ | no |
| $y$ | $`u_{\rm old}\sin\psi_{\rm old} + v_{\rm old}\cos\psi_{\rm old}`$ | no |
| $\psi$ | $`r_{\rm old}`$ | no |

Since $`f(q_n + \alpha k;\,\mathbf{p}) = f(q_n;\,\mathbf{p})`$ for every
$\alpha$, $f$ is constant across the sub-steps,
$`k_1 = k_2 = k_3 = k_4 = h f(q_n)`$, and the combination collapses to

$$q_{n+1} = q_n + h\,f(q_n)$$

— plain **forward Euler**, with $O(h)$ global error rather than $O(h^4)$. The
step $h = 10^{-4}$ s is small enough that this is invisible (deviation from the
reference below $5\times10^{-10}$ m).

The RK4 scaffolding is kept for two reasons: bit-exact reproducibility of the
reference (the same floating-point operations in the same order), and
future-proofing — a nonlinear tyre model that reads $v$ inside $\dot v$ would
make the sub-steps non-trivial and the shell would then earn its accuracy
without a structural rewrite.

## 8. Step ordering

```text
for n = 0 .. num_steps-1:
    k*        = nearest_index(x, y, from k_prev)      # windowed search
    k_la      = min(k* + lookahead, N_ref-1)
    (e_y, e_ψ) = errors at k_la
    f_y       = −C · β                                 # β from the PREVIOUS step
    if control_counter ≥ steps_per_tc:                 # every tc seconds
        update f_x, N  (PI + damping, saturated)
    log(t, x, y, ψ, u, v, r, β, e_y, e_ψ, N, f_x, ref[k*], path_err)
    integrate u, v, r, x, y, ψ   (frozen old values)
    t += h ;  β = atan2(v, u)                          # β for the NEXT step
```

Logging happens **before** integration, so sample $n$ reports the state at
$t = n h$ together with the command that was applied over $[nh, (n{+}1)h)$.

## 9. Metrics

`path_err` is measured against the **nearest** point (not the look-ahead one),
so it is a true geometric distance:

$$\mathrm{path\_err}_n = \bigl\lVert (x_n,y_n) - (x_{\rm ref}[k^\*], y_{\rm ref}[k^\*]) \bigr\rVert_2$$

Aggregates: RMS and max of `path_err`, RMS and max-abs of $`e_y`$ and
$`e_\psi`$, and max-abs of $N$. With the defaults these are pinned by the smoke
test at `path_error_rms` = 0.013762 m, `path_error_max` = 0.030000 m,
`ey_rms` = 0.020243 m, `ey_max` = 0.032639 m, `epsi_rms` = 0.300645 rad,
`epsi_max` = 0.579404 rad, `max|N|` = 28.495415 N·m.

Note that $`e_\psi`$ is large in RMS because it is measured against the
look-ahead tangent: on the arc the reference heading 0.12 m ahead differs from
the current heading by construction, so a nonzero steady $`e_\psi`$ is the
correct tracking behaviour, not an error.
