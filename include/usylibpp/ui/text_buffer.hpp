#pragma once

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <deque>
#include <string>
#include <string_view>
#include <utility>

namespace usylibpp::ui {

/// Editable single-line text state with selection, word navigation, and coalesced undo/redo history.
struct TextBuffer {
    struct UndoEntry {
        std::wstring text;
        size_t caret = 0;
        size_t anchor = 0;
    };

    static constexpr size_t MAX_UNDO = 256;

    std::wstring text;
    size_t caret = 0;
    size_t anchor = 0;

    TextBuffer() = default;
    TextBuffer(const TextBuffer&) = default;
    TextBuffer& operator=(const TextBuffer&) = default;
    TextBuffer(TextBuffer&&) noexcept = default;
    TextBuffer& operator=(TextBuffer&&) noexcept = default;
    virtual ~TextBuffer() = default;

    virtual void set(std::wstring s) noexcept {
        text = std::move(s);
        caret = anchor = text.size();
        clear_history();
    }

    void select_all() noexcept {
        anchor = 0;
        caret = text.size();
        break_undo_group();
    }

    [[nodiscard]] bool has_selection() const noexcept { return caret != anchor; }

    [[nodiscard]] std::wstring_view selection() const noexcept {
        const auto [a, b] = sel();
        return std::wstring_view{text}.substr(a, b - a);
    }

    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }

    void break_undo_group() noexcept {
        last_kind_ = EditKind::None;
        last_pos_ = 0;
        last_char_ = 0;
    }

    void clear_history() noexcept {
        undo_.clear();
        redo_.clear();
        break_undo_group();
    }

    /// Returns true when the text changed.
    bool on_char(wchar_t c) {
        if (c < 0x20 || c == 0x7f) return false;
        const bool merge = !has_selection() && last_kind_ == EditKind::Insert && caret == last_pos_ &&
                           (!std::iswspace(last_char_) || std::iswspace(c));
        if (!merge) push_undo();
        apply_replace_selection(std::wstring_view{&c, 1});
        last_kind_ = EditKind::Insert;
        last_pos_ = caret;
        last_char_ = c;
        return true;
    }

    /// Replaces the current selection (or inserts at caret). Returns true when text changed.
    bool replace_selection(std::wstring_view s) {
        if (!has_selection() && s.empty()) return false;
        if (has_selection() && selection() == s) {
            caret = anchor = sel().second;
            break_undo_group();
            return false;
        }
        push_undo();
        apply_replace_selection(s);
        last_kind_ = EditKind::Discrete;
        return true;
    }

    /// Replaces `[pos, pos + len)` with `s` and moves caret to end of inserted text. Returns true when text changed.
    bool replace_range(size_t pos, size_t len, std::wstring_view s) {
        pos = std::min(pos, text.size());
        len = std::min(len, text.size() - pos);
        if (std::wstring_view{text}.substr(pos, len) == s) {
            caret = anchor = pos + s.size();
            break_undo_group();
            return false;
        }
        push_undo();
        text.replace(pos, len, s);
        caret = anchor = pos + s.size();
        last_kind_ = EditKind::Discrete;
        return true;
    }

    bool replace_all(std::wstring_view s) { return replace_range(0, text.size(), s); }

    bool backspace(bool word) { return delete_step(false, word); }
    bool del(bool word) { return delete_step(true, word); }

    bool undo() { return step_history(undo_, redo_); }
    bool redo() { return step_history(redo_, undo_); }

    void move_left(bool word, bool extend) noexcept {
        if (!extend && has_selection()) {
            move_to(sel().first, false);
        } else {
            move_to(word ? word_left(caret) : (caret ? caret - 1 : 0), extend);
        }
    }

    void move_right(bool word, bool extend) noexcept {
        if (!extend && has_selection()) {
            move_to(sel().second, false);
        } else {
            move_to(word ? word_right(caret) : std::min(text.size(), caret + 1), extend);
        }
    }

    void move_home(bool extend) noexcept { move_to(0, extend); }
    void move_end(bool extend) noexcept { move_to(text.size(), extend); }

    void move_to(size_t to, bool extend) noexcept {
        caret = std::min(to, text.size());
        if (!extend) anchor = caret;
        break_undo_group();
    }

    void select_word_at_caret() noexcept {
        anchor = word_left(std::min(text.size(), caret + 1));
        caret = word_right(anchor);
        break_undo_group();
    }

protected:
    [[nodiscard]] std::pair<size_t, size_t> sel() const noexcept { return {std::min(caret, anchor), std::max(caret, anchor)}; }

private:
    enum class EditKind : uint8_t { None, Insert, Backspace, Delete, Discrete };

    void push_undo() {
        if (undo_.size() >= MAX_UNDO) undo_.pop_front();
        undo_.push_back({.text = text, .caret = caret, .anchor = anchor});
        redo_.clear();
    }

    bool step_history(std::deque<UndoEntry>& from, std::deque<UndoEntry>& to) {
        if (from.empty()) return false;
        if (to.size() >= MAX_UNDO) to.pop_front();
        to.push_back({.text = std::move(text), .caret = caret, .anchor = anchor});
        UndoEntry target = std::move(from.back());
        from.pop_back();
        text = std::move(target.text);
        caret = std::min(target.caret, text.size());
        anchor = std::min(target.anchor, text.size());
        break_undo_group();
        return true;
    }

    bool delete_step(bool forward, bool word) {
        if (!has_selection()) {
            if (forward ? (caret >= text.size()) : (caret == 0)) return false;
            const size_t other = forward ? (word ? word_right(caret) : caret + 1) : (word ? word_left(caret) : caret - 1);
            if (!word) {
                const wchar_t deleted = text[forward ? caret : other];
                const EditKind kind = forward ? EditKind::Delete : EditKind::Backspace;
                const bool merge = last_kind_ == kind && caret == last_pos_ &&
                                   (!std::iswspace(last_char_) || std::iswspace(deleted));
                if (!merge) push_undo();
                anchor = other;
                apply_replace_selection({});
                last_kind_ = kind;
                last_pos_ = caret;
                last_char_ = deleted;
                return true;
            }
            push_undo();
            anchor = other;
        } else {
            push_undo();
        }
        apply_replace_selection({});
        last_kind_ = EditKind::Discrete;
        return true;
    }

    void apply_replace_selection(std::wstring_view s) {
        const auto [a, b] = sel();
        text.replace(a, b - a, s);
        caret = anchor = a + s.size();
    }

    [[nodiscard]] size_t word_left(size_t i) const noexcept {
        while (i > 0 && std::iswspace(text[i - 1])) --i;
        while (i > 0 && !std::iswspace(text[i - 1]) && !is_sep(text[i - 1])) --i;
        if (i > 0 && i == caret && is_sep(text[i - 1])) --i;
        return i;
    }

    [[nodiscard]] size_t word_right(size_t i) const noexcept {
        const size_t n = text.size();
        if (i < n && is_sep(text[i])) return i + 1;
        while (i < n && !std::iswspace(text[i]) && !is_sep(text[i])) ++i;
        while (i < n && std::iswspace(text[i])) ++i;
        return i;
    }

    static bool is_sep(wchar_t c) noexcept { return c == L'\\' || c == L'/' || c == L':' || c == L'.' || c == L'_' || c == L'-'; }

    std::deque<UndoEntry> undo_;
    std::deque<UndoEntry> redo_;
    EditKind last_kind_ = EditKind::None;
    size_t last_pos_ = 0;
    wchar_t last_char_ = 0;
};

} // namespace usylibpp::ui
