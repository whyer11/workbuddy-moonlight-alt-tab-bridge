package local.workbuddy.alttab.adapters;

import local.workbuddy.alttab.BridgeController;
import local.workbuddy.alttab.domain.BridgeEndpointSnapshot;
import local.workbuddy.alttab.ports.PadBridgeMonitor;

public final class ShizukuPadBridgeMonitor implements PadBridgeMonitor {
    @Override public BridgeEndpointSnapshot read() { return BridgeController.endpointSnapshot(); }
}
