# Shared environment for the Android tasks. Source it, don't run it:
#   . Platforms/Android/env.sh
# Everything can be overridden from outside; the defaults match a stock Android Studio install.

export ANDROID_HOME="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}}"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-$ANDROID_HOME/ndk/28.2.13676358}"
export PATH="$PATH:$ANDROID_HOME/platform-tools"

# gradle needs a JDK 17: take the first candidate that is actually installed
for o2_jdk in "${JAVA_HOME:-}" \
              "/Applications/Android Studio.app/Contents/jbr/Contents/Home" \
              "$(/usr/libexec/java_home -v 17 2>/dev/null)" \
              "$(brew --prefix openjdk@17 2>/dev/null)/libexec/openjdk.jdk/Contents/Home"; do
    if [ -x "$o2_jdk/bin/java" ]; then
        export JAVA_HOME="$o2_jdk"
        break
    fi
done
unset o2_jdk

[ -x "${JAVA_HOME:-}/bin/java" ] || echo "android env: no JDK 17 found, set JAVA_HOME or 'brew install openjdk@17'" >&2
[ -d "$ANDROID_HOME" ] || echo "android env: no SDK at $ANDROID_HOME, set ANDROID_HOME" >&2
