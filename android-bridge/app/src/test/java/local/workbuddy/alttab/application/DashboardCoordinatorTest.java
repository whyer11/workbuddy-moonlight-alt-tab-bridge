package local.workbuddy.alttab.application;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

import org.junit.Test;

import local.workbuddy.alttab.domain.BridgeEndpointSnapshot;
import local.workbuddy.alttab.ports.ComputerBridgeMonitor;
import local.workbuddy.alttab.ports.ComputerHostSettings;
import local.workbuddy.alttab.ports.PadBridgeMonitor;

public final class DashboardCoordinatorTest {
    @Test
    public void padAndComputerProbesStartIndependently() throws Exception {
        CountDownLatch bothStarted = new CountDownLatch(2);
        PadBridgeMonitor pad = () -> { bothStarted.countDown(); await(bothStarted); return readyAll(); };
        ComputerBridgeMonitor computer = (host, timeout) -> {
            bothStarted.countDown(); await(bothStarted); return readyComputer();
        };
        CountDownLatch delivered = new CountDownLatch(1);
        DashboardCoordinator coordinator = new DashboardCoordinator(
                pad, computer, new MemoryHost(), Runnable::run);
        try {
            coordinator.start(state -> delivered.countDown());
            assertTrue(bothStarted.await(500, TimeUnit.MILLISECONDS));
            assertTrue(delivered.await(1000, TimeUnit.MILLISECONDS));
        } finally { coordinator.close(); }
    }

    @Test
    public void stopSuppressesLateCallback() throws Exception {
        CountDownLatch release = new CountDownLatch(1);
        PadBridgeMonitor pad = () -> { await(release); return readyAll(); };
        ComputerBridgeMonitor computer = (host, timeout) -> { await(release); return readyComputer(); };
        AtomicBoolean callback = new AtomicBoolean();
        DashboardCoordinator coordinator = new DashboardCoordinator(
                pad, computer, new MemoryHost(), Runnable::run);
        coordinator.start(state -> callback.set(true));
        coordinator.stop(); release.countDown();
        Thread.sleep(200);
        coordinator.close();
        assertFalse(callback.get());
    }

    private static BridgeEndpointSnapshot readyAll() {
        return BridgeEndpointSnapshot.ready("0.7.0", 2,
                new LinkedHashSet<>(Arrays.asList("alt_tab", "windows_key", "arrow_keys")));
    }
    private static BridgeEndpointSnapshot readyComputer() {
        return BridgeEndpointSnapshot.ready("0.7.0", 2,
                new LinkedHashSet<>(Arrays.asList("alt_tab", "windows_key")));
    }
    private static void await(CountDownLatch latch) {
        try { latch.await(1, TimeUnit.SECONDS); }
        catch (InterruptedException error) { Thread.currentThread().interrupt(); }
    }
    private static final class MemoryHost implements ComputerHostSettings {
        private String host = "127.0.0.1";
        public String get() { return host; }
        public void set(String value) { host = value; }
    }
}
