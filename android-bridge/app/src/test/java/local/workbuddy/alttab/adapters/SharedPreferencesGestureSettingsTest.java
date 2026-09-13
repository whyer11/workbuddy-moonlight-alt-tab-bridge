package local.workbuddy.alttab.adapters;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

import local.workbuddy.alttab.domain.GestureConfig;

public final class SharedPreferencesGestureSettingsTest {
    @Test
    public void missingValuesLoadMeasuredDefaults() {
        FakeStorage storage = new FakeStorage();

        GestureConfig config = new SharedPreferencesGestureSettings(storage).load();

        assertEquals(25, config.speedPercent());
        assertEquals(1000, config.continuationMs());
        assertEquals(150, config.reacquireMs());
    }

    @Test
    public void loadedValuesAreNormalizedAtTheBoundary() {
        FakeStorage storage = new FakeStorage();
        storage.speed = 103;
        storage.continuation = -1;
        storage.reacquire = 501;

        GestureConfig config = new SharedPreferencesGestureSettings(storage).load();

        assertEquals(100, config.speedPercent());
        assertEquals(0, config.continuationMs());
        assertEquals(500, config.reacquireMs());
    }

    @Test
    public void savePersistsBothNormalizedValuesAtomically() {
        FakeStorage storage = new FakeStorage();
        SharedPreferencesGestureSettings settings =
                new SharedPreferencesGestureSettings(storage);

        settings.save(GestureConfig.normalized(28, 1549, 237));

        assertEquals(30, storage.speed);
        assertEquals(1500, storage.continuation);
        assertEquals(237, storage.reacquire);
        assertEquals(1, storage.saveCount);
    }

    private static final class FakeStorage
            implements SharedPreferencesGestureSettings.Storage {
        private int speed = Integer.MIN_VALUE;
        private int continuation = Integer.MIN_VALUE;
        private int reacquire = Integer.MIN_VALUE;
        private int saveCount;

        @Override
        public int readSpeedPercent(int fallback) {
            return speed == Integer.MIN_VALUE ? fallback : speed;
        }

        @Override
        public int readContinuationMs(int fallback) {
            return continuation == Integer.MIN_VALUE ? fallback : continuation;
        }

        @Override
        public int readReacquireMs(int fallback) {
            return reacquire == Integer.MIN_VALUE ? fallback : reacquire;
        }

        @Override
        public void save(int speedPercent, int continuationMs, int reacquireMs) {
            speed = speedPercent;
            continuation = continuationMs;
            reacquire = reacquireMs;
            saveCount++;
        }
    }
}
