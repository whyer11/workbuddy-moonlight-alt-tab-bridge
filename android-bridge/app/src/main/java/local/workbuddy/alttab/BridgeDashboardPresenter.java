package local.workbuddy.alttab;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class BridgeDashboardPresenter {
    public static final class RuleView {
        private final String id;
        private final String title;
        private final String pathText;
        private final String configuredText;
        private final String padText;
        private final String computerText;
        private final boolean padRunning;
        private final BridgeDashboardState.ComputerState computerState;

        private RuleView(BridgeDashboardState.RuleRuntime runtime) {
            BridgeRule rule = runtime.rule();
            id = rule.id();
            title = rule.title();
            pathText = rule.padInput() + " → " + rule.padOutput()
                    + " → " + rule.windowsAction();
            configuredText = runtime.configured() ? "已配置" : "未配置";
            padText = runtime.padRunning() ? "Pad 端运行中" : "Pad 端未运行";
            padRunning = runtime.padRunning();
            computerState = runtime.computerState();
            if (computerState == BridgeDashboardState.ComputerState.ONLINE) {
                computerText = "电脑端在线";
            } else if (computerState == BridgeDashboardState.ComputerState.NOT_REQUIRED) {
                computerText = "电脑端无需桥接";
            } else {
                computerText = "电脑端离线";
            }
        }

        public String id() {
            return id;
        }

        public String title() {
            return title;
        }

        public String pathText() {
            return pathText;
        }

        public String configuredText() {
            return configuredText;
        }

        public String padText() {
            return padText;
        }

        public String computerText() {
            return computerText;
        }

        public boolean padRunning() {
            return padRunning;
        }

        public BridgeDashboardState.ComputerState computerState() {
            return computerState;
        }
    }

    public static final class DashboardView {
        private final BridgeDashboardState.OverallStatus overallStatus;
        private final String overallText;
        private final String versionText;
        private final List<RuleView> rules;
        private final Map<String, RuleView> rulesById;

        private DashboardView(BridgeDashboardState.OverallStatus overallStatus,
                              String overallText, String versionText,
                              List<RuleView> rules) {
            this.overallStatus = overallStatus;
            this.overallText = overallText;
            this.versionText = versionText;
            this.rules = Collections.unmodifiableList(new ArrayList<>(rules));
            Map<String, RuleView> indexed = new LinkedHashMap<>();
            for (RuleView rule : rules) {
                indexed.put(rule.id(), rule);
            }
            rulesById = Collections.unmodifiableMap(indexed);
        }

        public BridgeDashboardState.OverallStatus overallStatus() {
            return overallStatus;
        }

        public String overallText() {
            return overallText;
        }

        public String versionText() {
            return versionText;
        }

        public List<RuleView> rules() {
            return rules;
        }

        public RuleView rule(String id) {
            RuleView rule = rulesById.get(id);
            if (rule == null) {
                throw new IllegalArgumentException("Unknown bridge rule: " + id);
            }
            return rule;
        }
    }

    private BridgeDashboardPresenter() {
    }

    public static DashboardView render(BridgeDashboardState state, String appVersion,
                                       int padProtocolVersion, String hostVersion) {
        String overallText;
        if (state.overall() == BridgeDashboardState.OverallStatus.HEALTHY) {
            overallText = "全链路正常";
        } else if (state.overall() == BridgeDashboardState.OverallStatus.PAD_ERROR) {
            overallText = "Pad 端异常";
        } else {
            overallText = "电脑端离线";
        }

        String padVersion = padProtocolVersion > 0
                ? "v" + padProtocolVersion : "未连接";
        String computerVersion = hostVersion == null || hostVersion.isEmpty()
                ? "离线" : hostVersion;
        String versionText = "App " + appVersion
                + " · Pad 协议 " + padVersion
                + " · Windows Host " + computerVersion;

        List<RuleView> ruleViews = new ArrayList<>();
        for (BridgeDashboardState.RuleRuntime runtime : state.rules()) {
            ruleViews.add(new RuleView(runtime));
        }
        return new DashboardView(state.overall(), overallText, versionText, ruleViews);
    }
}
