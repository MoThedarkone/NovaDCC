#include "gui_console.h"
#include "log.h"
#include <iostream>

GuiConsole& GuiConsole::instance() {
    static GuiConsole inst;
    return inst;
}

GuiConsole::GuiConsole() {}
GuiConsole::~GuiConsole() { restoreStdStreams(); }

void GuiConsole::append(const std::string& s) {
    std::lock_guard<std::mutex> lk(mtx_);
    buffer_.push_back(s);
    while(buffer_.size() > maxLines_) buffer_.pop_front();
}

std::vector<std::string> GuiConsole::lines() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return std::vector<std::string>(buffer_.begin(), buffer_.end());
}

void GuiConsole::clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    buffer_.clear();
}

void GuiConsole::installStdStreams() {
    if (coutRedirect_ || cerrRedirect_) return; // already installed
    coutBuf_ = std::cout.rdbuf();
    cerrBuf_ = std::cerr.rdbuf();
    coutRedirect_ = new CapturingBuf(this, coutBuf_);
    cerrRedirect_ = new CapturingBuf(this, cerrBuf_);
    std::cout.rdbuf(coutRedirect_);
    std::cerr.rdbuf(cerrRedirect_);
}

void GuiConsole::restoreStdStreams() {
    if(coutRedirect_) {
        std::cout.rdbuf(coutBuf_);
        delete coutRedirect_;
        coutRedirect_ = nullptr;
        coutBuf_ = nullptr;
    }
    if(cerrRedirect_) {
        std::cerr.rdbuf(cerrBuf_);
        delete cerrRedirect_;
        cerrRedirect_ = nullptr;
        cerrBuf_ = nullptr;
    }
}

// CapturingBuf implementation
GuiConsole::CapturingBuf::CapturingBuf(GuiConsole* owner, std::streambuf* orig) : owner_(owner), orig_(orig) {}
GuiConsole::CapturingBuf::~CapturingBuf() {}

int GuiConsole::CapturingBuf::overflow(int_type c) {
    if (c == traits_type::eof()) return traits_type::not_eof(c);
    char ch = (char)c;
    // forward to original buffer
    if(orig_) orig_->sputc(ch);
    // append and flush on newline
    char buf[2] = { ch, 0 };
    owner_->append(std::string(buf));
    // Also log via Log at debug level for capture
    {
        std::string s(1, ch);
        Log::log(Log::Level::Debug, s);
    }
    return c;
}

std::streamsize GuiConsole::CapturingBuf::xsputn(const char_type* s, std::streamsize n) {
    if(orig_) orig_->sputn(s, n);
    // append as a single string
    owner_->append(std::string(s, (size_t)n));
    // Also log the whole string
    Log::log(Log::Level::Debug, std::string(s, (size_t)n));
    return n;
}
