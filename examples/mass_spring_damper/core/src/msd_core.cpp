// msd_core.cpp — mass-spring-damper forced-response simulator (RK4).

#include "msd_core.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <vector>

namespace {

bool case_ok(const MsdCase& c) {
    return c.m > 0.0 && c.k >= 0.0
        && std::isfinite(c.c) && std::isfinite(c.force_amplitude)
        && std::isfinite(c.force_omega)
        && std::isfinite(c.x0) && std::isfinite(c.v0);
}

bool sampling_ok(const MsdSamplingConfig& s) {
    return s.dt > 0.0 && s.stop > 0.0 && std::isfinite(s.dt) && std::isfinite(s.stop);
}

// External force f(t) = F·sin(ω·t).
inline double external_force(double t, double F, double w) {
    return F * std::sin(w * t);
}

// Derivative of the state (x, v). The plant is linear, so the derivative
// depends on (x, v, t) — we keep the t argument explicit so RK4's
// midpoint samples evaluate the external force at the right instant.
inline void deriv(double x, double v, double t,
                  double m, double c, double k, double F, double w,
                  double& dx, double& dv)
{
    dx = v;
    dv = (external_force(t, F, w) - c * v - k * x) / m;
}

}  // namespace

struct MsdSimulation {
    std::vector<double> t, x, v, force;
    double final_x = 0.0;
    double final_v = 0.0;
    double max_abs_x = 0.0;
    double max_abs_v = 0.0;
};

extern "C" {

MSD_CORE_API const char* msd_core_version(void) {
    return "msd_core 1.0.0";
}

MSD_CORE_API void msd_core_default_case(MsdCase* c) {
    if (!c) return;
    c->m = 1.0;
    c->c = 2.0;
    c->k = 5.0;
    c->force_amplitude = 0.5;
    c->force_omega     = 2.0;
    c->x0 = 0.0;
    c->v0 = 0.0;
}

MSD_CORE_API void msd_core_default_sampling(MsdSamplingConfig* s) {
    if (!s) return;
    s->dt   = 0.001;
    s->stop = 10.0;
}

MSD_CORE_API int32_t msd_core_derived(const MsdCase* c,
                                      double* out_omega_n,
                                      double* out_zeta)
{
    if (!c || c->m <= 0.0 || c->k < 0.0) return 0;
    const double omega_n = std::sqrt(c->k / c->m);
    // zeta = c / (2 sqrt(m·k))  is undefined when m·k == 0; report 0 then.
    double zeta = 0.0;
    const double denom = 2.0 * std::sqrt(c->m * c->k);
    if (denom > 0.0) zeta = c->c / denom;
    if (out_omega_n) *out_omega_n = omega_n;
    if (out_zeta)    *out_zeta    = zeta;
    return 1;
}

MSD_CORE_API MsdSimulation* msd_core_simulate(const MsdCase* casep,
                                              const MsdSamplingConfig* sp)
{
    if (!casep || !sp || !case_ok(*casep) || !sampling_ok(*sp)) return nullptr;

    // times = np.arange(0.0, stop + dt, dt)
    // np.arange excludes the endpoint; with stop+dt the last value is the
    // largest k·dt < stop + dt. We use the same formula:
    //     n = floor((stop + dt - 0.0) / dt + tiny) so the sample count
    //     matches what NumPy would produce.
    const double dt   = sp->dt;
    const double stop = sp->stop;
    // Use a tiny epsilon to absorb dt-multiplication float noise.
    const int n = static_cast<int>((stop + dt) / dt - 1e-12) + 1;
    if (n < 2) return nullptr;

    auto* sim = new (std::nothrow) MsdSimulation;
    if (!sim) return nullptr;
    sim->t.assign(n, 0.0);
    sim->x.assign(n, 0.0);
    sim->v.assign(n, 0.0);
    sim->force.assign(n, 0.0);

    const double m  = casep->m;
    const double cf = casep->c;
    const double k  = casep->k;
    const double F  = casep->force_amplitude;
    const double w  = casep->force_omega;

    // Initial sample.
    sim->t[0]     = 0.0;
    sim->x[0]     = casep->x0;
    sim->v[0]     = casep->v0;
    sim->force[0] = external_force(0.0, F, w);

    // RK4 loop. Each step uses dt and four derivative evaluations.
    for (int i = 0; i < n - 1; ++i) {
        const double t  = i * dt;
        const double x_ = sim->x[i];
        const double v_ = sim->v[i];

        double k1x, k1v;
        deriv(x_, v_, t, m, cf, k, F, w, k1x, k1v);
        double k2x, k2v;
        deriv(x_ + 0.5 * dt * k1x, v_ + 0.5 * dt * k1v, t + 0.5 * dt,
              m, cf, k, F, w, k2x, k2v);
        double k3x, k3v;
        deriv(x_ + 0.5 * dt * k2x, v_ + 0.5 * dt * k2v, t + 0.5 * dt,
              m, cf, k, F, w, k3x, k3v);
        double k4x, k4v;
        deriv(x_ + dt * k3x, v_ + dt * k3v, t + dt,
              m, cf, k, F, w, k4x, k4v);

        const double t_next = (i + 1) * dt;
        sim->t[i + 1]     = t_next;
        sim->x[i + 1]     = x_ + dt * (k1x + 2.0 * k2x + 2.0 * k3x + k4x) / 6.0;
        sim->v[i + 1]     = v_ + dt * (k1v + 2.0 * k2v + 2.0 * k3v + k4v) / 6.0;
        sim->force[i + 1] = external_force(t_next, F, w);
    }

    sim->final_x = sim->x.back();
    sim->final_v = sim->v.back();
    double mx = 0.0, mv = 0.0;
    for (int i = 0; i < n; ++i) {
        mx = std::max(mx, std::fabs(sim->x[i]));
        mv = std::max(mv, std::fabs(sim->v[i]));
    }
    sim->max_abs_x = mx;
    sim->max_abs_v = mv;
    return sim;
}

MSD_CORE_API void msd_core_free_simulation(MsdSimulation* sim) { delete sim; }

MSD_CORE_API int32_t msd_core_sim_length(const MsdSimulation* sim) {
    return sim ? static_cast<int32_t>(sim->t.size()) : 0;
}

static int32_t copy_vec(const std::vector<double>& v, double* buf, int32_t cap) {
    if (!buf) return 0;
    const int32_t n = static_cast<int32_t>(v.size());
    if (cap < n) return 0;
    for (int32_t i = 0; i < n; ++i) buf[i] = v[i];
    return n;
}

MSD_CORE_API int32_t msd_core_sim_copy_time(const MsdSimulation* s, double* b, int32_t c)
{ return s ? copy_vec(s->t, b, c) : 0; }
MSD_CORE_API int32_t msd_core_sim_copy_position(const MsdSimulation* s, double* b, int32_t c)
{ return s ? copy_vec(s->x, b, c) : 0; }
MSD_CORE_API int32_t msd_core_sim_copy_velocity(const MsdSimulation* s, double* b, int32_t c)
{ return s ? copy_vec(s->v, b, c) : 0; }
MSD_CORE_API int32_t msd_core_sim_copy_force(const MsdSimulation* s, double* b, int32_t c)
{ return s ? copy_vec(s->force, b, c) : 0; }

MSD_CORE_API double msd_core_sim_final_position(const MsdSimulation* s)   { return s ? s->final_x   : 0.0; }
MSD_CORE_API double msd_core_sim_final_velocity(const MsdSimulation* s)   { return s ? s->final_v   : 0.0; }
MSD_CORE_API double msd_core_sim_max_abs_position(const MsdSimulation* s) { return s ? s->max_abs_x : 0.0; }
MSD_CORE_API double msd_core_sim_max_abs_velocity(const MsdSimulation* s) { return s ? s->max_abs_v : 0.0; }

}  // extern "C"
