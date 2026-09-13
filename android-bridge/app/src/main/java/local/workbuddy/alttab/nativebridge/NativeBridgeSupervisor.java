package local.workbuddy.alttab.nativebridge;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

import local.workbuddy.alttab.domain.GestureConfig;

public final class NativeBridgeSupervisor implements AutoCloseable {
    private static final long[] BACKOFF_MILLIS = {1000, 2000, 4000, 8000};
    private final String executable;
    private final AtomicReference<NativeBridgeStatus> status =
            new AtomicReference<>(NativeBridgeStatus.error("starting"));
    private final AtomicReference<GestureConfig> gestureConfig =
            new AtomicReference<>(GestureConfig.defaults());
    private final AtomicLong configGeneration = new AtomicLong();
    private final Object signal = new Object();
    private volatile boolean active = true;
    private volatile boolean closed;
    private volatile Process process;
    private final Thread thread;

    public NativeBridgeSupervisor(String executable) {
        this.executable = executable;
        thread = new Thread(this::run, "WorkBuddyBridgeSupervisor");
        thread.setDaemon(true);
        thread.start();
    }

    public NativeBridgeStatus snapshot() { return status.get(); }
    public boolean active() { return active; }

    static List<String> commandFor(String executable, GestureConfig config) {
        return Arrays.asList(
                executable,
                "--three-finger-speed-percent", String.valueOf(config.speedPercent()),
                "--three-finger-continuation-ms", String.valueOf(config.continuationMs()));
    }

    public void setGestureConfig(GestureConfig value) {
        GestureConfig next = GestureConfig.normalized(
                value.speedPercent(), value.continuationMs());
        GestureConfig previous = gestureConfig.getAndSet(next);
        if (previous.equals(next)) {
            return;
        }
        configGeneration.incrementAndGet();
        Process current = process;
        if (current != null) {
            current.destroy();
        }
        if (active) {
            status.set(NativeBridgeStatus.error("restarting-config"));
        }
        synchronized (signal) { signal.notifyAll(); }
    }

    public void setActive(boolean value) {
        active = value;
        if (!value) {
            Process current = process;
            if (current != null) current.destroy();
            status.set(NativeBridgeStatus.error("paused"));
        }
        synchronized (signal) { signal.notifyAll(); }
    }

    private void run() {
        int failures = 0;
        while (!closed) {
            if (!active) { waitForSignal(0); continue; }
            long launchedGeneration = configGeneration.get();
            try {
                Process current = new ProcessBuilder(
                        commandFor(executable, gestureConfig.get()))
                        .redirectErrorStream(true)
                        .start();
                process = current;
                boolean sawReady = false;
                try (BufferedReader reader = new BufferedReader(new InputStreamReader(current.getInputStream()))) {
                    String line;
                    while (!closed && active && (line = reader.readLine()) != null) {
                        NativeBridgeStatus parsed = NativeCapabilityParser.parse(line);
                        if (parsed.ready() && line.startsWith("WB_NATIVE_READY_V2")) {
                            status.set(parsed); sawReady = true; failures = 0;
                        }
                    }
                }
                current.waitFor();
                process = null;
                if (!closed && active
                        && launchedGeneration != configGeneration.get()) {
                    failures = 0;
                    continue;
                }
                if (!closed && active) status.set(NativeBridgeStatus.error(
                        sawReady ? "native-exited" : "ready-timeout"));
            } catch (Throwable error) {
                process = null;
                if (!closed && active) status.set(NativeBridgeStatus.error("launch-failed"));
            }
            if (!closed && active) waitForSignal(BACKOFF_MILLIS[Math.min(failures++, BACKOFF_MILLIS.length - 1)]);
        }
    }

    private void waitForSignal(long millis) {
        synchronized (signal) {
            try { if (!closed && (millis > 0 || !active)) signal.wait(millis == 0 ? 0 : millis); }
            catch (InterruptedException ignored) { Thread.currentThread().interrupt(); }
        }
    }

    @Override public void close() {
        closed = true;
        Process current = process;
        if (current != null) current.destroy();
        synchronized (signal) { signal.notifyAll(); }
        try { thread.join(1500); } catch (InterruptedException ignored) { Thread.currentThread().interrupt(); }
        current = process;
        if (current != null && current.isAlive()) current.destroyForcibly();
    }
}
