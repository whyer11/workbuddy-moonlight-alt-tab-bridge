package local.workbuddy.alttab;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.function.IntConsumer;
import local.workbuddy.alttab.adapters.SharedPreferencesComputerHostSettings;
import local.workbuddy.alttab.adapters.SharedPreferencesGestureSettings;
import local.workbuddy.alttab.adapters.ShizukuPadBridgeMonitor;
import local.workbuddy.alttab.adapters.UdpComputerBridgeMonitor;
import local.workbuddy.alttab.application.DashboardCoordinator;
import local.workbuddy.alttab.domain.DashboardState;
import local.workbuddy.alttab.domain.EndpointState;
import local.workbuddy.alttab.domain.GestureConfig;
import local.workbuddy.alttab.generated.GeneratedBridgeSchema;
import local.workbuddy.alttab.ports.GestureSettings;

public final class ProbeActivity extends Activity {
    private final Map<String, RuleCardViews> ruleCards = new LinkedHashMap<>();
    private Handler handler;
    private DashboardCoordinator coordinator;
    private TextView overallStatus;
    private TextView bridgeStatus;
    private TextView versionStatus;
    private EditText computerHost;
    private Button activeToggle;
    private GestureSettings gestureSettings;
    private GestureConfig gestureConfig;
    private boolean bridgeActive = true;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        gestureSettings = new SharedPreferencesGestureSettings(this);
        gestureConfig = gestureSettings.load();
        BridgeController.initialize(this, gestureConfig);
        handler = new Handler(Looper.getMainLooper());
        SharedPreferencesComputerHostSettings hostSettings =
                new SharedPreferencesComputerHostSettings(this);
        coordinator = new DashboardCoordinator(new ShizukuPadBridgeMonitor(),
                new UdpComputerBridgeMonitor(), hostSettings, command -> handler.post(command));

        String deployedHost = getIntent().getStringExtra("computerHost");
        if (deployedHost != null && !deployedHost.trim().isEmpty()) {
            hostSettings.set(deployedHost);
        }

        setContentView(createContentView());
    }

    @Override
    protected void onResume() {
        super.onResume();
        coordinator.start(this::renderDashboard);
    }

    @Override
    protected void onPause() {
        coordinator.stop();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        coordinator.close();
        super.onDestroy();
    }

    private View createContentView() {
        int pagePadding = dp(24);
        ScrollView scrollView = new ScrollView(this);
        scrollView.setFillViewport(true);
        scrollView.setBackgroundColor(Color.rgb(245, 247, 251));

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(pagePadding, pagePadding, pagePadding, pagePadding);
        scrollView.addView(root, new ScrollView.LayoutParams(
                ScrollView.LayoutParams.MATCH_PARENT,
                ScrollView.LayoutParams.WRAP_CONTENT));

        TextView title = text(getString(R.string.bridge_title), 28, Color.rgb(23, 32, 51));
        title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        root.addView(title);

        TextView description = text(
                getString(R.string.bridge_description), 16, Color.rgb(83, 94, 117));
        description.setPadding(0, dp(8), 0, dp(18));
        root.addView(description);

        overallStatus = text("正在检查…", 22, Color.WHITE);
        overallStatus.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        overallStatus.setGravity(Gravity.CENTER);
        overallStatus.setPadding(dp(18), dp(14), dp(18), dp(14));
        setRoundedBackground(overallStatus, Color.rgb(88, 101, 126), 16);
        root.addView(overallStatus, matchWrapWithBottomMargin(14));

        bridgeStatus = text(BridgeController.statusText(), 16, Color.rgb(44, 55, 77));
        root.addView(bridgeStatus, matchWrapWithBottomMargin(10));

        Button start = new Button(this);
        start.setText(R.string.start_bridge);
        start.setAllCaps(false);
        start.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                BridgeController.requestPermissionOrStart();
                requestImmediateRefresh();
            }
        });
        root.addView(start, matchWrapWithBottomMargin(22));

        activeToggle = new Button(this);
        activeToggle.setText(R.string.pause_bridge);
        activeToggle.setAllCaps(false);
        activeToggle.setOnClickListener(view -> {
            boolean requested = !bridgeActive;
            if (coordinator.setBridgeActive(requested)) {
                bridgeActive = requested;
                activeToggle.setText(requested ? R.string.pause_bridge : R.string.resume_bridge);
            }
        });
        root.addView(activeToggle, matchWrapWithBottomMargin(22));

        TextView computerTitle = text(getString(R.string.computer_status_title),
                20, Color.rgb(23, 32, 51));
        computerTitle.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        root.addView(computerTitle, matchWrapWithBottomMargin(8));

        LinearLayout hostRow = new LinearLayout(this);
        hostRow.setOrientation(LinearLayout.HORIZONTAL);
        hostRow.setGravity(Gravity.CENTER_VERTICAL);
        computerHost = new EditText(this);
        computerHost.setSingleLine(true);
        computerHost.setText(coordinator.host());
        computerHost.setHint(R.string.computer_host_hint);
        computerHost.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        computerHost.setImeOptions(EditorInfo.IME_ACTION_DONE);
        hostRow.addView(computerHost, new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));

        Button saveHost = new Button(this);
        saveHost.setText(R.string.save_computer_host);
        saveHost.setAllCaps(false);
        saveHost.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                coordinator.setHost(computerHost.getText().toString());
                computerHost.setText(coordinator.host());
                Toast.makeText(ProbeActivity.this,
                        R.string.computer_host_saved, Toast.LENGTH_SHORT).show();
                coordinator.refreshNow();
            }
        });
        hostRow.addView(saveHost);
        root.addView(hostRow);

        TextView hostHelp = text(getString(R.string.computer_host_help),
                14, Color.rgb(99, 110, 132));
        root.addView(hostHelp, matchWrapWithBottomMargin(22));

        root.addView(createGestureSettingsCard(), matchWrapWithBottomMargin(22));

        TextView rulesTitle = text(getString(R.string.bridge_rules_title),
                22, Color.rgb(23, 32, 51));
        rulesTitle.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        root.addView(rulesTitle, matchWrapWithBottomMargin(10));

        for (BridgeRule rule : BridgeRules.all()) {
            root.addView(createRuleCard(rule), matchWrapWithBottomMargin(12));
        }

        versionStatus = text("", 14, Color.rgb(99, 110, 132));
        versionStatus.setGravity(Gravity.CENTER_HORIZONTAL);
        versionStatus.setPadding(0, dp(8), 0, dp(12));
        root.addView(versionStatus);
        return scrollView;
    }

    private View createGestureSettingsCard() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(18), dp(16), dp(18), dp(16));
        card.setElevation(dp(2));
        setRoundedBackground(card, Color.WHITE, 16);

        TextView title = text(getString(R.string.gesture_settings_title),
                20, Color.rgb(23, 32, 51));
        title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        card.addView(title, matchWrapWithBottomMargin(10));

        card.addView(createSteppedSlider(
                getString(R.string.three_finger_speed),
                "%",
                GeneratedBridgeSchema.THREE_FINGER_SPEED_PERCENT_MIN,
                GeneratedBridgeSchema.THREE_FINGER_SPEED_PERCENT_MAX,
                GeneratedBridgeSchema.THREE_FINGER_SPEED_PERCENT_STEP,
                gestureConfig.speedPercent(),
                value -> saveGestureConfig(gestureConfig.withSpeedPercent(value))));

        card.addView(createSteppedSlider(
                getString(R.string.three_finger_continuation),
                " ms",
                GeneratedBridgeSchema.THREE_FINGER_CONTINUATION_MS_MIN,
                GeneratedBridgeSchema.THREE_FINGER_CONTINUATION_MS_MAX,
                GeneratedBridgeSchema.THREE_FINGER_CONTINUATION_MS_STEP,
                gestureConfig.continuationMs(),
                value -> saveGestureConfig(gestureConfig.withContinuationMs(value))));

        TextView help = text(getString(R.string.gesture_moonlight_help),
                14, Color.rgb(99, 110, 132));
        help.setPadding(0, dp(6), 0, 0);
        card.addView(help);
        return card;
    }

    private View createSteppedSlider(String title,
                                     String suffix,
                                     int min,
                                     int max,
                                     int step,
                                     int current,
                                     IntConsumer onCommitted) {
        LinearLayout container = new LinearLayout(this);
        container.setOrientation(LinearLayout.VERTICAL);

        TextView valueLabel = text(title + "：" + current + suffix,
                16, Color.rgb(44, 55, 77));
        container.addView(valueLabel);

        SeekBar seekBar = new SeekBar(this);
        seekBar.setMax((max - min) / step);
        seekBar.setProgress((current - min) / step);
        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar bar, int progress, boolean fromUser) {
                valueLabel.setText(title + "：" + (min + progress * step) + suffix);
            }

            @Override
            public void onStartTrackingTouch(SeekBar bar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar bar) {
                onCommitted.accept(min + bar.getProgress() * step);
            }
        });
        container.addView(seekBar);
        return container;
    }

    private void saveGestureConfig(GestureConfig next) {
        gestureConfig = next;
        gestureSettings.save(next);
        BridgeController.setGestureConfig(next);
        requestImmediateRefresh();
    }

    private View createRuleCard(BridgeRule rule) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(18), dp(16), dp(18), dp(16));
        card.setElevation(dp(2));
        setRoundedBackground(card, Color.WHITE, 16);

        TextView title = text(rule.title(), 19, Color.rgb(23, 32, 51));
        title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        card.addView(title);

        TextView path = text(rule.padInput() + " → " + rule.padOutput()
                + " → " + rule.windowsAction(), 16, Color.rgb(72, 83, 105));
        path.setPadding(0, dp(7), 0, dp(12));
        card.addView(path);

        LinearLayout statuses = new LinearLayout(this);
        statuses.setOrientation(LinearLayout.HORIZONTAL);
        statuses.setGravity(Gravity.START);
        TextView configured = badge("已配置");
        TextView pad = badge("正在检查 Pad…");
        TextView computer = badge(rule.requiresComputer()
                ? "正在检查电脑…" : "电脑端无需桥接");
        statuses.addView(configured, wrapWrapWithEndMargin(8));
        statuses.addView(pad, wrapWrapWithEndMargin(8));
        statuses.addView(computer);
        card.addView(statuses);

        setBadge(configured, "已配置", Color.rgb(26, 127, 80));
        setBadge(pad, "正在检查 Pad…", Color.rgb(88, 101, 126));
        setBadge(computer, computer.getText().toString(),
                rule.requiresComputer() ? Color.rgb(88, 101, 126) : Color.rgb(69, 92, 122));
        ruleCards.put(rule.id(), new RuleCardViews(configured, pad, computer));
        return card;
    }

    private void requestImmediateRefresh() {
        bridgeStatus.setText(BridgeController.statusText());
        coordinator.refreshNow();
    }

    private void renderDashboard(DashboardState dashboard) {
        boolean healthy = dashboard.overall() == DashboardState.Overall.HEALTHY;
        overallStatus.setText(healthy ? "全链路正常" : "桥接链路异常");
        int overallColor = healthy ? Color.rgb(26, 127, 80) : Color.rgb(184, 117, 25);
        setRoundedBackground(overallStatus, overallColor, 16);

        for (DashboardState.RuleState rule : dashboard.rules()) {
            RuleCardViews views = ruleCards.get(rule.id());
            if (views == null) {
                continue;
            }
            setBadge(views.configured, "已配置", Color.rgb(26, 127, 80));
            setBadge(views.pad, endpointText("Pad", rule.pad()), endpointColor(rule.pad()));
            setBadge(views.computer, endpointText("电脑", rule.computer()),
                    endpointColor(rule.computer()));
        }
        String padVersion = dashboard.padEndpoint().productVersion().isEmpty()
                ? "未知" : dashboard.padEndpoint().productVersion();
        String hostVersion = dashboard.computerEndpoint().productVersion().isEmpty()
                ? "离线" : dashboard.computerEndpoint().productVersion();
        versionStatus.setText("App " + BuildConfig.VERSION_NAME + " · Pad "
                + padVersion + " · Windows Host " + hostVersion);
        bridgeStatus.setText(BridgeController.statusText());
    }

    private static String endpointText(String endpoint, EndpointState state) {
        if (state == EndpointState.READY) return endpoint + "端运行中";
        if (state == EndpointState.NOT_REQUIRED) return endpoint + "端无需桥接";
        if (state == EndpointState.UNSUPPORTED) return endpoint + "端不支持";
        if (state == EndpointState.OFFLINE) return endpoint + "端离线";
        return endpoint + "端状态未知";
    }

    private static int endpointColor(EndpointState state) {
        if (state == EndpointState.READY) return Color.rgb(26, 127, 80);
        if (state == EndpointState.NOT_REQUIRED) return Color.rgb(69, 92, 122);
        if (state == EndpointState.UNSUPPORTED) return Color.rgb(180, 57, 57);
        return Color.rgb(184, 117, 25);
    }

    private TextView text(String value, int sizeSp, int color) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(sizeSp);
        view.setTextColor(color);
        return view;
    }

    private TextView badge(String value) {
        TextView view = text(value, 14, Color.WHITE);
        view.setGravity(Gravity.CENTER);
        view.setPadding(dp(10), dp(6), dp(10), dp(6));
        return view;
    }

    private void setBadge(TextView view, String value, int color) {
        view.setText(value);
        setRoundedBackground(view, color, 20);
    }

    private void setRoundedBackground(View view, int color, int radiusDp) {
        GradientDrawable background = view.getBackground() instanceof GradientDrawable
                ? (GradientDrawable) view.getBackground() : new GradientDrawable();
        background.setColor(color);
        background.setCornerRadius(dp(radiusDp));
        if (view.getBackground() != background) view.setBackground(background);
    }

    private LinearLayout.LayoutParams matchWrapWithBottomMargin(int marginDp) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        params.bottomMargin = dp(marginDp);
        return params;
    }

    private LinearLayout.LayoutParams wrapWrapWithEndMargin(int marginDp) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        params.setMarginEnd(dp(marginDp));
        return params;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private static final class RuleCardViews {
        final TextView configured;
        final TextView pad;
        final TextView computer;

        RuleCardViews(TextView configured, TextView pad, TextView computer) {
            this.configured = configured;
            this.pad = pad;
            this.computer = computer;
        }
    }
}
