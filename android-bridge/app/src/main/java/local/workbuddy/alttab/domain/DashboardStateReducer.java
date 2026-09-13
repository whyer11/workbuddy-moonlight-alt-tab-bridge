package local.workbuddy.alttab.domain;

import java.util.ArrayList;
import java.util.List;

import local.workbuddy.alttab.generated.GeneratedBridgeSchema;

public final class DashboardStateReducer {
    private DashboardStateReducer() {}

    public static DashboardState reduce(BridgeEndpointSnapshot pad,
                                        BridgeEndpointSnapshot computer) {
        List<DashboardState.RuleState> rules = new ArrayList<>();
        boolean healthy = true;
        for (GeneratedBridgeSchema.RuleDefinition definition : GeneratedBridgeSchema.RULES) {
            EndpointState padState = endpointRuleState(pad, definition.id,
                    definition.pad, false);
            EndpointState computerState = endpointRuleState(computer, definition.id,
                    definition.windows, true);
            if (padState != EndpointState.READY
                    || (definition.windows && computerState != EndpointState.READY)) {
                healthy = false;
            }
            rules.add(new DashboardState.RuleState(definition.id, padState, computerState));
        }
        return new DashboardState(healthy ? DashboardState.Overall.HEALTHY
                : DashboardState.Overall.DEGRADED, rules, pad, computer);
    }

    private static EndpointState endpointRuleState(BridgeEndpointSnapshot endpoint,
                                                   String ruleId, boolean required,
                                                   boolean notRequiredAllowed) {
        if (!required) return notRequiredAllowed ? EndpointState.NOT_REQUIRED : EndpointState.UNSUPPORTED;
        if (endpoint.state() == EndpointState.READY) {
            if (endpoint.protocolVersion() != GeneratedBridgeSchema.PROTOCOL_VERSION) {
                return EndpointState.UNKNOWN;
            }
            return endpoint.ruleIds().contains(ruleId)
                    ? EndpointState.READY : EndpointState.UNSUPPORTED;
        }
        return endpoint.state();
    }
}
