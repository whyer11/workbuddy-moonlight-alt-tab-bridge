package local.workbuddy.alttab.application;

import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

import local.workbuddy.alttab.BridgeController;
import local.workbuddy.alttab.domain.BridgeEndpointSnapshot;
import local.workbuddy.alttab.domain.DashboardState;
import local.workbuddy.alttab.domain.DashboardStateReducer;
import local.workbuddy.alttab.ports.ComputerBridgeMonitor;
import local.workbuddy.alttab.ports.ComputerHostSettings;
import local.workbuddy.alttab.ports.PadBridgeMonitor;

public final class DashboardCoordinator implements AutoCloseable {
    public interface Listener { void onState(DashboardState state); }
    private static final int ENDPOINT_TIMEOUT_MS = 1000;
    private final PadBridgeMonitor padMonitor;
    private final ComputerBridgeMonitor computerMonitor;
    private final ComputerHostSettings hostSettings;
    private final Executor callbackExecutor;
    private final ExecutorService padProbe = Executors.newSingleThreadExecutor();
    private final ExecutorService computerProbe = Executors.newSingleThreadExecutor();
    private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();
    private final AtomicBoolean running = new AtomicBoolean();
    private final AtomicBoolean refreshActive = new AtomicBoolean();
    private final AtomicBoolean pending = new AtomicBoolean();
    private final AtomicInteger generation = new AtomicInteger();
    private volatile Listener listener;

    public DashboardCoordinator(PadBridgeMonitor padMonitor,
                                ComputerBridgeMonitor computerMonitor,
                                ComputerHostSettings hostSettings,
                                Executor callbackExecutor) {
        this.padMonitor = padMonitor; this.computerMonitor = computerMonitor;
        this.hostSettings = hostSettings; this.callbackExecutor = callbackExecutor;
    }

    public void start(Listener listener) {
        this.listener = listener; running.set(true); generation.incrementAndGet(); refreshNow();
    }

    public void stop() { running.set(false); generation.incrementAndGet(); listener = null; }

    public void refreshNow() {
        if (!running.get()) return;
        if (!refreshActive.compareAndSet(false, true)) { pending.set(true); return; }
        final int currentGeneration = generation.get();
        scheduler.execute(() -> refresh(currentGeneration));
    }

    public String host() { return hostSettings.get(); }
    public void setHost(String host) { hostSettings.set(host); refreshNow(); }
    public boolean setBridgeActive(boolean active) {
        boolean changed = BridgeController.setActive(active); refreshNow(); return changed;
    }

    private void refresh(int currentGeneration) {
        Future<BridgeEndpointSnapshot> padFuture = padProbe.submit(padMonitor::read);
        Future<BridgeEndpointSnapshot> computerFuture = computerProbe.submit(
                () -> computerMonitor.read(hostSettings.get(), ENDPOINT_TIMEOUT_MS));
        long deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(ENDPOINT_TIMEOUT_MS + 250L);
        BridgeEndpointSnapshot pad = await(padFuture, deadline, "pad-timeout");
        BridgeEndpointSnapshot computer = await(computerFuture, deadline, "computer-timeout");
        DashboardState state = DashboardStateReducer.reduce(pad, computer);
        if (running.get() && generation.get() == currentGeneration) {
            Listener current = listener;
            if (current != null) callbackExecutor.execute(() -> {
                if (running.get() && generation.get() == currentGeneration) current.onState(state);
            });
        }
        refreshActive.set(false);
        if (pending.getAndSet(false)) refreshNow();
        else if (running.get() && generation.get() == currentGeneration)
            scheduler.schedule(this::refreshNow,
                    state.overall() == DashboardState.Overall.HEALTHY ? 5 : 2, TimeUnit.SECONDS);
    }

    private static BridgeEndpointSnapshot await(Future<BridgeEndpointSnapshot> future,
                                                 long deadline, String error) {
        try {
            long remaining = deadline - System.nanoTime();
            if (remaining <= 0) throw new java.util.concurrent.TimeoutException();
            return future.get(remaining, TimeUnit.NANOSECONDS);
        } catch (Exception failure) {
            future.cancel(true); return BridgeEndpointSnapshot.unknown(error);
        }
    }

    @Override public void close() {
        stop(); scheduler.shutdownNow(); padProbe.shutdownNow(); computerProbe.shutdownNow();
    }
}
