package local.workbuddy.alttab;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class ProbeActivity extends Activity {
    private TextView status;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        BridgeController.initialize(this);

        int padding = Math.round(32 * getResources().getDisplayMetrics().density);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER_HORIZONTAL);
        root.setPadding(padding, padding, padding, padding);

        TextView title = new TextView(this);
        title.setText(R.string.bridge_title);
        title.setTextSize(28);
        title.setTextColor(Color.BLACK);
        root.addView(title);

        TextView description = new TextView(this);
        description.setText(R.string.bridge_description);
        description.setTextSize(18);
        description.setTextColor(Color.DKGRAY);
        description.setPadding(0, padding, 0, padding);
        root.addView(description);

        status = new TextView(this);
        status.setTextSize(20);
        status.setTextColor(Color.BLACK);
        root.addView(status);

        Button start = new Button(this);
        start.setText(R.string.start_bridge);
        start.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                BridgeController.requestPermissionOrStart();
                refreshRepeatedly();
            }
        });
        root.addView(start);

        setContentView(root);
        refreshRepeatedly();
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshRepeatedly();
    }

    private void refreshRepeatedly() {
        Handler handler = new Handler(Looper.getMainLooper());
        int[] delays = new int[]{0, 500, 1500, 3000};
        for (int delay : delays) {
            handler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    status.setText(BridgeController.statusText());
                }
            }, delay);
        }
    }
}
