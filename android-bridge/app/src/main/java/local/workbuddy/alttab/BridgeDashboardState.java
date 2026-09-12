package local.workbuddy.alttab;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

public final class BridgeDashboardState {
    public enum OverallStatus {
        PAD_ERROR,
        COMPUTER_OFFLINE,
        HEALTHY
    }

    public enum ComputerState {
        ONLINE,
        OFFLINE,
        NOT_REQUIRED
    }

    public static final class RuleRuntime {
        private final BridgeRule rule;
        private final boolean configured;
        private final boolean padRunning;
        private final ComputerState computerState;

        private RuleRuntime(BridgeRule rule, boolean configured, boolean padRunning,
                            ComputerState computerState) {
            this.rule = rule;
            this.configured = configured;
            this.padRunning = padRunning;
            this.computerState = computerState;
        }

        public BridgeRule rule() {
            return rule;
        }

        public boolean configured() {
            return configured;
        }

        public boolean padRunning() {
            return padRunning;
        }

        public ComputerState computerState() {
            return computerState;
        }
    }

    private final OverallStatus overall;
    private final List<RuleRuntime> rules;
    private final Map<String, RuleRuntime> rulesById;

    private BridgeDashboardState(OverallStatus overall, List<RuleRuntime> rules) {
        this.overall = overall;
        this.rules = Collections.unmodifiableList(new ArrayList<>(rules));
        Map<String, RuleRuntime> indexed = new LinkedHashMap<>();
        for (RuleRuntime runtime : rules) {
            indexed.put(runtime.rule().id(), runtime);
        }
        rulesById = Collections.unmodifiableMap(indexed);
    }

    public static BridgeDashboardState evaluate(boolean padRunning,
                                                boolean computerReachable,
                                                Set<String> computerRuleIds) {
        List<RuleRuntime> runtimeRules = new ArrayList<>();
        boolean allRequiredComputerRulesOnline = true;
        for (BridgeRule rule : BridgeRules.all()) {
            ComputerState computerState;
            if (!rule.requiresComputer()) {
                computerState = ComputerState.NOT_REQUIRED;
            } else if (computerReachable && computerRuleIds.contains(rule.id())) {
                computerState = ComputerState.ONLINE;
            } else {
                computerState = ComputerState.OFFLINE;
                allRequiredComputerRulesOnline = false;
            }
            runtimeRules.add(new RuleRuntime(rule, true, padRunning, computerState));
        }

        OverallStatus overall;
        if (!padRunning) {
            overall = OverallStatus.PAD_ERROR;
        } else if (!allRequiredComputerRulesOnline) {
            overall = OverallStatus.COMPUTER_OFFLINE;
        } else {
            overall = OverallStatus.HEALTHY;
        }
        return new BridgeDashboardState(overall, runtimeRules);
    }

    public OverallStatus overall() {
        return overall;
    }

    public List<RuleRuntime> rules() {
        return rules;
    }

    public RuleRuntime rule(String id) {
        RuleRuntime runtime = rulesById.get(id);
        if (runtime == null) {
            throw new IllegalArgumentException("Unknown bridge rule: " + id);
        }
        return runtime;
    }
}
