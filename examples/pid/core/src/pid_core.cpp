// pid_core.cpp — simulator + C ABI wrapper.

#include "pid_core.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <vector>

namespace {

bool is_finite_cfg(const PidConfig& c) {
    return std::isfinite(c.theta_start)
        && std::isfinite(c.theta_goal)
        && std::isfinite(c.offset)
        && std::isfinite(c.kp) && std::isfinite(c.ki) && std::isfinite(c.kd)
        && std::isfinite(c.dt)             && c.dt             > 0.0
        && std::isfinite(c.integral_clamp) && c.integral_clamp >= 0.0
        && std::isfinite(c.output_clamp)   && c.output_clamp   >= 0.0;
}

}  // namespace

struct PidSimulation {
    std::vector<double> t;
    std::vector<double> theta;
};

extern "C" {

PID_CORE_API const char* pid_core_version(void) {
    return "pid_core 1.0.0";
}

PID_CORE_API void pid_core_default_config(PidConfig* cfg) {
    if (!cfg) return;
    cfg->theta_start    = 0.0;
    cfg->theta_goal     = 90.0;
    cfg->offset         = 0.0;
    cfg->time_length    = 150;
    cfg->kp             = 0.10;
    cfg->ki             = 0.01;
    cfg->kd             = 0.0;
    cfg->dt             = 1.0;
    cfg->integral_clamp = 0.0;
    cfg->output_clamp   = 0.0;
}

PID_CORE_API PidSimulation* pid_core_simulate(const PidConfig* cfg) {
    if (!cfg || !is_finite_cfg(*cfg) || cfg->time_length < 2) return nullptr;

    const int N = cfg->time_length;
    auto* sim = new (std::nothrow) PidSimulation;
    if (!sim) return nullptr;
    sim->t.resize(static_cast<std::size_t>(N));
    sim->theta.resize(static_cast<std::size_t>(N));

    sim->t[0]     = 0.0;
    sim->theta[0] = cfg->theta_start;

    double theta_current = cfg->theta_start;
    double error_sum     = 0.0;
    double error_pre     = 0.0;
    const double dt      = cfg->dt;

    for (int t = 1; t < N; ++t) {
        const double error      = cfg->theta_goal - theta_current;
        error_sum              += error * dt;
        const double error_diff = (error - error_pre) / dt;
        const double m_raw = cfg->kp * error
                           + cfg->ki * error_sum
                           + cfg->kd * error_diff;

        // Output clamp (actuator saturation); 0 = disabled.
        const double m = (cfg->output_clamp > 0.0)
            ? std::max(-cfg->output_clamp, std::min(cfg->output_clamp, m_raw))
            : m_raw;

        theta_current += m;
        theta_current -= cfg->offset;

        // Integral anti-windup; 0 = disabled.
        if (cfg->integral_clamp > 0.0)
            error_sum = std::max(-cfg->integral_clamp,
                                 std::min(cfg->integral_clamp, error_sum));

        error_pre = error;

        sim->t[static_cast<std::size_t>(t)]     = static_cast<double>(t) * dt;
        sim->theta[static_cast<std::size_t>(t)] = theta_current;
    }
    return sim;
}

PID_CORE_API void pid_core_free_simulation(PidSimulation* sim) {
    delete sim;
}

PID_CORE_API int32_t pid_core_sim_length(const PidSimulation* sim) {
    if (!sim) return 0;
    return static_cast<int32_t>(sim->t.size());
}

PID_CORE_API int32_t pid_core_sim_copy_time(
    const PidSimulation* sim, double* buffer, int32_t buffer_len)
{
    if (!sim || !buffer) return 0;
    const int32_t n = static_cast<int32_t>(sim->t.size());
    if (buffer_len < n) return 0;
    for (int32_t i = 0; i < n; ++i) buffer[i] = sim->t[i];
    return n;
}

PID_CORE_API int32_t pid_core_sim_copy_theta(
    const PidSimulation* sim, double* buffer, int32_t buffer_len)
{
    if (!sim || !buffer) return 0;
    const int32_t n = static_cast<int32_t>(sim->theta.size());
    if (buffer_len < n) return 0;
    for (int32_t i = 0; i < n; ++i) buffer[i] = sim->theta[i];
    return n;
}

PID_CORE_API double pid_core_sim_final_theta(const PidSimulation* sim) {
    if (!sim || sim->theta.empty()) return 0.0;
    return sim->theta.back();
}

PID_CORE_API double pid_core_sim_max_theta(const PidSimulation* sim) {
    if (!sim || sim->theta.empty()) return 0.0;
    return *std::max_element(sim->theta.begin(), sim->theta.end());
}

PID_CORE_API double pid_core_sim_min_theta(const PidSimulation* sim) {
    if (!sim || sim->theta.empty()) return 0.0;
    return *std::min_element(sim->theta.begin(), sim->theta.end());
}

}  // extern "C"
