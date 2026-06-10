import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CMAKE = ROOT / "codex_quota_t5" / "CMakeLists.txt"
UI = ROOT / "codex_quota_t5" / "src" / "codex_ui.c"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def function_body(source: str, name: str) -> str:
    match = re.search(rf"static .*?\b{name}\([^)]*\)\s*\{{", source, re.S)
    if not match:
        raise AssertionError(f"function not found: {name}")

    start = match.end()
    depth = 1
    pos = start
    while pos < len(source) and depth:
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
        pos += 1
    if depth:
        raise AssertionError(f"function body not closed: {name}")
    return source[start : pos - 1]


class UiCjkFontTest(unittest.TestCase):
    def test_cmake_links_simsun_cjk_font_into_app(self):
        cmake = read(CMAKE)
        self.assertIn("LV_FONT_SIMSUN_16_CJK=1", cmake)
        self.assertIn("lv_font_simsun_16_cjk.c", cmake)
        self.assertRegex(cmake, r"set\(APP_SRCS[\s\S]*lv_font_simsun_16_cjk\.c")

    def test_ui_declares_cjk_font_helper(self):
        ui = read(UI)
        body = function_body(ui, "font_for_cjk_text")
        self.assertIn("&lv_font_simsun_16_cjk", body)

    def test_quota_card_text_uses_cjk_font(self):
        ui = read(UI)
        body = function_body(ui, "create_quota_card")
        for field in ("card->title", "card->used", "card->reset"):
            self.assertRegex(
                body,
                rf"lv_obj_set_style_text_font\({re.escape(field)},\s*font_for_cjk_text\(\),\s*0\)",
            )
        self.assertRegex(
            body,
            r"lv_obj_set_style_text_font\(card->percent,\s*font_for_percent\(screen_w\),\s*0\)",
        )

    def test_diag_labels_and_values_use_cjk_font(self):
        ui = read(UI)
        section = function_body(ui, "create_diag_section")
        item = function_body(ui, "create_diag_item")
        self.assertIn("lv_obj_set_style_text_font(lbl, font_for_cjk_text(), 0)", section)
        self.assertIn("lv_obj_set_style_text_font(lbl, font_for_cjk_text(), 0)", item)
        self.assertIn("lv_obj_set_style_text_font(val, font_for_cjk_text(), 0)", item)
        self.assertIn("lv_label_set_long_mode(val, LV_LABEL_LONG_DOT)", item)

    def test_status_bar_uses_cjk_font_and_width_limit(self):
        ui = read(UI)
        create = function_body(ui, "codex_ui_create")
        self.assertIn("lv_obj_set_style_text_font(g_status_label, font_for_cjk_text(), 0)", create)
        self.assertIn("lv_label_set_long_mode(g_status_label, LV_LABEL_LONG_DOT)", create)

    def test_runtime_chinese_labels_are_not_forced_to_ascii(self):
        ui = read(UI)
        update = function_body(ui, "update_card")
        self.assertNotIn("format_label_ascii", ui)
        self.assertIn("window->label[0] ? window->label : fallback_title", update)


if __name__ == "__main__":
    unittest.main()
