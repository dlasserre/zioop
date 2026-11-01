#pragma once

#include <raylib.h>

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gui {
    enum class Weight { Regular, Medium, SemiBold };

    class Typography {
    public:
        Typography() = default;

        Typography(const Typography &) = delete;

        Typography &operator=(const Typography &) = delete;

        ~Typography() = default;

        void unload();

        [[nodiscard]] Vector2 measure(const std::string &text, int size, Weight weight = Weight::Regular) const;

        void draw(const std::string &text, Vector2 position, int size, Color color,
                  Weight weight = Weight::Regular) const;

        void drawShadowed(const std::string &text, Vector2 position, int size, Color color,
                          Weight weight = Weight::Regular) const;

        [[nodiscard]] std::string fit(const std::string &text, int size, float maxWidth,
                                      Weight weight = Weight::Regular) const;

        [[nodiscard]] std::vector<std::string> wrap(const std::string &text, int size, float maxWidth,
                                                    std::size_t maxLines, Weight weight = Weight::Regular) const;

    private:
        [[nodiscard]] const Font &font(Weight weight, int size) const;

        mutable std::map<std::pair<Weight, int>, Font> m_fonts;
    };
}
