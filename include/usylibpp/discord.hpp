#pragma once

#include "aliases.hpp" // IWYU pragma: export
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

    /**
     * Add https:// to link if it does not begin with that
     * URL encodes link with strings::encode_full_url
     */
    [[nodiscard]] inline constexpr std::string https_if_needed(const std::string_view link) {
        if (!(link.starts_with("https://") || link.starts_with("http://"))) {
            std::string ret;
            ret.reserve(link.size() + 8);
            ret.append("https://");
            ret.append(link);
            return strings::encode_full_url(ret);
        }
        
        return strings::encode_full_url(link);
    }

    /**
     * Make sure to escape the content being passed in if desired
     */
    [[nodiscard]] inline constexpr std::string remove_link_embed(const std::string_view link) {
        std::string ret;
        ret.reserve(link.size() + 2);

        ret.push_back('<');
        ret.append(link);
        ret.push_back('>');
        return ret;
    }

    /**
     * Make sure to escape the content being passed in if desired
     */
    [[nodiscard]] inline constexpr std::string as_hyperlink(const std::string_view text, const std::string_view link) {
        std::string ret;
        ret.reserve(text.size() + link.size() + 4);

        ret.push_back('[');
        ret.append(text);
        ret.push_back(']');
        ret.push_back('(');
        ret.append(link);
        ret.push_back(')');
        return ret;
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