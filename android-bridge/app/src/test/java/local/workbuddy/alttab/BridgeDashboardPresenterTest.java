package local.workbuddy.alttab;

import static org.junit.Assert.assertEquals;

import java.util.Arrays;
import java.util.HashSet;

import org.junit.Test;

public final class BridgeDashboardPresenterTest {
    @Test
    public void presenterRendersConfigurationAndRuntimeSeparately() {
        BridgeDashboardState state = BridgeDashboardState.evaluate(
                true, true, new HashSet<>(Arrays.asList("alt_tab", "windows_key")));

        BridgeDashboardPresenter.DashboardView view = BridgeDashboardPresenter.render(
                state, "0.6.0", 1, "0.6.0");

        assertEquals("全链路正常", view.overallText());
        assertEquals("已配置", view.rule("alt_tab").configuredText());
        assertEquals("Pad 端运行中", view.rule("alt_tab").padText());
        assertEquals("电脑端在线", view.rule("alt_tab").computerText());
        assertEquals("电脑端无需桥接", view.rule("arrow_keys").computerText());
        assertEquals("Alt+Tab → Alt+F12 → Windows Alt+Tab",
                view.rule("alt_tab").pathText());
        assertEquals("App 0.6.0 · Pad 协议 v1 · Windows Host 0.6.0",
                view.versionText());
    }

    @Test
    public void presenterExplainsPadAndComputerFailures() {
        BridgeDashboardPresenter.DashboardView padFailure = BridgeDashboardPresenter.render(
                BridgeDashboardState.evaluate(false, false, new HashSet<>()),
                "0.6.0", 0, "");
        assertEquals("Pad 端异常", padFailure.overallText());
        assertEquals("Pad 端未运行", padFailure.rule("windows_key").padText());
        assertEquals("电脑端离线", padFailure.rule("windows_key").computerText());
        assertEquals("App 0.6.0 · Pad 协议 未连接 · Windows Host 离线",
                padFailure.versionText());

        BridgeDashboardPresenter.DashboardView hostFailure = BridgeDashboardPresenter.render(
                BridgeDashboardState.evaluate(true, false, new HashSet<>()),
                "0.6.0", 1, "");
        assertEquals("电脑端离线", hostFailure.overallText());
    }
}
