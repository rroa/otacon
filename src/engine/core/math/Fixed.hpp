// Fixed.hpp — in-house Q16.16 fixed-point scalar.
//
// Teaching notes
// --------------
// A fixed-point number stores a fraction as an integer scaled by a constant.
// Here we use Q16.16: a 32-bit integer where the top 16 bits are the integer
// part and the bottom 16 bits are the fraction (resolution 1/65536 ~= 1.5e-5).
//
//   real value  ==  raw / 65536
//
// Why bother, when modern FPUs are fast?  Three didactic reasons:
//   1. Determinism: integer math gives bit-identical results on every CPU,
//      which matters for replays / lockstep netcode.
//   2. Historically (and on tiny MCUs / GPUs without an FPU) integer ALUs were
//      far cheaper than floating point — the classic "perf gain".
//   3. It makes the cost of precision and range *visible*: Q16.16 can only
//      represent +-32767.99998, so you must reason about overflow.
//
// Range caveat: an endless runner's world X grows without bound as you
// run, so it would overflow Q16.16. That is exactly why the live simulation
// defaults to float (see Scalar.hpp) and fixed-point is an opt-in teaching
// build — the limitation is part of the lesson.
#pragma once
#include <cstdint>
#include <string>

namespace otacon {

class Fixed {
public:
    using raw_t = std::int32_t;
    static constexpr int kFracBits = 16;
    static constexpr raw_t kOne = raw_t(1) << kFracBits;

    constexpr Fixed() : raw_(0) {}
    // Construct from an int (exact) or a float/double (rounded).
    constexpr Fixed(int v) : raw_(raw_t(v) << kFracBits) {}
    constexpr Fixed(long v) : raw_(raw_t(v) << kFracBits) {}
    Fixed(float v)  : raw_(raw_t(v * float(kOne)  + (v >= 0 ? 0.5f : -0.5f))) {}
    Fixed(double v) : raw_(raw_t(v * double(kOne) + (v >= 0 ? 0.5  : -0.5 ))) {}

    static constexpr Fixed fromRaw(raw_t r) { Fixed f; f.raw_ = r; return f; }
    constexpr raw_t raw() const { return raw_; }

    float  toFloat()  const { return float(raw_)  / float(kOne); }
    double toDouble() const { return double(raw_) / double(kOne); }
    int    toInt()    const { return raw_ >> kFracBits; }          // truncates toward -inf

    // Arithmetic. Multiply/divide promote to 64-bit to avoid intermediate
    // overflow, then rescale by the fractional shift.
    Fixed operator+(Fixed o) const { return fromRaw(raw_ + o.raw_); }
    Fixed operator-(Fixed o) const { return fromRaw(raw_ - o.raw_); }
    Fixed operator-()        const { return fromRaw(-raw_); }
    Fixed operator*(Fixed o) const {
        return fromRaw(raw_t((std::int64_t(raw_) * o.raw_) >> kFracBits));
    }
    Fixed operator/(Fixed o) const {
        if (o.raw_ == 0) return fromRaw(raw_ >= 0 ? INT32_MAX : INT32_MIN);
        return fromRaw(raw_t((std::int64_t(raw_) << kFracBits) / o.raw_));
    }
    Fixed& operator+=(Fixed o) { raw_ += o.raw_; return *this; }
    Fixed& operator-=(Fixed o) { raw_ -= o.raw_; return *this; }
    Fixed& operator*=(Fixed o) { *this = *this * o; return *this; }
    Fixed& operator/=(Fixed o) { *this = *this / o; return *this; }

    bool operator==(Fixed o) const { return raw_ == o.raw_; }
    bool operator!=(Fixed o) const { return raw_ != o.raw_; }
    bool operator< (Fixed o) const { return raw_ <  o.raw_; }
    bool operator<=(Fixed o) const { return raw_ <= o.raw_; }
    bool operator> (Fixed o) const { return raw_ >  o.raw_; }
    bool operator>=(Fixed o) const { return raw_ >= o.raw_; }

private:
    raw_t raw_;
};

// Free helpers mirroring <cmath> so generic code works for float and Fixed.
inline Fixed abs(Fixed f)   { return f < Fixed(0) ? -f : f; }
inline Fixed floorFx(Fixed f) {
    Fixed::raw_t r = f.raw();
    return Fixed::fromRaw(r & ~(Fixed::kOne - 1));   // mask off fractional bits
}
Fixed sqrtFx(Fixed f);                                // defined in Fixed.cpp
std::string toString(Fixed f);

} // namespace otacon
