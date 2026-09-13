package local.workbuddy.alttab.ports;

import local.workbuddy.alttab.domain.BridgeEndpointSnapshot;

public interface ComputerBridgeMonitor { BridgeEndpointSnapshot read(String host, int timeoutMs); }
