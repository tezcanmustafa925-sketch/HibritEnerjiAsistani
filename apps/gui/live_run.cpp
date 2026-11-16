#include "live_run.h"

#include <algorithm>
#include <exception>
#include <utility>

namespace gui {

namespace {

// Kayan nokta birikimi (ör. 60 kez 1/60 s) bir adımı 1e-12 farkla kaçırmasın
constexpr double kTimeTolerance_s = 1e-9;

const hea::DriveCycle& emptyCycle()
{
    static const hea::DriveCycle c;
    return c;
}

const RunResult& emptyResult()
{
    static const RunResult r;
    return r;
}

} // namespace

struct LiveRun::State {
    hea::DriveCycle cycle;
    std::unique_ptr<hea::Strategy> strategy;
    std::unique_ptr<hea::Simulation> sim; // cycle ve strategy'ye referans tutar
    RunResult result;
    double pending_s = 0.0; // henüz adıma dönüşmemiş simülasyon süresi
    std::string error;
};

LiveRun::LiveRun() = default;
LiveRun::~LiveRun() = default;
LiveRun::LiveRun(LiveRun&&) noexcept = default;
LiveRun& LiveRun::operator=(LiveRun&&) noexcept = default;

std::string LiveRun::start(const Session& session, const RunSettings& settings)
{
    auto s = std::make_unique<State>();
    const std::string err = session.prepare(settings, s->cycle, s->strategy);
    if (!err.empty()) {
        return err;
    }
    try {
        s->sim = std::make_unique<hea::Simulation>(session.config().sim, s->cycle, *s->strategy);
    } catch (const std::exception& e) {
        return e.what();
    }
    s->result.cycleName = s->cycle.name;
    s->result.strategyName = s->strategy->name();
    s->result.initialSoc = session.config().sim.initialSoc;
    s->result.steps.reserve(s->cycle.samples.size());
    s->result.summary = hea::summarize(s->result.steps, s->result.initialSoc);
    state_ = std::move(s);
    return {};
}

bool LiveRun::finished() const
{
    return state_ != nullptr && (state_->sim->finished() || !state_->error.empty());
}

const std::string& LiveRun::error() const
{
    static const std::string none;
    return state_ != nullptr ? state_->error : none;
}

std::size_t LiveRun::advance(std::size_t maxSteps)
{
    if (state_ == nullptr || finished()) return 0;
    State& s = *state_;
    std::size_t n = 0;
    try {
        while (n < maxSteps && !s.sim->finished()) {
            s.result.steps.push_back(s.sim->step());
            ++n;
        }
    } catch (const std::exception& e) {
        s.error = e.what();
    }
    if (n > 0) {
        s.result.summary = hea::summarize(s.result.steps, s.result.initialSoc);
    }
    if (finished()) s.pending_s = 0.0;
    return n;
}

std::size_t LiveRun::advanceRealTime(double realSeconds, double speed)
{
    if (state_ == nullptr || finished()) return 0;
    if (!(realSeconds > 0.0) || !(speed > 0.0)) return 0; // NaN de buraya düşer
    State& s = *state_;
    s.pending_s += std::min(realSeconds, kMaxFrameSeconds) * speed;
    // Biriken süreye sığan adım sayısı (Simulation'ın sıradaki örneği = atılan adım + 1)
    std::size_t k = 0;
    for (std::size_t i = s.result.steps.size() + 1; i < s.cycle.samples.size(); ++i) {
        const double dt = s.cycle.samples[i].time_s - s.cycle.samples[i - 1].time_s;
        if (dt > s.pending_s + kTimeTolerance_s) break;
        s.pending_s -= dt;
        ++k;
    }
    if (s.pending_s < 0.0) s.pending_s = 0.0; // tolerans payı eksiye düşürmesin
    return advance(k);
}

std::size_t LiveRun::stepCount() const
{
    return state_ != nullptr ? state_->result.steps.size() : 0;
}

std::size_t LiveRun::totalSteps() const
{
    if (state_ == nullptr || state_->cycle.samples.empty()) return 0;
    return state_->cycle.samples.size() - 1;
}

double LiveRun::time_s() const
{
    if (state_ == nullptr) return 0.0;
    if (!state_->result.steps.empty()) return state_->result.steps.back().time_s;
    return startTime_s();
}

double LiveRun::startTime_s() const
{
    return state_ != nullptr && !state_->cycle.samples.empty() ? state_->cycle.samples.front().time_s
                                                                : 0.0;
}

double LiveRun::endTime_s() const
{
    return state_ != nullptr && !state_->cycle.samples.empty() ? state_->cycle.samples.back().time_s
                                                                : 0.0;
}

const hea::DriveCycle& LiveRun::cycle() const
{
    return state_ != nullptr ? state_->cycle : emptyCycle();
}

const RunResult& LiveRun::result() const
{
    return state_ != nullptr ? state_->result : emptyResult();
}

} // namespace gui
