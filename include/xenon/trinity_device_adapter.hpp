#pragma once

#include "xenon/native_midi_input.hpp"
#include "xenon/trinity_device_bus.hpp"

#include <cstddef>
#include <memory>

namespace xenon {

class ITrinityDeviceAdapter {
public:
    virtual ~ITrinityDeviceAdapter() = default;
    [[nodiscard]] virtual TrinityDeviceKind kind() const noexcept = 0;
    virtual void start() = 0;
    virtual void stop() noexcept = 0;
    [[nodiscard]] virtual bool active() const noexcept = 0;
    virtual std::size_t pump(TrinityDeviceBus& bus) = 0;
};

class MidiTrinityAdapter final : public ITrinityDeviceAdapter {
public:
    MidiTrinityAdapter(TrinityDeviceKind kind, unsigned int midi_input_index);
    ~MidiTrinityAdapter() override;
    [[nodiscard]] TrinityDeviceKind kind() const noexcept override;
    void start() override;
    void stop() noexcept override;
    [[nodiscard]] bool active() const noexcept override;
    std::size_t pump(TrinityDeviceBus& bus) override;
private:
    [[nodiscard]] DeviceGesture map(const NativeMidiMessage& message) const;
    TrinityDeviceKind kind_;
    unsigned int input_index_{0};
    NativeMidiInput input_;
};

} // namespace xenon
