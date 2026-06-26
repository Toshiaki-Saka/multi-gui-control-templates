// tdof_core.cpp — C ABI wrapper for the 2-DOF comparison core.

#include "tdof_core.h"
#include "tdof.hpp"

#include <cmath>
#include <cstring>
#include <new>
#include <vector>

namespace {

bool valid_config(const TdofConfig& c) {
    return c.m > 0.0 && std::isfinite(c.c) && std::isfinite(c.k)
        && std::isfinite(c.kp) && std::isfinite(c.ki) && std::isfinite(c.kd)
        && std::isfinite(c.ref)
        && c.t_end > 0.0 && c.dt > 0.0 && c.dt < c.t_end;
}

tdof::TransferFunction select_tf(const TdofConfig& cfg, int which) {
    switch (which) {
        case 0:  return tdof::build_plant(cfg);
        case 1:  return tdof::build_pid(cfg);
        case 2:  return tdof::build_reference_filter(cfg);
        case 3:  return tdof::build_closed_loop(cfg);
        default: return tdof::TransferFunction();
    }
}

}  // namespace

struct TdofSimulation {
    std::vector<double> t;
    std::vector<double> r;
    std::vector<double> z;
    std::vector<double> y_pid;
    std::vector<double> y_2dof;
    double ref = 1.0;
};

extern "C" {

TDOF_CORE_API const char* tdof_core_version(void) {
    return "tdof_core 1.0.0";
}

TDOF_CORE_API void tdof_core_default_config(TdofConfig* cfg) {
    if (!cfg) return;
    cfg->m   = 1.0e-2;
    cfg->c   = 1.5e-2;
    cfg->k   = 1.0;
    cfg->kp  = 2.0;
    cfg->ki  = 10.0;
    cfg->kd  = 0.1;
    cfg->ref = 10.0;
    cfg->t_end = 2.0;
    cfg->dt    = 0.01;
}

TDOF_CORE_API int32_t tdof_core_get_tf(
    const TdofConfig* cfg, int32_t which,
    double* num, int32_t* num_len,
    double* den, int32_t* den_len)
{
    if (!cfg || !num_len || !den_len) return 0;
    if (which < 0 || which > 3) return 0;

    tdof::TransferFunction tf = select_tf(*cfg, which);
    const int32_t nN = static_cast<int32_t>(tf.num.size());
    const int32_t nD = static_cast<int32_t>(tf.den.size());

    const int32_t capN = *num_len;
    const int32_t capD = *den_len;
    *num_len = nN;
    *den_len = nD;

    if (!num || !den) return 1;            // size-query only
    if (capN < nN || capD < nD) return 0;  // insufficient buffers

    for (int32_t i = 0; i < nN; ++i) num[i] = tf.num[i];
    for (int32_t i = 0; i < nD; ++i) den[i] = tf.den[i];
    return 1;
}

TDOF_CORE_API TdofSimulation* tdof_core_simulate(const TdofConfig* cfg) {
    if (!cfg || !valid_config(*cfg)) return nullptr;

    // Build the time grid: np.arange(0, t_end, dt)
    const int N = static_cast<int>(std::ceil(cfg->t_end / cfg->dt - 1e-12));
    if (N <= 0) return nullptr;

    auto* sim = new (std::nothrow) TdofSimulation;
    if (!sim) return nullptr;
    sim->ref = cfg->ref;

    sim->t.resize(N);
    sim->r.assign(N, 1.0);
    for (int i = 0; i < N; ++i) sim->t[i] = i * cfg->dt;

    try {
        const tdof::TransferFunction K2  = tdof::build_reference_filter(*cfg);
        const tdof::TransferFunction Gyz = tdof::build_closed_loop(*cfg);

        // z = K2 * r
        sim->z = tdof::forced_response(K2, sim->r, cfg->dt);
        // y_pid = Gyz * r
        const tdof::StateSpace gss = tdof::tf_to_ss(Gyz);
        sim->y_pid  = tdof::forced_response_ss(gss, sim->r, cfg->dt);
        // y_2dof = Gyz * z
        sim->y_2dof = tdof::forced_response_ss(gss, sim->z, cfg->dt);
    } catch (...) {
        delete sim;
        return nullptr;
    }
    return sim;
}

TDOF_CORE_API void tdof_core_free_simulation(TdofSimulation* sim) {
    delete sim;
}

TDOF_CORE_API int32_t tdof_core_sim_length(const TdofSimulation* sim) {
    if (!sim) return 0;
    return static_cast<int32_t>(sim->t.size());
}

namespace {
int32_t copy_vec(const std::vector<double>& v, double scale,
                 double* buffer, int32_t buffer_len)
{
    const int32_t n = static_cast<int32_t>(v.size());
    if (!buffer || buffer_len < n) return 0;
    for (int32_t i = 0; i < n; ++i) buffer[i] = v[i] * scale;
    return n;
}
}  // namespace

TDOF_CORE_API int32_t tdof_core_sim_copy_time(
    const TdofSimulation* sim, double* buffer, int32_t buffer_len) {
    if (!sim) return 0;
    return copy_vec(sim->t, 1.0, buffer, buffer_len);
}

TDOF_CORE_API int32_t tdof_core_sim_copy_r(
    const TdofSimulation* sim, double* buffer, int32_t buffer_len) {
    if (!sim) return 0;
    return copy_vec(sim->r, sim->ref, buffer, buffer_len);
}

TDOF_CORE_API int32_t tdof_core_sim_copy_z(
    const TdofSimulation* sim, double* buffer, int32_t buffer_len) {
    if (!sim) return 0;
    return copy_vec(sim->z, sim->ref, buffer, buffer_len);
}

TDOF_CORE_API int32_t tdof_core_sim_copy_y_pid(
    const TdofSimulation* sim, double* buffer, int32_t buffer_len) {
    if (!sim) return 0;
    return copy_vec(sim->y_pid, sim->ref, buffer, buffer_len);
}

TDOF_CORE_API int32_t tdof_core_sim_copy_y_2dof(
    const TdofSimulation* sim, double* buffer, int32_t buffer_len) {
    if (!sim) return 0;
    return copy_vec(sim->y_2dof, sim->ref, buffer, buffer_len);
}

}  // extern "C"
