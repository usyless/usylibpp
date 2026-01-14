#pragma once

#include "strings.hpp"

namespace usylibpp::discord {
    /**
     * This isnt exhaustive and probably isnt efficient
     */
    inline constexpr void escape_chars_inline(std::string& str) {
        strings::replace_all_inplace(str, "\\", "\\\\"); // backslash itself
        strings::replace_all_inplace(str, "*", "\\*"); // bold / italics
        strings::replace_all_inplace(str, "@", "\\@"); // mentions
        strings::replace_all_inplace(str, ">", "\\>");
        strings::replace_all_inplace(str, "<", "\\<");
        strings::replace_all_inplace(str, "#", "\\#"); // channels?
        strings::replace_all_inplace(str, "-", "\\-"); // lists
        strings::replace_all_inplace(str, "_", "\\_"); // italic
        strings::replace_all_inplace(str, "`", "\\`"); // code
        strings::replace_all_inplace(str, "~", "\\~"); // strikethrough
        strings::replace_all_inplace(str, "(", "\\("); // url
        strings::replace_all_inplace(str, ")", "\\)"); // url
        strings::replace_all_inplace(str, "[", "\\["); // url
        strings::replace_all_inplace(str, "]", "\\]"); // url
        strings::replace_all_inplace(str, "{", "\\{"); // idk
        strings::replace_all_inplace(str, "}", "\\}"); // idk
        strings::replace_all_inplace(str, "&", "\\&"); // idk
        strings::replace_all_inplace(str, "|", "\\|"); // spoilers
        strings::replace_all_inplace(str, "^", "\\^"); // idk
    }

    [[nodiscard]] inline constexpr std::string escape_chars(const std::string_view str) {
        std::string ret{str};
        escape_chars_inline(ret);
        return ret;
    }

    template <bool escape>
    [[nodiscard]] inline constexpr std::string remove_link_embed(const std::string_view link) {
        constexpr auto MAKE_NO_EMBED = [](const std::string_view str) -> std::string {
            std::string ret;
            ret.reserve(str.size() + 2);
            ret.push_back('<');
            ret.append(str);
            ret.push_back('>');
            return ret;
        };

        if constexpr (escape) return MAKE_NO_EMBED(escape_chars(link));
        else return MAKE_NO_EMBED(link);
    }

    /**
     * Appends https:// if link does not begin with http:// or https://
     * Will url encode link
     * Disabling escape also disables validity checks
     */
    template <bool escape>
    [[nodiscard]] inline constexpr std::string as_hyperlink(const std::string_view text, const std::string_view link) {
        constexpr auto MAKE_LINK = [](const std::string_view text, const std::string_view link) -> std::string {
            std::string ret;
            ret.reserve(text.size() + link.size() + 4);

            ret.push_back('[');
            ret.append(text);
            ret.push_back(']');
            ret.push_back('(');
            ret.append(link);
            ret.push_back(')');
            return ret;
        };

        if constexpr (!escape) {
            return MAKE_LINK(text, strings::url_encode(link));
        } else {
            std::string link_escaped;

            if (!link.starts_with("https://") || !link.starts_with("http://")) {
                link_escaped.resize(link.size() + 8);
                link_escaped.append("https://");
                link_escaped.append(link);
            } else {
                link_escaped = link;
            }

            escape_chars_inline(link_escaped);

            return MAKE_LINK(escape_chars(text), strings::url_encode(link_escaped));
        }
    }

    [[nodiscard]] inline constexpr std::string mention_from_id(const std::string_view id) {
        std::string out;
        out.reserve(id.size() + 3);
        out.push_back('<');
        out.push_back('@');
        out.append(id);
        out.push_back('>');
        return out;
    }
}