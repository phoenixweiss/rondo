// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
#pragma once
#include "model.hpp"
#include <atomic>
#include <memory>
namespace rondo {
struct BluetoothDevice {
  std::string address, name;
  bool paired=false, trusted=false, connected=false;
  bool named=false, input=false;
};
struct BluetoothState {
  bool available=false, enabled=false;
  bool discovering=false;
  std::vector<BluetoothDevice> devices;
};
struct BluetoothError : std::runtime_error { using std::runtime_error::runtime_error; };
bool bluetoothAddress(const std::string &);
std::vector<BluetoothDevice> bluetoothDevices(const std::string &);
BluetoothState bluetoothObjects(const std::string &);
BluetoothDevice bluetoothInfo(const std::string &, const std::string &);
class BluetoothAdapter {
public:
  std::atomic<bool> cancelled{false};
  virtual ~BluetoothAdapter()=default;
  virtual bool supported() const {return false;}
  virtual BluetoothState status() {return {};}
  virtual BluetoothState enable(bool) {throw BluetoothError("unavailable");}
  virtual BluetoothState scan() {throw BluetoothError("unavailable");}
  virtual void stopScan() {}
  virtual BluetoothState connect(const std::string &) {throw BluetoothError("unavailable");}
  virtual BluetoothState disconnect(const std::string &) {throw BluetoothError("unavailable");}
  virtual BluetoothState forget(const std::string &) {throw BluetoothError("unavailable");}
};
class BluezBluetooth : public BluetoothAdapter {
protected:
  virtual std::string command(const std::vector<std::string> &,int timeout=8)=0;
  virtual void start()=0;
  virtual std::optional<BluetoothState> snapshot() {return std::nullopt;}
public:
  bool supported() const override {return true;}
  BluetoothState status() override;
  BluetoothState enable(bool) override;
  BluetoothState scan() override;
  BluetoothState connect(const std::string &) override;
  BluetoothState disconnect(const std::string &) override;
  BluetoothState forget(const std::string &) override;
};
// A single client owns the discovery session and pairing agent. Also exercised
// with a real fixture subprocess on macOS; no host radio is used by tests.
class BluetoothProcess {
  int fd=-1, pid=-1;
  std::atomic<bool> &cancelled;
  std::string readSome(int milliseconds);
public:
  BluetoothProcess(const std::vector<std::string> &,std::atomic<bool> &);
  ~BluetoothProcess();
  BluetoothProcess(const BluetoothProcess &)=delete;
  std::string request(const std::string &,const std::vector<std::string> &,int milliseconds);
  std::string finish(int milliseconds);
};
std::shared_ptr<BluetoothAdapter> bluetoothAdapter(const fs::path &helper,bool active);
}
