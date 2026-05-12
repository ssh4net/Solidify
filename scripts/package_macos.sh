#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
DIST_DIR="${DIST_DIR:-"$BUILD_DIR/dist"}"
APP_NAME="${APP_NAME:-Solidify}"
BUNDLE_ID="${BUNDLE_ID:-com.erium.solidify}"
SIGN_IDENTITY="${SIGN_IDENTITY:-}"
NOTARY_PROFILE="${NOTARY_PROFILE:-}"
SKIP_NOTARY="${SKIP_NOTARY:-0}"

VERSION="$(
    sed -nE 's/^project\(Solidify .* VERSION ([0-9]+(\.[0-9]+){1,3}).*/\1/p' "$ROOT_DIR/CMakeLists.txt" | head -n 1
)"
if [[ -z "$VERSION" ]]; then
    echo "Could not read Solidify project version from CMakeLists.txt" >&2
    exit 1
fi

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "macOS packaging must run on macOS." >&2
    exit 1
fi
if [[ -z "$SIGN_IDENTITY" ]]; then
    echo "SIGN_IDENTITY is required." >&2
    echo "Example: SIGN_IDENTITY=\"Developer ID Application: Your Name (TEAMID)\" $0" >&2
    exit 1
fi
if [[ "$SKIP_NOTARY" != "1" && -z "$NOTARY_PROFILE" ]]; then
    echo "NOTARY_PROFILE is required unless SKIP_NOTARY=1." >&2
    echo "Create one with: xcrun notarytool store-credentials <profile-name> --apple-id <email> --team-id <team-id>" >&2
    exit 1
fi

EXECUTABLE="$BUILD_DIR/$APP_NAME"
CONFIG_FILE="$BUILD_DIR/sldf_config.toml"
PLIST_TEMPLATE="$ROOT_DIR/packaging/macos/Info.plist.in"
ENTITLEMENTS="$ROOT_DIR/packaging/macos/Solidify.entitlements"
ICON_SOURCE="$ROOT_DIR/Solidify/src/sldf.ico"
APP="$DIST_DIR/$APP_NAME.app"
DMG="$DIST_DIR/${APP_NAME}-${VERSION}-macos-arm64.dmg"
STAGE="$DIST_DIR/dmg-stage"
ICONSET="$DIST_DIR/Solidify.iconset"

cleanup()
{
    rm -rf "$STAGE" "$ICONSET"
}
trap cleanup EXIT

if [[ ! -x "$EXECUTABLE" ]]; then
    echo "Missing executable: $EXECUTABLE" >&2
    echo "Run: cmake --build \"$BUILD_DIR\" --config Release" >&2
    exit 1
fi
if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "Missing config file: $CONFIG_FILE" >&2
    echo "Run: cmake --build \"$BUILD_DIR\" --config Release" >&2
    exit 1
fi

rm -rf "$APP" "$STAGE" "$ICONSET"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" "$DIST_DIR" "$STAGE"

sed \
    -e "s|@APP_NAME@|$APP_NAME|g" \
    -e "s|@BUNDLE_ID@|$BUNDLE_ID|g" \
    -e "s|@VERSION@|$VERSION|g" \
    "$PLIST_TEMPLATE" > "$APP/Contents/Info.plist"

ditto --noextattr --noacl "$EXECUTABLE" "$APP/Contents/MacOS/$APP_NAME"
ditto --noextattr --noacl "$CONFIG_FILE" "$APP/Contents/Resources/sldf_config.toml"

mkdir -p "$ICONSET"
sips -s format png -z 16 16 "$ICON_SOURCE" --out "$ICONSET/icon_16x16.png" >/dev/null
sips -s format png -z 32 32 "$ICON_SOURCE" --out "$ICONSET/icon_16x16@2x.png" >/dev/null
sips -s format png -z 32 32 "$ICON_SOURCE" --out "$ICONSET/icon_32x32.png" >/dev/null
sips -s format png -z 64 64 "$ICON_SOURCE" --out "$ICONSET/icon_32x32@2x.png" >/dev/null
sips -s format png -z 128 128 "$ICON_SOURCE" --out "$ICONSET/icon_128x128.png" >/dev/null
sips -s format png -z 256 256 "$ICON_SOURCE" --out "$ICONSET/icon_128x128@2x.png" >/dev/null
sips -s format png -z 256 256 "$ICON_SOURCE" --out "$ICONSET/icon_256x256.png" >/dev/null
sips -s format png -z 512 512 "$ICON_SOURCE" --out "$ICONSET/icon_256x256@2x.png" >/dev/null
sips -s format png -z 512 512 "$ICON_SOURCE" --out "$ICONSET/icon_512x512.png" >/dev/null
sips -s format png -z 1024 1024 "$ICON_SOURCE" --out "$ICONSET/icon_512x512@2x.png" >/dev/null
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/Solidify.icns"

codesign --force \
    --sign "$SIGN_IDENTITY" \
    --options runtime \
    --timestamp \
    --entitlements "$ENTITLEMENTS" \
    "$APP"

codesign --verify --deep --strict --verbose=4 "$APP"
codesign -dv --verbose=4 "$APP"

rm -rf "$STAGE"
mkdir -p "$STAGE"
ditto --noextattr --noacl "$APP" "$STAGE/$APP_NAME.app"
ln -s /Applications "$STAGE/Applications"

hdiutil create \
    -volname "$APP_NAME Installer" \
    -srcfolder "$STAGE" \
    -ov \
    -format UDZO \
    "$DMG"

codesign --force \
    --sign "$SIGN_IDENTITY" \
    --timestamp \
    "$DMG"

codesign --verify --verbose=4 "$DMG"

if [[ "$SKIP_NOTARY" != "1" ]]; then
    xcrun notarytool submit "$DMG" \
        --keychain-profile "$NOTARY_PROFILE" \
        --wait

    xcrun stapler staple "$DMG"
    xcrun stapler validate "$DMG"
else
    echo "Skipping notarization because SKIP_NOTARY=1."
fi

echo "Created: $DMG"
