package local.workbuddy.alttab.domain;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import org.junit.Test;

public final class GestureConfigTest {
    @Test
    public void defaultsMatchMeasuredBaseline() {
        GestureConfig config = GestureConfig.defaults();

        assertEquals(25, config.speedPercent());
        assertEquals(1000, config.continuationMs());
    }

    @Test
    public void outOfRangeValuesAreClampedAndSnapped() {
        GestureConfig low = GestureConfig.normalized(7, -1);
        GestureConfig high = GestureConfig.normalized(103, 2100);

        assertEquals(10, low.speedPercent());
        assertEquals(0, low.continuationMs());
        assertEquals(100, high.speedPercent());
        assertEquals(2000, high.continuationMs());
    }

    @Test
    public void inRangeValuesSnapToNearestSupportedStep() {
        GestureConfig config = GestureConfig.normalized(28, 1049);

        assertEquals(30, config.speedPercent());
        assertEquals(1000, config.continuationMs());
    }

    @Test
    public void withMethodsReturnNewValueWithoutMutatingOriginal() {
        GestureConfig original = GestureConfig.defaults();
        GestureConfig faster = original.withSpeedPercent(40);
        GestureConfig longer = original.withContinuationMs(1500);

        assertEquals(25, original.speedPercent());
        assertEquals(1000, original.continuationMs());
        assertEquals(40, faster.speedPercent());
        assertEquals(1000, faster.continuationMs());
        assertEquals(25, longer.speedPercent());
        assertEquals(1500, longer.continuationMs());
    }

    @Test
    public void equalityDependsOnBothSettings() {
        GestureConfig first = GestureConfig.normalized(25, 1000);
        GestureConfig same = GestureConfig.normalized(25, 1000);
        GestureConfig differentSpeed = GestureConfig.normalized(30, 1000);
        GestureConfig differentContinuation = GestureConfig.normalized(25, 900);

        assertEquals(first, same);
        assertEquals(first.hashCode(), same.hashCode());
        assertNotEquals(first, differentSpeed);
        assertNotEquals(first, differentContinuation);
    }
}
