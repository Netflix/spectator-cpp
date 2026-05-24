#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <registry.h>
#include <config.h>
#include <writer.h>
#include <writer_config.h>

#include <sstream>

namespace py = pybind11;
using Map = std::unordered_map<std::string, std::string>;

PYBIND11_MODULE(spectator_cpp, m)
{
    m.doc() = "spectator-cpp Python bindings — thin wrapper over the C++ spectator library";

    // ------------------------------------------------------------------
    // WriterConfig
    // ------------------------------------------------------------------
    py::class_<spectator::WriterConfig>(m, "WriterConfig",
        "Configures how metrics are sent to spectatord.\n\n"
        "location values: 'udp', 'udp://host:port', 'unix', 'unix:///path',\n"
        "                 'memory', 'noop'")
        .def(py::init<const std::string&>(), py::arg("location"),
             "Create an unbuffered WriterConfig.")
        .def(py::init<const std::string&, unsigned int>(),
             py::arg("location"), py::arg("buffer_size"),
             "Create a buffered WriterConfig. buffer_size bytes are accumulated\n"
             "before a single socket send. Use 60*1024 for high throughput.");

    // ------------------------------------------------------------------
    // Config
    // ------------------------------------------------------------------
    py::class_<spectator::Config>(m, "Config",
        "Registry configuration combining writer settings with optional common tags.")
        .def(py::init<const spectator::WriterConfig&, const Map&>(),
             py::arg("writer_config"),
             py::arg("extra_tags") = Map{},
             "Create a Config. extra_tags are merged into every metric.");

    // ------------------------------------------------------------------
    // Registry
    // ------------------------------------------------------------------
    py::class_<spectator::Registry>(m, "Registry",
        "Main entry point. Create one per application, then call the meter\n"
        "methods (counter_increment, gauge_set, timer_record, etc.) to record metrics.")
        .def(py::init<const spectator::Config&>(), py::arg("config"))

        // Counter
        .def("counter_increment",
             [](spectator::Registry& r, const std::string& name, const Map& tags) {
                 r.CreateCounter(name, tags).Increment();
             },
             py::arg("name"), py::arg("tags") = Map{},
             "Increment a counter by 1.")

        .def("counter_add",
             [](spectator::Registry& r, const std::string& name, int64_t delta, const Map& tags) {
                 r.CreateCounter(name, tags).Increment(delta);
             },
             py::arg("name"), py::arg("delta"), py::arg("tags") = Map{},
             "Increment a counter by delta (must be > 0).")

        // Gauge
        .def("gauge_set",
             [](spectator::Registry& r, const std::string& name, double value, const Map& tags) {
                 r.CreateGauge(name, tags).Set(value);
             },
             py::arg("name"), py::arg("value"), py::arg("tags") = Map{},
             "Set a gauge to the given value.")

        .def("gauge_set_ttl",
             [](spectator::Registry& r, const std::string& name, double value,
                int ttl_seconds, const Map& tags) {
                 r.CreateGauge(name, tags, ttl_seconds).Set(value);
             },
             py::arg("name"), py::arg("value"), py::arg("ttl_seconds"),
             py::arg("tags") = Map{},
             "Set a gauge with an expiry TTL in seconds.")

        // MaxGauge
        .def("max_gauge_set",
             [](spectator::Registry& r, const std::string& name, double value, const Map& tags) {
                 r.CreateMaxGauge(name, tags).Set(value);
             },
             py::arg("name"), py::arg("value"), py::arg("tags") = Map{},
             "Update a max-gauge (tracks the maximum value seen).")

        // AgeGauge
        .def("age_gauge_set",
             [](spectator::Registry& r, const std::string& name, double seconds, const Map& tags) {
                 r.CreateAgeGauge(name, tags).Set(seconds);
             },
             py::arg("name"), py::arg("seconds"), py::arg("tags") = Map{},
             "Set an age-gauge to the given number of seconds since the last event.")

        .def("age_gauge_now",
             [](spectator::Registry& r, const std::string& name, const Map& tags) {
                 r.CreateAgeGauge(name, tags).Now();
             },
             py::arg("name"), py::arg("tags") = Map{},
             "Record the current time as the most recent event for this age-gauge.")

        // MonotonicCounter
        .def("monotonic_counter_set",
             [](spectator::Registry& r, const std::string& name, double amount, const Map& tags) {
                 r.CreateMonotonicCounter(name, tags).Set(amount);
             },
             py::arg("name"), py::arg("amount"), py::arg("tags") = Map{},
             "Update a monotonically-increasing counter (rate derived by spectatord).")

        .def("monotonic_counter_uint_set",
             [](spectator::Registry& r, const std::string& name, uint64_t amount, const Map& tags) {
                 r.CreateMonotonicCounterUint(name, tags).Set(amount);
             },
             py::arg("name"), py::arg("amount"), py::arg("tags") = Map{},
             "Update a uint64 monotonically-increasing counter.")

        // Timer
        .def("timer_record",
             [](spectator::Registry& r, const std::string& name, double seconds, const Map& tags) {
                 r.CreateTimer(name, tags).Record(seconds);
             },
             py::arg("name"), py::arg("seconds"), py::arg("tags") = Map{},
             "Record a duration in seconds.")

        // PercentileTimer
        .def("pct_timer_record",
             [](spectator::Registry& r, const std::string& name, double seconds, const Map& tags) {
                 r.CreatePercentTimer(name, tags).Record(seconds);
             },
             py::arg("name"), py::arg("seconds"), py::arg("tags") = Map{},
             "Record a duration in seconds with percentile bucketing.")

        // DistributionSummary
        .def("dist_record",
             [](spectator::Registry& r, const std::string& name, int64_t amount, const Map& tags) {
                 r.CreateDistributionSummary(name, tags).Record(amount);
             },
             py::arg("name"), py::arg("amount"), py::arg("tags") = Map{},
             "Record a size or amount as a distribution summary.")

        // PercentileDistributionSummary
        .def("pct_dist_record",
             [](spectator::Registry& r, const std::string& name, int64_t amount, const Map& tags) {
                 r.CreatePercentDistributionSummary(name, tags).Record(amount);
             },
             py::arg("name"), py::arg("amount"), py::arg("tags") = Map{},
             "Record a size or amount with percentile bucketing.")

        // Memory writer utility
        .def("dump_lines",
             [](spectator::Registry&) {
                 std::string dump = spectator::Writer::DumpMemory();
                 std::vector<std::string> lines;
                 std::istringstream ss(dump);
                 std::string line;
                 while (std::getline(ss, line))
                     if (!line.empty()) lines.push_back(line);
                 return lines;
             },
             "Return all lines written to the memory writer as a list of strings.\n"
             "Only meaningful when the registry was created with WriterConfig('memory').");
}
