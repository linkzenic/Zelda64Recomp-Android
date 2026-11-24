package io.github.zelda64recomp;

import android.content.Context;
import android.util.Log;

import com.chaquo.python.PyObject;
import com.chaquo.python.Python;
import com.chaquo.python.android.AndroidPlatform;

public final class RandoGenerator {
    private static final String TAG = "ZeldaRando";
    private static Context appContext;

    private RandoGenerator() {
    }

    public static void init(Context context) {
        appContext = context.getApplicationContext();
    }

    public static boolean generate(String yamlDirectory, String outputDirectory) {
        try {
            if (appContext == null) {
                Log.e(TAG, "Cannot generate a randomizer seed before the app is ready");
                return false;
            }
            if (!Python.isStarted()) {
                Python.start(new AndroidPlatform(appContext));
            }

            PyObject result = Python.getInstance()
                    .getModule("rando_generator")
                    .callAttr("generate", yamlDirectory, outputDirectory);
            return result.toBoolean();
        } catch (Throwable throwable) {
            Log.e(TAG, "Randomizer seed generation failed", throwable);
            return false;
        }
    }
}
