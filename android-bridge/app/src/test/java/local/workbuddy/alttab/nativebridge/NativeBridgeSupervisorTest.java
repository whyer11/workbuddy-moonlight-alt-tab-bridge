package local.workbuddy.alttab.nativebridge;

import static org.junit.Assert.assertEquals;

import java.util.Arrays;

import org.junit.Test;

import local.workbuddy.alttab.domain.GestureConfig;

public final class NativeBridgeSupervisorTest {
    @Test
    public void commandContainsBothGestureSettingsInStableOrder() {
        assertEquals(Arrays.asList(
                        "/data/local/tmp/alttab_evdev_bridge",
                        "--three-finger-speed-percent", "25",
                        "--three-finger-continuation-ms", "1000"),
                NativeBridgeSupervisor.commandFor(
                        "/data/local/tmp/alttab_evdev_bridge",
                        GestureConfig.defaults()));
    }

    @Test
    public void commandUsesCurrentNonDefaultValues() {
        assertEquals(Arrays.asList(
                        "/bridge",
                        "--three-finger-speed-percent", "40",
                        "--three-finger-continuation-ms", "500"),
                NativeBridgeSupervisor.commandFor(
                        "/bridge", GestureConfig.normalized(40, 500)));
    }
}
