package local.workbuddy.alttab.nativebridge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public final class NativeCapabilityParserTest {
    @Test
    public void parsesStrictDescriptionAndReadyLines() {
        NativeBridgeStatus description = NativeCapabilityParser.parse(
                "WB_NATIVE_V2 version=0.7.0 protocol=2 rules=alt_tab,windows_key,arrow_keys");
        assertTrue(description.ready());
        assertEquals(3, description.ruleIds().size());

        NativeBridgeStatus ready = NativeCapabilityParser.parse(
                "WB_NATIVE_READY_V2 version=0.7.0 protocol=2 rules=alt_tab,windows_key,arrow_keys physical=/dev/input/event18");
        assertTrue(ready.ready());
        assertEquals("/dev/input/event18", ready.physicalPath());
    }

    @Test
    public void rejectsDuplicateUnknownAndMismatchedFields() {
        assertFalse(NativeCapabilityParser.parse("WB_NATIVE_V2 version=0.7.0 protocol=2 rules=alt_tab rules=windows_key").ready());
        assertFalse(NativeCapabilityParser.parse("WB_NATIVE_V2 version=0.7.0 protocol=2 rules=alt_tab extra=x").ready());
        assertFalse(NativeCapabilityParser.parse("WB_NATIVE_V2 version=0.6.0 protocol=1 rules=alt_tab").ready());
        assertFalse(NativeCapabilityParser.parse("garbage").ready());
    }
}
