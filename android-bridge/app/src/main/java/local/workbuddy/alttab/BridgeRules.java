package local.workbuddy.alttab;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public final class BridgeRules {
    public static final String ALT_TAB = "alt_tab";
    public static final String WINDOWS_KEY = "windows_key";
    public static final String ARROW_KEYS = "arrow_keys";

    private static final List<BridgeRule> RULES = Collections.unmodifiableList(Arrays.asList(
            new BridgeRule(
                    ALT_TAB,
                    "Alt+Tab 窗口切换",
                    "Alt+Tab",
                    "Alt+F12",
                    "Windows Alt+Tab",
                    true),
            new BridgeRule(
                    WINDOWS_KEY,
                    "Command 作为 Windows 键",
                    "Command / Left Meta",
                    "Scroll Lock",
                    "左 Windows 键",
                    true),
            new BridgeRule(
                    ARROW_KEYS,
                    "方向键兼容修复",
                    "上 / 下 / 左 / 右",
                    "原键值直通",
                    "Windows 方向键",
                    false)
    ));

    private BridgeRules() {
    }

    public static List<BridgeRule> all() {
        return RULES;
    }
}
