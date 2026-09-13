package local.workbuddy.alttab.adapters;

import local.workbuddy.alttab.ComputerStatus;
import local.workbuddy.alttab.ComputerStatusClient;
import local.workbuddy.alttab.domain.BridgeEndpointSnapshot;
import local.workbuddy.alttab.ports.ComputerBridgeMonitor;

public final class UdpComputerBridgeMonitor implements ComputerBridgeMonitor {
    private final ComputerStatusClient client = new ComputerStatusClient();
    @Override public BridgeEndpointSnapshot read(String host, int timeoutMs) {
        ComputerStatus status = client.probe(host, timeoutMs);
        return status.reachable()
                ? BridgeEndpointSnapshot.ready(status.hostVersion(), 2, status.ruleIds())
                : BridgeEndpointSnapshot.offline("udp-timeout");
    }
}
