package io.github.zelda64recomp.cleanboot;

import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

/** Normalized, transactional layout model shared with Linkzenic's Ship touch UI. */
final class TouchLayout {
    private final Map<String, float[]> controls = new LinkedHashMap<>();
    private final Set<String> hidden = new LinkedHashSet<>();

    float[] get(String key) {
        return controls.get(key);
    }

    boolean isVisible(String key) {
        return !hidden.contains(key);
    }

    void setVisible(String key, boolean visible) {
        if (!key.matches("[a-z]{1,16}") || key.equals("eye") || key.equals("settings")) {
            return;
        }
        if (visible) {
            hidden.remove(key);
        } else {
            hidden.add(key);
        }
    }

    void clear() {
        controls.clear();
        hidden.clear();
    }

    TouchLayout copy() {
        return decode(encode());
    }

    void set(String key, float x, float y, float scale) {
        if (!key.matches("[a-z]{1,16}") || !Float.isFinite(x) || !Float.isFinite(y)
                || !Float.isFinite(scale)) {
            return;
        }
        controls.put(key, new float[] {
                Math.max(0, Math.min(1, x)),
                Math.max(0, Math.min(1, y)),
                Math.max(.5f, Math.min(2, scale))
        });
    }

    String encode() {
        StringBuilder value = new StringBuilder("v2");
        for (Map.Entry<String, float[]> entry : controls.entrySet()) {
            float[] position = entry.getValue();
            value.append(';').append(entry.getKey()).append('=')
                    .append(position[0]).append(',')
                    .append(position[1]).append(',')
                    .append(position[2]);
        }
        for (String key : hidden) {
            value.append(";!").append(key);
        }
        return value.toString();
    }

    static TouchLayout decode(String value) {
        TouchLayout result = new TouchLayout();
        if (value == null || value.length() > 4096) {
            return result;
        }
        String[] fields = value.split(";", -1);
        if (fields.length == 0 || (!fields[0].equals("v1") && !fields[0].equals("v2"))) {
            return result;
        }
        boolean supportsVisibility = fields[0].equals("v2");
        for (int index = 1; index < fields.length; index++) {
            if (supportsVisibility && fields[index].startsWith("!")) {
                result.setVisible(fields[index].substring(1), false);
                continue;
            }
            String[] pair = fields[index].split("=", -1);
            if (pair.length != 2) {
                continue;
            }
            String[] position = pair[1].split(",", -1);
            if (position.length != 3) {
                continue;
            }
            try {
                result.set(pair[0], Float.parseFloat(position[0]),
                        Float.parseFloat(position[1]), Float.parseFloat(position[2]));
            } catch (NumberFormatException ignored) {
                // Ignore malformed saved controls without discarding the remaining layout.
            }
        }
        return result;
    }

    static boolean floatingStickContains(int stick, float x, float y) {
        if (!Float.isFinite(x) || !Float.isFinite(y) || x < 0 || x > 1 || y < 0 || y > 1) {
            return false;
        }
        return stick == 0 ? x < .5f : stick == 1 && x >= .5f;
    }

    static float resizedScale(float original, float width, float height,
                              float requestedWidth, float requestedHeight) {
        if (!Float.isFinite(original) || !Float.isFinite(width) || !Float.isFinite(height)
                || !Float.isFinite(requestedWidth) || !Float.isFinite(requestedHeight)
                || width <= 0 || height <= 0) {
            return 1;
        }
        float ratio = (requestedWidth * width + requestedHeight * height)
                / (width * width + height * height);
        return Math.max(.5f, Math.min(2, original * ratio));
    }
}
