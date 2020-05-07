#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <tsl/hopscotch_set.h>
#include "config.h"
#include "counter.h"
#include "logger.h"
#include "http_client.h"
#include "measurement.h"

namespace spectator {

namespace {
template <typename R>
std::shared_ptr<Counter> get_counter(R* registry, Tags tags) {
  static constexpr auto kSpectatorMeasurements = "spectator.measurements";
  return registry->GetCounter(
      registry->CreateId(kSpectatorMeasurements, std::move(tags)));
}
}  // namespace

template <typename R>
class Publisher {
 public:
  explicit Publisher(R* registry)
      : registry_{registry},
        started_{false},
        should_stop_{false},
        sentMetrics_{get_counter(registry, Tags{{"id", "sent"}})},
        invalidMetrics_{get_counter(
            registry, Tags{{"id", "dropped"}, {"error", "validation"}})},
        droppedHttp_{get_counter(
            registry, Tags{{"id", "dropped"}, {"error", "http-error"}})},
        droppedOther_{get_counter(
            registry, Tags{{"id", "dropped"}, {"error", "other"}})} {}
  Publisher(const Publisher&) = delete;
  ~Publisher() {
    if (started_) {
      Stop();
    }
  }
  void Start() {
    if (!http_initialized_) {
      http_initialized_ = true;
      HttpClient::GlobalInit();
    }
    const auto& cfg = registry_->GetConfig();
    auto logger = registry_->GetLogger();
    if (cfg.uri.empty()) {
      logger->warn("Registry config has no uri. Ignoring start request");
      return;
    }
    if (started_.exchange(true)) {
      logger->warn("Registry already started. Ignoring start request");

      return;
    }

    sender_thread_ = std::thread(&Publisher::sender, this);
  }

  void Stop() {
    if (started_.exchange(false)) {
      should_stop_ = true;
      cv_.notify_all();
      sender_thread_.join();
    }

    if (http_initialized_.exchange(false)) {
      HttpClient::GlobalShutdown();
    }
  }

 private:
  R* registry_;
  std::atomic<bool> started_;
  std::atomic<bool> http_initialized_{false};
  std::atomic<bool> should_stop_;
  std::mutex cv_mutex_;
  std::condition_variable cv_;
  std::thread sender_thread_;
  std::shared_ptr<Counter> sentMetrics_;
  std::shared_ptr<Counter> invalidMetrics_;
  std::shared_ptr<Counter> droppedHttp_;
  std::shared_ptr<Counter> droppedOther_;

  void sender() noexcept {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    const auto& cfg = registry_->GetConfig();

    auto freq_millis = cfg.frequency.count();
    auto logger = registry_->GetLogger();
    if (freq_millis < 1000) {
      logger->error(
          "Frequency should be expressed in milliseconds. Got {} which is too "
          "low",
          freq_millis);
      exit(1);
    }

    logger->info("Starting to send metrics to {} every {}s.", cfg.uri,
                 cfg.frequency.count() / 1000);

    while (!should_stop_) {
      auto start = R::clock::now();
      size_t sent = 0u, err = 0u;
      try {
        std::tie(sent, err) = send_metrics();
      } catch (std::exception& e) {
        logger->error("Ignoring exception while sending metrics: {}", e.what());
      }
      auto elapsed = R::clock::now() - start;
      auto millis = duration_cast<milliseconds>(elapsed).count();
      logger->debug("Sent {} metrics in {} ms ({} errors)", sent, millis, err);

      if (millis < freq_millis) {
        std::unique_lock<std::mutex> lock{cv_mutex_};
        auto sleep = cfg.frequency - elapsed;
        logger->debug("Sleeping {}ms until the next interval",
                      freq_millis - millis);
        cv_.wait_for(lock, sleep);
      }
    }
    logger->info("Stopping Publisher");
  }

  // for testing
 protected:
  using StrTable = ska::flat_hash_map<std::string, int>;
  StrTable build_str_table(rapidjson::Document* payload,
                           std::vector<Measurement>::const_iterator first,
                           std::vector<Measurement>::const_iterator last) {
    StrTable strings;
    const auto& common_tags = registry_->GetConfig().common_tags;
    for (const auto& tag : common_tags) {
      strings[tag.first] = 0;
      strings[tag.second] = 0;
    }
    strings["name"] = 0;
    for (auto it = first; it != last; ++it) {
      const auto& m = *it;
      strings[m.id->Name()] = 0;
      for (const auto& tag : m.id->GetTags()) {
        strings[tag.first] = 0;
        strings[tag.second] = 0;
      }
    }
    std::vector<std::string> sorted_strings;
    sorted_strings.reserve(strings.size());
    for (const auto& s : strings) {
      sorted_strings.emplace_back(s.first);
    }
    std::sort(sorted_strings.begin(), sorted_strings.end());
    for (auto i = 0u; i < sorted_strings.size(); ++i) {
      strings[sorted_strings[i]] = i;
    }
    auto& alloc = payload->GetAllocator();
    payload->PushBack(static_cast<int64_t>(strings.size()), alloc);
    for (const auto& s : sorted_strings) {
      rapidjson::Value str(s.c_str(),
                           static_cast<rapidjson::SizeType>(s.length()), alloc);
      payload->PushBack(str, alloc);
    }
    return strings;
  }

  enum class Op { Add = 0, Max = 10 };

  Op op_from_tags(const Tags& tags) {
    auto stat = tags.at("statistic");
    if (stat == "count" || stat == "totalAmount" || stat == "totalTime" ||
        stat == "totalOfSquares" || stat == "percentile") {
      return Op::Add;
    }
    return Op::Max;
  }

  template <typename T>
  void add_tags(rapidjson::Document* payload, const StrTable& strings,
                const T& tags) {
    using rapidjson::Value;
    auto& alloc = payload->GetAllocator();
    for (const auto& tag : tags) {
      auto k_pair = strings.find(tag.first);
      auto v_pair = strings.find(tag.second);
      assert(k_pair != strings.end());
      assert(v_pair != strings.end());
      payload->PushBack(k_pair->second, alloc);
      payload->PushBack(v_pair->second, alloc);
    }
  }

  void append_measurement(rapidjson::Document* payload, const StrTable& strings,
                          const std::vector<int>& common_ids,
                          const Measurement& m) {
    auto& alloc = payload->GetAllocator();
    auto op = op_from_tags(m.id->GetTags());
    auto common_tags_size = common_ids.size() / 2;
    int64_t total_tags = m.id->GetTags().size() + 1 + common_tags_size;
    payload->PushBack(total_tags, alloc);
    for (auto i : common_ids) {
      payload->PushBack(i, alloc);
    }
    add_tags(payload, strings, m.id->GetTags());
    auto name_idx = strings.find("name")->second;
    auto name_value_idx = strings.find(m.id->Name())->second;
    payload->PushBack(name_idx, alloc);
    payload->PushBack(name_value_idx, alloc);
    auto op_num = static_cast<int>(op);
    payload->PushBack(op_num, alloc);
    payload->PushBack(m.value, alloc);
  }

  std::vector<int> get_common_ids(const StrTable& strings) {
    auto tags = registry_->GetConfig().common_tags;
    std::vector<int> ids;
    ids.reserve(tags.size() * 2);
    for (const auto& tag : tags) {
      auto key_pair = strings.find(tag.first);
      auto val_pair = strings.find(tag.second);
      assert(key_pair != strings.end());
      assert(val_pair != strings.end());
      ids.emplace_back(key_pair->second);
      ids.emplace_back(val_pair->second);
    }
    return ids;
  }

  rapidjson::Document measurements_to_json(
      std::vector<Measurement>::const_iterator first,
      std::vector<Measurement>::const_iterator last) {
    using rapidjson::Document;
    using rapidjson::kArrayType;
    Document payload{kArrayType};

    auto strings = build_str_table(&payload, first, last);
    auto common_ids = get_common_ids(strings);
    for (auto it = first; it != last; ++it) {
      append_measurement(&payload, strings, common_ids, *it);
    }
    return payload;
  }

  static HttpClientConfig get_http_config(const Config& cfg) {
    auto read_timeout = cfg.read_timeout;
    auto connect_timeout = cfg.connect_timeout;
    if (read_timeout.count() == 0) {
      read_timeout = std::chrono::seconds(2);
    }
    if (connect_timeout.count() == 0) {
      connect_timeout = std::chrono::seconds(2);
    }
    return HttpClientConfig{connect_timeout, read_timeout, true, false, true};
  }

  std::pair<size_t, size_t> send_metrics() {
    const auto& cfg = registry_->GetConfig();
    auto http_cfg = get_http_config(cfg);
    HttpClient client{registry_, std::move(http_cfg)};
    auto batch_size =
        static_cast<std::vector<Measurement>::difference_type>(cfg.batch_size);
    auto measurements = registry_->Measurements();
    if (!cfg.is_enabled()) {
      return std::make_pair(0, 0);
    }

    auto from = measurements.begin();
    auto end = measurements.end();
    auto num_err = 0u;
    auto num_sent = 0u;
    auto logger = registry_->GetLogger();
    tsl::hopscotch_set<std::string> err_messages;
    while (from != end) {
      auto to_end = std::distance(from, end);
      auto to_advance = std::min(batch_size, to_end);
      auto to = from;
      std::advance(to, to_advance);
      auto http_response = client.Post(cfg.uri, measurements_to_json(from, to));
      auto http_code = http_response.status;
      if (http_code == 200) {
        num_sent += to_advance;
      } else if (http_code > 200 && http_code < 500) {
        rapidjson::Document body;
        body.Parse(http_response.raw_body.c_str(),
                   http_response.raw_body.size());
        if (body.HasParseError()) {
          logger->error("Unable to parse JSON response from {} - status {}",
                        cfg.uri, http_code);
          num_err += to_advance;
          droppedOther_->Add(to_advance);
        } else {
          if (body.HasMember("errorCount")) {
            auto err_count = body["errorCount"].GetInt();
            num_err += err_count;
            num_sent += to_advance - err_count;
            invalidMetrics_->Add(err_count);
            sentMetrics_->Add(to_advance - err_count);
            auto messages = body["message"].GetArray();
            for (auto& msg : messages) {
              err_messages.insert(
                  std::string(msg.GetString(), msg.GetStringLength()));
            }
          } else {
            logger->error(
                "Missing errorCount field in JSON response from {}: {}",
                cfg.uri, http_code);
            droppedOther_->Add(to_advance);
            num_err += to_advance;
          }
        }
      } else {
        droppedHttp_->Add(to_advance);
        num_err += to_advance;
      }
      from = to;
    }
    if (num_err > 0) {
      logger->info("Sent {}/{} - Validation failures: {}", num_sent,
                   measurements.size(), num_err);
    } else {
      logger->debug("Sent {} measurements with no errors", measurements.size());
    }
    for (const auto& m : err_messages) {
      logger->info("Validation error: {}", m);
    }
    return std::make_pair(num_sent, num_err);
  }
};

}  // namespace spectator
