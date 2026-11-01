#include "zioop/gui/Typography.hpp"

#include <cmath>
#include <ranges>
#include <sstream>

#include "zioop/gui/Assets.hpp"

namespace gui {
    namespace {
        const char *fileFor(const Weight weight) {
            switch (weight) {
                case Weight::Medium: return "fonts/Poppins-Medium.ttf";
                case Weight::SemiBold: return "fonts/Poppins-SemiBold.ttf";
                case Weight::Regular: break;
            }
            return "fonts/Poppins-Regular.ttf";
        }

        std::vector<int> codepoints() {
            std::vector<int> points;
            for (int c = 32; c < 127; ++c) {
                points.push_back(c);
            }
            for (int c = 160; c < 384; ++c) {
                points.push_back(c);
            }
            for (const int c: {0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x20AC, 0x00B7}) {
                points.push_back(c);
            }
            return points;
        }

        Vector2 snap(const Vector2 position) {
            return {std::round(position.x), std::round(position.y)};
        }
    }

    const Font &Typography::font(const Weight weight, const int size) const {
        const auto key = std::make_pair(weight, size);
        if (const auto it = m_fonts.find(key); it != m_fonts.end()) {
            return it->second;
        }

        auto points = codepoints();
        const auto path = assetPath(fileFor(weight));
        Font loaded = LoadFontEx(path.c_str(), size, points.data(), static_cast<int>(points.size()));
        if (!IsFontValid(loaded)) {
            loaded = GetFontDefault();
        } else {
            SetTextureFilter(loaded.texture, TEXTURE_FILTER_BILINEAR);
        }
        return m_fonts.emplace(key, loaded).first->second;
    }

    void Typography::unload() {
        const Font fallback = GetFontDefault();
        for (auto &loaded: m_fonts | std::views::values) {
            if (loaded.texture.id != fallback.texture.id) {
                UnloadFont(loaded);
            }
        }
        m_fonts.clear();
    }

    Vector2 Typography::measure(const std::string &text, const int size, const Weight weight) const {
        return MeasureTextEx(font(weight, size), text.c_str(), static_cast<float>(size), 0.0f);
    }

    void Typography::draw(const std::string &text, const Vector2 position, const int size, const Color color,
                          const Weight weight) const {
        DrawTextEx(font(weight, size), text.c_str(), snap(position), static_cast<float>(size), 0.0f, color);
    }

    void Typography::drawShadowed(const std::string &text, const Vector2 position, const int size, const Color color,
                                  const Weight weight) const {
        draw(text, {position.x, position.y + 1.0f}, size, Color{0, 0, 0, static_cast<unsigned char>(color.a * 0.6f)}, weight);
        draw(text, position, size, color, weight);
    }

    std::string Typography::fit(const std::string &text, const int size, const float maxWidth,
                                const Weight weight) const {
        if (measure(text, size, weight).x <= maxWidth) {
            return text;
        }
        std::string fitted = text;
        while (!fitted.empty() && measure(fitted + "…", size, weight).x > maxWidth) {
            fitted.pop_back();
            while (!fitted.empty() && (static_cast<unsigned char>(fitted.back()) & 0xC0) == 0x80) {
                fitted.pop_back();
            }
            if (!fitted.empty() && (static_cast<unsigned char>(fitted.back()) & 0xC0) == 0xC0) {
                fitted.pop_back();
            }
        }
        while (!fitted.empty() && fitted.back() == ' ') {
            fitted.pop_back();
        }
        return fitted + "…";
    }

    std::vector<std::string> Typography::wrap(const std::string &text, const int size, const float maxWidth,
                                              const std::size_t maxLines, const Weight weight) const {
        std::vector<std::string> lines;
        std::istringstream words(text);
        std::string word;
        std::string line;

        while (words >> word) {
            const auto candidate = line.empty() ? word : line + " " + word;
            if (measure(candidate, size, weight).x <= maxWidth || line.empty()) {
                line = candidate;
                continue;
            }
            lines.push_back(line);
            line = word;
        }
        if (!line.empty()) {
            lines.push_back(line);
        }

        if (lines.size() > maxLines && maxLines > 0) {
            lines.resize(maxLines);
            lines.back() = fit(lines.back() + " …", size, maxWidth, weight);
        }
        for (auto &current: lines) {
            current = fit(current, size, maxWidth, weight);
        }
        return lines;
    }
}
