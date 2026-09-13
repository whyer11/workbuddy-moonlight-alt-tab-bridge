package local.workbuddy.alttab.nativebridge;

import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.Set;

public final class NativeBridgeStatus {
    private final boolean ready;
    private final String version;
    private final int protocol;
    private final Set<String> ruleIds;
    private final String physicalPath;
    private final String errorCode;

    private NativeBridgeStatus(boolean ready, String version, int protocol,
                               Set<String> ruleIds, String physicalPath, String errorCode) {
        this.ready = ready; this.version = version; this.protocol = protocol;
        this.ruleIds = Collections.unmodifiableSet(new LinkedHashSet<>(ruleIds));
        this.physicalPath = physicalPath; this.errorCode = errorCode;
    }

    public static NativeBridgeStatus ready(String version, int protocol,
                                           Set<String> rules, String physicalPath) {
        return new NativeBridgeStatus(true, version, protocol, rules,
                physicalPath == null ? "" : physicalPath, "");
    }
    public static NativeBridgeStatus error(String code) {
        return new NativeBridgeStatus(false, "", 0, Collections.emptySet(), "", code);
    }
    public boolean ready() { return ready; }
    public String version() { return version; }
    public int protocol() { return protocol; }
    public Set<String> ruleIds() { return ruleIds; }
    public String physicalPath() { return physicalPath; }
    public String errorCode() { return errorCode; }
}
