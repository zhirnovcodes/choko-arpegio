#pragma once
#include <cstdint>

namespace ca {

// The chord the player holds. Fixed size; no allocation.
class HeldNotes {
public:
    // velocity 0 = note-off.
    void noteOn(int pitch, int velocity) {
        if (pitch < 0 || pitch > 127) return;
        if (velocity <= 0) {
            noteOff(pitch);
            return;
        }
        if (velocity_[pitch] == 0) ++count_;
        velocity_[pitch] = static_cast<std::uint8_t>(velocity > 127 ? 127 : velocity);
        ++version_;
    }

    void noteOff(int pitch) {
        if (pitch < 0 || pitch > 127 || velocity_[pitch] == 0) return;
        velocity_[pitch] = 0;
        --count_;
        ++version_;
    }

    void clear() {
        for (auto& v : velocity_) v = 0;
        count_ = 0;
        ++version_;
    }

    int count() const { return count_; }
    bool empty() const { return count_ == 0; }
    int velocity(int pitch) const { return pitch >= 0 && pitch <= 127 ? velocity_[pitch] : 0; }
    // Changes on every note-on / note-off; lets the engine see that the chord changed.
    unsigned version() const { return version_; }

    // Spec §4 step 2: held notes ascending, index floor(value * count). -1 when nothing is held.
    int pick(double value) const {
        if (count_ == 0) return -1;
        int index = static_cast<int>(value * count_);
        if (index < 0) index = 0;
        if (index >= count_) index = count_ - 1;
        for (int pitch = 0; pitch < 128; ++pitch) {
            if (velocity_[pitch] != 0 && index-- == 0) return pitch;
        }
        return -1;
    }

private:
    std::uint8_t velocity_[128] = {};
    int count_ = 0;
    unsigned version_ = 0;
};

} // namespace ca
