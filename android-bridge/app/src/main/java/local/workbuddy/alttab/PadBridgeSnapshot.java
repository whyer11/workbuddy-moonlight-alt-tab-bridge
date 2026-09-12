package local.workbuddy.alttab;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class PadBridgeSnapshot {
    private static final PadBridgeSnapshot OFFLINE = new PadBridgeSnapshot(
            false, 0, Collections.emptyList());

    private final boolean running;
    private final int protocolVersion;
    private final List<String> ruleIds;

    private PadBridgeSnapshot(boolean running, int protocolVersion, List<String> ruleIds) {
        this.running = running;
        this.protocolVersion = protocolVersion;
        this.ruleIds = Collections.unmodifiableList(new ArrayList<>(ruleIds));
    }

    public static PadBridgeSnapshot from(boolean binderConnected, boolean nativeAlive,
                                         int protocolVersion, List<String> ruleIds) {
        return new PadBridgeSnapshot(
                binderConnected && nativeAlive,
                protocolVersion,
                ruleIds == null ? Collections.emptyList() : ruleIds);
    }

    public static PadBridgeSnapshot offline() {
        return OFFLINE;
    }

    public boolean running() {
        return running;
    }

    public int protocolVersion() {
        return protocolVersion;
    }

    public List<String> ruleIds() {
        return ruleIds;
    }
}
