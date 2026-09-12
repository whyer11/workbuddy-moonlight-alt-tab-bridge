package local.workbuddy.alttab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.util.Arrays;

import org.junit.Test;

public final class PadBridgeSnapshotTest {
    @Test
    public void snapshotRunsOnlyWhenBinderAndNativeProcessAreAlive() {
        assertFalse(PadBridgeSnapshot.from(
                true, false, 1, Arrays.asList("alt_tab")).running());
        assertFalse(PadBridgeSnapshot.from(
                false, true, 1, Arrays.asList("alt_tab")).running());
        assertTrue(PadBridgeSnapshot.from(
                true, true, 1,
                Arrays.asList("alt_tab", "windows_key", "arrow_keys")).running());
    }

    @Test
    public void offlineSnapshotCarriesNoStaleVersionOrRules() {
        PadBridgeSnapshot snapshot = PadBridgeSnapshot.offline();

        assertFalse(snapshot.running());
        assertEquals(0, snapshot.protocolVersion());
        assertTrue(snapshot.ruleIds().isEmpty());
    }

    @Test(expected = UnsupportedOperationException.class)
    public void ruleIdsCannotBeMutatedByTheDashboard() {
        PadBridgeSnapshot.from(true, true, 1, Arrays.asList("alt_tab"))
                .ruleIds().clear();
    }
}
