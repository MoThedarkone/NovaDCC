#pragma once
#include <deque>
#include <mutex>
#include <string>
#include <streambuf>
#include <vector>

class GuiConsole {
public:
    static GuiConsole& instance();
    void append(const std::string& s);
    std::vector<std::string> lines() const;
    void clear();
    void setMaxLines(size_t n) { maxLines_ = n; }

    // redirect stdout/stderr to the GUI console
    void installStdStreams();
    void restoreStdStreams();

private:
    GuiConsole();
    ~GuiConsole();

    size_t maxLines_ = 1000;
    mutable std::mutex mtx_;
    std::deque<std::string> buffer_;

    std::streambuf* coutBuf_ = nullptr;
    std::streambuf* cerrBuf_ = nullptr;

    struct CapturingBuf : std::streambuf {
        CapturingBuf(GuiConsole* owner, std::streambuf* orig);
        ~CapturingBuf();
    protected:
        virtual int_type overflow(int_type c) override;
        virtual std::streamsize xsputn(const char_type* s, std::streamsize n) override;
    private:
        GuiConsole* owner_ = nullptr;
        std::streambuf* orig_ = nullptr;
    };

    CapturingBuf* coutRedirect_ = nullptr;
    CapturingBuf* cerrRedirect_ = nullptr;
};
