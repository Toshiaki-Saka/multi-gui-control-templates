// track_core.cpp — planar-motion + PI path-tracking simulator.
//
// Reproduces planar_path_tracking_pi_tuned.py exactly. The Python script
// integrates each state with its own RK4 call, evaluating the derivative
// at the start of the step with the *old* state values. We replicate that
// step-by-step instead of bundling everything into a vector RK4, so the
// numerical trajectory is bit-for-bit identical.

#include "track_core.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

bool is_finite_cfg(const TrackConfig& c) {
    return c.m > 0.0 && c.izz > 0.0 && std::isfinite(c.cornering_power)
        && c.h > 0.0 && c.tc > 0.0 && c.total_time > 0.0
        && std::isfinite(c.target_speed)
        && std::isfinite(c.ky_p) && std::isfinite(c.ky_i)
        && std::isfinite(c.kpsi_p) && std::isfinite(c.kpsi_i)
        && std::isfinite(c.kr_damping)
        && c.n_moment_limit > 0.0 && c.fx_limit > 0.0
        && c.error_integral_limit > 0.0
        && c.lookahead_index >= 0
        && c.straight1_len >= 0.0 && c.radius > 0.0
        && c.straight2_len >= 0.0 && c.ds > 0.0;
}

double wrap_to_pi(double a) {
    double r = std::fmod(a + kPi, 2.0 * kPi);
    if (r < 0) r += 2.0 * kPi;
    return r - kPi;
}

double saturate(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}

// ---- per-state derivatives (signatures mirror the Python ones) -------

// Plant derivatives. The Python script wraps each in a `def *_dot` with
// extra arguments (m, fx, r, v, ...) and integrates via a generic RK4
// helper. The result is that each derivative is evaluated at the same
// (u_old, v_old, r_old, x_old, y_old, psi_old) throughout the four RK4
// sub-steps — i.e. the function arguments are "frozen" while only the
// integrated variable y itself changes. We do the same.

double f_u (double v_state_unused, double fx, double r, double v, double m) {
    (void)v_state_unused;
    return fx / m + r * v;
}
double f_v (double v_state_unused, double fy, double r, double u, double m) {
    (void)v_state_unused;
    return fy / m - r * u;
}
double f_r (double r_state_unused, double n_moment, double izz) {
    (void)r_state_unused;
    return n_moment / izz;
}
double f_x (double x_state_unused, double u, double v, double psi) {
    (void)x_state_unused;
    return u * std::cos(psi) - v * std::sin(psi);
}
double f_y (double y_state_unused, double u, double v, double psi) {
    (void)y_state_unused;
    return u * std::sin(psi) + v * std::cos(psi);
}
double f_psi(double psi_state_unused, double r) {
    (void)psi_state_unused;
    return r;
}

// Generic scalar RK4. The "derivative" only depends on the state through
// the integrated variable y (the Python script's *_dot functions never
// actually read y), so the result is just y + h * f(y_unused) — but we
// keep the four-sub-step form so the integration order is preserved
// and any future variant that *does* read y would Just Work.
template <typename F>
double rk4_scalar(F f, double y, double h) {
    double k1 = h * f(y);
    double k2 = h * f(y + 0.5 * k1);
    double k3 = h * f(y + 0.5 * k2);
    double k4 = h * f(y + k3);
    return y + (k1 + 2.0 * k2 + 2.0 * k3 + k4) / 6.0;
}

// ---- reference path generation ---------------------------------------

struct RefPath {
    std::vector<double> x, y, psi;
};

RefPath build_reference(const TrackConfig& c) {
    RefPath p;
    const double ds = c.ds;

    // Segment 1: straight along +x from 0 to straight1_len (exclusive).
    // Match np.arange(0.0, straight1_len, ds).
    const int n1 = static_cast<int>(std::ceil(c.straight1_len / ds - 1e-12));
    p.x.reserve(n1 + 256);
    for (int i = 0; i < n1; ++i) {
        p.x.push_back(i * ds);
        p.y.push_back(0.0);
        p.psi.push_back(0.0);
    }

    // Segment 2: left arc, theta in [0, pi/2) with step ds/radius.
    const double dtheta = ds / c.radius;
    const int n2 = static_cast<int>(std::ceil((kPi / 2.0) / dtheta - 1e-12));
    const double cx = c.straight1_len;
    const double cy = c.radius;
    for (int i = 0; i < n2; ++i) {
        const double th = i * dtheta;
        p.x.push_back(cx + c.radius * std::sin(th));
        p.y.push_back(cy - c.radius * std::cos(th));
        p.psi.push_back(th);
    }

    // Segment 3: straight along +y from y2_end for straight2_len (exclusive).
    if (n2 > 0) {
        const double y3_start = p.y.back();
        const double x3_const = p.x.back();
        const int n3 = static_cast<int>(std::ceil(c.straight2_len / ds - 1e-12));
        for (int i = 0; i < n3; ++i) {
            p.x.push_back(x3_const);
            p.y.push_back(y3_start + i * ds);
            p.psi.push_back(kPi / 2.0);
        }
    }
    return p;
}

int find_nearest_reference_index(double x, double y,
                                 const std::vector<double>& xr,
                                 const std::vector<double>& yr,
                                 int previous_index,
                                 int search_window = 120)
{
    const int n = static_cast<int>(xr.size());
    const int start = std::max(0, previous_index - 5);
    const int end   = std::min(n, previous_index + search_window);
    int best = start;
    double best_d = std::numeric_limits<double>::infinity();
    for (int i = start; i < end; ++i) {
        const double dx = xr[i] - x;
        const double dy = yr[i] - y;
        const double d2 = dx * dx + dy * dy;
        if (d2 < best_d) { best_d = d2; best = i; }
    }
    return best;
}

}  // namespace

struct TrackReferencePath {
    RefPath path;
};

struct TrackSimulation {
    std::vector<double> T, X, Y, PSI, U, V, R, BETA;
    std::vector<double> EY, EPSI, N_MOMENT, FX;
    std::vector<double> X_REF_LOG, Y_REF_LOG, PSI_REF_LOG;
    std::vector<double> PATH_ERR;

    double path_error_rms = 0;
    double path_error_max = 0;
    double ey_rms = 0, ey_max = 0;
    double epsi_rms = 0, epsi_max = 0;
    double nmoment_max = 0;
};

extern "C" {

TRACK_CORE_API const char* track_core_version(void) {
    return "track_core 1.0.0";
}

TRACK_CORE_API void track_core_default_config(TrackConfig* cfg) {
    if (!cfg) return;
    cfg->m              = 0.1;
    cfg->izz            = 1.0;
    cfg->cornering_power = 20.0;
    cfg->h              = 1.0e-4;
    cfg->tc             = 1.0e-3;
    cfg->total_time     = 0.90;
    cfg->target_speed   = 1.0;
    cfg->ky_p           = 400.0;
    cfg->ky_i           = 0.0;
    cfg->kpsi_p         = 200.0;
    cfg->kpsi_i         = 0.0;
    cfg->kr_damping     = 20.0;
    cfg->n_moment_limit = 500.0;
    cfg->fx_limit       = 5.0;
    cfg->error_integral_limit = 0.2;
    cfg->lookahead_index = 60;
    cfg->initial_y_offset = -0.03;
    cfg->initial_heading_deg = 3.0;
    cfg->straight1_len = 0.30;
    cfg->radius        = 0.20;
    cfg->straight2_len = 0.30;
    cfg->ds            = 0.002;
}

// ----- reference path -------------------------------------------------

TRACK_CORE_API TrackReferencePath* track_core_make_reference(const TrackConfig* cfg) {
    if (!cfg || !is_finite_cfg(*cfg)) return nullptr;
    auto* h = new (std::nothrow) TrackReferencePath;
    if (!h) return nullptr;
    h->path = build_reference(*cfg);
    return h;
}

TRACK_CORE_API void track_core_free_reference(TrackReferencePath* h) { delete h; }

TRACK_CORE_API int32_t track_core_ref_length(const TrackReferencePath* h) {
    return h ? static_cast<int32_t>(h->path.x.size()) : 0;
}

static int32_t copy_vec(const std::vector<double>& v, double* buf, int32_t cap) {
    if (!buf) return 0;
    const int32_t n = static_cast<int32_t>(v.size());
    if (cap < n) return 0;
    for (int32_t i = 0; i < n; ++i) buf[i] = v[i];
    return n;
}

TRACK_CORE_API int32_t track_core_ref_copy_x(const TrackReferencePath* h, double* b, int32_t c)
{ return h ? copy_vec(h->path.x, b, c) : 0; }
TRACK_CORE_API int32_t track_core_ref_copy_y(const TrackReferencePath* h, double* b, int32_t c)
{ return h ? copy_vec(h->path.y, b, c) : 0; }
TRACK_CORE_API int32_t track_core_ref_copy_psi(const TrackReferencePath* h, double* b, int32_t c)
{ return h ? copy_vec(h->path.psi, b, c) : 0; }

// ----- simulation -----------------------------------------------------

TRACK_CORE_API TrackSimulation* track_core_simulate(const TrackConfig* cfg) {
    if (!cfg || !is_finite_cfg(*cfg)) return nullptr;

    const RefPath ref = build_reference(*cfg);
    if (ref.x.empty()) return nullptr;

    const int num_steps = static_cast<int>(cfg->total_time / cfg->h);
    if (num_steps <= 0) return nullptr;

    auto* sim = new (std::nothrow) TrackSimulation;
    if (!sim) return nullptr;

    sim->T.assign(num_steps, 0.0);
    sim->X.assign(num_steps, 0.0);
    sim->Y.assign(num_steps, 0.0);
    sim->PSI.assign(num_steps, 0.0);
    sim->U.assign(num_steps, 0.0);
    sim->V.assign(num_steps, 0.0);
    sim->R.assign(num_steps, 0.0);
    sim->BETA.assign(num_steps, 0.0);
    sim->EY.assign(num_steps, 0.0);
    sim->EPSI.assign(num_steps, 0.0);
    sim->N_MOMENT.assign(num_steps, 0.0);
    sim->FX.assign(num_steps, 0.0);
    sim->X_REF_LOG.assign(num_steps, 0.0);
    sim->Y_REF_LOG.assign(num_steps, 0.0);
    sim->PSI_REF_LOG.assign(num_steps, 0.0);
    sim->PATH_ERR.assign(num_steps, 0.0);

    // Initial state.
    double u = cfg->target_speed;
    double v = 0.0;
    double r = 0.0;
    double x = 0.0;
    double y = cfg->initial_y_offset;
    double psi = cfg->initial_heading_deg * kPi / 180.0;
    double t = 0.0;

    double fx = 0.0, fy = 0.0, n_moment = 0.0;
    double beta = 0.0;
    double speed_int = 0.0, ey_int = 0.0, epsi_int = 0.0;

    int ref_index = 0;
    int control_counter = 0;
    const int control_interval_steps =
        std::max(1, static_cast<int>(cfg->tc / cfg->h));
    const int ref_len = static_cast<int>(ref.x.size());

    for (int n = 0; n < num_steps; ++n) {
        // ----- find reference point -----
        const int nearest_index = find_nearest_reference_index(
            x, y, ref.x, ref.y, ref_index);
        ref_index = nearest_index;
        const int tracking_index =
            std::min(ref_len - 1, nearest_index + cfg->lookahead_index);

        // ----- tracking error (at look-ahead point) -----
        const double dx = x - ref.x[tracking_index];
        const double dy = y - ref.y[tracking_index];
        const double psi_r = ref.psi[tracking_index];
        const double e_y = -std::sin(psi_r) * dx + std::cos(psi_r) * dy;
        const double e_psi = wrap_to_pi(psi_r - psi);

        // ----- lateral force (slip-angle linear model) -----
        fy = -cfg->cornering_power * beta;

        // ----- control update (every tc seconds) -----
        if (control_counter >= control_interval_steps) {
            control_counter = 0;
            const double speed = std::sqrt(u * u + v * v);
            const double speed_err = cfg->target_speed - speed;
            speed_int += speed_err * cfg->tc;
            fx = saturate(100.0 * speed_err + 0.1 * speed_int,
                          -cfg->fx_limit, cfg->fx_limit);

            ey_int   += e_y   * cfg->tc;
            epsi_int += e_psi * cfg->tc;
            ey_int   = saturate(ey_int,   -cfg->error_integral_limit,
                                           cfg->error_integral_limit);
            epsi_int = saturate(epsi_int, -cfg->error_integral_limit,
                                           cfg->error_integral_limit);

            const double n_raw =
                  -cfg->ky_p   * e_y
                - cfg->ky_i   * ey_int
                + cfg->kpsi_p * e_psi
                + cfg->kpsi_i * epsi_int
                - cfg->kr_damping * r;

            n_moment = saturate(n_raw,
                                -cfg->n_moment_limit,
                                 cfg->n_moment_limit);
        }
        ++control_counter;

        // ----- log -----
        sim->T[n] = t; sim->X[n] = x; sim->Y[n] = y; sim->PSI[n] = psi;
        sim->U[n] = u; sim->V[n] = v; sim->R[n] = r; sim->BETA[n] = beta;
        sim->EY[n] = e_y; sim->EPSI[n] = e_psi;
        sim->N_MOMENT[n] = n_moment; sim->FX[n] = fx;
        sim->X_REF_LOG[n]   = ref.x[nearest_index];
        sim->Y_REF_LOG[n]   = ref.y[nearest_index];
        sim->PSI_REF_LOG[n] = ref.psi[nearest_index];
        const double pex = x - ref.x[nearest_index];
        const double pey = y - ref.y[nearest_index];
        sim->PATH_ERR[n] = std::sqrt(pex * pex + pey * pey);

        // ----- RK4 integration (one variable at a time, "old" values frozen) -----
        // Python passes (m, fx, r_old, v_old) etc. as the args to *_dot
        // and these are kept constant across the four RK4 sub-steps.
        const double u_old   = u;
        const double v_old   = v;
        const double r_old   = r;
        const double x_old   = x;
        const double y_old   = y;
        const double psi_old = psi;

        const double m_  = cfg->m;
        const double izz = cfg->izz;

        u = rk4_scalar(
            [&](double){ return f_u(0.0, fx, r_old, v_old, m_); },
            u_old, cfg->h);
        v = rk4_scalar(
            [&](double){ return f_v(0.0, fy, r_old, u_old, m_); },
            v_old, cfg->h);
        r = rk4_scalar(
            [&](double){ return f_r(0.0, n_moment, izz); },
            r_old, cfg->h);
        x = rk4_scalar(
            [&](double){ return f_x(0.0, u_old, v_old, psi_old); },
            x_old, cfg->h);
        y = rk4_scalar(
            [&](double){ return f_y(0.0, u_old, v_old, psi_old); },
            y_old, cfg->h);
        psi = rk4_scalar(
            [&](double){ return f_psi(0.0, r_old); },
            psi_old, cfg->h);

        t += cfg->h;
        beta = std::atan2(v, u);
    }

    // ----- metrics -----
    double sum_pe2 = 0, max_pe = 0;
    double sum_ey2 = 0, max_ey = 0;
    double sum_epsi2 = 0, max_epsi = 0;
    double max_nm = 0;
    for (int i = 0; i < num_steps; ++i) {
        sum_pe2 += sim->PATH_ERR[i] * sim->PATH_ERR[i];
        if (sim->PATH_ERR[i] > max_pe) max_pe = sim->PATH_ERR[i];
        sum_ey2 += sim->EY[i] * sim->EY[i];
        const double aey = std::fabs(sim->EY[i]);
        if (aey > max_ey) max_ey = aey;
        sum_epsi2 += sim->EPSI[i] * sim->EPSI[i];
        const double aep = std::fabs(sim->EPSI[i]);
        if (aep > max_epsi) max_epsi = aep;
        const double anm = std::fabs(sim->N_MOMENT[i]);
        if (anm > max_nm) max_nm = anm;
    }
    const double inv_n = 1.0 / num_steps;
    sim->path_error_rms = std::sqrt(sum_pe2 * inv_n);
    sim->path_error_max = max_pe;
    sim->ey_rms   = std::sqrt(sum_ey2 * inv_n);
    sim->ey_max   = max_ey;
    sim->epsi_rms = std::sqrt(sum_epsi2 * inv_n);
    sim->epsi_max = max_epsi;
    sim->nmoment_max = max_nm;
    return sim;
}

TRACK_CORE_API void track_core_free_simulation(TrackSimulation* s) { delete s; }

TRACK_CORE_API int32_t track_core_sim_length(const TrackSimulation* s) {
    return s ? static_cast<int32_t>(s->T.size()) : 0;
}

#define COPY(field) \
    TRACK_CORE_API int32_t track_core_sim_copy_##field( \
        const TrackSimulation* s, double* b, int32_t c) { \
        return s ? copy_vec(s->field, b, c) : 0; \
    }

// Accessor function names map to vector field names:
TRACK_CORE_API int32_t track_core_sim_copy_time   (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->T, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_x      (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->X, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_y      (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->Y, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_psi    (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->PSI, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_u      (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->U, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_v      (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->V, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_r      (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->R, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_beta   (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->BETA, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_ey     (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->EY, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_epsi   (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->EPSI, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_nmoment(const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->N_MOMENT, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_fx     (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->FX, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_x_ref  (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->X_REF_LOG, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_y_ref  (const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->Y_REF_LOG, b, c) : 0; }
TRACK_CORE_API int32_t track_core_sim_copy_psi_ref(const TrackSimulation* s, double* b, int32_t c) { return s ? copy_vec(s->PSI_REF_LOG, b, c) : 0; }

TRACK_CORE_API double track_core_sim_path_error_rms (const TrackSimulation* s) { return s ? s->path_error_rms : 0.0; }
TRACK_CORE_API double track_core_sim_path_error_max (const TrackSimulation* s) { return s ? s->path_error_max : 0.0; }
TRACK_CORE_API double track_core_sim_ey_rms         (const TrackSimulation* s) { return s ? s->ey_rms : 0.0; }
TRACK_CORE_API double track_core_sim_ey_max_abs     (const TrackSimulation* s) { return s ? s->ey_max : 0.0; }
TRACK_CORE_API double track_core_sim_epsi_rms       (const TrackSimulation* s) { return s ? s->epsi_rms : 0.0; }
TRACK_CORE_API double track_core_sim_epsi_max_abs   (const TrackSimulation* s) { return s ? s->epsi_max : 0.0; }
TRACK_CORE_API double track_core_sim_nmoment_max_abs(const TrackSimulation* s) { return s ? s->nmoment_max : 0.0; }

}  // extern "C"
