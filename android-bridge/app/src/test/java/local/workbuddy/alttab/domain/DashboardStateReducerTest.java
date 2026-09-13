package local.workbuddy.alttab.domain;

import static org.junit.Assert.assertEquals;

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashSet;

import org.junit.Test;

public final class DashboardStateReducerTest {
    @Test
    public void missingPadCapabilityIsUnsupportedOnlyForThatRule() {
        BridgeEndpointSnapshot pad = BridgeEndpointSnapshot.ready(
                "0.7.0", 2, ids("alt_tab", "arrow_keys"));
        BridgeEndpointSnapshot computer = BridgeEndpointSnapshot.ready(
                "0.7.0", 2, ids("alt_tab", "windows_key"));

        DashboardState state = DashboardStateReducer.reduce(pad, computer);

        assertEquals(EndpointState.READY, state.rule("alt_tab").pad());
        assertEquals(EndpointState.UNSUPPORTED, state.rule("windows_key").pad());
        assertEquals(EndpointState.READY, state.rule("arrow_keys").pad());
    }

    @Test
    public void computerOfflineDoesNotAffectPadOnlyArrowRule() {
        DashboardState state = DashboardStateReducer.reduce(
                BridgeEndpointSnapshot.ready("0.7.0", 2,
                        ids("alt_tab", "windows_key", "arrow_keys")),
                BridgeEndpointSnapshot.offline("timeout"));

        assertEquals(EndpointState.OFFLINE, state.rule("alt_tab").computer());
        assertEquals(EndpointState.NOT_REQUIRED, state.rule("arrow_keys").computer());
        assertEquals(DashboardState.Overall.DEGRADED, state.overall());
    }

    @Test
    public void protocolMismatchIsUnknownInsteadOfReady() {
        BridgeEndpointSnapshot pad = BridgeEndpointSnapshot.ready(
                "0.6.0", 1, ids("alt_tab", "windows_key", "arrow_keys"));

        DashboardState state = DashboardStateReducer.reduce(
                pad, BridgeEndpointSnapshot.offline("not-checked"));

        assertEquals(EndpointState.UNKNOWN, state.rule("alt_tab").pad());
        assertEquals(EndpointState.UNKNOWN, state.rule("windows_key").pad());
        assertEquals(EndpointState.UNKNOWN, state.rule("arrow_keys").pad());
    }

    private static LinkedHashSet<String> ids(String... values) {
        return new LinkedHashSet<>(Arrays.asList(values));
    }
}
