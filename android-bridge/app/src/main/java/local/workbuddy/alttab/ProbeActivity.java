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
import android.widget.TextView;
import android.widget.Toast;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class ProbeActivity extends Activity {
    private static final long REFRESH_INTERVAL_MILLIS = 2000;
    private static final int COMPUTER_TIMEOUT_MILLIS = 800;

    private final Map<String, RuleCardViews> ruleCards = new LinkedHashMap<>();
    private Handler handler;
    private ExecutorService statusExecutor;
    private ComputerHostStore hostStore;
    private ComputerStatusClient computerStatusClient;
    private TextView overallStatus;
    private TextView bridgeStatus;
    private TextView versionStatus;
    private EditText computerHost;
    private boolean resumed;
    private volatile int refreshGeneration;

    private final Runnable scheduledRefresh = new Runnable() {
        @Override
        public void run() {
            refreshDashboard();
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        BridgeController.initialize(this);
        handler = new Handler(Looper.getMainLooper());
        statusExecutor = Executors.newSingleThreadExecutor();
        hostStore = new ComputerHostStore(this);
        computerStatusClient = new ComputerStatusClient();

        String deployedHost = getIntent().getStringExtra("computerHost");
        if (deployedHost != null && !deployedHost.trim().isEmpty()) {
            hostStore.set(deployedHost);
        }

        setContentView(createContentView());
    }

    @Override
    protected void onResume() {
        super.onResume();
        resumed = true;
        handler.removeCallbacks(scheduledRefresh);
        refreshDashboard();
    }

    @Override
    protected void onPause() {
        resumed = false;
        refreshGeneration++;
        handler.removeCallbacks(scheduledRefresh);
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        handler.removeCallbacks(scheduledRefresh);
        statusExecutor.shutdownNow();
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

        TextView computerTitle = text(getString(R.string.computer_status_title),
                20, Color.rgb(23, 32, 51));
        computerTitle.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        root.addView(computerTitle, matchWrapWithBottomMargin(8));

        LinearLayout hostRow = new LinearLayout(this);
        hostRow.setOrientation(LinearLayout.HORIZONTAL);
        hostRow.setGravity(Gravity.CENTER_VERTICAL);
        computerHost = new EditText(this);
        computerHost.setSingleLine(true);
        computerHost.setText(hostStore.get());
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
                hostStore.set(computerHost.getText().toString());
                computerHost.setText(hostStore.get());
                Toast.makeText(ProbeActivity.this,
                        R.string.computer_host_saved, Toast.LENGTH_SHORT).show();
                requestImmediateRefresh();
            }
        });
        hostRow.addView(saveHost);
        root.addView(hostRow);

        TextView hostHelp = text(getString(R.string.computer_host_help),
                14, Color.rgb(99, 110, 132));
        root.addView(hostHelp, matchWrapWithBottomMargin(22));

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
        handler.removeCallbacks(scheduledRefresh);
        refreshGeneration++;
        refreshDashboard();
    }

    private void refreshDashboard() {
        if (!resumed) {
            return;
        }
        final int generation = ++refreshGeneration;
        bridgeStatus.setText(BridgeController.statusText());
        final String host = hostStore.get();

        statusExecutor.execute(new Runnable() {
            @Override
            public void run() {
                PadBridgeSnapshot pad = BridgeController.padSnapshot();
                ComputerStatus computer = computerStatusClient.probe(
                        host, COMPUTER_TIMEOUT_MILLIS);
                BridgeDashboardState state = BridgeDashboardState.evaluate(
                        pad.running(), computer.reachable(), computer.ruleIds());
                final BridgeDashboardPresenter.DashboardView dashboard =
                        BridgeDashboardPresenter.render(
                                state, BuildConfig.VERSION_NAME,
                                pad.protocolVersion(), computer.hostVersion());
                handler.post(new Runnable() {
                    @Override
                    public void run() {
                        if (!resumed || generation != refreshGeneration || isDestroyed()) {
                            return;
                        }
                        renderDashboard(dashboard);
                        bridgeStatus.setText(BridgeController.statusText());
                        handler.postDelayed(scheduledRefresh, REFRESH_INTERVAL_MILLIS);
                    }
                });
            }
        });
    }

    private void renderDashboard(BridgeDashboardPresenter.DashboardView dashboard) {
        overallStatus.setText(dashboard.overallText());
        int overallColor;
        if (dashboard.overallStatus() == BridgeDashboardState.OverallStatus.HEALTHY) {
            overallColor = Color.rgb(26, 127, 80);
        } else if (dashboard.overallStatus() == BridgeDashboardState.OverallStatus.PAD_ERROR) {
            overallColor = Color.rgb(180, 57, 57);
        } else {
            overallColor = Color.rgb(184, 117, 25);
        }
        setRoundedBackground(overallStatus, overallColor, 16);

        for (BridgeDashboardPresenter.RuleView rule : dashboard.rules()) {
            RuleCardViews views = ruleCards.get(rule.id());
            if (views == null) {
                continue;
            }
            setBadge(views.configured, rule.configuredText(), Color.rgb(26, 127, 80));
            setBadge(views.pad, rule.padText(), rule.padRunning()
                    ? Color.rgb(26, 127, 80) : Color.rgb(180, 57, 57));
            int computerColor;
            if (rule.computerState() == BridgeDashboardState.ComputerState.ONLINE) {
                computerColor = Color.rgb(26, 127, 80);
            } else if (rule.computerState()
                    == BridgeDashboardState.ComputerState.NOT_REQUIRED) {
                computerColor = Color.rgb(69, 92, 122);
            } else {
                computerColor = Color.rgb(184, 117, 25);
            }
            setBadge(views.computer, rule.computerText(), computerColor);
        }
        versionStatus.setText(dashboard.versionText());
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
        GradientDrawable background = new GradientDrawable();
        background.setColor(color);
        background.setCornerRadius(dp(radiusDp));
        view.setBackground(background);
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
