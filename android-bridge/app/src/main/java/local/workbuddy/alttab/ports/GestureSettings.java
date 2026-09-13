package local.workbuddy.alttab.ports;

import local.workbuddy.alttab.domain.GestureConfig;

public interface GestureSettings {
    GestureConfig load();
    void save(GestureConfig config);
}
