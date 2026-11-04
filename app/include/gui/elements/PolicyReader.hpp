#pragma once

#include "gui/InputFrame.hpp"
#include "gui/UiRenderer.hpp"
#include "network/ApiClient.hpp"

#include <string>
#include <string_view>
#include <vector>

class PolicyReader {
public:
    enum class Outcome { None, Accepted, Declined };

    void open(std::vector<PolicyBlock> blocks);
    bool isOpen() const { return open_; }
    Outcome update(const InputFrame& input);
    void render(UiRenderer& ui, std::string_view title, std::string_view decline, std::string_view accept);

private:
    struct Line {
        std::string text;
        float x;
        float y;
        float size;
        u32 color;
        bool dot = false;
    };

    void layout(UiRenderer& ui);
    void addWrapped(UiRenderer& ui, std::string_view text, float indent, float size, u32 color, float spaceBefore,
                    float spaceAfter);
    float maxScroll() const;

    std::vector<PolicyBlock> blocks_;
    std::vector<Line> lines_;
    float contentHeight_ = 0.0F;
    float scroll_ = 0.0F;
    float lastTouchY_ = -1.0F;
    bool open_ = false;
    bool laidOut_ = false;
};
