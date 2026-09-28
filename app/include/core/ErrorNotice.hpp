#pragma once

#include <string>
#include <utility>

class ErrorNotice {
public:
    void show(std::string title, std::string message, std::string subject = {}, std::string location = {}) {
        title_ = std::move(title);
        message_ = std::move(message);
        subject_ = std::move(subject);
        location_ = std::move(location);
        visible_ = true;
    }
    void dismiss() { visible_ = false; }

    bool visible() const { return visible_; }
    const std::string& title() const { return title_; }
    const std::string& message() const { return message_; }
    const std::string& subject() const { return subject_; }
    const std::string& location() const { return location_; }

private:
    bool visible_ = false;
    std::string title_;
    std::string message_;
    std::string subject_;
    std::string location_;
};
