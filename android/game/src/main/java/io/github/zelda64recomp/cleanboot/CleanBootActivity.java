package io.github.zelda64recomp.cleanboot;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.util.Log;
import android.view.Gravity;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.BufferedWriter;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public final class CleanBootActivity extends SDLActivity implements SensorEventListener {
    private static final String TAG = "ZeldaCleanBoot";
    private static final int OPEN_FILE_REQUEST = 1;
    private static final int OPEN_MOD_FILES_REQUEST = 2;
    private static final int OPEN_DRIVER_FILE_REQUEST = 3;
    private static final int OPEN_CLOCK_TEXTURE_FILE_REQUEST = 4;
    private static final int OPEN_LEGACY_DATA_TREE_REQUEST = 5;
    private static final int OPEN_LEGACY_DATA_STAGING_TREE_REQUEST = 6;
    private static final int OPEN_UPGRADE_DATA_TREE_REQUEST = 7;
    private static final String RELEASE_PACKAGE = "io.github.zelda64recomp";
    private static final String LEGACY_DATA_AUTHORITY = "com.android.externalstorage.documents";
    private static final String LEGACY_DATA_DOCUMENT_ID = "primary:Zelda64";
    private static final String PREF_UPGRADE_DATA_DECIDED = "upgradeDataDecided";
    private static final String CUSTOM_DRIVER_DIRECTORY = "custom_driver";
    private static final String CUSTOM_DRIVER_NAME_FILE = "selected_driver.txt";
    private static final String CUSTOM_DRIVER_DISPLAY_NAME_FILE = "selected_driver_display.txt";
    private static final String CLOCK_TEXTURE_DIRECTORY = "clock_texture_packs";
    private static final String CLOCK_TEXTURE_FILE = "clock_texture_pack.o2r";
    private static final String CLOCK_TEXTURE_DISPLAY_FILE = "clock_texture_pack_display.txt";
    private static final String MIGRATION_DIRECTORY = "migration";
    private static final String LEGACY_INVENTORY_FILE = "legacy_inventory.tsv";
    private static final String MIGRATION_STAGING_DIRECTORY = "staging";
    private static final String MIGRATION_COPY_JOURNAL_FILE = "copy_journal.tsv";
    private static final String MIGRATION_SOURCE_FILE = "source_uri.txt";
    private static final String MIGRATION_STAGING_COMPLETE_FILE = "staging_complete.txt";
    private static final String MIGRATION_BACKUP_DIRECTORY = "backup";
    private static final String MIGRATION_ACTIVATION_REQUEST_FILE = "activate_on_next_launch";
    private static final String MIGRATION_ACTIVATION_JOURNAL_FILE = "activation_journal.tsv";
    private static final String MIGRATION_ACTIVATION_COMPLETE_FILE = "activation_complete.txt";
    private static final String PREFERENCES_NAME = "io.github.zelda64recomp.prefs";
    private static final String PREF_CONTROLS_HIDDEN = "touchControlsHidden";
    private static final String PREF_CONTROLS_DISABLED = "touchControlsDisabled";
    private static final String PREF_FACE_LAYOUT = "touchFaceButtonLayout";
    private static final String PREF_TOUCH_STYLE = "integratedTouchStyle";
    private static final String PREF_TOUCH_SCALE = "integratedTouchScale";
    private static final String PREF_TOUCH_OPACITY = "integratedTouchOpacity";
    private static final String PREF_TOUCH_FLOATING_LEFT = "touchFloatingLeftStick";
    private static final String PREF_TOUCH_FLOATING_RIGHT = "touchFloatingRightStick";
    private static final String PREF_TOUCH_TRANSPARENT_STICKS = "touchTransparentSticks";
    private static final String PREF_CAMERA_X_SENSITIVITY = "touchCameraXSensitivity";
    private static final String PREF_CAMERA_Y_SENSITIVITY = "touchCameraYSensitivity";
    private static final String PREF_BUNDLED_NATIVE_MODS_VERSION = "bundledNativeModsVersion";
    private static final String BUNDLED_MODS_ASSET_DIRECTORY = "bundled_mods";
    private static final int BUNDLED_NATIVE_MODS_VERSION = 3;
    private static final String[] BUNDLED_NATIVE_MOD_FILES = {
            "ProxyMM_KV.nrm",
            "ProxyRecomp_KV005.so",
            "magemods_audio_api.nrm",
            "magemods_audio_api.so",
            "yazmt_mm_playermodelmanager.nrm",
            "yazmt_mm_playermodelmanager_fsmodels.nrm",
            "yazmt_mm_playermodelmanager_fsmodels_extlib.so"
    };
    private static final int FACE_LAYOUT_ABXY = 0;
    private static final int FACE_LAYOUT_NINTENDO = 1;
    private static final int FACE_LAYOUT_GAMECUBE = 2;
    private static final int ATTACH_RETRY_LIMIT = 20;
    private static final long ATTACH_RETRY_DELAY_MS = 250;
    private static final float LEFT_TOUCH_ZONE_WIDTH = 0.46f;
    private static final float RIGHT_TOUCH_ZONE_START = 0.52f;
    private static final float LEFT_STICK_RADIUS_DP = 51.0f;
    private static final float RIGHT_STICK_X_PER_PIXEL = 0.10f;
    private static final float RIGHT_STICK_Y_PER_PIXEL = 0.30f;
    private static final int DEFAULT_CAMERA_SENSITIVITY = 100;
    private static final int MIN_CAMERA_SENSITIVITY = 25;
    private static final int MAX_CAMERA_SENSITIVITY = 400;
    private static final int ANDROID_SENSOR_ACCELEROMETER = 0;
    private static final int ANDROID_SENSOR_GYROSCOPE = 1;

    private static native void nativeOnFilePickerResult(String path);
    private static native void nativeOnModFilePickerResult(String[] paths);
    private static native void nativeSetRendererPaused(boolean paused);
    private static native void nativeReloadClockTexturePack();
    private native boolean attachTouchController();
    private native void detachTouchController();
    private native void setTouchButton(int button, boolean pressed);
    private native void setTouchAxis(int axis, short value);
    native void setTouchGameButton(int mask, boolean down);
    private native void resetTouchGameButtons();
    private native boolean toggleIntegratedTouchSettingsNative();
    private native boolean isIntegratedTouchSettingsOpenNative();
    private native boolean isIntegratedTouchUiCapturingNative();
    private native int getIntegratedTouchContentNative();
    private native void setIntegratedTouchLayoutNative(float[] anchors, boolean active);
    private native void submitAndroidMotionSensor(
            int sensorType, float x, float y, float z, long timestampNanoseconds);

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final Runnable attachRetry = this::retryTouchControllerAttachment;
    private SharedPreferences preferences;
    private View overlayView;
    private ViewGroup controlsGroup;
    private FrameLayout leftJoystick;
    private ImageView leftJoystickKnob;
    private View touchArea;
    private LinkzenicTouchView integratedTouchView;
    private boolean touchControllerAttached;
    private int attachRetryCount;
    private int leftPointerId = MotionEvent.INVALID_POINTER_ID;
    private int rightPointerId = MotionEvent.INVALID_POINTER_ID;
    private float leftStartX;
    private float leftStartY;
    private float rightStartX;
    private float rightStartY;
    private volatile int cameraXSensitivity = DEFAULT_CAMERA_SENSITIVITY;
    private volatile int cameraYSensitivity = DEFAULT_CAMERA_SENSITIVITY;
    private SensorManager sensorManager;
    private Sensor accelerometerSensor;
    private Sensor gyroscopeSensor;
    private boolean motionSensorsRegistered;
    private File programDirectory;
    private File dataDirectory;
    private File customDriverDirectory;
    private String selectedCustomDriverName;
    private String selectedCustomDriverDisplayName;
    private volatile boolean legacyStagingInProgress;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        preferences = getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE);
        cameraXSensitivity = clampCameraSensitivity(
                preferences.getInt(PREF_CAMERA_X_SENSITIVITY, DEFAULT_CAMERA_SENSITIVITY));
        cameraYSensitivity = clampCameraSensitivity(
                preferences.getInt(PREF_CAMERA_Y_SENSITIVITY, DEFAULT_CAMERA_SENSITIVITY));
        programDirectory = new File(getFilesDir(), "program");
        dataDirectory = new File(getFilesDir(), "data");

        try {
            copyAssetTree("program", programDirectory);
            if (!dataDirectory.exists() && !dataDirectory.mkdirs()) {
                throw new IOException("Unable to create " + dataDirectory);
            }
            activateStagedMigrationIfRequested();
            seedBundledNativeMods(dataDirectory);
            configureCustomDriverState();
        } catch (IOException exception) {
            Log.e(TAG, "Unable to prepare runtime files", exception);
            finish();
            return;
        }

        super.onCreate(savedInstanceState);
        io.github.zelda64recomp.RandoGenerator.init(this);
        applyImmersiveFullscreen();
        requestHighRefreshRate();
        setupMotionSensors();
        setupTouchOverlay();
        attachOrScheduleTouchController();
        offerUpgradeDataImport();
    }

    @Override
    public void loadLibraries() {
        super.loadLibraries();
        nativeSetenv("APP_PROGRAM_PATH", programDirectory.getAbsolutePath());
        nativeSetenv("APP_FOLDER_PATH", dataDirectory.getAbsolutePath());
        nativeSetenv("APP_NATIVE_LIBRARY_DIR", getApplicationInfo().nativeLibraryDir);
        nativeSetenv("APP_ANDROID_VERSION_NAME", getAndroidVersionName());
        applyCustomDriverEnvironment();
        applyClockTexturePackEnvironment();
    }

    private String getAndroidVersionName() {
        try {
            PackageInfo packageInfo = getPackageManager().getPackageInfo(getPackageName(), 0);
            if (packageInfo.versionName != null) {
                return packageInfo.versionName;
            }
        } catch (PackageManager.NameNotFoundException exception) {
            Log.w(TAG, "Unable to read package version", exception);
        }
        return "0.1.0";
    }

    @Override
    protected void onResume() {
        super.onResume();
        applyImmersiveFullscreen();
        requestHighRefreshRate();
        if (!touchControllerAttached) {
            scheduleTouchControllerAttachment();
        }
        registerMotionSensors();
    }

    @Override
    protected void onPause() {
        unregisterMotionSensors();
        resetIntegratedTouchInputs();
        nativeSetRendererPaused(true);
        super.onPause();
    }

    @Override
    public void onBackPressed() {
        if (integratedTouchView != null && integratedTouchView.cancelEditing()) {
            return;
        }
        super.onBackPressed();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyImmersiveFullscreen();
            requestHighRefreshRate();
            nativeSetRendererPaused(false);
        }
    }

    private void applyImmersiveFullscreen() {
        Window window = getWindow();
        if (window == null) {
            return;
        }

        View decorView = window.getDecorView();
        if (decorView == null) {
            return;
        }

        window.addFlags(android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false);
            WindowInsetsController controller = decorView.getWindowInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        }

        decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    private void requestHighRefreshRate() {
        Window window = getWindow();
        if (window == null) {
            return;
        }

        Display display = getWindowManager().getDefaultDisplay();
        if (display == null) {
            return;
        }

        float bestRefreshRate = 0.0f;
        int bestModeId = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            for (Display.Mode mode : display.getSupportedModes()) {
                if (mode.getRefreshRate() > bestRefreshRate) {
                    bestRefreshRate = mode.getRefreshRate();
                    bestModeId = mode.getModeId();
                }
            }
        } else {
            bestRefreshRate = display.getRefreshRate();
        }

        if (bestRefreshRate <= 0.0f) {
            return;
        }

        android.view.WindowManager.LayoutParams parameters = window.getAttributes();
        parameters.preferredRefreshRate = bestRefreshRate;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && bestModeId != 0) {
            parameters.preferredDisplayModeId = bestModeId;
        }
        window.setAttributes(parameters);
        Log.i(TAG, "Requested display refresh rate " + bestRefreshRate + " Hz modeId=" + bestModeId);
    }

    @Override
    protected void onDestroy() {
        mainHandler.removeCallbacks(attachRetry);
        unregisterMotionSensors();
        if (touchControllerAttached) {
            detachTouchController();
            touchControllerAttached = false;
        }
        super.onDestroy();
    }

    private void setupMotionSensors() {
        sensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        if (sensorManager == null) {
            Log.w(TAG, "Motion sensors are unavailable: no sensor manager");
            return;
        }

        accelerometerSensor = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        gyroscopeSensor = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
        Log.i(TAG, "Motion sensors: accelerometer=" + (accelerometerSensor != null)
                + ", gyroscope=" + (gyroscopeSensor != null));
    }

    private void registerMotionSensors() {
        if (sensorManager == null || motionSensorsRegistered) {
            return;
        }

        boolean registered = false;
        if (accelerometerSensor != null) {
            registered |= sensorManager.registerListener(
                    this, accelerometerSensor, SensorManager.SENSOR_DELAY_GAME);
        }
        if (gyroscopeSensor != null) {
            registered |= sensorManager.registerListener(
                    this, gyroscopeSensor, SensorManager.SENSOR_DELAY_GAME);
        }
        motionSensorsRegistered = registered;
    }

    private void unregisterMotionSensors() {
        if (sensorManager == null || !motionSensorsRegistered) {
            return;
        }

        sensorManager.unregisterListener(this);
        motionSensorsRegistered = false;
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (event.sensor == null) {
            return;
        }

        int sensorType = event.sensor.getType();
        if (sensorType != Sensor.TYPE_ACCELEROMETER && sensorType != Sensor.TYPE_GYROSCOPE) {
            return;
        }

        float[] values = orientMotionValues(event.values);
        if (sensorType == Sensor.TYPE_ACCELEROMETER) {
            submitAndroidMotionSensor(
                    ANDROID_SENSOR_ACCELEROMETER,
                    -values[0],
                    values[1],
                    values[2],
                    event.timestamp);
        } else {
            submitAndroidMotionSensor(
                    ANDROID_SENSOR_GYROSCOPE,
                    values[0],
                    values[1],
                    values[2],
                    event.timestamp);
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    }

    private float[] orientMotionValues(float[] values) {
        float x;
        float y;
        switch (getWindowManager().getDefaultDisplay().getRotation()) {
            case Surface.ROTATION_90:
                x = -values[1];
                y = values[0];
                break;
            case Surface.ROTATION_180:
                x = -values[0];
                y = -values[1];
                break;
            case Surface.ROTATION_270:
                x = values[1];
                y = -values[0];
                break;
            case Surface.ROTATION_0:
            default:
                x = values[0];
                y = values[1];
                break;
        }
        return new float[] {x, y, values[2]};
    }

    private void attachOrScheduleTouchController() {
        touchControllerAttached = attachTouchController();
        if (touchControllerAttached) {
            attachRetryCount = 0;
            mainHandler.removeCallbacks(attachRetry);
        } else {
            scheduleTouchControllerAttachment();
        }
    }

    private void scheduleTouchControllerAttachment() {
        mainHandler.removeCallbacks(attachRetry);
        if (!touchControllerAttached && attachRetryCount < ATTACH_RETRY_LIMIT) {
            mainHandler.postDelayed(attachRetry, ATTACH_RETRY_DELAY_MS);
        }
    }

    private void retryTouchControllerAttachment() {
        if (touchControllerAttached) {
            return;
        }
        attachRetryCount++;
        attachOrScheduleTouchController();
    }

    private void ensureTouchControllerAttached() {
        if (!touchControllerAttached) {
            attachOrScheduleTouchController();
        }
    }

    private void setupTouchOverlay() {
        LayoutInflater inflater = (LayoutInflater) getSystemService(LAYOUT_INFLATER_SERVICE);
        overlayView = inflater.inflate(R.layout.touchcontrol_overlay, null);
        FrameLayout.LayoutParams overlayParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT);
        addContentView(overlayView, overlayParams);

        controlsGroup = overlayView.findViewById(R.id.button_group);
        leftJoystick = overlayView.findViewById(R.id.left_joystick);
        leftJoystickKnob = overlayView.findViewById(R.id.left_joystick_knob);
        touchArea = overlayView.findViewById(R.id.touch_area);

        bindButton(R.id.buttonL, ControllerButtons.BUTTON_LB);
        bindButton(R.id.buttonR, ControllerButtons.BUTTON_RB);
        bindButton(R.id.buttonStart, ControllerButtons.BUTTON_START);
        bindButton(R.id.buttonBack, ControllerButtons.BUTTON_BACK);
        bindButton(R.id.buttonDpadUp, ControllerButtons.BUTTON_DPAD_UP);
        bindButton(R.id.buttonDpadDown, ControllerButtons.BUTTON_DPAD_DOWN);
        bindButton(R.id.buttonDpadLeft, ControllerButtons.BUTTON_DPAD_LEFT);
        bindButton(R.id.buttonDpadRight, ControllerButtons.BUTTON_DPAD_RIGHT);
        bindAxisButton(R.id.buttonZ, ControllerButtons.AXIS_LT);
        bindAxisButton(R.id.buttonZR, ControllerButtons.AXIS_RT);

        applyFaceLayout(preferences.getInt(PREF_FACE_LAYOUT, FACE_LAYOUT_ABXY));
        setupTouchArea();

        overlayView.findViewById(R.id.buttonToggle).setOnClickListener(view -> {
            boolean hidden = controlsGroup.getVisibility() == View.VISIBLE;
            preferences.edit().putBoolean(PREF_CONTROLS_HIDDEN, hidden).apply();
            applyTouchControlsVisibility();
        });
        applyTouchControlsVisibility();

        integratedTouchView = new LinkzenicTouchView(this);
        FrameLayout.LayoutParams integratedParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT);
        addContentView(integratedTouchView, integratedParams);
        applyTouchControlsVisibility();
    }

    private void bindButton(int viewId, int buttonId) {
        Button button = overlayView.findViewById(viewId);
        button.setOnTouchListener((view, event) -> {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    ensureTouchControllerAttached();
                    setTouchButton(buttonId, true);
                    setButtonPressed(button, true);
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    setTouchButton(buttonId, false);
                    setButtonPressed(button, false);
                    return true;
                default:
                    return true;
            }
        });
    }

    private void bindAxisButton(int viewId, int axisId) {
        Button button = overlayView.findViewById(viewId);
        button.setOnTouchListener((view, event) -> {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    ensureTouchControllerAttached();
                    setTouchAxis(axisId, Short.MAX_VALUE);
                    setButtonPressed(button, true);
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    setTouchAxis(axisId, (short) 0);
                    setButtonPressed(button, false);
                    return true;
                default:
                    return true;
            }
        });
    }

    private static void setButtonPressed(Button button, boolean pressed) {
        button.setPressed(pressed);
        float scale = pressed ? 0.92f : 1.0f;
        button.animate().scaleX(scale).scaleY(scale).setDuration(60).start();
    }

    private void applyFaceLayout(int layout) {
        layout = normalizeFaceLayout(layout);
        Button south = overlayView.findViewById(R.id.buttonSouth);
        Button east = overlayView.findViewById(R.id.buttonEast);
        Button west = overlayView.findViewById(R.id.buttonWest);
        Button north = overlayView.findViewById(R.id.buttonNorth);
        FrameLayout cluster = overlayView.findViewById(R.id.action_button_cluster);

        if (layout == FACE_LAYOUT_GAMECUBE) {
            setViewSize(cluster, 200, 170);
            configureFaceButton(south, "A", 70, 70, Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL, 20, 0, 0, 20);
            configureFaceButton(east, "B", 46, 46, Gravity.BOTTOM | Gravity.START, 30, 0, 0, 20);
            configureFaceButton(west, "X", 46, 46, Gravity.TOP | Gravity.END, 0, 37, 0, 0);
            configureFaceButton(north, "Y", 46, 46, Gravity.TOP | Gravity.CENTER_HORIZONTAL, 0, 15, 0, 0);
        } else {
            setViewSize(cluster, 132, 132);
            boolean nintendo = layout == FACE_LAYOUT_NINTENDO;
            configureFaceButton(south, nintendo ? "A" : "B", 44, 44, Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL, 0, 0, 0, 0);
            configureFaceButton(east, nintendo ? "B" : "A", 44, 44, Gravity.CENTER_VERTICAL | Gravity.END, 0, 0, 0, 0);
            configureFaceButton(west, nintendo ? "X" : "Y", 44, 44, Gravity.CENTER_VERTICAL | Gravity.START, 0, 0, 0, 0);
            configureFaceButton(north, nintendo ? "Y" : "X", 44, 44, Gravity.TOP | Gravity.CENTER_HORIZONTAL, 0, 0, 0, 0);
        }

        bindFaceButtons(layout);
    }

    private void bindFaceButtons(int layout) {
        releaseFaceButtons();

        if (layout == FACE_LAYOUT_ABXY) {
            bindButton(R.id.buttonSouth, ControllerButtons.BUTTON_B);
            bindButton(R.id.buttonEast, ControllerButtons.BUTTON_A);
            bindButton(R.id.buttonWest, ControllerButtons.BUTTON_Y);
            bindButton(R.id.buttonNorth, ControllerButtons.BUTTON_X);
        } else {
            bindButton(R.id.buttonSouth, ControllerButtons.BUTTON_A);
            bindButton(R.id.buttonEast, ControllerButtons.BUTTON_B);
            bindButton(R.id.buttonWest, ControllerButtons.BUTTON_X);
            bindButton(R.id.buttonNorth, ControllerButtons.BUTTON_Y);
        }
    }

    private void releaseFaceButtons() {
        setTouchButton(ControllerButtons.BUTTON_A, false);
        setTouchButton(ControllerButtons.BUTTON_B, false);
        setTouchButton(ControllerButtons.BUTTON_X, false);
        setTouchButton(ControllerButtons.BUTTON_Y, false);
    }

    private void configureFaceButton(Button button, String text, int width, int height,
                                     int gravity, int start, int top, int end, int bottom) {
        FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) button.getLayoutParams();
        params.width = dp(width);
        params.height = dp(height);
        params.gravity = gravity;
        params.setMargins(dp(start), dp(top), dp(end), dp(bottom));
        params.setMarginStart(dp(start));
        params.setMarginEnd(dp(end));
        button.setLayoutParams(params);
        button.setText(text);
    }

    private void setViewSize(View view, int width, int height) {
        ViewGroup.LayoutParams params = view.getLayoutParams();
        params.width = dp(width);
        params.height = dp(height);
        view.setLayoutParams(params);
    }

    private int dp(float value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private void setupTouchArea() {
        float leftRadius = dp(LEFT_STICK_RADIUS_DP);
        leftJoystick.setVisibility(View.INVISIBLE);
        touchArea.setOnTouchListener((view, event) -> {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_POINTER_DOWN:
                    return startTouchPointer(view, event, event.getActionIndex());
                case MotionEvent.ACTION_MOVE:
                    updateTouchPointers(event, leftRadius);
                    return true;
                case MotionEvent.ACTION_POINTER_UP:
                case MotionEvent.ACTION_UP:
                    releaseTouchPointer(event.getPointerId(event.getActionIndex()));
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    resetLeftStick();
                    resetRightStick();
                    return true;
                default:
                    return true;
            }
        });
    }

    private boolean startTouchPointer(View view, MotionEvent event, int index) {
        float x = event.getX(index);
        float y = event.getY(index);
        int pointerId = event.getPointerId(index);
        if (x < view.getWidth() * LEFT_TOUCH_ZONE_WIDTH && leftPointerId == MotionEvent.INVALID_POINTER_ID) {
            ensureTouchControllerAttached();
            leftPointerId = pointerId;
            leftStartX = x;
            leftStartY = y;
            leftJoystick.setX(x - leftJoystick.getWidth() / 2.0f);
            leftJoystick.setY(y - leftJoystick.getHeight() / 2.0f);
            leftJoystick.setVisibility(View.VISIBLE);
            return true;
        }
        if (x > view.getWidth() * RIGHT_TOUCH_ZONE_START && rightPointerId == MotionEvent.INVALID_POINTER_ID) {
            ensureTouchControllerAttached();
            rightPointerId = pointerId;
            rightStartX = x;
            rightStartY = y;
            return true;
        }
        return false;
    }

    private void updateTouchPointers(MotionEvent event, float leftRadius) {
        int leftIndex = event.findPointerIndex(leftPointerId);
        if (leftIndex >= 0) {
            float x = event.getX(leftIndex) - leftStartX;
            float y = event.getY(leftIndex) - leftStartY;
            float distance = (float) Math.sqrt(x * x + y * y);
            if (distance > leftRadius) {
                float scale = leftRadius / distance;
                x *= scale;
                y *= scale;
            }
            leftJoystickKnob.setTranslationX(x);
            leftJoystickKnob.setTranslationY(y);
            setTouchAxis(ControllerButtons.AXIS_LX, (short) (x / leftRadius * Short.MAX_VALUE));
            setTouchAxis(ControllerButtons.AXIS_LY, (short) (y / leftRadius * Short.MAX_VALUE));
        }

        int rightIndex = event.findPointerIndex(rightPointerId);
        if (rightIndex >= 0) {
            float x = event.getX(rightIndex) - rightStartX;
            float y = event.getY(rightIndex) - rightStartY;
            setTouchAxis(ControllerButtons.AXIS_RX, touchAxis(
                    x, RIGHT_STICK_X_PER_PIXEL * cameraXSensitivity / 100.0f));
            setTouchAxis(ControllerButtons.AXIS_RY, touchAxis(
                    y, RIGHT_STICK_Y_PER_PIXEL * cameraYSensitivity / 100.0f));
        }
    }

    private void applyTouchControlsVisibility() {
        boolean disabled = preferences.getBoolean(PREF_CONTROLS_DISABLED, false);
        int style = getIntegratedTouchStyleFromNative();
        boolean integrated = !disabled && style != LinkzenicTouchView.STYLE_CLASSIC;
        overlayView.setVisibility(disabled || integrated ? View.GONE : View.VISIBLE);
        boolean hidden = preferences.getBoolean(PREF_CONTROLS_HIDDEN, false);
        controlsGroup.setVisibility(hidden ? View.INVISIBLE : View.VISIBLE);
        if (integratedTouchView != null) {
            integratedTouchView.configure(style, !disabled,
                    preferences.getBoolean(PREF_TOUCH_FLOATING_LEFT, true),
                    preferences.getBoolean(PREF_TOUCH_FLOATING_RIGHT, true),
                    preferences.getFloat(PREF_TOUCH_SCALE, 1.0f),
                    preferences.getFloat(PREF_TOUCH_OPACITY, .55f),
                    preferences.getBoolean(PREF_TOUCH_TRANSPARENT_STICKS, false));
        }
    }

    private static int normalizeFaceLayout(int layout) {
        return Math.max(FACE_LAYOUT_ABXY, Math.min(FACE_LAYOUT_GAMECUBE, layout));
    }

    private static int clampCameraSensitivity(int sensitivity) {
        return Math.max(MIN_CAMERA_SENSITIVITY, Math.min(MAX_CAMERA_SENSITIVITY, sensitivity));
    }

    public boolean areTouchControlsDisabledFromNative() {
        return preferences.getBoolean(PREF_CONTROLS_DISABLED, false);
    }

    public void setTouchControlsDisabledFromNative(boolean disabled) {
        preferences.edit().putBoolean(PREF_CONTROLS_DISABLED, disabled).apply();
        runOnUiThread(this::applyTouchControlsVisibility);
    }

    public int getTouchFaceButtonLayoutFromNative() {
        return normalizeFaceLayout(preferences.getInt(PREF_FACE_LAYOUT, FACE_LAYOUT_ABXY));
    }

    public void setTouchFaceButtonLayoutFromNative(int layout) {
        int normalizedLayout = normalizeFaceLayout(layout);
        preferences.edit().putInt(PREF_FACE_LAYOUT, normalizedLayout).apply();
        runOnUiThread(() -> applyFaceLayout(normalizedLayout));
    }

    public int getIntegratedTouchStyleFromNative() {
        return Math.max(LinkzenicTouchView.STYLE_CLASSIC,
                Math.min(LinkzenicTouchView.STYLE_MODERN,
                        preferences.getInt(PREF_TOUCH_STYLE, LinkzenicTouchView.STYLE_MODERN)));
    }

    public void setIntegratedTouchStyleFromNative(int style) {
        int normalized = Math.max(LinkzenicTouchView.STYLE_CLASSIC,
                Math.min(LinkzenicTouchView.STYLE_MODERN, style));
        preferences.edit().putInt(PREF_TOUCH_STYLE, normalized).apply();
        runOnUiThread(this::applyTouchControlsVisibility);
    }

    public int getIntegratedTouchScaleFromNative() {
        return Math.round(preferences.getFloat(PREF_TOUCH_SCALE, 1.0f) * 100);
    }

    public void setIntegratedTouchScaleFromNative(int percent) {
        float scale = Math.max(75, Math.min(150, percent)) / 100.0f;
        preferences.edit().putFloat(PREF_TOUCH_SCALE, scale).apply();
        runOnUiThread(this::applyTouchControlsVisibility);
    }

    public int getIntegratedTouchOpacityFromNative() {
        return Math.round(preferences.getFloat(PREF_TOUCH_OPACITY, .55f) * 100);
    }

    public void setIntegratedTouchOpacityFromNative(int percent) {
        float opacity = Math.max(15, Math.min(100, percent)) / 100.0f;
        preferences.edit().putFloat(PREF_TOUCH_OPACITY, opacity).apply();
        runOnUiThread(this::applyTouchControlsVisibility);
    }

    public void editIntegratedTouchFromNative() {
        runOnUiThread(() -> {
            if (integratedTouchView == null
                    || getIntegratedTouchStyleFromNative() == LinkzenicTouchView.STYLE_CLASSIC) {
                Toast.makeText(this, "Select an integrated touch style first.", Toast.LENGTH_LONG).show();
            } else {
                integratedTouchView.startEditing();
            }
        });
    }

    void setIntegratedTouchButton(int button, boolean pressed) {
        ensureTouchControllerAttached();
        setTouchButton(button, pressed);
    }

    void setIntegratedTouchAxis(int axis, short value) {
        ensureTouchControllerAttached();
        setTouchAxis(axis, value);
    }

    void resetIntegratedTouchInputs() {
        resetTouchGameButtons();
        setTouchButton(ControllerButtons.BUTTON_A, false);
        setTouchButton(ControllerButtons.BUTTON_B, false);
        setTouchButton(ControllerButtons.BUTTON_X, false);
        setTouchButton(ControllerButtons.BUTTON_Y, false);
        setTouchButton(ControllerButtons.BUTTON_BACK, false);
        setTouchButton(ControllerButtons.BUTTON_START, false);
        setTouchButton(ControllerButtons.BUTTON_RS, false);
        setTouchButton(ControllerButtons.BUTTON_LB, false);
        setTouchButton(ControllerButtons.BUTTON_RB, false);
        setTouchButton(ControllerButtons.BUTTON_DPAD_UP, false);
        setTouchButton(ControllerButtons.BUTTON_DPAD_DOWN, false);
        setTouchButton(ControllerButtons.BUTTON_DPAD_LEFT, false);
        setTouchButton(ControllerButtons.BUTTON_DPAD_RIGHT, false);
        for (int axis = ControllerButtons.AXIS_LX; axis <= ControllerButtons.AXIS_RT; axis++) {
            setTouchAxis(axis, (short) 0);
        }
    }

    boolean toggleIntegratedTouchSettings() {
        return toggleIntegratedTouchSettingsNative();
    }

    boolean isIntegratedTouchSettingsOpen() {
        return isIntegratedTouchSettingsOpenNative();
    }

    boolean isIntegratedTouchUiCapturing() {
        return isIntegratedTouchUiCapturingNative();
    }

    int getIntegratedTouchContent() {
        return getIntegratedTouchContentNative();
    }

    void setIntegratedTouchLayout(float[] anchors, boolean active) {
        setIntegratedTouchLayoutNative(anchors, active);
    }

    public int getTouchCameraXSensitivityFromNative() {
        return cameraXSensitivity;
    }

    public void setTouchCameraXSensitivityFromNative(int sensitivity) {
        cameraXSensitivity = clampCameraSensitivity(sensitivity);
        preferences.edit().putInt(PREF_CAMERA_X_SENSITIVITY, cameraXSensitivity).apply();
    }

    public int getTouchCameraYSensitivityFromNative() {
        return cameraYSensitivity;
    }

    public void setTouchCameraYSensitivityFromNative(int sensitivity) {
        cameraYSensitivity = clampCameraSensitivity(sensitivity);
        preferences.edit().putInt(PREF_CAMERA_Y_SENSITIVITY, cameraYSensitivity).apply();
    }

    private static short touchAxis(float delta, float scale) {
        float value = Math.max(-1.0f, Math.min(1.0f, delta * scale));
        return (short) Math.round(value * Short.MAX_VALUE);
    }

    private void releaseTouchPointer(int pointerId) {
        if (pointerId == leftPointerId) {
            resetLeftStick();
        }
        if (pointerId == rightPointerId) {
            resetRightStick();
        }
    }

    private void resetLeftStick() {
        leftPointerId = MotionEvent.INVALID_POINTER_ID;
        setTouchAxis(ControllerButtons.AXIS_LX, (short) 0);
        setTouchAxis(ControllerButtons.AXIS_LY, (short) 0);
        leftJoystickKnob.setTranslationX(0.0f);
        leftJoystickKnob.setTranslationY(0.0f);
        leftJoystick.setVisibility(View.INVISIBLE);
    }

    private void resetRightStick() {
        rightPointerId = MotionEvent.INVALID_POINTER_ID;
        setTouchAxis(ControllerButtons.AXIS_RX, (short) 0);
        setTouchAxis(ControllerButtons.AXIS_RY, (short) 0);
    }

    public void openFilePicker() {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

            try {
                startActivityForResult(intent, OPEN_FILE_REQUEST);
            } catch (RuntimeException exception) {
                Log.e(TAG, "Unable to open the system file picker", exception);
                nativeOnFilePickerResult(null);
            }
        });
    }

    public void openModFilePicker() {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
            intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {
                    "application/zip",
                    "application/octet-stream",
                    "application/x-zip-compressed"
            });

            try {
                startActivityForResult(intent, OPEN_MOD_FILES_REQUEST);
            } catch (RuntimeException exception) {
                Log.e(TAG, "Unable to open the mod file picker", exception);
                nativeOnModFilePickerResult(new String[0]);
            }
        });
    }

    public void openDriverFilePicker() {
        openSystemFilePicker(OPEN_DRIVER_FILE_REQUEST, new String[] {
                "application/zip",
                "application/octet-stream",
                "application/x-zip-compressed"
        }, "GPU driver");
    }

    public void openClockTextureFilePicker() {
        openSystemFilePicker(OPEN_CLOCK_TEXTURE_FILE_REQUEST, new String[] {
                "application/zip",
                "application/octet-stream",
                "application/x-zip-compressed"
        }, "clock texture pack");
    }

    private void offerUpgradeDataImport() {
        if (mSurface == null || !RELEASE_PACKAGE.equals(getPackageName())
                || preferences.getBoolean(PREF_UPGRADE_DATA_DECIDED, false)) {
            return;
        }

        final PackageInfo packageInfo;
        try {
            packageInfo = getPackageManager().getPackageInfo(getPackageName(), 0);
        } catch (PackageManager.NameNotFoundException exception) {
            Log.w(TAG, "Unable to determine whether the app was updated", exception);
            return;
        }

        File migrationComplete = new File(new File(dataDirectory, MIGRATION_DIRECTORY),
                MIGRATION_ACTIVATION_COMPLETE_FILE);
        if (packageInfo.firstInstallTime == packageInfo.lastUpdateTime
                || migrationComplete.isFile()) {
            preferences.edit().putBoolean(PREF_UPGRADE_DATA_DECIDED, true).apply();
            return;
        }

        // Keep SDL from starting while the upgrade prompt is active. The import
        // closes this activity so the next launch can activate the verified files.
        mSurface.setVisibility(View.INVISIBLE);
        new AlertDialog.Builder(this)
                .setTitle("Import existing Zelda64 data?")
                .setMessage("If you used the shared Zelda64 folder, select it to keep your saves, "
                        + "mods and settings. Data already stored in the app stays available.")
                .setPositiveButton("Choose Zelda64 folder",
                        (dialog, which) -> openLegacyDataTreePicker(OPEN_UPGRADE_DATA_TREE_REQUEST))
                .setNegativeButton("Skip", (dialog, which) -> {
                    preferences.edit().putBoolean(PREF_UPGRADE_DATA_DECIDED, true).apply();
                    mSurface.setVisibility(View.VISIBLE);
                })
                .setCancelable(false)
                .show();
    }

    public void openLegacyDataInventoryPicker() {
        openLegacyDataTreePicker(OPEN_LEGACY_DATA_TREE_REQUEST);
    }

    public void openLegacyDataStagingPicker() {
        openLegacyDataTreePicker(OPEN_LEGACY_DATA_STAGING_TREE_REQUEST);
    }

    public void scheduleLegacyDataActivation() {
        runOnUiThread(() -> {
            File migrationDirectory = new File(dataDirectory, MIGRATION_DIRECTORY);
            File stagingDirectory = new File(
                    migrationDirectory, MIGRATION_STAGING_DIRECTORY);
            File copyJournal = new File(
                    migrationDirectory, MIGRATION_COPY_JOURNAL_FILE);
            File stagingComplete = new File(
                    migrationDirectory, MIGRATION_STAGING_COMPLETE_FILE);
            if (legacyStagingInProgress || !stagingDirectory.isDirectory()
                    || !copyJournal.isFile() || !stagingComplete.isFile()) {
                Toast.makeText(this, "No verified staged data is available.",
                        Toast.LENGTH_LONG).show();
                return;
            }
            try {
                writeDurableTextFile(new File(migrationDirectory,
                        MIGRATION_ACTIVATION_REQUEST_FILE), "requested\n");
                Toast.makeText(this,
                        "Staged data will activate on the next app launch.",
                        Toast.LENGTH_LONG).show();
            } catch (IOException exception) {
                Log.e(TAG, "Unable to schedule staged-data activation", exception);
                Toast.makeText(this, "Unable to schedule staged-data activation.",
                        Toast.LENGTH_LONG).show();
            }
        });
    }

    private void openLegacyDataTreePicker(int requestCode) {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                    | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI,
                        DocumentsContract.buildDocumentUri(
                                LEGACY_DATA_AUTHORITY, LEGACY_DATA_DOCUMENT_ID));
            }
            try {
                startActivityForResult(intent, requestCode);
            } catch (RuntimeException exception) {
                Log.e(TAG, "Unable to open the existing-data folder picker", exception);
                Toast.makeText(this, "Unable to open folder picker.", Toast.LENGTH_LONG).show();
                if (requestCode == OPEN_UPGRADE_DATA_TREE_REQUEST) {
                    offerUpgradeDataImport();
                }
            }
        });
    }

    public void clearCustomDriver() {
        runOnUiThread(() -> {
            clearCustomDriverSelection();
            Toast.makeText(this, "Using system GPU driver after restart.", Toast.LENGTH_LONG).show();
        });
    }

    private void openSystemFilePicker(int requestCode, String[] mimeTypes, String description) {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

            try {
                startActivityForResult(intent, requestCode);
            } catch (RuntimeException exception) {
                Log.e(TAG, "Unable to open the " + description + " picker", exception);
                Toast.makeText(this, "Unable to open file picker.", Toast.LENGTH_LONG).show();
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == OPEN_DRIVER_FILE_REQUEST) {
            if (resultCode == Activity.RESULT_OK && data != null && data.getData() != null) {
                handlePickedDriverFile(data.getData());
            }
            return;
        }

        if (requestCode == OPEN_CLOCK_TEXTURE_FILE_REQUEST) {
            if (resultCode == Activity.RESULT_OK && data != null && data.getData() != null) {
                handlePickedClockTexturePack(data.getData());
            }
            return;
        }

        if (requestCode == OPEN_LEGACY_DATA_TREE_REQUEST) {
            if (resultCode == Activity.RESULT_OK && data != null && data.getData() != null) {
                handleLegacyDataTree(data.getData());
            }
            return;
        }

        if (requestCode == OPEN_LEGACY_DATA_STAGING_TREE_REQUEST
                || requestCode == OPEN_UPGRADE_DATA_TREE_REQUEST) {
            if (resultCode == Activity.RESULT_OK && data != null && data.getData() != null) {
                Uri treeUri = data.getData();
                boolean upgrading = requestCode == OPEN_UPGRADE_DATA_TREE_REQUEST;
                if (upgrading && (!LEGACY_DATA_AUTHORITY.equals(treeUri.getAuthority())
                        || !LEGACY_DATA_DOCUMENT_ID.equals(DocumentsContract.getTreeDocumentId(treeUri)))) {
                    new AlertDialog.Builder(this)
                            .setMessage("Select the Zelda64 folder in internal storage.")
                            .setPositiveButton("Choose folder",
                                    (dialog, which) -> openLegacyDataTreePicker(requestCode))
                            .setNegativeButton("Cancel", (dialog, which) -> offerUpgradeDataImport())
                            .setCancelable(false)
                            .show();
                } else {
                    handleLegacyDataStagingTree(treeUri, upgrading);
                }
            } else if (requestCode == OPEN_UPGRADE_DATA_TREE_REQUEST) {
                offerUpgradeDataImport();
            }
            return;
        }

        if (requestCode != OPEN_FILE_REQUEST) {
            if (requestCode == OPEN_MOD_FILES_REQUEST) {
                handleModFilePickerResult(resultCode, data);
            }
            return;
        }

        String path = null;
        if (resultCode == Activity.RESULT_OK && data != null && data.getData() != null) {
            try {
                path = copySelectedFile(data.getData()).getAbsolutePath();
            } catch (IOException exception) {
                Log.e(TAG, "Unable to read the selected file", exception);
            }
        }
        nativeOnFilePickerResult(path);
    }

    private void configureCustomDriverState() throws IOException {
        customDriverDirectory = new File(getFilesDir(), CUSTOM_DRIVER_DIRECTORY);
        String driverName = readTextFile(
                new File(customDriverDirectory, CUSTOM_DRIVER_NAME_FILE), null);
        if (driverName == null || driverName.isEmpty()) {
            return;
        }

        driverName = sanitizeFileName(driverName);
        if (!new File(customDriverDirectory, driverName).isFile()) {
            clearCustomDriverFiles();
            return;
        }

        selectedCustomDriverName = driverName;
        selectedCustomDriverDisplayName = readTextFile(
                new File(customDriverDirectory, CUSTOM_DRIVER_DISPLAY_NAME_FILE), driverName);
    }

    private void handlePickedDriverFile(Uri uri) {
        try {
            clearCustomDriverFiles();
            if (!customDriverDirectory.exists() && !customDriverDirectory.mkdirs()) {
                throw new IOException("Unable to create " + customDriverDirectory);
            }

            String displayName = sanitizeFileName(getDisplayName(uri));
            ArrayList<String> libraries = new ArrayList<>();
            if (displayName.toLowerCase(Locale.US).endsWith(".zip")) {
                copyDriverZip(uri, libraries);
            } else {
                libraries.add(copyDriverLibrary(uri, displayName));
            }

            String driverName = chooseDriverLibrary(libraries);
            if (driverName == null) {
                clearCustomDriverSelection();
                Toast.makeText(this, "No Vulkan driver library was found.", Toast.LENGTH_LONG).show();
                return;
            }

            writeTextFile(new File(customDriverDirectory, CUSTOM_DRIVER_NAME_FILE), driverName);
            writeTextFile(new File(customDriverDirectory, CUSTOM_DRIVER_DISPLAY_NAME_FILE), displayName);
            selectedCustomDriverName = driverName;
            selectedCustomDriverDisplayName = displayName;
            applyCustomDriverEnvironment();
            Toast.makeText(this, "GPU driver selected after restart: " + displayName,
                    Toast.LENGTH_LONG).show();
        } catch (IOException exception) {
            Log.e(TAG, "Unable to import the custom GPU driver", exception);
            clearCustomDriverSelection();
            Toast.makeText(this, "Unable to import GPU driver.", Toast.LENGTH_LONG).show();
        }
    }

    private void handlePickedClockTexturePack(Uri uri) {
        try {
            File directory = new File(dataDirectory, CLOCK_TEXTURE_DIRECTORY);
            if (!directory.exists() && !directory.mkdirs()) {
                throw new IOException("Unable to create " + directory);
            }

            String displayName = sanitizeFileName(getDisplayName(uri));
            copyUri(uri, new File(directory, CLOCK_TEXTURE_FILE));
            writeTextFile(new File(directory, CLOCK_TEXTURE_DISPLAY_FILE), displayName);
            applyClockTexturePackEnvironment();
            nativeReloadClockTexturePack();
            Toast.makeText(this, "Clock texture pack imported.", Toast.LENGTH_SHORT).show();
        } catch (IOException exception) {
            Log.e(TAG, "Unable to import the clock texture pack", exception);
            Toast.makeText(this, "Unable to import clock texture pack.", Toast.LENGTH_LONG).show();
        }
    }

    private void handleLegacyDataTree(Uri treeUri) {
        try {
            getContentResolver().takePersistableUriPermission(
                    treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException exception) {
            Log.w(TAG, "The selected folder did not grant persistent read access", exception);
        }

        Toast.makeText(this, "Inspecting existing Zelda64 data…", Toast.LENGTH_SHORT).show();
        new Thread(() -> {
            try {
                long[] totals = writeLegacyDataInventory(treeUri);
                Log.i(TAG, "Legacy data inventory complete: files=" + totals[0]
                        + ", directories=" + totals[2] + ", bytes=" + totals[1]);
                runOnUiThread(() -> Toast.makeText(this,
                        "Inventory complete: " + totals[0] + " files, "
                                + formatByteCount(totals[1]) + ". No source files changed.",
                        Toast.LENGTH_LONG).show());
            } catch (Exception exception) {
                Log.e(TAG, "Unable to inventory the selected existing-data folder", exception);
                runOnUiThread(() -> Toast.makeText(this,
                        "Unable to inspect the selected folder.", Toast.LENGTH_LONG).show());
            }
        }, "ZeldaLegacyInventory").start();
    }

    private void handleLegacyDataStagingTree(Uri treeUri, boolean upgrading) {
        if (legacyStagingInProgress) {
            Toast.makeText(this, "Legacy data staging is already running.",
                    Toast.LENGTH_LONG).show();
            return;
        }
        try {
            getContentResolver().takePersistableUriPermission(
                    treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException exception) {
            Log.w(TAG, "The selected folder did not grant persistent read access", exception);
        }

        legacyStagingInProgress = true;
        AlertDialog progress = upgrading ? new AlertDialog.Builder(this)
                .setTitle("Importing Zelda64 data")
                .setMessage("Copying and verifying the selected folder.")
                .setCancelable(false)
                .show() : null;
        if (!upgrading) {
            Toast.makeText(this, "Staging and verifying existing Zelda64 data…",
                    Toast.LENGTH_SHORT).show();
        }
        new Thread(() -> {
            try {
                MigrationCopyTotals totals = stageLegacyData(treeUri);
                Log.i(TAG, "Legacy data staging complete: copied=" + totals.copiedFiles
                        + ", resumed=" + totals.resumedFiles
                        + ", directories=" + totals.directories
                        + ", bytes=" + totals.verifiedBytes
                        + ", failures=" + totals.failedFiles);
                if (upgrading) {
                    if (totals.failedFiles != 0 || totals.copiedFiles + totals.resumedFiles == 0) {
                        throw new IOException("The selected folder did not yield a complete import");
                    }
                    writeDurableTextFile(new File(new File(dataDirectory, MIGRATION_DIRECTORY),
                            MIGRATION_ACTIVATION_REQUEST_FILE), "requested\n");
                    runOnUiThread(() -> {
                        progress.dismiss();
                        new AlertDialog.Builder(this)
                                .setTitle("Zelda64 data is ready")
                                .setMessage("Close and reopen the app to finish importing.")
                                .setCancelable(false)
                                .setPositiveButton("Close app", (dialog, which) -> finish())
                                .show();
                    });
                    return;
                }
                String message = "Staging complete: "
                        + (totals.copiedFiles + totals.resumedFiles) + " verified files, "
                        + formatByteCount(totals.verifiedBytes) + ".";
                if (totals.failedFiles != 0) {
                    message += " " + totals.failedFiles + " failed; retry to resume.";
                } else {
                    message += " Live data was not replaced.";
                }
                String finalMessage = message;
                runOnUiThread(() -> Toast.makeText(this, finalMessage, Toast.LENGTH_LONG).show());
            } catch (Exception exception) {
                Log.e(TAG, "Unable to stage the selected existing-data folder", exception);
                runOnUiThread(() -> {
                    if (upgrading) {
                        progress.dismiss();
                        new AlertDialog.Builder(this)
                                .setTitle("Zelda64 import incomplete")
                                .setMessage("Your original files are available.")
                                .setPositiveButton("OK", (dialog, which) -> offerUpgradeDataImport())
                                .setCancelable(false)
                                .show();
                    } else {
                        Toast.makeText(this, "Unable to stage the selected folder.",
                                Toast.LENGTH_LONG).show();
                    }
                });
            } finally {
                legacyStagingInProgress = false;
            }
        }, "ZeldaLegacyStaging").start();
    }

    private static final class MigrationRecord {
        final long size;
        final long lastModified;
        final String sha256;

        MigrationRecord(long size, long lastModified, String sha256) {
            this.size = size;
            this.lastModified = lastModified;
            this.sha256 = sha256;
        }
    }

    private static final class MigrationCopyTotals {
        long copiedFiles;
        long resumedFiles;
        long failedFiles;
        long directories;
        long verifiedBytes;
    }

    private MigrationCopyTotals stageLegacyData(Uri treeUri) throws IOException {
        String rootDocumentId = DocumentsContract.getTreeDocumentId(treeUri);
        ArrayDeque<String[]> pending = new ArrayDeque<>();
        pending.add(new String[] {rootDocumentId, ""});

        File migrationDirectory = new File(dataDirectory, MIGRATION_DIRECTORY);
        File stagingDirectory = new File(migrationDirectory, MIGRATION_STAGING_DIRECTORY);
        if (!stagingDirectory.exists() && !stagingDirectory.mkdirs()) {
            throw new IOException("Unable to create " + stagingDirectory);
        }
        verifyMigrationSource(migrationDirectory, treeUri);
        File completionMarker = new File(
                migrationDirectory, MIGRATION_STAGING_COMPLETE_FILE);
        if (completionMarker.exists() && !completionMarker.delete()) {
            throw new IOException("Unable to reset staged-data completion marker");
        }
        File journal = new File(migrationDirectory, MIGRATION_COPY_JOURNAL_FILE);
        Map<String, MigrationRecord> verifiedRecords = readMigrationJournal(journal);
        ensureMigrationJournalHeader(journal);

        MigrationCopyTotals totals = new MigrationCopyTotals();
        while (!pending.isEmpty()) {
            String[] current = pending.removeFirst();
            Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                    treeUri, current[0]);
            String[] projection = {
                    DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    DocumentsContract.Document.COLUMN_MIME_TYPE,
                    DocumentsContract.Document.COLUMN_SIZE,
                    DocumentsContract.Document.COLUMN_LAST_MODIFIED
            };
            try (Cursor cursor = getContentResolver().query(
                    childrenUri, projection, null, null, null)) {
                if (cursor == null) {
                    throw new IOException("Unable to list a selected directory");
                }
                while (cursor.moveToNext()) {
                    String documentId = cursor.getString(0);
                    String name = cursor.getString(1);
                    String mimeType = cursor.getString(2);
                    long size = cursor.isNull(3) ? 0 : Math.max(0, cursor.getLong(3));
                    long lastModified = cursor.isNull(4) ? 0 : cursor.getLong(4);
                    String relativePath = current[1].isEmpty()
                            ? name : current[1] + "/" + name;
                    boolean isDirectory = DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType);

                    try {
                        File destination = resolveMigrationDestination(
                                stagingDirectory, relativePath);
                        if (isDirectory) {
                            if (!destination.exists() && !destination.mkdirs()) {
                                throw new IOException("Unable to create staged directory");
                            }
                            if (!destination.isDirectory()) {
                                throw new IOException("Staged path is not a directory");
                            }
                            totals.directories++;
                            pending.addLast(new String[] {documentId, relativePath});
                            continue;
                        }

                        MigrationRecord record = verifiedRecords.get(relativePath);
                        if (isVerifiedStagedFile(destination, size, lastModified, record)) {
                            totals.resumedFiles++;
                            totals.verifiedBytes += size;
                            logMigrationProgress(totals);
                            continue;
                        }

                        Uri documentUri = DocumentsContract.buildDocumentUriUsingTree(
                                treeUri, documentId);
                        String digest = copyAndVerifyMigrationFile(documentUri, destination);
                        if (destination.length() != size) {
                            throw new IOException("Staged file size does not match source metadata");
                        }
                        if (lastModified > 0) {
                            destination.setLastModified(lastModified);
                        }
                        appendMigrationJournal(journal, "verified", relativePath, size,
                                lastModified, digest, "");
                        verifiedRecords.put(relativePath,
                                new MigrationRecord(size, lastModified, digest));
                        totals.copiedFiles++;
                        totals.verifiedBytes += size;
                        logMigrationProgress(totals);
                    } catch (Exception exception) {
                        totals.failedFiles++;
                        appendMigrationJournal(journal, "failed", relativePath, size,
                                lastModified, "", exception.getMessage());
                        Log.e(TAG, "Unable to stage legacy path: " + relativePath, exception);
                        logMigrationProgress(totals);
                    }
                }
            }
        }
        if (totals.failedFiles == 0) {
            writeDurableTextFile(completionMarker,
                    "files=" + (totals.copiedFiles + totals.resumedFiles) + "\n"
                            + "directories=" + totals.directories + "\n"
                            + "bytes=" + totals.verifiedBytes + "\n");
        }
        return totals;
    }

    private static void logMigrationProgress(MigrationCopyTotals totals) {
        long processedFiles = totals.copiedFiles + totals.resumedFiles + totals.failedFiles;
        if (processedFiles != 0 && processedFiles % 100 == 0) {
            Log.i(TAG, "Legacy data staging progress: files=" + processedFiles
                    + ", bytes=" + totals.verifiedBytes
                    + ", failures=" + totals.failedFiles);
        }
    }

    private static void verifyMigrationSource(File migrationDirectory, Uri treeUri)
            throws IOException {
        File sourceFile = new File(migrationDirectory, MIGRATION_SOURCE_FILE);
        String selectedSource = treeUri.toString();
        String existingSource = readTextFile(sourceFile, null);
        if (existingSource == null) {
            writeTextFile(sourceFile, selectedSource);
            return;
        }
        if (!existingSource.equals(selectedSource)) {
            throw new IOException(
                    "Private staging already belongs to a different selected folder");
        }
    }

    private File resolveMigrationDestination(File stagingDirectory, String relativePath)
            throws IOException {
        if (relativePath == null || relativePath.isEmpty()) {
            throw new IOException("Migration path is empty");
        }
        String[] components = relativePath.split("/", -1);
        File destination = stagingDirectory;
        for (String component : components) {
            if (component.isEmpty() || component.equals(".") || component.equals("..")
                    || component.indexOf('\\') >= 0) {
                throw new IOException("Migration path contains an unsafe component");
            }
            destination = new File(destination, component);
        }
        String stagingPath = stagingDirectory.getCanonicalPath();
        String destinationPath = destination.getCanonicalPath();
        if (!destinationPath.startsWith(stagingPath + File.separator)) {
            throw new IOException("Migration path escapes private staging");
        }
        return destination;
    }

    private boolean isVerifiedStagedFile(File destination, long size, long lastModified,
            MigrationRecord record) throws IOException {
        if (record == null || record.size != size || record.lastModified != lastModified
                || !destination.isFile() || destination.length() != size) {
            return false;
        }
        return record.sha256.equals(sha256File(destination));
    }

    private String copyAndVerifyMigrationFile(Uri source, File destination) throws IOException {
        File parent = destination.getParentFile();
        if (parent == null || (!parent.exists() && !parent.mkdirs())) {
            throw new IOException("Unable to create staged parent directory");
        }
        File temporary = new File(parent, destination.getName() + ".part");
        if (temporary.exists() && !temporary.delete()) {
            throw new IOException("Unable to replace incomplete staged file");
        }

        MessageDigest sourceDigest = newSha256Digest();
        byte[] buffer = new byte[256 * 1024];
        try (InputStream input = getContentResolver().openInputStream(source);
                FileOutputStream output = new FileOutputStream(temporary, false)) {
            if (input == null) {
                throw new IOException("Unable to open source document");
            }
            int count;
            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
                sourceDigest.update(buffer, 0, count);
            }
            output.flush();
            output.getFD().sync();
        } catch (IOException exception) {
            temporary.delete();
            throw exception;
        }

        String sourceHash = bytesToHex(sourceDigest.digest());
        String stagedHash = sha256File(temporary);
        if (!sourceHash.equals(stagedHash)) {
            temporary.delete();
            throw new IOException("Staged file digest does not match source");
        }
        if (destination.exists() && !destination.delete()) {
            temporary.delete();
            throw new IOException("Unable to replace previously staged file");
        }
        if (!temporary.renameTo(destination)) {
            temporary.delete();
            throw new IOException("Unable to finish staged file");
        }
        return sourceHash;
    }

    private static MessageDigest newSha256Digest() throws IOException {
        try {
            return MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException exception) {
            throw new IOException("SHA-256 is unavailable", exception);
        }
    }

    private static String sha256File(File file) throws IOException {
        MessageDigest digest = newSha256Digest();
        byte[] buffer = new byte[256 * 1024];
        try (InputStream input = new FileInputStream(file)) {
            int count;
            while ((count = input.read(buffer)) != -1) {
                digest.update(buffer, 0, count);
            }
        }
        return bytesToHex(digest.digest());
    }

    private static String bytesToHex(byte[] bytes) {
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            result.append(String.format(Locale.US, "%02x", value & 0xff));
        }
        return result.toString();
    }

    private static Map<String, MigrationRecord> readMigrationJournal(File journal)
            throws IOException {
        Map<String, MigrationRecord> records = new HashMap<>();
        if (!journal.isFile()) {
            return records;
        }
        try (BufferedReader reader = new BufferedReader(new FileReader(journal))) {
            String line;
            while ((line = reader.readLine()) != null) {
                String[] fields = line.split("\\t", -1);
                if (fields.length < 6 || !fields[0].equals("verified")) {
                    continue;
                }
                try {
                    String path = unescapeInventoryField(fields[1]);
                    long size = Long.parseLong(fields[2]);
                    long lastModified = Long.parseLong(fields[3]);
                    if (fields[4].matches("[0-9a-f]{64}")) {
                        records.put(path, new MigrationRecord(size, lastModified, fields[4]));
                    }
                } catch (IllegalArgumentException ignored) {
                    // Ignore an incomplete final line and retry that file.
                }
            }
        }
        return records;
    }

    private static void ensureMigrationJournalHeader(File journal) throws IOException {
        if (journal.isFile() && journal.length() != 0) {
            return;
        }
        appendMigrationJournalLine(journal,
                "status\tpath\tsize\tlast_modified\tsha256\terror\n");
    }

    private static void appendMigrationJournal(File journal, String status, String path,
            long size, long lastModified, String sha256, String error) throws IOException {
        String line = status + '\t' + escapeInventoryField(path) + '\t' + size + '\t'
                + lastModified + '\t' + sha256 + '\t'
                + escapeInventoryField(error == null ? "" : error) + '\n';
        appendMigrationJournalLine(journal, line);
    }

    private static void appendMigrationJournalLine(File journal, String line) throws IOException {
        File parent = journal.getParentFile();
        if (parent == null || (!parent.exists() && !parent.mkdirs())) {
            throw new IOException("Unable to create migration journal directory");
        }
        try (FileOutputStream output = new FileOutputStream(journal, true)) {
            output.write(line.getBytes(StandardCharsets.UTF_8));
            output.flush();
            output.getFD().sync();
        }
    }

    private void activateStagedMigrationIfRequested() throws IOException {
        File migrationDirectory = new File(dataDirectory, MIGRATION_DIRECTORY);
        File request = new File(migrationDirectory, MIGRATION_ACTIVATION_REQUEST_FILE);
        if (!request.isFile()) {
            return;
        }

        File stagingDirectory = new File(migrationDirectory, MIGRATION_STAGING_DIRECTORY);
        File copyJournal = new File(migrationDirectory, MIGRATION_COPY_JOURNAL_FILE);
        File stagingComplete = new File(
                migrationDirectory, MIGRATION_STAGING_COMPLETE_FILE);
        if (!stagingDirectory.isDirectory() || !copyJournal.isFile()
                || !stagingComplete.isFile()) {
            throw new IOException("Requested migration activation has no staged data");
        }

        Map<String, MigrationRecord> verifiedRecords = readMigrationJournal(copyJournal);
        if (verifiedRecords.isEmpty()) {
            throw new IOException("Requested migration activation has no verified files");
        }

        File backupDirectory = new File(migrationDirectory, MIGRATION_BACKUP_DIRECTORY);
        if (!backupDirectory.exists() && !backupDirectory.mkdirs()) {
            throw new IOException("Unable to create migration backup directory");
        }
        File activationJournal = new File(
                migrationDirectory, MIGRATION_ACTIVATION_JOURNAL_FILE);
        Set<String> completedPaths = readActivationJournal(activationJournal);
        ensureActivationJournalHeader(activationJournal);

        long activatedFiles = 0;
        long preservedFiles = 0;
        for (Map.Entry<String, MigrationRecord> entry : verifiedRecords.entrySet()) {
            String relativePath = entry.getKey();
            MigrationRecord record = entry.getValue();
            if (completedPaths.contains(relativePath)) {
                continue;
            }
            if (isProtectedMigrationPath(relativePath)) {
                appendActivationJournal(
                        activationJournal, "preserved", relativePath, record.sha256);
                completedPaths.add(relativePath);
                preservedFiles++;
                continue;
            }

            File stagedFile = resolveMigrationDestination(stagingDirectory, relativePath);
            File liveFile = resolveLiveMigrationTarget(relativePath);
            if (!stagedFile.isFile()) {
                if (liveFile.isFile() && liveFile.length() == record.size
                        && record.sha256.equals(sha256File(liveFile))) {
                    appendActivationJournal(
                            activationJournal, "activated", relativePath, record.sha256);
                    completedPaths.add(relativePath);
                    activatedFiles++;
                    continue;
                }
                throw new IOException("Verified staged file is missing: " + relativePath);
            }
            if (stagedFile.length() != record.size) {
                throw new IOException("Verified staged file size changed: " + relativePath);
            }

            File liveParent = liveFile.getParentFile();
            if (liveParent == null || (!liveParent.exists() && !liveParent.mkdirs())) {
                throw new IOException("Unable to create live migration directory");
            }
            if (liveFile.exists()) {
                File backupFile = resolveMigrationDestination(backupDirectory, relativePath);
                File backupParent = backupFile.getParentFile();
                if (backupParent == null
                        || (!backupParent.exists() && !backupParent.mkdirs())) {
                    throw new IOException("Unable to create migration backup path");
                }
                if (backupFile.exists()) {
                    throw new IOException("Both live and backup files exist: " + relativePath);
                }
                if (!liveFile.renameTo(backupFile)) {
                    throw new IOException("Unable to back up live file: " + relativePath);
                }
            }
            if (!stagedFile.renameTo(liveFile)) {
                throw new IOException("Unable to activate staged file: " + relativePath);
            }
            if (record.lastModified > 0) {
                liveFile.setLastModified(record.lastModified);
            }
            appendActivationJournal(
                    activationJournal, "activated", relativePath, record.sha256);
            completedPaths.add(relativePath);
            activatedFiles++;
            long processed = activatedFiles + preservedFiles;
            if (processed % 500 == 0) {
                Log.i(TAG, "Legacy data activation progress: processed=" + processed);
            }
        }

        writeDurableTextFile(new File(migrationDirectory,
                MIGRATION_ACTIVATION_COMPLETE_FILE),
                "activated=" + activatedFiles + "\npreserved=" + preservedFiles + "\n");
        if (!request.delete()) {
            Log.w(TAG, "Unable to remove completed migration activation request");
        }
        Log.i(TAG, "Legacy data activation complete: activated=" + activatedFiles
                + ", preserved=" + preservedFiles);
    }

    private File resolveLiveMigrationTarget(String relativePath) throws IOException {
        if (relativePath.equals(MIGRATION_DIRECTORY)
                || relativePath.startsWith(MIGRATION_DIRECTORY + "/")) {
            throw new IOException("Legacy data cannot replace migration state");
        }
        return resolveMigrationDestination(dataDirectory, relativePath);
    }

    private static boolean isProtectedMigrationPath(String relativePath) {
        for (String fileName : BUNDLED_NATIVE_MOD_FILES) {
            if (relativePath.equals("mods/" + fileName)) {
                return true;
            }
        }
        return false;
    }

    private static Set<String> readActivationJournal(File journal) throws IOException {
        Set<String> completedPaths = new HashSet<>();
        if (!journal.isFile()) {
            return completedPaths;
        }
        try (BufferedReader reader = new BufferedReader(new FileReader(journal))) {
            String line;
            while ((line = reader.readLine()) != null) {
                String[] fields = line.split("\\t", -1);
                if (fields.length >= 3
                        && (fields[0].equals("activated") || fields[0].equals("preserved"))) {
                    completedPaths.add(unescapeInventoryField(fields[1]));
                }
            }
        }
        return completedPaths;
    }

    private static void ensureActivationJournalHeader(File journal) throws IOException {
        if (journal.isFile() && journal.length() != 0) {
            return;
        }
        appendMigrationJournalLine(journal, "status\tpath\tsha256\n");
    }

    private static void appendActivationJournal(
            File journal, String status, String path, String sha256) throws IOException {
        appendMigrationJournalLine(journal,
                status + '\t' + escapeInventoryField(path) + '\t' + sha256 + '\n');
    }

    private static void writeDurableTextFile(File file, String value) throws IOException {
        File parent = file.getParentFile();
        if (parent == null || (!parent.exists() && !parent.mkdirs())) {
            throw new IOException("Unable to create parent directory for " + file);
        }
        try (FileOutputStream output = new FileOutputStream(file, false)) {
            output.write((value == null ? "" : value).getBytes(StandardCharsets.UTF_8));
            output.flush();
            output.getFD().sync();
        }
    }

    private long[] writeLegacyDataInventory(Uri treeUri) throws IOException {
        String rootDocumentId = DocumentsContract.getTreeDocumentId(treeUri);
        ArrayDeque<String[]> pending = new ArrayDeque<>();
        pending.add(new String[] {rootDocumentId, ""});

        File directory = new File(dataDirectory, MIGRATION_DIRECTORY);
        if (!directory.exists() && !directory.mkdirs()) {
            throw new IOException("Unable to create " + directory);
        }
        File destination = new File(directory, LEGACY_INVENTORY_FILE);
        File temporary = new File(directory, LEGACY_INVENTORY_FILE + ".tmp");

        long fileCount = 0;
        long directoryCount = 0;
        long totalBytes = 0;
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(temporary, false))) {
            writer.write("type\tpath\tsize\tlast_modified\tmime_type\n");
            while (!pending.isEmpty()) {
                String[] current = pending.removeFirst();
                Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                        treeUri, current[0]);
                String[] projection = {
                        DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                        DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                        DocumentsContract.Document.COLUMN_MIME_TYPE,
                        DocumentsContract.Document.COLUMN_SIZE,
                        DocumentsContract.Document.COLUMN_LAST_MODIFIED
                };
                try (Cursor cursor = getContentResolver().query(
                        childrenUri, projection, null, null, null)) {
                    if (cursor == null) {
                        throw new IOException("Unable to list a selected directory");
                    }
                    while (cursor.moveToNext()) {
                        String documentId = cursor.getString(0);
                        String name = cursor.getString(1);
                        String mimeType = cursor.getString(2);
                        long size = cursor.isNull(3) ? 0 : cursor.getLong(3);
                        long lastModified = cursor.isNull(4) ? 0 : cursor.getLong(4);
                        String relativePath = current[1].isEmpty()
                                ? name : current[1] + "/" + name;
                        boolean isDirectory = DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType);

                        writer.write(isDirectory ? "directory" : "file");
                        writer.write('\t');
                        writer.write(escapeInventoryField(relativePath));
                        writer.write('\t');
                        writer.write(Long.toString(size));
                        writer.write('\t');
                        writer.write(Long.toString(lastModified));
                        writer.write('\t');
                        writer.write(escapeInventoryField(mimeType));
                        writer.write('\n');

                        if (isDirectory) {
                            directoryCount++;
                            pending.addLast(new String[] {documentId, relativePath});
                        } else {
                            fileCount++;
                            totalBytes += Math.max(0, size);
                        }
                    }
                }
            }
            writer.write("summary\tfiles\t" + fileCount + "\tdirectories\t" + directoryCount + "\n");
            writer.write("summary\ttotal_bytes\t" + totalBytes + "\t\t\n");
        } catch (IOException | RuntimeException exception) {
            temporary.delete();
            throw exception;
        }

        if (destination.exists() && !destination.delete()) {
            temporary.delete();
            throw new IOException("Unable to replace " + destination);
        }
        if (!temporary.renameTo(destination)) {
            temporary.delete();
            throw new IOException("Unable to finish " + destination);
        }
        return new long[] {fileCount, totalBytes, directoryCount};
    }

    private static String escapeInventoryField(String value) {
        if (value == null) {
            return "";
        }
        return value.replace("\\", "\\\\")
                .replace("\t", "\\t")
                .replace("\r", "\\r")
                .replace("\n", "\\n");
    }

    private static String unescapeInventoryField(String value) {
        StringBuilder result = new StringBuilder(value.length());
        boolean escaped = false;
        for (int index = 0; index < value.length(); index++) {
            char character = value.charAt(index);
            if (!escaped) {
                if (character == '\\') {
                    escaped = true;
                } else {
                    result.append(character);
                }
                continue;
            }
            switch (character) {
                case 't':
                    result.append('\t');
                    break;
                case 'r':
                    result.append('\r');
                    break;
                case 'n':
                    result.append('\n');
                    break;
                default:
                    result.append(character);
                    break;
            }
            escaped = false;
        }
        if (escaped) {
            result.append('\\');
        }
        return result.toString();
    }

    private static String formatByteCount(long bytes) {
        if (bytes < 1024) {
            return bytes + " B";
        }
        double value = bytes;
        String[] units = {"KiB", "MiB", "GiB", "TiB"};
        int unit = -1;
        do {
            value /= 1024.0;
            unit++;
        } while (value >= 1024.0 && unit < units.length - 1);
        return String.format(Locale.US, "%.1f %s", value, units[unit]);
    }

    private void applyCustomDriverEnvironment() {
        if (selectedCustomDriverName == null || selectedCustomDriverName.isEmpty()) {
            nativeSetenv("APP_CUSTOM_VULKAN_DRIVER_DIR", "");
            nativeSetenv("APP_CUSTOM_VULKAN_DRIVER_NAME", "");
            nativeSetenv("APP_CUSTOM_VULKAN_DRIVER_DISPLAY_NAME", "");
            return;
        }

        nativeSetenv("APP_CUSTOM_VULKAN_DRIVER_DIR", customDriverDirectory.getAbsolutePath() + File.separator);
        nativeSetenv("APP_CUSTOM_VULKAN_DRIVER_NAME", selectedCustomDriverName);
        nativeSetenv("APP_CUSTOM_VULKAN_DRIVER_DISPLAY_NAME",
                selectedCustomDriverDisplayName == null
                        ? selectedCustomDriverName : selectedCustomDriverDisplayName);
    }

    private void applyClockTexturePackEnvironment() {
        File directory = new File(dataDirectory, CLOCK_TEXTURE_DIRECTORY);
        File pack = new File(directory, CLOCK_TEXTURE_FILE);
        if (pack.isFile()) {
            nativeSetenv("APP_CLOCK_TEXTURE_PACK_PATH", pack.getAbsolutePath());
            nativeSetenv("APP_CLOCK_TEXTURE_PACK_DISPLAY_NAME", readTextFile(
                    new File(directory, CLOCK_TEXTURE_DISPLAY_FILE), pack.getName()));
        } else {
            nativeSetenv("APP_CLOCK_TEXTURE_PACK_PATH", "");
            nativeSetenv("APP_CLOCK_TEXTURE_PACK_DISPLAY_NAME", "Bundled");
        }
    }

    private void copyDriverZip(Uri uri, ArrayList<String> libraries) throws IOException {
        try (InputStream rawInput = getContentResolver().openInputStream(uri)) {
            if (rawInput == null) {
                throw new IOException("The selected driver archive could not be opened");
            }
            try (ZipInputStream input = new ZipInputStream(rawInput)) {
                ZipEntry entry;
                while ((entry = input.getNextEntry()) != null) {
                    String name = sanitizeFileName(new File(entry.getName()).getName());
                    if (entry.isDirectory() || !name.toLowerCase(Locale.US).endsWith(".so")) {
                        continue;
                    }
                    File destination = new File(customDriverDirectory, name);
                    copyStream(input, destination);
                    destination.setReadable(true, true);
                    destination.setExecutable(true, true);
                    libraries.add(name);
                }
            }
        }
    }

    private String copyDriverLibrary(Uri uri, String displayName) throws IOException {
        String name = displayName.toLowerCase(Locale.US).endsWith(".so")
                ? displayName : "libvulkan_freedreno.so";
        File destination = new File(customDriverDirectory, name);
        copyUri(uri, destination);
        destination.setReadable(true, true);
        destination.setExecutable(true, true);
        return name;
    }

    private static String chooseDriverLibrary(ArrayList<String> libraries) {
        for (String name : libraries) {
            if ("libvulkan_freedreno.so".equals(name)) {
                return name;
            }
        }
        for (String name : libraries) {
            if (name.toLowerCase(Locale.US).contains("vulkan")) {
                return name;
            }
        }
        return libraries.isEmpty() ? null : libraries.get(0);
    }

    private void clearCustomDriverSelection() {
        selectedCustomDriverName = null;
        selectedCustomDriverDisplayName = null;
        applyCustomDriverEnvironment();
        clearCustomDriverFiles();
    }

    private void clearCustomDriverFiles() {
        deleteRecursively(customDriverDirectory);
        if (customDriverDirectory != null && !customDriverDirectory.exists()
                && !customDriverDirectory.mkdirs()) {
            Log.w(TAG, "Unable to recreate " + customDriverDirectory);
        }
    }

    private void handleModFilePickerResult(int resultCode, Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null) {
            nativeOnModFilePickerResult(new String[0]);
            return;
        }

        ArrayList<String> paths = new ArrayList<>();
        File importDirectory = new File(getCacheDir(), "mod-imports");
        try {
            if (!importDirectory.exists() && !importDirectory.mkdirs()) {
                throw new IOException("Unable to create " + importDirectory);
            }

            ClipData clipData = data.getClipData();
            if (clipData != null) {
                for (int index = 0; index < clipData.getItemCount(); index++) {
                    Uri uri = clipData.getItemAt(index).getUri();
                    if (uri != null) {
                        paths.add(copySelectedModFile(uri, importDirectory).getAbsolutePath());
                    }
                }
            } else if (data.getData() != null) {
                paths.add(copySelectedModFile(data.getData(), importDirectory).getAbsolutePath());
            }
        } catch (IOException exception) {
            Log.e(TAG, "Unable to import selected mod files", exception);
            for (String path : paths) {
                File partialFile = new File(path);
                if (!partialFile.delete()) {
                    Log.w(TAG, "Unable to remove incomplete mod import " + partialFile);
                }
            }
            paths.clear();
        }
        nativeOnModFilePickerResult(paths.toArray(new String[0]));
    }

    private File copySelectedModFile(Uri uri, File importDirectory) throws IOException {
        String displayName = getDisplayName(uri);
        File destination = uniqueFile(importDirectory, sanitizeFileName(displayName));
        try (InputStream input = getContentResolver().openInputStream(uri);
             OutputStream output = new FileOutputStream(destination)) {
            if (input == null) {
                throw new IOException("The selected mod could not be opened");
            }

            byte[] buffer = new byte[64 * 1024];
            int count;
            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
        }
        return destination;
    }

    private String getDisplayName(Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int nameColumn = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (nameColumn >= 0) {
                    String name = cursor.getString(nameColumn);
                    if (name != null && !name.isEmpty()) {
                        return name;
                    }
                }
            }
        } catch (RuntimeException exception) {
            Log.w(TAG, "Unable to read the selected mod name", exception);
        }
        String fallback = uri.getLastPathSegment();
        return fallback == null || fallback.isEmpty() ? "selected.nrm" : fallback;
    }

    private static String sanitizeFileName(String name) {
        String sanitized = name.replaceAll("[\\\\/:*?\"<>|]", "_").trim();
        return sanitized.isEmpty() ? "selected.nrm" : sanitized;
    }

    private void copyUri(Uri uri, File destination) throws IOException {
        try (InputStream input = getContentResolver().openInputStream(uri)) {
            if (input == null) {
                throw new IOException("The selected file could not be opened");
            }
            copyStream(input, destination);
        }
    }

    private static void copyStream(InputStream input, File destination) throws IOException {
        try (OutputStream output = new FileOutputStream(destination)) {
            byte[] buffer = new byte[1024 * 1024];
            int count;
            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
        }
    }

    private static void writeTextFile(File file, String value) throws IOException {
        try (FileWriter writer = new FileWriter(file, false)) {
            writer.write(value == null ? "" : value);
        }
    }

    private static String readTextFile(File file, String fallback) {
        if (file == null || !file.isFile()) {
            return fallback;
        }
        try (InputStream input = new FileInputStream(file)) {
            byte[] bytes = new byte[(int) Math.min(file.length(), 4096)];
            int count = input.read(bytes);
            if (count <= 0) {
                return fallback;
            }
            String value = new String(bytes, 0, count).trim();
            return value.isEmpty() ? fallback : value;
        } catch (IOException exception) {
            return fallback;
        }
    }

    private static void deleteRecursively(File file) {
        if (file == null || !file.exists()) {
            return;
        }
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteRecursively(child);
                }
            }
        }
        if (!file.delete()) {
            Log.w(TAG, "Unable to delete " + file);
        }
    }

    private static File uniqueFile(File directory, String name) {
        File candidate = new File(directory, name);
        if (!candidate.exists()) {
            return candidate;
        }

        int dot = name.lastIndexOf('.');
        String base = dot > 0 ? name.substring(0, dot) : name;
        String extension = dot > 0 ? name.substring(dot) : "";
        for (int suffix = 1; ; suffix++) {
            candidate = new File(directory, base + "-" + suffix + extension);
            if (!candidate.exists()) {
                return candidate;
            }
        }
    }

    private File copySelectedFile(Uri uri) throws IOException {
        File importDirectory = new File(getCacheDir(), "imports");
        if (!importDirectory.exists() && !importDirectory.mkdirs()) {
            throw new IOException("Unable to create " + importDirectory);
        }

        File temporaryFile = new File(importDirectory, "selected-file.tmp");
        File selectedFile = new File(importDirectory, "selected-file.bin");
        try {
            try (InputStream input = getContentResolver().openInputStream(uri);
                 OutputStream output = new FileOutputStream(temporaryFile)) {
                if (input == null) {
                    throw new IOException("The selected document could not be opened");
                }

                byte[] buffer = new byte[64 * 1024];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    output.write(buffer, 0, count);
                }
            }

            if (selectedFile.exists() && !selectedFile.delete()) {
                throw new IOException("Unable to replace " + selectedFile);
            }
            if (!temporaryFile.renameTo(selectedFile)) {
                throw new IOException("Unable to finish importing " + selectedFile);
            }
            return selectedFile;
        } catch (IOException exception) {
            if (temporaryFile.exists() && !temporaryFile.delete()) {
                Log.w(TAG, "Unable to remove incomplete import " + temporaryFile);
            }
            throw exception;
        }
    }

    private void copyAssetTree(String assetPath, File destination) throws IOException {
        AssetManager assets = getAssets();
        String[] children = assets.list(assetPath);
        if (children == null || children.length == 0) {
            copyAssetFile(assets, assetPath, destination);
            return;
        }

        if (!destination.exists() && !destination.mkdirs()) {
            throw new IOException("Unable to create " + destination);
        }
        for (String child : children) {
            copyAssetTree(assetPath + "/" + child, new File(destination, child));
        }
    }

    private void seedBundledNativeMods(File dataDirectory) throws IOException {
        int seededVersion = preferences.getInt(PREF_BUNDLED_NATIVE_MODS_VERSION, 0);
        if (seededVersion == BUNDLED_NATIVE_MODS_VERSION) {
            return;
        }

        File modsDirectory = new File(dataDirectory, "mods");
        if (!modsDirectory.exists() && !modsDirectory.mkdirs()) {
            throw new IOException("Unable to create " + modsDirectory);
        }

        AssetManager assets = getAssets();
        for (String fileName : BUNDLED_NATIVE_MOD_FILES) {
            File destination = new File(modsDirectory, fileName);
            File temporaryFile = new File(modsDirectory, fileName + ".tmp");
            copyAssetFile(
                    assets,
                    BUNDLED_MODS_ASSET_DIRECTORY + "/" + fileName,
                    temporaryFile);

            if (destination.exists() && !destination.delete()) {
                throw new IOException("Unable to replace " + destination);
            }
            if (!temporaryFile.renameTo(destination)) {
                throw new IOException("Unable to finish installing " + destination);
            }
        }

        boolean saved = preferences.edit()
                .putInt(PREF_BUNDLED_NATIVE_MODS_VERSION, BUNDLED_NATIVE_MODS_VERSION)
                .commit();
        if (!saved) {
            throw new IOException("Unable to record bundled native mod version");
        }
    }

    private static void copyAssetFile(AssetManager assets, String assetPath, File destination)
            throws IOException {
        File parent = destination.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Unable to create " + parent);
        }

        try (InputStream input = assets.open(assetPath);
             OutputStream output = new FileOutputStream(destination)) {
            byte[] buffer = new byte[16 * 1024];
            int count;
            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
        }
    }
}
