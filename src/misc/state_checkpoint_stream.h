/* GPL-2.0-or-later. Bounded capture of original component streams. */
#ifndef DOSBOX_STATE_CHECKPOINT_STREAM_H
#define DOSBOX_STATE_CHECKPOINT_STREAM_H
#include <streambuf>
#include <string>
#include <utility>
class StateCheckpointStream final : public std::streambuf {
    std::string bytes_;
    size_t limit_;
    bool failed_=false;
protected:
    std::streamsize xsputn(const char *data, std::streamsize count) override {
        if (count <= 0) return 0;
        const size_t n = static_cast<size_t>(count);
        if (failed_ || n > limit_ - bytes_.size()) { failed_=true; return 0; }
        try { bytes_.append(data, n); }
        catch (...) { failed_=true; throw; }
        return count;
    }
    int_type overflow(int_type value) override {
        if (traits_type::eq_int_type(value, traits_type::eof()))
            return traits_type::not_eof(value);
        const char byte = traits_type::to_char_type(value);
        return xsputn(&byte, 1) == 1 ? value : traits_type::eof();
    }
public:
    explicit StateCheckpointStream(size_t limit) : limit_(limit) {}
    bool failed() const { return failed_; }
    std::string take() { return std::move(bytes_); }
};
#endif
