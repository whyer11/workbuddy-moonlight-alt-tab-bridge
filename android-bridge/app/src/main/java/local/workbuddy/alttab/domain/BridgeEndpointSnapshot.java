package local.workbuddy.alttab.domain;

import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.Set;

public final class BridgeEndpointSnapshot {
    private final EndpointState state;
    private final String productVersion;
    private final int protocolVersion;
    private final Set<String> ruleIds;
    private final String errorCode;

    private BridgeEndpointSnapshot(EndpointState state, String productVersion,
                                   int protocolVersion, Set<String> ruleIds, String errorCode) {
        this.state = state;
        this.productVersion = productVersion == null ? "" : productVersion;
        this.protocolVersion = protocolVersion;
        this.ruleIds = Collections.unmodifiableSet(new LinkedHashSet<>(
                ruleIds == null ? Collections.emptySet() : ruleIds));
        this.errorCode = errorCode == null ? "" : errorCode;
    }

    public static BridgeEndpointSnapshot ready(String productVersion, int protocolVersion,
                                               Set<String> ruleIds) {
        return new BridgeEndpointSnapshot(EndpointState.READY, productVersion,
                protocolVersion, ruleIds, "");
    }

    public static BridgeEndpointSnapshot offline(String errorCode) {
        return new BridgeEndpointSnapshot(EndpointState.OFFLINE, "", 0,
                Collections.emptySet(), errorCode);
    }

    public static BridgeEndpointSnapshot unknown(String errorCode) {
        return new BridgeEndpointSnapshot(EndpointState.UNKNOWN, "", 0,
                Collections.emptySet(), errorCode);
    }

    public EndpointState state() { return state; }
    public String productVersion() { return productVersion; }
    public int protocolVersion() { return protocolVersion; }
    public Set<String> ruleIds() { return ruleIds; }
    public String errorCode() { return errorCode; }
}
