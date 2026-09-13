package local.workbuddy.alttab.domain;

import java.util.Objects;

import local.workbuddy.alttab.generated.GeneratedBridgeSchema;

public final class GestureConfig {
    private final int speedPercent;
    private final int continuationMs;
    private final int reacquireMs;

    private GestureConfig(int speedPercent, int continuationMs, int reacquireMs) {
        this.speedPercent = speedPercent;
        this.continuationMs = continuationMs;
        this.reacquireMs = reacquireMs;
    }

    public static GestureConfig defaults() {
        return normalized(
                GeneratedBridgeSchema.THREE_FINGER_SPEED_PERCENT_DEFAULT,
                GeneratedBridgeSchema.THREE_FINGER_CONTINUATION_MS_DEFAULT,
                GeneratedBridgeSchema.THREE_FINGER_REACQUIRE_MS_DEFAULT);
    }

    public static GestureConfig normalized(
            int speedPercent, int continuationMs, int reacquireMs) {
        return new GestureConfig(
                snap(speedPercent,
                        GeneratedBridgeSchema.THREE_FINGER_SPEED_PERCENT_MIN,
                        GeneratedBridgeSchema.THREE_FINGER_SPEED_PERCENT_MAX,
                        GeneratedBridgeSchema.THREE_FINGER_SPEED_PERCENT_STEP),
                snap(continuationMs,
                        GeneratedBridgeSchema.THREE_FINGER_CONTINUATION_MS_MIN,
                        GeneratedBridgeSchema.THREE_FINGER_CONTINUATION_MS_MAX,
                        GeneratedBridgeSchema.THREE_FINGER_CONTINUATION_MS_STEP),
                snap(reacquireMs,
                        GeneratedBridgeSchema.THREE_FINGER_REACQUIRE_MS_MIN,
                        GeneratedBridgeSchema.THREE_FINGER_REACQUIRE_MS_MAX,
                        GeneratedBridgeSchema.THREE_FINGER_REACQUIRE_MS_STEP));
    }

    private static int snap(int value, int min, int max, int step) {
        int clamped = Math.max(min, Math.min(max, value));
        int snapped = min + Math.round((clamped - min) / (float) step) * step;
        return Math.max(min, Math.min(max, snapped));
    }

    public int speedPercent() {
        return speedPercent;
    }

    public int continuationMs() {
        return continuationMs;
    }

    public int reacquireMs() {
        return reacquireMs;
    }

    public GestureConfig withSpeedPercent(int value) {
        return normalized(value, continuationMs, reacquireMs);
    }

    public GestureConfig withContinuationMs(int value) {
        return normalized(speedPercent, value, reacquireMs);
    }

    public GestureConfig withReacquireMs(int value) {
        return normalized(speedPercent, continuationMs, value);
    }

    @Override
    public boolean equals(Object other) {
        if (this == other) return true;
        if (!(other instanceof GestureConfig)) return false;
        GestureConfig that = (GestureConfig) other;
        return speedPercent == that.speedPercent
                && continuationMs == that.continuationMs
                && reacquireMs == that.reacquireMs;
    }

    @Override
    public int hashCode() {
        return Objects.hash(speedPercent, continuationMs, reacquireMs);
    }

    @Override
    public String toString() {
        return "GestureConfig{speedPercent=" + speedPercent
                + ", continuationMs=" + continuationMs
                + ", reacquireMs=" + reacquireMs + '}';
    }
}
