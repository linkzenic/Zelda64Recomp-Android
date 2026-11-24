package io.github.zelda64recomp.cleanboot;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.util.SparseArray;
import android.util.SparseBooleanArray;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;

import java.util.ArrayList;
import java.util.Arrays;

import org.libsdl.app.SDLActivity;

/**
 * Zelda64 adapter for the integrated touch controller used by Linkzenic's
 * Ship of Harkinian Android port.
 */
final class LinkzenicTouchView extends View {
    static final int STYLE_CLASSIC = 0;
    static final int STYLE_N64 = 1;
    static final int STYLE_GAMECUBE = 2;
    static final int STYLE_MODERN = 3;

    private static final String PREFERENCES_NAME = "linkzenic-integrated-touch-layouts";
    private static final int NO_INPUT = -1;
    private static final int HUD_GAMEPLAY = 1 << 10;

    private final CleanBootActivity host;
    private final SharedPreferences preferences;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Drawable modernDpad;
    private final Typeface labelTypeface;
    private final Typeface utilityTypeface;
    private final ArrayList<Control> controls = new ArrayList<>();
    private final SparseArray<Control> fingers = new SparseArray<>();
    private final SparseBooleanArray forwardedTouches = new SparseBooleanArray();
    private final RectF[] editorButtons = {new RectF(), new RectF(), new RectF(), new RectF()};
    private final float[] hudAnchors = new float[30];

    private TouchLayout placement = new TouchLayout();
    private TouchLayout savedPlacement;
    private int style = STYLE_MODERN;
    private boolean enabled;
    private boolean shown = true;
    private boolean editing;
    private boolean menuOpen;
    private boolean floatingLeft = true;
    private boolean floatingRight = true;
    private boolean transparentSticks;
    private int hudContent;
    private float scale = 1;
    private float opacity = .55f;
    private float unit = 1;
    private float safeLeft;
    private float safeTop;
    private float safeRight;
    private float safeBottom;
    private String selected = "";
    private Control dragged;
    private int dragPointer = -1;
    private float dragX;
    private float dragY;
    private int resizeCorner = -1;
    private float resizeStartX;
    private float resizeStartY;
    private float resizeWidth;
    private float resizeHeight;
    private float resizeScale;
    private float fixedX;
    private float fixedY;
    private TouchLayout gestureStart;

    private static final class Control {
        final String key;
        final String label;
        final int button;
        final int axis;
        final int color;
        final int stick;
        final RectF bounds;
        int pointer = -1;
        float centerX;
        float centerY;
        float deltaX;
        float deltaY;

        Control(String key, String label, int button, int axis, int color,
                int stick, RectF bounds) {
            this.key = key;
            this.label = label;
            this.button = button;
            this.axis = axis;
            this.color = color;
            this.stick = stick;
            this.bounds = bounds;
            centerX = bounds.centerX();
            centerY = bounds.centerY();
        }
    }

    LinkzenicTouchView(CleanBootActivity host) {
        super(host);
        this.host = host;
        preferences = host.getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE);
        modernDpad = host.getDrawable(R.drawable.linkzenic_touch_cross).mutate();
        labelTypeface = Typeface.createFromAsset(host.getAssets(), "fonts/FiraSansCondensed-Bold.ttf");
        utilityTypeface = Typeface.createFromAsset(host.getAssets(), "fonts/MaterialSymbolsRounded-Regular.ttf");
        paint.setTypeface(labelTypeface);
        setFocusable(false);
        setContentDescription("Linkzenic integrated touch controller");
        setVisibility(GONE);
    }

    void configure(int nextStyle, boolean enabled, boolean floatingLeft,
                   boolean floatingRight, float scale, float opacity,
                   boolean transparentSticks) {
        nextStyle = Math.max(STYLE_CLASSIC, Math.min(STYLE_MODERN, nextStyle));
        if (style != nextStyle) {
            cancelInputs();
            editing = false;
            style = nextStyle;
            placement = TouchLayout.decode(preferences.getString("preset" + style, ""));
            shown = true;
        }
        this.enabled = enabled && style != STYLE_CLASSIC;
        this.floatingLeft = floatingLeft;
        this.floatingRight = floatingRight;
        this.transparentSticks = transparentSticks;
        this.scale = Math.max(.75f, Math.min(1.5f, scale));
        this.opacity = Math.max(.15f, Math.min(1, opacity));
        menuOpen = this.enabled && host.isIntegratedTouchSettingsOpen();
        setVisibility(this.enabled ? VISIBLE : GONE);
        cancelInputs();
        layoutControls();
        invalidate();
    }

    void startEditing() {
        if (!enabled || editing) {
            return;
        }
        cancelInputs();
        savedPlacement = placement.copy();
        editing = true;
        menuOpen = false;
        selected = "";
        shown = true;
        layoutControls();
        invalidate();
        announceForAccessibility("Drag controls to move them. Select a control and drag a corner handle to resize.");
    }

    boolean cancelEditing() {
        if (!editing) {
            return false;
        }
        finishEditing(false);
        return true;
    }

    private void finishEditing(boolean save) {
        if (save) {
            preferences.edit().putString("preset" + style, placement.encode()).apply();
        } else if (savedPlacement != null) {
            placement = savedPlacement;
        }
        editing = false;
        menuOpen = true;
        dragged = null;
        dragPointer = -1;
        resizeCorner = -1;
        gestureStart = null;
        cancelInputs();
        layoutControls();
        invalidate();
    }

    private void add(String key, String label, int button, int axis, int color,
                     float x, float y, float width, float height, int stick) {
        float[] custom = placement.get(key);
        float controlScale = custom == null ? 1 : custom[2];
        float widthPixels = Math.min(width * unit * controlScale,
                getWidth() - safeLeft - safeRight);
        float heightPixels = Math.min(height * unit * controlScale,
                getHeight() - safeTop - safeBottom);
        float centerX = custom == null ? x * getWidth() : custom[0] * getWidth();
        float centerY = custom == null ? y * getHeight() : custom[1] * getHeight();
        centerX = Math.max(safeLeft + widthPixels / 2,
                Math.min(getWidth() - safeRight - widthPixels / 2, centerX));
        centerY = Math.max(safeTop + heightPixels / 2,
                Math.min(getHeight() - safeBottom - heightPixels / 2, centerY));
        controls.add(new Control(key, label, button, axis, color, stick,
                new RectF(centerX - widthPixels / 2, centerY - heightPixels / 2,
                        centerX + widthPixels / 2, centerY + heightPixels / 2)));
    }

    private void face(String key, String label, int button, int color,
                      float x, float y, float size) {
        add(key, label, button, NO_INPUT, color, x, y, size, size, NO_INPUT);
    }

    private void dockedFace(String key, String label, int button, int color,
                            float rightOffset, float bottomOffset, float size) {
        face(key, label, button, color,
                (getWidth() - (rightOffset + size / 2) * unit) / getWidth(),
                (getHeight() - (bottomOffset + size / 2) * unit) / getHeight(), size);
    }

    private void layoutControls() {
        controls.clear();
        if (getWidth() <= 0 || getHeight() <= 0 || !enabled) {
            host.setIntegratedTouchLayout(hudAnchors, false);
            return;
        }
        float density = getResources().getDisplayMetrics().density;
        unit = scale * density * Math.max(.82f,
                Math.min(1.35f, getHeight() / (390 * density)));
        safeLeft = safeRight = safeTop = safeBottom = (style == STYLE_MODERN ? 16 : 12) * unit;
        WindowInsets insets = getRootWindowInsets();
        if (insets != null && android.os.Build.VERSION.SDK_INT >= 30) {
            android.graphics.Insets safe = insets.getInsets(
                    WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            safeLeft += safe.left;
            safeRight += safe.right;
            safeTop += safe.top;
            safeBottom += safe.bottom;
        }

        final int gray = 0xff626b78;
        final int yellow = 0xffdfb52b;
        if (shown || editing) {
            if (style == STYLE_N64) {
                face("a", "A", ControllerButtons.BUTTON_A, 0xff346bcc, .79f, .80f, 52);
                face("b", "B", ControllerButtons.BUTTON_B, 0xff38a05f, .70f, .69f, 44);
                face("cl", "◀", ControllerButtons.BUTTON_X, yellow, .84f, .54f, 36);
                face("cd", "▼", ControllerButtons.BUTTON_Y, yellow, .90f, .66f, 36);
                face("cr", "▶", ControllerButtons.BUTTON_RB, yellow, .96f, .54f, 36);
                face("cu", "▲", ControllerButtons.BUTTON_LB, yellow, .90f, .42f, 36);
                add("z", "Z", NO_INPUT, ControllerButtons.AXIS_LT, gray, .10f, .13f, 64, 32, NO_INPUT);
                add("l", "L", ControllerButtons.BUTTON_LB, NO_INPUT, gray, .10f, .25f, 64, 30, NO_INPUT);
                add("r", "R", ControllerButtons.BUTTON_RB, NO_INPUT, gray, .90f, .13f, 64, 32, NO_INPUT);
            } else if (style == STYLE_GAMECUBE) {
                dockedFace("a", "A", ControllerButtons.BUTTON_A, 0xff22707b, 62, 64, 74);
                dockedFace("b", "B", ControllerButtons.BUTTON_B, 0xffa13d42, 158, 48, 58);
                dockedFace("y", "Y", ControllerButtons.BUTTON_Y, 0xff715b96, 124, 138, 58);
                dockedFace("x", "X", ControllerButtons.BUTTON_X, 0xff537397, 28, 144, 58);
                float triggerX = (getWidth() - 63 * unit) / getWidth();
                add("zr", "Z", ControllerButtons.BUTTON_RB, NO_INPUT, 0xff764f9e,
                        triggerX, 95 * unit / getHeight(), 78, 46, NO_INPUT);
                add("zl", "L", NO_INPUT, ControllerButtons.AXIS_LT, gray,
                        63 * unit / getWidth(), 41 * unit / getHeight(), 78, 46, NO_INPUT);
                add("r", "R", NO_INPUT, ControllerButtons.AXIS_RT, gray,
                        triggerX, 41 * unit / getHeight(), 78, 46, NO_INPUT);
            } else {
                float centerX = getWidth() - safeRight - 93 * unit;
                float centerY = getHeight() - safeBottom - 93 * unit;
                face("a", "A", ControllerButtons.BUTTON_A, 0xff426e54,
                        (centerX + 44 * unit) / getWidth(), centerY / getHeight(), 44);
                face("b", "B", ControllerButtons.BUTTON_B, 0xff85535b,
                        centerX / getWidth(), (centerY + 44 * unit) / getHeight(), 44);
                face("x", "X", ControllerButtons.BUTTON_X, 0xff4d6d91,
                        centerX / getWidth(), (centerY - 44 * unit) / getHeight(), 44);
                face("y", "Y", ControllerButtons.BUTTON_Y, 0xff918157,
                        (centerX - 44 * unit) / getWidth(), centerY / getHeight(), 44);
                float leftX = (safeLeft + 31.5f * unit) / getWidth();
                float rightX = (getWidth() - safeRight - 31.5f * unit) / getWidth();
                add("zl", "ZL", NO_INPUT, ControllerButtons.AXIS_LT, gray,
                        leftX, (safeTop + 17.25f * unit) / getHeight(), 63, 34.5f, NO_INPUT);
                add("l", "L", ControllerButtons.BUTTON_LB, NO_INPUT, gray,
                        leftX, (safeTop + 61.75f * unit) / getHeight(), 63, 34.5f, NO_INPUT);
                add("zr", "ZR", NO_INPUT, ControllerButtons.AXIS_RT, gray,
                        rightX, (safeTop + 17.25f * unit) / getHeight(), 63, 34.5f, NO_INPUT);
                add("r", "R", ControllerButtons.BUTTON_RB, NO_INPUT, gray,
                        rightX, (safeTop + 61.75f * unit) / getHeight(), 63, 34.5f, NO_INPUT);
            }

            float dpadX = usesDpadCross()
                    ? (safeLeft + 48 * unit) / getWidth() : .12f;
            float dpadY = usesDpadCross()
                    ? Math.max(safeTop + 162 * unit,
                    getHeight() - safeBottom - 204 * unit) / getHeight() : .51f;
            float dpadSize = usesDpadCross() ? 32 : 28;
            float dpadOffsetX = usesDpadCross() ? 32 * unit / getWidth() : .045f;
            float dpadOffsetY = usesDpadCross() ? 32 * unit / getHeight() : .08f;
            face("du", "▲", ControllerButtons.BUTTON_DPAD_UP, gray,
                    dpadX, dpadY - dpadOffsetY, dpadSize);
            face("dd", "▼", ControllerButtons.BUTTON_DPAD_DOWN, gray,
                    dpadX, dpadY + dpadOffsetY, dpadSize);
            face("dl", "◀", ControllerButtons.BUTTON_DPAD_LEFT, gray,
                    dpadX - dpadOffsetX, dpadY, dpadSize);
            face("dr", "▶", ControllerButtons.BUTTON_DPAD_RIGHT, gray,
                    dpadX + dpadOffsetX, dpadY, dpadSize);

            float footerHeight = 28;
            float startWidth = 66;
            float settingsWidth = 72;
            float iconWidth = 36;
            float startX = .54f;
            float settingsX = .42f;
            // Leave space for the 3DS clock during gameplay. Keep the usual
            // footer position on the launcher and file-select screens.
            boolean gameplayFooter = editing || (hudContent & HUD_GAMEPLAY) != 0;
            float footerY = gameplayFooter
                    ? .863f - footerHeight * unit / (2 * getHeight())
                    : .91f;
            float footerUnitX = unit / getWidth();
            float footerGap = Math.max(8 * footerUnitX,
                    startX - settingsX - (startWidth + settingsWidth) * footerUnitX / 2);
            float mapX = startX + (startWidth + iconWidth) * footerUnitX / 2 + footerGap;
            float fairyX = settingsX - (settingsWidth + iconWidth) * footerUnitX / 2 - footerGap;
            boolean hasFairy = style != STYLE_N64;
            float footerLeft = hasFairy ? fairyX - iconWidth * footerUnitX / 2
                    : settingsX - settingsWidth * footerUnitX / 2;
            float footerRight = mapX + iconWidth * footerUnitX / 2;
            float footerShift = gameplayFooter ? .5f - (footerLeft + footerRight) / 2 : 0;
            add("start", "Start", ControllerButtons.BUTTON_START, NO_INPUT, gray,
                    startX + footerShift, footerY, startWidth, footerHeight, NO_INPUT);
            add("settings", "Settings", NO_INPUT, NO_INPUT, gray,
                    settingsX + footerShift, footerY, settingsWidth, footerHeight, NO_INPUT);
            add("map", "Map", ControllerButtons.BUTTON_LB, NO_INPUT, gray,
                    mapX + footerShift, footerY, iconWidth, footerHeight, NO_INPUT);
            if (hasFairy) {
                add("fairy", "Fairy", ControllerButtons.BUTTON_RS, NO_INPUT, gray,
                        fairyX + footerShift, footerY, iconWidth, footerHeight, NO_INPUT);
            }
            add("move", "Move", NO_INPUT, NO_INPUT, gray,
                    style == STYLE_MODERN ? (safeLeft + 75 * unit) / getWidth() : .15f,
                    style == STYLE_MODERN ? (getHeight() - safeBottom - 75 * unit) / getHeight() : .80f,
                    style == STYLE_MODERN ? 150 : 132,
                    style == STYLE_MODERN ? 150 : 132, 0);
            add("look", "Look", NO_INPUT, NO_INPUT, gray,
                    style == STYLE_MODERN ? .62f : .59f,
                    style == STYLE_MODERN ? .62f : .69f,
                    style == STYLE_MODERN ? 150 : 120,
                    style == STYLE_MODERN ? 150 : 120, 1);
        }
        face("eye", "◉", NO_INPUT, gray,
                style == STYLE_MODERN ? (getWidth() - safeRight - 15 * unit) / getWidth() : .95f,
                style == STYLE_MODERN ? (getHeight() - safeBottom - 15 * unit) / getHeight() : .92f,
                style == STYLE_MODERN ? 30 : 28);
        publishHudLayout();
    }

    private int hudSlot(Control control) {
        if (control.key.equals("b")) return 0;
        if (control.key.equals("du")) return 4;
        if (control.key.equals("dd")) return 5;
        if (control.key.equals("dl")) return 6;
        if (control.key.equals("dr")) return 7;
        if (control.key.equals("fairy")) return 8;
        if (control.key.equals("a")) return 9;
        if (style == STYLE_N64) {
            if (control.key.equals("cu")) return 8;
            if (control.key.equals("cl")) return 1;
            if (control.key.equals("cd")) return 2;
            if (control.key.equals("cr")) return 3;
        } else if (style == STYLE_GAMECUBE) {
            if (control.key.equals("y")) return 1;
            if (control.key.equals("zr")) return 2;
            if (control.key.equals("x")) return 3;
        } else if (style == STYLE_MODERN) {
            if (control.key.equals("x")) return 1;
            if (control.key.equals("y")) return 2;
            if (control.key.equals("zr")) return 3;
        }
        return -1;
    }

    private void publishHudLayout() {
        Arrays.fill(hudAnchors, 0);
        if (getWidth() > 0 && getHeight() > 0) {
            for (Control control : controls) {
                int slot = hudSlot(control);
                if (slot < 0 || !placement.isVisible(control.key)) continue;
                int base = slot * 3;
                hudAnchors[base] = control.bounds.centerX() / getWidth();
                hudAnchors[base + 1] = control.bounds.centerY() / getHeight();
                hudAnchors[base + 2] = Math.min(control.bounds.width(), control.bounds.height())
                        * .76f / getHeight();
            }
        }
        host.setIntegratedTouchLayout(hudAnchors,
                enabled && shown && !editing && !menuOpen);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        cancelInputs();
        layoutControls();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (!enabled) {
            return;
        }
        boolean actualMenuOpen = !editing && host.isIntegratedTouchSettingsOpen();
        if (actualMenuOpen != menuOpen) {
            menuOpen = actualMenuOpen;
            publishHudLayout();
        }
        // Keep the overlay attached and drawing frames so it can observe Settings closing.
        if (menuOpen) {
            postInvalidateOnAnimation();
            return;
        }
        int nextHudContent = host.getIntegratedTouchContent();
        boolean gameplayChanged = ((hudContent ^ nextHudContent) & HUD_GAMEPLAY) != 0;
        hudContent = nextHudContent;
        if (gameplayChanged) {
            layoutControls();
        }
        if (editing) {
            paint.setColor(0xee080b12);
            canvas.drawRect(0, 0, getWidth(), getHeight(), paint);
        }
        if (usesDpadCross()) {
            drawDpadCross(canvas);
        }
        for (Control control : controls) {
            if (usesDpadCross() && isDpadDirection(control.key)) {
                continue;
            }
            if (!placement.isVisible(control.key) && !editing) {
                continue;
            }
            if (!editing && !isAvailableInCurrentScreen(control)) {
                continue;
            }
            boolean down = control.pointer >= 0;
            int slot = hudSlot(control);
            boolean hasContent = !editing && slot >= 0 && slot != 8 && (hudContent & (1 << slot)) != 0;
            if (control.stick >= 0 && !editing && isFloating(control) && !down) {
                continue;
            }
            float occupiedFill = style == STYLE_N64 ? .12f : .18f;
            int alpha = editing ? (placement.isVisible(control.key) ? 190 : 45)
                    : Math.round(255 * opacity * (hasContent ? occupiedFill : .70f));
            boolean transparentBase = control.stick >= 0
                    && (transparentSticks || isFloating(control)) && !editing;
            if (transparentBase) {
                alpha = 0;
            }
            paint.setStyle(Paint.Style.FILL);
            int displayColor = hasContent && style != STYLE_N64 ? 0xff626b78 : control.color;
            paint.setColor((displayColor & 0xffffff) | (alpha << 24));
            drawControlShape(canvas, control, paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth((editing && selected.equals(control.key) ? 3 : 1.5f) * unit);
            int outlineAlpha = Math.round(opacity * 255);
            paint.setColor(transparentBase ? Color.TRANSPARENT
                    : editing && selected.equals(control.key) ? 0xffffcf64
                    : editing && !placement.isVisible(control.key) ? 0xff777777
                    : (outlineAlpha << 24) | (displayColor & 0xffffff));
            drawControlShape(canvas, control, paint);

            paint.setStyle(Paint.Style.FILL);
            boolean tatlCalling = control.key.equals("cu") && (hudContent & (1 << 8)) != 0;
            paint.setColor(tatlCalling ? 0xffffdf79 : Color.WHITE);
            float buttonScale = Math.min(control.bounds.width(), control.bounds.height())
                    / ((control.key.equals("a") && style == STYLE_GAMECUBE ? 74 : 58) * unit);
            paint.setTextAlign(hasContent ? Paint.Align.RIGHT : Paint.Align.CENTER);
            boolean faceLetter = slot >= 0 && control.label.length() == 1;
            float baseSize = hasContent ? 13 : 24;
            if (!hasContent && control.key.equals("a") && style == STYLE_GAMECUBE) {
                baseSize = 31;
            }
            paint.setTextSize((faceLetter ? baseSize * buttonScale : 13) * unit);
            if ((control.key.equals("fairy") || control.key.equals("map")) && !editing) {
                drawUtilityIcon(canvas, control);
            } else if (control.stick >= 0 && !editing) {
                canvas.drawCircle(control.centerX + control.deltaX * control.bounds.width() * .34f,
                        control.centerY + control.deltaY * control.bounds.height() * .34f,
                        12 * unit, paint);
            } else {
                String label = control.label + (editing && !placement.isVisible(control.key) ? " (off)" : "");
                float textX = hasContent
                        ? control.bounds.right - 6 * unit * buttonScale : control.bounds.centerX();
                float textY = hasContent
                        ? control.bounds.bottom - 6 * unit * buttonScale - paint.descent()
                        : control.bounds.centerY() - (paint.ascent() + paint.descent()) / 2;
                canvas.drawText(label, textX, textY, paint);
            }
        }
        if (editing) {
            drawEditor(canvas);
        }
        postInvalidateOnAnimation();
    }

    private void drawControlShape(Canvas canvas, Control control, Paint controlPaint) {
        if (control.stick >= 0 || Math.abs(control.bounds.width() - control.bounds.height()) < .5f) {
            canvas.drawOval(control.bounds, controlPaint);
        } else {
            canvas.drawRoundRect(control.bounds, 12 * unit, 12 * unit, controlPaint);
        }
    }

    private void drawUtilityIcon(Canvas canvas, Control control) {
        boolean fairy = control.key.equals("fairy");
        paint.setTypeface(utilityTypeface);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(Math.min(control.bounds.width(), control.bounds.height()) * .72f);
        paint.setColor(fairy && (hudContent & (1 << 8)) != 0 ? 0xffffdf79 : Color.WHITE);
        float baseline = control.bounds.centerY() - (paint.ascent() + paint.descent()) / 2;
        canvas.drawText(fairy ? "\ue65f" : "\ue55b", control.bounds.centerX(), baseline, paint);
        paint.setTypeface(labelTypeface);
    }

    private void drawDpadCross(Canvas canvas) {
        if (!shown && !editing) {
            return;
        }
        Control up = find("du");
        Control down = find("dd");
        Control left = find("dl");
        Control right = find("dr");
        if (up == null || down == null || left == null || right == null) {
            return;
        }
        modernDpad.setBounds(Math.round(left.bounds.left), Math.round(up.bounds.top),
                Math.round(right.bounds.right), Math.round(down.bounds.bottom));
        modernDpad.setAlpha(editing ? 255 : Math.round(opacity * 255));
        modernDpad.draw(canvas);
    }

    private void drawEditor(Canvas canvas) {
        Control selectedControl = find(selected);
        if (selectedControl != null) {
            RectF frame = new RectF(selectedControl.bounds);
            frame.inset(-4 * unit, -4 * unit);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(1.5f * unit);
            paint.setColor(0xffffcf64);
            canvas.drawRect(frame, paint);
            paint.setStyle(Paint.Style.FILL);
            for (int corner = 0; corner < 4; corner++) {
                float x = (corner & 1) == 0 ? frame.left : frame.right;
                float y = (corner & 2) == 0 ? frame.top : frame.bottom;
                paint.setColor(0xff273340);
                canvas.drawCircle(x, y, 8 * unit, paint);
                paint.setStyle(Paint.Style.STROKE);
                paint.setColor(0xffffcf64);
                canvas.drawCircle(x, y, 8 * unit, paint);
                paint.setStyle(Paint.Style.FILL);
            }
        }
        boolean canHide = selectedControl != null
                && !selected.equals("settings") && !selected.equals("eye");
        String[] labels = {"Save", "Reset", "Cancel",
                canHide && !placement.isVisible(selected) ? "Show" : "Hide"};
        float panelUnit = Math.min(unit, getWidth() / 500f);
        float gap = 8 * panelUnit;
        float width = Math.min(90 * panelUnit,
                (getWidth() - safeLeft - safeRight - 3 * gap) / 4);
        float start = (getWidth() - 4 * width - 3 * gap) / 2;
        float panelY = safeTop + 32 * panelUnit;
        paint.setTextAlign(Paint.Align.CENTER);
        for (int index = 0; index < editorButtons.length; index++) {
            RectF bounds = editorButtons[index];
            bounds.set(start + index * (width + gap), panelY,
                    start + index * (width + gap) + width, panelY + 40 * panelUnit);
            paint.setColor(index == 0 ? 0xff66572c
                    : index == 3 && !canHide ? 0xff20252d : 0xff2b3a4b);
            canvas.drawRoundRect(bounds, 5 * panelUnit, 5 * panelUnit, paint);
            paint.setColor(index == 3 && !canHide ? 0xff888888 : Color.WHITE);
            paint.setTextSize(14 * panelUnit);
            canvas.drawText(labels[index], bounds.centerX(),
                    bounds.centerY() - (paint.ascent() + paint.descent()) / 2, paint);
        }
        paint.setColor(Color.WHITE);
        paint.setTextSize(12 * panelUnit);
        String help = selectedControl == null
                ? "Select a control · Drag to move · Drag corner handles to resize"
                : "Selected: " + selectedControl.label + " · "
                + (placement.isVisible(selected) ? "On" : "Off");
        canvas.drawText(help, getWidth() / 2f, panelY - 12 * panelUnit, paint);
    }

    private boolean isFloating(Control control) {
        return control.stick == 0 ? floatingLeft : control.stick == 1 && floatingRight;
    }

    private boolean usesDpadCross() {
        return style == STYLE_MODERN || style == STYLE_GAMECUBE;
    }

    private static boolean isDpadDirection(String key) {
        return key.equals("du") || key.equals("dd") || key.equals("dl") || key.equals("dr");
    }

    private boolean isAvailableInCurrentScreen(Control control) {
        return (hudContent & HUD_GAMEPLAY) != 0 ||
                (!control.key.equals("fairy") && !control.key.equals("map"));
    }

    private boolean openSettingsAt(MotionEvent event) {
        if (editing || menuOpen || event.getActionMasked() != MotionEvent.ACTION_DOWN) {
            return false;
        }
        Control settings = find("settings");
        if (settings == null || !placement.isVisible(settings.key) ||
                !settings.bounds.contains(event.getX(), event.getY()) ||
                !host.toggleIntegratedTouchSettings()) {
            return false;
        }
        menuOpen = true;
        cancelInputs();
        publishHudLayout();
        invalidate();
        return true;
    }

    private void moveStick(Control control, float x, float y) {
        float radius = Math.min(control.bounds.width(), control.bounds.height()) * .34f;
        control.deltaX = (x - control.centerX) / radius;
        control.deltaY = (y - control.centerY) / radius;
        float length = (float) Math.hypot(control.deltaX, control.deltaY);
        if (length > 1) {
            control.deltaX /= length;
            control.deltaY /= length;
        }
        int axisX = control.stick == 0 ? ControllerButtons.AXIS_LX : ControllerButtons.AXIS_RX;
        int axisY = control.stick == 0 ? ControllerButtons.AXIS_LY : ControllerButtons.AXIS_RY;
        host.setIntegratedTouchAxis(axisX, (short) Math.round(control.deltaX * Short.MAX_VALUE));
        host.setIntegratedTouchAxis(axisY, (short) Math.round(control.deltaY * Short.MAX_VALUE));
    }

    private void press(Control control, boolean down) {
        if (style == STYLE_MODERN || style == STYLE_N64) {
            int mask = gameButton(control.key);
            if (mask != 0) {
                if (!down && (control.key.equals("l") || control.key.equals("map"))) {
                    Control other = find(control.key.equals("l") ? "map" : "l");
                    if (other != null && other.pointer >= 0) return;
                }
                host.setTouchGameButton(mask, down);
                return;
            }
        }
        if (control.button != NO_INPUT) {
            host.setIntegratedTouchButton(control.button, down);
        }
        if (control.axis != NO_INPUT) {
            host.setIntegratedTouchAxis(control.axis, down ? Short.MAX_VALUE : (short) 0);
        }
    }

    private static int gameButton(String key) {
        // N64 button bits. Keep the item actions aligned with hudSlot().
        switch (key) {
            case "a": return 0x8000;
            case "b": return 0x4000;
            case "zl":
            case "z": return 0x2000;
            case "start": return 0x1000;
            case "du": return 0x0800;
            case "dd": return 0x0400;
            case "dl": return 0x0200;
            case "dr": return 0x0100;
            case "l":
            case "map": return 0x0020;
            case "r": return 0x0010;
            case "fairy":
            case "cu": return 0x0008;
            case "y":
            case "cd": return 0x0004;
            case "x":
            case "cl": return 0x0002;
            case "zr":
            case "cr": return 0x0001;
            default: return 0;
        }
    }

    private void release(Control control) {
        if (control.stick >= 0) {
            int axisX = control.stick == 0 ? ControllerButtons.AXIS_LX : ControllerButtons.AXIS_RX;
            int axisY = control.stick == 0 ? ControllerButtons.AXIS_LY : ControllerButtons.AXIS_RY;
            host.setIntegratedTouchAxis(axisX, (short) 0);
            host.setIntegratedTouchAxis(axisY, (short) 0);
        } else {
            press(control, false);
        }
        control.pointer = -1;
        control.deltaX = 0;
        control.deltaY = 0;
    }

    private void cancelInputs() {
        for (int index = 0; index < fingers.size(); index++) {
            release(fingers.valueAt(index));
        }
        fingers.clear();
        host.resetIntegratedTouchInputs();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (!enabled || (!editing && host.isIntegratedTouchSettingsOpen())) {
            return false;
        }
        if (openSettingsAt(event)) {
            return true;
        }
        if (host.isIntegratedTouchUiCapturing()) {
            int uiAction = event.getActionMasked();
            if (uiAction == MotionEvent.ACTION_MOVE) {
                for (int pointerIndex = 0; pointerIndex < event.getPointerCount(); pointerIndex++) {
                    forwardToGame(event, pointerIndex, uiAction);
                }
            } else if (uiAction == MotionEvent.ACTION_CANCEL) {
                for (int pointerIndex = 0; pointerIndex < event.getPointerCount(); pointerIndex++) {
                    forwardToGame(event, pointerIndex, MotionEvent.ACTION_UP);
                }
            } else {
                forwardToGame(event, event.getActionIndex(), uiAction);
            }
            return true;
        }
        if (editing) {
            return editorTouch(event);
        }
        int action = event.getActionMasked();
        int index = event.getActionIndex();
        int pointer = event.getPointerId(index);
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            float x = event.getX(index);
            float y = event.getY(index);
            Control chosen = null;
            for (Control control : controls) {
                if (control.stick < 0 && control.pointer < 0 && placement.isVisible(control.key)
                        && isAvailableInCurrentScreen(control) && !control.key.equals("settings")
                        && (!menuOpen || control.key.equals("eye"))
                        && control.bounds.contains(x, y)) {
                    chosen = control;
                    break;
                }
            }
            if (chosen == null) {
                for (Control control : controls) {
                    if (control.stick >= 0 && control.pointer < 0 && placement.isVisible(control.key)
                            && ((!isFloating(control) && control.bounds.contains(x, y))
                            || (isFloating(control) && TouchLayout.floatingStickContains(
                            control.stick, x / getWidth(), y / getHeight())))) {
                        chosen = control;
                        break;
                    }
                }
            }
            if (chosen == null) {
                forwardedTouches.put(pointer, true);
                forwardToGame(event, index, action);
                return true;
            }
            if (chosen.key.equals("eye")) {
                cancelInputs();
                shown = !shown;
                layoutControls();
                invalidate();
                return true;
            }
            chosen.pointer = pointer;
            fingers.put(pointer, chosen);
            if (chosen.stick >= 0) {
                if (isFloating(chosen)) {
                    chosen.centerX = x;
                    chosen.centerY = y;
                }
                moveStick(chosen, x, y);
            } else {
                press(chosen, true);
            }
            invalidate();
            return true;
        }
        if (action == MotionEvent.ACTION_MOVE) {
            for (int pointerIndex = 0; pointerIndex < event.getPointerCount(); pointerIndex++) {
                int movingPointer = event.getPointerId(pointerIndex);
                if (forwardedTouches.get(movingPointer)) {
                    forwardToGame(event, pointerIndex, action);
                }
                Control control = fingers.get(event.getPointerId(pointerIndex));
                if (control != null && control.stick >= 0) {
                    moveStick(control, event.getX(pointerIndex), event.getY(pointerIndex));
                }
            }
            invalidate();
            return fingers.size() > 0;
        }
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            if (forwardedTouches.get(pointer)) {
                forwardToGame(event, index, action);
                forwardedTouches.delete(pointer);
                return true;
            }
            Control control = fingers.get(pointer);
            if (control != null) {
                release(control);
                fingers.remove(pointer);
                invalidate();
                return true;
            }
        }
        if (action == MotionEvent.ACTION_CANCEL) {
            for (int pointerIndex = 0; pointerIndex < event.getPointerCount(); pointerIndex++) {
                int cancelledPointer = event.getPointerId(pointerIndex);
                if (forwardedTouches.get(cancelledPointer)) {
                    forwardToGame(event, pointerIndex, MotionEvent.ACTION_UP);
                }
            }
            forwardedTouches.clear();
            cancelInputs();
            layoutControls();
            return true;
        }
        return fingers.size() > 0;
    }

    private void forwardToGame(MotionEvent event, int index, int action) {
        if (host.isIntegratedTouchUiCapturing()) {
            float x = event.getX(index);
            float y = event.getY(index);
            if (action == MotionEvent.ACTION_DOWN) {
                SDLActivity.onNativeMouse(0, MotionEvent.ACTION_HOVER_MOVE, x, y, false);
            }
            int buttonState = action == MotionEvent.ACTION_DOWN ? 1 : 0;
            SDLActivity.onNativeMouse(buttonState, action, x, y, false);
            return;
        }
        int deviceId = event.getDeviceId();
        if (deviceId < 0) {
            deviceId--;
        }
        float pressure = Math.min(1, event.getPressure(index));
        SDLActivity.onNativeTouch(deviceId, event.getPointerId(index), action,
                event.getX(index) / getWidth(), event.getY(index) / getHeight(), pressure);
    }

    private boolean editorTouch(MotionEvent event) {
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            float x = event.getX();
            float y = event.getY();
            for (int index = 0; index < editorButtons.length; index++) {
                if (!editorButtons[index].contains(x, y)) {
                    continue;
                }
                if (index == 0) {
                    finishEditing(true);
                } else if (index == 1) {
                    placement.clear();
                    selected = "";
                    layoutControls();
                } else if (index == 2) {
                    finishEditing(false);
                } else {
                    toggleSelectedVisibility();
                }
                invalidate();
                return true;
            }
            dragged = null;
            resizeCorner = -1;
            gestureStart = placement.copy();
            Control current = find(selected);
            if (current != null) {
                RectF frame = new RectF(current.bounds);
                frame.inset(-4 * unit, -4 * unit);
                for (int corner = 0; corner < 4; corner++) {
                    float handleX = (corner & 1) == 0 ? frame.left : frame.right;
                    float handleY = (corner & 2) == 0 ? frame.top : frame.bottom;
                    if (Math.hypot(x - handleX, y - handleY) <= 16 * unit) {
                        dragged = current;
                        resizeCorner = corner;
                        dragPointer = event.getPointerId(0);
                        resizeStartX = x;
                        resizeStartY = y;
                        resizeWidth = current.bounds.width();
                        resizeHeight = current.bounds.height();
                        float[] custom = placement.get(selected);
                        resizeScale = custom == null ? 1 : custom[2];
                        fixedX = (corner & 1) == 0 ? current.bounds.right : current.bounds.left;
                        fixedY = (corner & 2) == 0 ? current.bounds.bottom : current.bounds.top;
                        return true;
                    }
                }
            }
            for (Control control : controls) {
                if (control.bounds.contains(x, y)) {
                    dragged = control;
                    break;
                }
            }
            if (dragged != null) {
                selected = dragged.key;
                dragPointer = event.getPointerId(0);
                dragX = x - dragged.bounds.centerX();
                dragY = y - dragged.bounds.centerY();
            }
        } else if (action == MotionEvent.ACTION_MOVE && dragged != null) {
            int index = event.findPointerIndex(dragPointer);
            if (index >= 0) {
                if (resizeCorner >= 0) {
                    float directionX = (resizeCorner & 1) == 0 ? -1 : 1;
                    float directionY = (resizeCorner & 2) == 0 ? -1 : 1;
                    float nextScale = TouchLayout.resizedScale(resizeScale, resizeWidth, resizeHeight,
                            resizeWidth + directionX * (event.getX(index) - resizeStartX),
                            resizeHeight + directionY * (event.getY(index) - resizeStartY));
                    float ratio = nextScale / resizeScale;
                    placement.set(selected,
                            (fixedX + directionX * resizeWidth * ratio / 2) / getWidth(),
                            (fixedY + directionY * resizeHeight * ratio / 2) / getHeight(),
                            nextScale);
                } else {
                    float[] custom = placement.get(selected);
                    placement.set(selected,
                            (event.getX(index) - dragX) / getWidth(),
                            (event.getY(index) - dragY) / getHeight(),
                            custom == null ? 1 : custom[2]);
                }
                layoutControls();
            }
        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL
                || (action == MotionEvent.ACTION_POINTER_UP
                && event.getPointerId(event.getActionIndex()) == dragPointer)) {
            if (action == MotionEvent.ACTION_CANCEL && gestureStart != null) {
                placement = gestureStart;
                layoutControls();
            }
            dragged = null;
            dragPointer = -1;
            resizeCorner = -1;
            gestureStart = null;
        }
        invalidate();
        return true;
    }

    private void toggleSelectedVisibility() {
        Control control = find(selected);
        if (control == null || selected.equals("settings") || selected.equals("eye")) {
            return;
        }
        boolean visible = !placement.isVisible(selected);
        placement.setVisible(selected, visible);
        layoutControls();
        announceForAccessibility((visible ? "Shown " : "Hidden ") + control.label);
    }

    private Control find(String key) {
        for (Control control : controls) {
            if (control.key.equals(key)) {
                return control;
            }
        }
        return null;
    }
}
