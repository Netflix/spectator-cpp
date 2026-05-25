#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <age_gauge.h>
#include <counter.h>
#include <dist_summary.h>
#include <gauge.h>
#include <max_gauge.h>
#include <monotonic_counter.h>
#include <monotonic_counter_uint.h>
#include <percentile_dist_summary.h>
#include <percentile_timer.h>
#include <registry.h>
#include <timer.h>
#include <config.h>
#include <writer.h>
#include <writer_config.h>

#include <sstream>
#include <memory>

namespace py = pybind11;
using Map = std::unordered_map<std::string, std::string>;

// Helper: expose a heap-allocated meter returned by unique_ptr so Python GC
// manages its lifetime — m_line lives as long as the Python object does.
template <typename M>
std::unique_ptr<M> make_meter(spectator::Registry& r,
                               const std::string& name, const Map& tags)
{
    return std::make_unique<M>(r.CreateNewId(name, tags));
}

PYBIND11_MODULE(spectator_cpp, m)
{
    m.doc() = "spectator-cpp Python bindings";

    // ------------------------------------------------------------------
    // WriterConfig
    // ------------------------------------------------------------------
    py::class_<spectator::WriterConfig>(m, "WriterConfig")
        .def(py::init<const std::string&>(), py::arg("location"))
        .def(py::init<const std::string&, unsigned int>(),
             py::arg("location"), py::arg("buffer_size"));

    // ------------------------------------------------------------------
    // Config
    // ------------------------------------------------------------------
    py::class_<spectator::Config>(m, "Config")
        .def(py::init<const spectator::WriterConfig&, const Map&>(),
             py::arg("writer_config"), py::arg("extra_tags") = Map{});

    // ------------------------------------------------------------------
    // Meter classes — Python holds a unique_ptr so m_line is reused
    // ------------------------------------------------------------------

    py::class_<spectator::Counter, std::unique_ptr<spectator::Counter>>(m, "Counter")
        .def("increment", py::overload_cast<>(&spectator::Counter::Increment, py::const_))
        .def("add", [](spectator::Counter& c, int64_t d) { c.Increment(d); },
             py::arg("delta"));

    py::class_<spectator::Gauge, std::unique_ptr<spectator::Gauge>>(m, "Gauge")
        .def("set", &spectator::Gauge::Set, py::arg("value"));

    py::class_<spectator::MaxGauge, std::unique_ptr<spectator::MaxGauge>>(m, "MaxGauge")
        .def("set", &spectator::MaxGauge::Set, py::arg("value"));

    py::class_<spectator::AgeGauge, std::unique_ptr<spectator::AgeGauge>>(m, "AgeGauge")
        .def("set", &spectator::AgeGauge::Set, py::arg("seconds"))
        .def("now", &spectator::AgeGauge::Now);

    py::class_<spectator::MonotonicCounter, std::unique_ptr<spectator::MonotonicCounter>>(m, "MonotonicCounter")
        .def("set", &spectator::MonotonicCounter::Set, py::arg("amount"));

    py::class_<spectator::MonotonicCounterUint, std::unique_ptr<spectator::MonotonicCounterUint>>(m, "MonotonicCounterUint")
        .def("set", &spectator::MonotonicCounterUint::Set, py::arg("amount"));

    py::class_<spectator::Timer, std::unique_ptr<spectator::Timer>>(m, "Timer")
        .def("record", &spectator::Timer::Record, py::arg("seconds"));

    py::class_<spectator::PercentileTimer, std::unique_ptr<spectator::PercentileTimer>>(m, "PercentileTimer")
        .def("record", &spectator::PercentileTimer::Record, py::arg("seconds"));

    py::class_<spectator::DistributionSummary, std::unique_ptr<spectator::DistributionSummary>>(m, "DistributionSummary")
        .def("record", &spectator::DistributionSummary::Record, py::arg("amount"));

    py::class_<spectator::PercentileDistributionSummary, std::unique_ptr<spectator::PercentileDistributionSummary>>(m, "PercentileDistributionSummary")
        .def("record", &spectator::PercentileDistributionSummary::Record, py::arg("amount"));

    // ------------------------------------------------------------------
    // Registry — factory methods return unique_ptr<Meter> so Python GC
    // keeps each C++ object alive, reusing m_line across calls.
    // ------------------------------------------------------------------
    py::class_<spectator::Registry>(m, "Registry")
        .def(py::init<const spectator::Config&>(), py::arg("config"))

        .def("counter",
             [](spectator::Registry& r, const std::string& name, const Map& tags) {
                 return make_meter<spectator::Counter>(r, name, tags);
             }, py::arg("name"), py::arg("tags") = Map{})

        .def("gauge",
             [](spectator::Registry& r, const std::string& name, const Map& tags) {
                 return make_meter<spectator::Gauge>(r, name, tags);
             }, py::arg("name"), py::arg("tags") = Map{})

        .def("gauge_ttl",
             [](spectator::Registry& r, const std::string& name, int ttl, const Map& tags) {
                 return std::make_unique<spectator::Gauge>(r.CreateNewId(name, tags), ttl);
             }, py::arg("name"), py::arg("ttl_seconds"), py::arg("tags") = Map{})

        .def("max_gauge",
             [](spectator::Registry& r, const std::string& name, const Map& tags) {
                 return make_meter<spectator::MaxGauge>(r, name, tags);
             }, py::arg("name"), py::arg("tags") = Map{})

        .def("age_gauge",
             [](spectator::Registry& r, const std::string& name, const Map& tags) {
                 return make_meter<spectator::AgeGauge>(r, name, tags);
             }, py::arg("name"), py::arg("tags") = Map{})

        .def("monotonic_counter",
             [](spectator::Registry& r, const std::string& name, const Map& tags) {
                 return make_meter<spectator::MonotonicCounter>(r, name, tags);
             }, py::arg("name"), py::arg("tags") = Map{})

        .def("monotonic_counter_uint",
             [](spectator::Registry& r, const std::string& name, const Map& tags) {
                 return make_meter<spectator::MonotonicCounterUint>(r, name, tags);
             }, py::arg("name"), py::arg("tags") = Map{})

        .def("timer",
             [](spectator::Registry& r, const std::string& name, const Map& tags) {
                 return make_meter<spectator::Timer>(r, name, tags);
             }, py::arg("name"), py::arg("tags") = Map{})

        .def("pct_timer",
             [](spectator::Registry& r, const std::string& name, const Map& tags) {
                 return make_meter<spectator::PercentileTimer>(r, name, tags);
             }, py::arg("name"), py::arg("tags") = Map{})

        .def("dist_summary",
             [](spectator::Registry& r, const std::string& name, const Map& tags) {
                 return make_meter<spectator::DistributionSummary>(r, name, tags);
             }, py::arg("name"), py::arg("tags") = Map{})

        .def("pct_dist_summary",
             [](spectator::Registry& r, const std::string& name, const Map& tags) {
                 return make_meter<spectator::PercentileDistributionSummary>(r, name, tags);
             }, py::arg("name"), py::arg("tags") = Map{})

        .def("dump_lines",
             [](spectator::Registry&) {
                 std::string dump = spectator::Writer::DumpMemory();
                 std::vector<std::string> lines;
                 std::istringstream ss(dump);
                 std::string line;
                 while (std::getline(ss, line))
                     if (!line.empty()) lines.push_back(line);
                 return lines;
             });
}
