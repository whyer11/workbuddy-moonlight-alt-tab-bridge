package local.workbuddy.alttab.domain;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class DashboardState {
    public enum Overall { HEALTHY, DEGRADED }

    public static final class RuleState {
        private final String id;
        private final EndpointState pad;
        private final EndpointState computer;

        RuleState(String id, EndpointState pad, EndpointState computer) {
            this.id = id;
            this.pad = pad;
            this.computer = computer;
        }

        public String id() { return id; }
        public EndpointState pad() { return pad; }
        public EndpointState computer() { return computer; }
    }

    private final Overall overall;
    private final List<RuleState> rules;
    private final Map<String, RuleState> byId;
    private final BridgeEndpointSnapshot pad;
    private final BridgeEndpointSnapshot computer;

    DashboardState(Overall overall, List<RuleState> rules,
                   BridgeEndpointSnapshot pad, BridgeEndpointSnapshot computer) {
        this.overall = overall;
        this.rules = Collections.unmodifiableList(new ArrayList<>(rules));
        LinkedHashMap<String, RuleState> indexed = new LinkedHashMap<>();
        for (RuleState rule : rules) indexed.put(rule.id(), rule);
        this.byId = Collections.unmodifiableMap(indexed);
        this.pad = pad;
        this.computer = computer;
    }

    public Overall overall() { return overall; }
    public List<RuleState> rules() { return rules; }
    public BridgeEndpointSnapshot padEndpoint() { return pad; }
    public BridgeEndpointSnapshot computerEndpoint() { return computer; }
    public RuleState rule(String id) {
        RuleState value = byId.get(id);
        if (value == null) throw new IllegalArgumentException("Unknown bridge rule: " + id);
        return value;
    }
}
