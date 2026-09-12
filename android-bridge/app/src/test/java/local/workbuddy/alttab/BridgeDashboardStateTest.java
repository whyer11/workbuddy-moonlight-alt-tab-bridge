package local.workbuddy.alttab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;

import org.junit.Test;

public final class BridgeDashboardStateTest {
    @Test
    public void aggregateDistinguishesPadFailureComputerOfflineAndHealthy() {
        assertEquals(BridgeDashboardState.OverallStatus.PAD_ERROR,
                BridgeDashboardState.evaluate(false, false, Collections.emptySet()).overall());
        assertEquals(BridgeDashboardState.OverallStatus.COMPUTER_OFFLINE,
                BridgeDashboardState.evaluate(true, false, Collections.emptySet()).overall());
        assertEquals(BridgeDashboardState.OverallStatus.HEALTHY,
                BridgeDashboardState.evaluate(true, true,
                        new HashSet<>(Arrays.asList("alt_tab", "windows_key"))).overall());
    }

    @Test
    public void missingComputerRuleMarksOnlyThatRuleOffline() {
        BridgeDashboardState state = BridgeDashboardState.evaluate(
                true, true, Collections.singleton("alt_tab"));

        assertEquals(BridgeDashboardState.OverallStatus.COMPUTER_OFFLINE, state.overall());
        assertEquals(BridgeDashboardState.ComputerState.ONLINE,
                state.rule("alt_tab").computerState());
        assertEquals(BridgeDashboardState.ComputerState.OFFLINE,
                state.rule("windows_key").computerState());
    }

    @Test
    public void arrowKeysNeverRequireTheComputerHost() {
        BridgeDashboardState.RuleRuntime arrows = BridgeDashboardState.evaluate(
                true, false, Collections.emptySet()).rule("arrow_keys");

        assertTrue(arrows.configured());
        assertTrue(arrows.padRunning());
        assertEquals(BridgeDashboardState.ComputerState.NOT_REQUIRED, arrows.computerState());
    }

    @Test
    public void padFailureIsVisibleOnEveryRule() {
        BridgeDashboardState state = BridgeDashboardState.evaluate(
                false, true, new HashSet<>(Arrays.asList("alt_tab", "windows_key")));

        for (BridgeDashboardState.RuleRuntime rule : state.rules()) {
            assertFalse(rule.padRunning());
        }
    }
}
