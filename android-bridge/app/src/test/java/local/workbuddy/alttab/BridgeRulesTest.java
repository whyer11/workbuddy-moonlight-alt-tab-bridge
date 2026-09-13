package local.workbuddy.alttab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

import org.junit.Test;

public final class BridgeRulesTest {
    @Test
    public void catalogDescribesEveryCurrentBridge() {
        List<BridgeRule> rules = BridgeRules.all();

        assertEquals(Arrays.asList("alt_tab", "windows_key", "arrow_keys", "three_finger_drag"),
                rules.stream().map(BridgeRule::id).collect(Collectors.toList()));
        assertEquals("Alt+Tab", rules.get(0).padInput());
        assertEquals("Alt+F12", rules.get(0).padOutput());
        assertEquals("Windows Alt+Tab", rules.get(0).windowsAction());
        assertEquals("Command / Left Meta", rules.get(1).padInput());
        assertEquals("Scroll Lock", rules.get(1).padOutput());
        assertEquals("左 Windows 键", rules.get(1).windowsAction());
        assertEquals("上 / 下 / 左 / 右", rules.get(2).padInput());
        assertEquals("原键值直通", rules.get(2).padOutput());
        assertEquals("Windows 方向键", rules.get(2).windowsAction());
        assertEquals("三指移动", rules.get(3).padInput());
        assertEquals("虚拟鼠标左键拖动", rules.get(3).padOutput());
        assertEquals("Windows 左键拖动", rules.get(3).windowsAction());
        assertTrue(rules.get(0).requiresComputer());
        assertTrue(rules.get(1).requiresComputer());
        assertFalse(rules.get(2).requiresComputer());
        assertFalse(rules.get(3).requiresComputer());
    }

    @Test(expected = UnsupportedOperationException.class)
    public void catalogCannotBeMutatedByTheDashboard() {
        BridgeRules.all().clear();
    }
}
