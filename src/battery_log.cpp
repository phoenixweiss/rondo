// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#include "battery_log.hpp"
#include <fstream>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <ctime>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <cctype>
#ifdef __linux__
#include <sys/klog.h>
#endif
namespace rondo {
static Json raw(const fs::path &path, Json &errors, const std::string &key) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    std::error_code error;
    if (fs::exists(path, error) || error) errors[key] = "unreadable";
    return nullptr;
  }
  char buffer[513];
  input.read(buffer, sizeof(buffer));
  auto count = input.gcount();
  if (count > 512) { errors[key] = "too_large"; return nullptr; }
  if (input.bad()) { errors[key] = "read_error"; return nullptr; }
  std::string text(buffer, size_t(count));
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == '\0'))
    text.pop_back();
  return text;
}
Json batterySnapshot(const fs::path &sys, const fs::path &proc) {
  Json result = {{"supplies", Json::object()}, {"errors", Json::object()}};
  auto &errors = result["errors"];
  result["boot_id"] = raw(proc / "sys/kernel/random/boot_id", errors, "boot_id");
  result["kernel_release"] = raw(proc / "sys/kernel/osrelease", errors, "kernel_release");
  result["uptime_raw"] = raw(proc / "uptime", errors, "uptime_raw");
  result["model"] = raw(sys / "firmware/devicetree/base/model", errors, "model");
  std::error_code error;
  auto root = sys / "class/power_supply";
  // This is console battery diagnostics, not a peripheral battery monitor.
  // Reading HID capacity can issue a synchronous Bluetooth GET_REPORT and wait
  // seconds in the kernel. Never open unknown power-supply nodes, even for type.
  result["supply_scope"] = "h700-pmic-only";
  if (!fs::is_directory(root,error)) {
    errors["power_supply"] = "unavailable";
    return result;
  }
  for (const char *supply : {"axp2202-battery", "axp2202-usb"}) {
    const auto path=root/supply;error.clear();
    if (!fs::is_directory(path,error)) continue;
    const std::string name=supply;
    Json fields = Json::object();
    for (const char *field : {"type", "present", "online", "status", "health", "capacity",
         "capacity_level", "voltage_now", "voltage_avg", "voltage_min_design", "voltage_max_design",
         "current_now", "current_avg", "power_now", "charge_now", "charge_full", "charge_full_design",
         "charge_counter", "energy_now", "energy_full", "energy_full_design", "cycle_count", "temp", "temp_ambient",
         "time_to_full_now", "time_to_empty_now", "technology", "manufacturer", "model_name",
         "constant_charge_current", "constant_charge_current_max", "constant_charge_voltage",
         "constant_charge_voltage_max", "input_current_limit", "input_voltage_limit", "usb_type"})
      fields[field] = raw(path / field, errors, name + "/" + field);
    auto driver = fs::read_symlink(path / "device/driver", error);
    fields["driver"] = error ? Json(nullptr) : Json(driver.filename().string());
    error.clear();
    result["supplies"][name] = std::move(fields);
  }
  return result;
}
// Device-tree properties are binary, big-endian and often contain NULs. Never
// pass them through the text reader or dump the entire tree (which has serials).
static Json binaryProperty(const fs::path &path, Json &errors, const std::string &key) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    std::error_code error;
    if (fs::exists(path, error) || error) errors[key] = "unreadable";
    return nullptr;
  }
  std::vector<uint8_t> bytes(4097);
  input.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
  auto count = input.gcount();
  if (count > 4096) { errors[key] = "too_large"; return nullptr; }
  if (input.bad()) { errors[key] = "read_error"; return nullptr; }
  bytes.resize(size_t(count));
  static const char hex[] = "0123456789abcdef";
  std::string encoded;
  for (auto byte : bytes) { encoded += hex[byte >> 4]; encoded += hex[byte & 15]; }
  Json result = {{"bytes", bytes.size()}, {"hex", encoded}, {"sha256", hashBytes(bytes)}};
  if (bytes.size() == 4)
    result["u32"] = (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) |
                    (uint32_t(bytes[2]) << 8) | uint32_t(bytes[3]);
  return result;
}
Json batteryPowerMessages(const std::string &text) {
  Json lines = Json::array();
  std::istringstream input(text); std::string line;
  while (std::getline(input, line)) {
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    // Avoid collecting unrelated kernel/network messages.
    if (lower.find("axp2202") == std::string::npos &&
        lower.find("model_update") == std::string::npos &&
        lower.find("battery model") == std::string::npos &&
        lower.find("curve_check") == std::string::npos &&
        lower.find("gauge_soc") == std::string::npos) continue;
    if (lines.size() == 64) lines.erase(lines.begin());
    lines.push_back(line.substr(0, 512));
  }
  return lines;
}
static Json powerKernelSnapshot(const fs::path &proc) {
  Json result = {{"lines", Json::array()}, {"available", false}};
#ifdef __linux__
  if (proc == "/proc") {
    std::vector<char> buffer(256 * 1024);
    // SYSLOG_ACTION_READ_ALL, not READ_CLEAR. Never open /dev/kmsg or PMIC.
    int size = klogctl(3, buffer.data(), int(buffer.size()));
    if (size < 0) result["errno"] = errno;
    else { result["available"] = true; result["lines"] = batteryPowerMessages(std::string(buffer.data(), size)); }
    return result;
  }
#else
  (void)proc;
#endif
  result["reason"] = "unsupported_or_isolated";
  return result;
}
Json batteryPassport(const fs::path &sys, const fs::path &proc) {
  Json result = {{"schema", 1}, {"read_only", true}, {"pmic_model_read", false},
      {"errors", Json::object()}, {"battery_nodes", Json::array()}};
  auto &errors = result["errors"];
  result["kernel_release"] = raw(proc / "sys/kernel/osrelease", errors, "kernel_release");
  result["kernel_build"] = raw(proc / "sys/kernel/version", errors, "kernel_build");
  result["kernel_tainted"] = raw(proc / "sys/kernel/tainted", errors, "kernel_tainted");
  result["power_kernel"] = powerKernelSnapshot(proc);
  auto supply = sys / "class/power_supply/axp2202-battery";
  std::error_code driverError;
  auto driver = fs::read_symlink(supply / "device/driver", driverError);
  result["driver"] = driverError ? Json(nullptr) : Json(driver.filename().string());
  driverError.clear();
  auto module = fs::read_symlink(supply / "device/driver/module", driverError);
  result["module"] = driverError ? Json(nullptr) : Json(module.filename().string());
  result["module_version"] = raw(supply / "device/driver/module/version", errors, "module_version");
  result["module_srcversion"] = raw(supply / "device/driver/module/srcversion", errors, "module_srcversion");
  // These identifiers cannot establish an exact source revision on their own.
  result["exact_driver_source_verified"] = false;
  auto tree = sys / "firmware/devicetree/base";
  result["device_model"] = raw(tree / "model", errors, "device_model");
  // Bounded discovery supports node/unit-address changes without hardcoding one
  // packaged DTB path. Do not follow symlinks or serialize arbitrary properties.
  std::vector<fs::path> nodes;
  std::error_code error;
  fs::recursive_directory_iterator it(tree, fs::directory_options::skip_permission_denied, error), end;
  if (error) errors["device_tree"] = "unavailable";
  size_t scanned = 0;
  while (it != end && !error && scanned < 8192) {
    ++scanned;
    if (it.depth() >= 20) it.disable_recursion_pending();
    if (!it->is_symlink(error) && it->is_directory(error)) nodes.push_back(it->path());
    it.increment(error);
  }
  if (error) errors["device_tree_scan"] = "read_error";
  if (it != end && scanned == 8192) errors["device_tree_scan"] = "entry_limit";
  std::sort(nodes.begin(), nodes.end());
  for (const auto &node : nodes) {
    auto compatible = raw(node / "compatible", errors, "compatible_probe");
    if (!compatible.is_string()) continue;
    const auto value = compatible.get<std::string>();
    // Match a whole NUL-delimited compatible entry, not an arbitrary substring.
    const std::string wanted = "x-powers,axp2202-bat-power-supply";
    bool match = false;
    for (size_t at = 0; at < value.size();) {
      auto to = value.find('\0', at);
      if (value.substr(at, to == std::string::npos ? to : to - at) == wanted) match = true;
      if (to == std::string::npos) break;
      at = to + 1;
    }
    if (!match) continue;
    if (result["battery_nodes"].size() == 4) { errors["battery_nodes"] = "node_limit"; break; }
    Json entry = {{"path", node.lexically_relative(tree).generic_string()}, {"compatible", compatible},
        {"properties", Json::object()}, {"selected_model", nullptr}};
    for (const char *field : {"status", "pmu_battery_cap", "pmu_battery_rdc", "pmu_init_chgvol",
         "pmu_runtime_chgcur", "pmu_suspend_chgcur", "pmu_shutdown_chgcur",
         "pmu_battery_warning_level1", "pmu_battery_warning_level2", "param"})
      entry["properties"][field] = binaryProperty(node / field, errors, entry["path"].get<std::string>() + "/" + field);
    auto param = entry["properties"]["param"];
    if (param.is_object() && param.contains("u32")) {
      std::vector<fs::path> targets;
      for (const auto &candidate : nodes) {
        auto phandle = binaryProperty(candidate / "phandle", errors, "phandle_probe");
        if (phandle.is_null()) phandle = binaryProperty(candidate / "linux,phandle", errors, "phandle_probe");
        if (phandle.is_object() && phandle.value("u32", Json(nullptr)) == param["u32"]) targets.push_back(candidate);
      }
      if (targets.size() != 1) errors["model_reference"] = targets.empty() ? "missing" : "ambiguous";
      else {
        const auto &target = targets.front();
        auto select = raw(target / "select", errors, "model_select");
        Json model = {{"node", target.lexically_relative(tree).generic_string()}, {"select", select}, {"parameter", nullptr}};
        if (select.is_string()) {
          auto name = select.get<std::string>();
          bool safe = !name.empty() && name.size() <= 64 && name != "." && name != ".." &&
              std::all_of(name.begin(), name.end(), [](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
              });
          std::error_code linkError;
          if (safe && !fs::is_symlink(target / name, linkError) &&
              !fs::is_symlink(target / name / "parameter", linkError))
            model["parameter"] = binaryProperty(target / name / "parameter", errors, "model_parameter");
          else errors["model_select"] = "unsafe_name_or_symlink";
        }
        entry["selected_model"] = std::move(model);
      }
    }
    result["battery_nodes"].push_back(std::move(entry));
  }
  if (result["battery_nodes"].empty()) errors["battery_node"] = "not_found";
  return result;
}
static Json storageSnapshot(const fs::path &directory, const fs::path &proc) {
  Json result = Json::object();
  auto path = directory;
  std::error_code error;
  while (!path.empty() && !fs::is_directory(path, error)) { path = path.parent_path(); error.clear(); }
  struct statvfs status {};
  if (!path.empty() && statvfs(path.c_str(), &status) == 0) {
    result["read_only"] = bool(status.f_flag & ST_RDONLY);
    result["available_bytes"] = uint64_t(status.f_bavail) * status.f_frsize;
  } else result["statvfs_errno"] = errno;
  result["power_kernel"] = powerKernelSnapshot(proc);
  result["fat_mounts"] = Json::array();
  std::ifstream mounts(proc / "mounts");
  std::string line;
  for (int n = 0; n < 256 && std::getline(mounts, line); ++n) {
    std::istringstream row(line); std::string device, mount, type, flags;
    if (row >> device >> mount >> type >> flags && (type == "vfat" || type == "exfat"))
      result["fat_mounts"].push_back({{"type", type}, {"options", flags.substr(0, 512)}});
  }
#ifdef __linux__
  if (proc == "/proc") {
    std::vector<char> buffer(256 * 1024);
    int size = klogctl(3, buffer.data(), int(buffer.size()));
    if (size < 0) result["kernel_errno"] = errno;
    else {
      std::istringstream input(std::string(buffer.data(), size));
      std::vector<std::string> matches;
      while (std::getline(input, line))
        if (line.find("FAT-fs") != std::string::npos || line.find("mmcblk") != std::string::npos ||
            line.find("I/O error") != std::string::npos || line.find("read-only") != std::string::npos) {
          if (matches.size() == 32) matches.erase(matches.begin());
          matches.push_back(line.substr(0, 512));
        }
      result["kernel_storage_tail"] = matches;
    }
  }
#endif
  return result;
}
struct Descriptor {
  int fd;
  ~Descriptor() { if (fd >= 0) close(fd); }
};
static void failed(const char *operation, const fs::path &path, int code) {
  throw fs::filesystem_error(operation, path, std::error_code(code, std::generic_category()));
}
static void appendJournal(const fs::path &directory, const std::string &role,
                          const std::string &line, size_t limit) {
  if (line.size() > limit) failed("record_size", directory, EFBIG);
  fs::create_directories(directory);
  auto lockPath = directory / (role + ".lock");
  Descriptor lock{open(lockPath.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600)};
  if (lock.fd < 0) failed("lock_open", lockPath, errno);
  if (flock(lock.fd, LOCK_EX | LOCK_NB)) failed("lock", lockPath, errno);
  auto path = directory / (role + ".jsonl"), previous = directory / (role + ".previous.jsonl");
  std::error_code error;
  auto size = fs::file_size(path, error);
  if (error && error != std::errc::no_such_file_or_directory) throw fs::filesystem_error("size", path, error);
  if (!error && size + line.size() + 1 > limit) {
    fs::remove(previous);
    fs::rename(path, previous);
  }
  Descriptor output{open(path.c_str(), O_CREAT | O_RDWR | O_APPEND | O_CLOEXEC, 0600)};
  if (output.fd < 0) failed("journal_open", path, errno);
  struct stat status {};
  if (fstat(output.fd, &status)) failed("journal_stat", path, errno);
  std::string bytes = line;
  char tail = '\n';
  if (status.st_size > 0) {
    if (pread(output.fd, &tail, 1, status.st_size - 1) != 1) failed("journal_tail", path, errno ? errno : EIO);
    if (tail != '\n') bytes.insert(bytes.begin(), '\n');
  }
  size_t offset = 0;
  while (offset < bytes.size()) {
    auto count = write(output.fd, bytes.data() + offset, bytes.size() - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) failed("journal_write", path, errno ? errno : EIO);
    offset += size_t(count);
  }
}
static constexpr size_t ramLimit = 256 * 1024;
static void recoverJournal(const fs::path &directory, const fs::path &fallback, const std::string &role) {
  if (fallback.empty()) return;
  auto lockPath = fallback / (role + ".lock");
  if (!fs::exists(lockPath)) return;
  Descriptor lock{open(lockPath.c_str(), O_RDWR | O_CLOEXEC)};
  if (lock.fd < 0) failed("recovery_lock", lockPath, errno);
  if (flock(lock.fd, LOCK_EX | LOCK_NB)) failed("recovery_lock", lockPath, errno);
  std::vector<uint8_t> bytes;
  for (const auto &name : {role + ".previous.jsonl", role + ".jsonl"}) {
    auto path = fallback / name;
    if (!fs::exists(path)) continue;
    if (fs::file_size(path) > ramLimit + 1) failed("recovery_size", path, EFBIG);
    auto part = readBytes(path); bytes.insert(bytes.end(), part.begin(), part.end());
    if (!bytes.empty() && bytes.back() != '\n') bytes.push_back('\n');
  }
  if (bytes.empty()) return;
  atomicWrite(directory / (role + ".recovered.jsonl"), bytes);
  fs::remove(fallback / (role + ".previous.jsonl"));
  fs::remove(fallback / (role + ".jsonl"));
}
void clearDiagnosticLogs(const fs::path &directory, const fs::path &fallback) {
  for (const auto &root : {directory, fallback}) {
    if (root.empty()) continue;
    for (const auto *role : {"shell", "runner"})
      for (const auto *suffix : {".jsonl", ".previous.jsonl", ".recovered.jsonl"})
        fs::remove(root / (std::string(role) + suffix));
  }
}
// The producer never reads sysfs/proc or touches the card. A bounded queue
// protects both frame pacing and memory when a driver or SD write is slow.
struct BatteryLog::Worker {
  struct Entry { uint64_t now; Json context; std::string event; };
  BatteryLog sink;
  std::mutex mutex;
  std::condition_variable changed;
  std::deque<Entry> queue;
  bool stopping = false, busy = false;
  uint64_t dropped = 0;
  Json error = Json::object();
  std::thread thread;
  Worker(const fs::path &directory, const std::string &role, const fs::path &sys,
         const fs::path &proc, size_t limit, const fs::path &fallback)
      : sink(directory, role, sys, proc, limit, fallback), thread([this] { run(); }) {}
  void run() {
    std::unique_lock<std::mutex> lock(mutex);
    for (;;) {
      changed.wait(lock, [this] { return stopping || !queue.empty(); });
      if (queue.empty() && stopping) return;
      auto entry = std::move(queue.front()); queue.pop_front(); busy = true;
      entry.context["diagnostic_events_dropped"] = dropped;
      lock.unlock();
      sink.sample(entry.now, entry.context, entry.event);
      auto result = sink.lastError;
      lock.lock(); error = std::move(result); busy = false; changed.notify_all();
    }
  }
  ~Worker() {
    { std::lock_guard<std::mutex> lock(mutex); stopping = true; }
    changed.notify_all(); thread.join();
  }
};
BatteryLog::~BatteryLog() = default;
void BatteryLog::drain() {
  if (!worker) return;
  std::unique_lock<std::mutex> lock(worker->mutex);
  worker->changed.wait(lock, [this] { return worker->queue.empty() && !worker->busy; });
  lastError = worker->error;
}
bool BatteryLog::sample(uint64_t now, const Json &context, const std::string &event) {
  if (!worker) return sampleSync(now, context, event);
  std::lock_guard<std::mutex> lock(worker->mutex);
  lastError = worker->error;
  if (event.empty() && now < enqueuePollAt) return lastError.empty();
  enqueuePollAt = now + 5000;
  if (worker->queue.size() == 64) { ++worker->dropped; return lastError.empty(); }
  worker->queue.push_back({now, context, event});
  worker->changed.notify_one();
  return lastError.empty();
}
BatteryLog::BatteryLog(fs::path directory, std::string role, fs::path sys, fs::path proc,
                       size_t limit, fs::path fallback, bool background)
    : directory(std::move(directory)), sys(std::move(sys)), proc(std::move(proc)),
      fallback(std::move(fallback)), role(std::move(role)), limit(limit) {
  if (this->role != "shell" && this->role != "runner")
    throw std::invalid_argument("Invalid battery log role");
  if (background) worker = std::make_unique<Worker>(this->directory, this->role, this->sys, this->proc, limit, this->fallback);
}
static Json displayComparison(const Json &snapshot, const Json &context, uint64_t now) {
  Json percent = nullptr;
  // Match the UI adapter's exact supply and validation, without reusing its value.
  const auto &supplies = snapshot.at("supplies");
  auto battery = supplies.find("axp2202-battery");
  if (battery != supplies.end() && (*battery)["capacity"].is_string()) {
    try {
      auto raw = (*battery)["capacity"].get<std::string>();size_t end = 0;
      int value = std::stoi(raw, &end);
      if (end == raw.size() && value >= 0 && value <= 100) percent = value;
    } catch (...) {}
  }
  auto display = context.value("battery_display", Json(nullptr));
  Json age = nullptr;
  if (display.is_object() && display.contains("submitted_monotonic_ms") &&
      display["submitted_monotonic_ms"].is_number_integer()) {
    auto submitted = display["submitted_monotonic_ms"].get<uint64_t>();
    if (submitted <= now) age = now - submitted;
  }
  return {{"system_percent", percent}, {"display", display}, {"display_age_ms", age}};
}
bool BatteryLog::sampleSync(uint64_t now, const Json &context, const std::string &event) {
  if (event.empty() && now < pollAt) return lastError.empty();
  pollAt = now + 5000;
  Json snapshot;
  Json key = Json::object();
  try {
    snapshot = batterySnapshot(sys, proc);
    for (auto it = snapshot["supplies"].begin(); it != snapshot["supplies"].end(); ++it)
      for (const char *field : {"capacity", "status", "online", "present", "health"})
        key[it.key()][field] = it.value()[field];
    auto comparison = displayComparison(snapshot, context, now);
    auto display = comparison["display"];
    // Frame timestamps change at 60 Hz; only meaningful display changes trigger a row.
    if (display.is_object())
      for (const char *field : {"screen", "header_visible", "percentage_visible", "source_percent", "displayed_percent", "label"})
        key["battery_display"][field] = display.value(field, Json(nullptr));
    if (event.empty() && key == lastKey && now - writtenAt < 60000 && lastError.empty()) return true;
    bool includePassport = !passportWritten || now - passportAt >= 30 * 60 * 1000;
    if (includePassport) {
      if (passport.is_null()) {
        passport = batteryPassport(sys, proc);
        passport["captured_monotonic_ms"] = now;
        passport["captured_utc"] = static_cast<long long>(time(nullptr));
        passport["boot_id"] = snapshot["boot_id"];
      }
      snapshot["battery_passport"] = passport;
    }
    snapshot["schema"] = 1;
    snapshot["utc"] = static_cast<long long>(time(nullptr));
    snapshot["monotonic_ms"] = now;
    snapshot["pid"] = getpid();
    snapshot["role"] = role;
    snapshot["event"] = event.empty() ? "sample" : event;
    snapshot["context"] = context;
    snapshot["battery_comparison"] = std::move(comparison);
    if (!event.empty()) snapshot["storage"] = storageSnapshot(directory, proc);
    auto line = snapshot.dump(-1, ' ', false, Json::error_handler_t::replace) + "\n";
    appendJournal(directory, role, line, limit);
    if (includePassport) { passportWritten = true; passportAt = now; }
    recoverJournal(directory, fallback, role);
    lastKey = std::move(key); writtenAt = now; lastError = Json::object();
    return true;
  } catch (const fs::filesystem_error &e) {
    lastError = {{"operation", e.what()}, {"path", e.path1().string()},
                 {"errno", e.code().value()}, {"message", e.code().message()}};
  } catch (const std::exception &e) { lastError = {{"message", e.what()}}; }
  snapshot["journal_error"] = lastError;
  snapshot["storage"] = storageSnapshot(directory, proc);
  if (!fallback.empty()) {
    try {
      appendJournal(fallback, role, snapshot.dump(-1, ' ', false, Json::error_handler_t::replace) + "\n", ramLimit);
      lastKey = std::move(key); writtenAt = now;
    } catch (const std::exception &e) { lastError["ram_error"] = e.what(); }
  }
  return false;
}
}
